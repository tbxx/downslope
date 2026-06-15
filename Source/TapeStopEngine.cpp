#include "TapeStopEngine.h"

#include <algorithm>
#include <cmath>

void TapeStopEngine::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    // Defensive: ignore garbage from the host (eg. a 0/negative SR during shutdown).
    if (sampleRate <= 0.0 || numChannels <= 0) return;

    fs  = sampleRate;
    nCh = std::max (1, numChannels);

    ringSize = static_cast<int> (std::ceil (sampleRate * ringBufferSeconds));
    ring.assign (static_cast<size_t> (nCh),
                 std::vector<float> (static_cast<size_t> (ringSize), 0.0f));
    lpZ.assign  (static_cast<size_t> (nCh), 0.0f);
    dcX1.assign (static_cast<size_t> (nCh), 0.0f);
    dcY1.assign (static_cast<size_t> (nCh), 0.0f);

    // DC blocker pole — single-pole HP at dcBlockerCornerHz.
    dcR = 1.0f - static_cast<float> (2.0 * pi * dcBlockerCornerHz / sampleRate);

    mixSm.prepare   (sampleRate, smoothingTimeMs);
    floorSm.prepare (sampleRate, smoothingTimeMs);
    mixSm.snap   (mix);
    floorSm.snap (floorPitch);

    reset();
}

void TapeStopEngine::enterSettling()
{
    state         = State::Settling;
    settleSamples = std::max (1.0, fs * settleCrossfadeSeconds);
    settleCounter = 0.0;
}

void TapeStopEngine::reset()
{
    writeIdx      = 0;
    readPos       = 0.0;
    state         = State::Idle;
    phase         = 0.0;
    settleCounter = 0.0;
    prevTrigger   = false;
    triggerHigh   = false;
    for (auto& ch : ring) std::fill (ch.begin(), ch.end(), 0.0f);
    std::fill (lpZ.begin(),  lpZ.end(),  0.0f);
    std::fill (dcX1.begin(), dcX1.end(), 0.0f);
    std::fill (dcY1.begin(), dcY1.end(), 0.0f);
}

float TapeStopEngine::applyCurve (float x, Curve c)
{
    x = std::clamp (x, 0.0f, 1.0f);
    switch (c)
    {
        case Curve::Linear:      return x;
        case Curve::Exponential: return x * x;                          // ease-in: slow then fast
        case Curve::Logarithmic: return 1.0f - (1.0f - x) * (1.0f - x); // ease-out: fast then slow
        case Curve::SCurve:      return x * x * (3.0f - 2.0f * x);
    }
    return x;
}

float TapeStopEngine::interpolateRead (int ch, double pos) const
{
    double w = std::fmod (pos, static_cast<double> (ringSize));
    if (w < 0.0) w += static_cast<double> (ringSize);

    const int i0   = static_cast<int> (std::floor (w));
    const double f = w - static_cast<double> (i0);
    const int im1  = (i0 - 1 + ringSize) % ringSize;
    const int i1   = (i0 + 1) % ringSize;
    const int i2   = (i0 + 2) % ringSize;

    const auto& b = ring[static_cast<size_t> (ch)];
    const float y0 = b[static_cast<size_t> (im1)];
    const float y1 = b[static_cast<size_t> (i0)];
    const float y2 = b[static_cast<size_t> (i1)];
    const float y3 = b[static_cast<size_t> (i2)];

    // Catmull-Rom cubic
    const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float bc =        y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c = -0.5f * y0              + 0.5f * y2;
    const float d =                    y1;
    const float ff = static_cast<float> (f);
    return ((a * ff + bc) * ff + c) * ff + d;
}

void TapeStopEngine::processBlock (const float* const* inputs,
                                   float* const* outputs,
                                   int numChannels,
                                   int numSamples)
{
    // Hard guard against host calling us before prepare() or after release.
    // Without this, the ring/lpZ/dc vectors are empty and any index would
    // walk off the end and corrupt the heap — a common cause of host crashes
    // that appear *outside* the plugin's own stack frames.
    if (ringSize <= 0 || ring.empty() || numSamples <= 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            if (outputs[ch] != nullptr)
                std::fill_n (outputs[ch], numSamples, 0.0f);
        return;
    }

    const int channels = std::min ({ numChannels, nCh, (int) ring.size() });

    const bool rising  =  triggerHigh && ! prevTrigger;
    const bool falling = ! triggerHigh &&   prevTrigger;
    prevTrigger = triggerHigh;

    // Edge-driven state transitions
    if (rising && state == State::Idle)
    {
        state = State::Stopping;
        phase = 0.0;
        // Start a few samples *behind* the write head so the cubic interp's
        // forward look-ahead (i+1, i+2) lands on valid recently-written
        // samples instead of stale wrap-around data — inaudible, but
        // eliminates the click at trigger onset.
        const int rp = (writeIdx - interpLatencySamples + ringSize) % ringSize;
        readPos = static_cast<double> (rp);
    }

    if (falling && triggerMode == TriggerMode::Momentary
        && (state == State::Stopping || state == State::Holding))
    {
        if (returnEnabled) { state = State::Returning; phase = 0.0; }
        else                 enterSettling();
    }

    const double stopSamples   = std::max (1.0, static_cast<double> (stopTimeSec)   * fs);
    const double returnSamples = std::max (1.0, static_cast<double> (returnTimeSec) * fs);

    // Tone rolloff shaping — hoisted, depends only on the (block-rate) toneAmount.
    // The amount knob sets how steeply the cutoff collapses as the rate falls
    // (exponent on `rate`): 1 = gentle, 6 = steep, default 0.5 -> 3.5. Cutoff is
    // clamped below Nyquist so the one-pole coefficient stays well-behaved at any
    // sample rate.
    const float toneExp    = 1.0f + toneAmount * 5.0f;
    const float toneMaxCut = std::min (toneMaxCutoffHz, 0.45f * static_cast<float> (fs));
    const float twoPiOverFs = static_cast<float> (2.0 * pi / fs);

    for (int n = 0; n < numSamples; ++n)
    {
        // Smoothed user params (zipper-noise-free during automation)
        const float floorNow = floorSm.next (floorPitch);
        const float mixNow   = mixSm.next   (mix);

        // Always feed the ring with the live input (guard against non-finite
        // samples from the host — keeps the ring sane during host shutdown).
        for (int ch = 0; ch < channels; ++ch)
        {
            const float in = inputs[ch][n];
            ring[static_cast<size_t> (ch)][static_cast<size_t> (writeIdx)]
                = std::isfinite (in) ? in : 0.0f;
        }

        float rate = 1.0f;

        switch (state)
        {
            case State::Idle:
                rate = 1.0f;
                break;

            case State::Stopping:
            {
                float p = static_cast<float> (phase / stopSamples);
                if (p >= 1.0f)
                {
                    p = 1.0f;
                    if (triggerMode == TriggerMode::Momentary && triggerHigh)
                        state = State::Holding;
                    else if (returnEnabled) { state = State::Returning; phase = 0.0; }
                    else                      enterSettling();
                }
                const float shaped = applyCurve (p, stopCurve);
                rate = 1.0f - shaped * (1.0f - floorNow);
                phase += 1.0;
                break;
            }

            case State::Holding:
                rate = floorNow;
                break;

            case State::Returning:
            {
                float p = static_cast<float> (phase / returnSamples);
                if (p >= 1.0f)
                {
                    p = 1.0f;
                    enterSettling();
                }
                const float shaped = applyCurve (p, returnCurve);
                rate = floorNow + shaped * (1.0f - floorNow);
                phase += 1.0;
                break;
            }

            case State::Settling:
                rate = 1.0f;
                break;
        }

        // Amplitude fade when the read rate gets very low (so floor=0 ends in silence, not DC)
        float amp = 1.0f;
        if (rate < ampFadeThreshold)
            amp = std::max (0.0f, rate / ampFadeThreshold);

        if (state != State::Idle)
            readPos += rate;
        else
            readPos = static_cast<double> (writeIdx);

        // Tone rolloff coefficient — onepole LP whose cutoff follows the playback
        // rate, emulating a tape losing high end as it slows. At full speed the
        // cutoff is open; as `rate` falls the cutoff collapses toward
        // toneMinCutoffHz, so the darkening is clearly audible well before the
        // rate drops into the amp-fade region near the floor.
        float tCoeff;
        if (! toneRolloff)
            tCoeff = 1.0f;
        else
        {
            const float cut = std::clamp (toneMaxCut * std::pow (std::max (0.0001f, rate), toneExp),
                                          toneMinCutoffHz, toneMaxCut);
            tCoeff = 1.0f - std::exp (-twoPiOverFs * cut);
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            const float dry = inputs[ch][n];
            float wet;

            if (state == State::Idle)
            {
                wet = dry;
            }
            else
            {
                const float r = interpolateRead (ch, readPos);
                float& z = lpZ[static_cast<size_t> (ch)];
                z += tCoeff * (r - z);
                wet = z * amp;
            }

            if (state == State::Settling)
            {
                const float t = std::min (1.0f, static_cast<float> (settleCounter / settleSamples));
                wet = wet * (1.0f - t) + dry * t;
            }

            // DC blocker on the wet path (cheap insurance against held DC)
            {
                float& x1 = dcX1[static_cast<size_t> (ch)];
                float& y1 = dcY1[static_cast<size_t> (ch)];
                const float y = wet - x1 + dcR * y1;
                x1 = wet;
                y1 = y;
                wet = y;
            }

            float out = dry * (1.0f - mixNow) + wet * mixNow;
            // Final NaN/inf safety
            if (! std::isfinite (out)) out = 0.0f;
            outputs[ch][n] = out;
        }

        if (state == State::Settling)
        {
            settleCounter += 1.0;
            if (settleCounter >= settleSamples)
            {
                state   = State::Idle;
                readPos = static_cast<double> (writeIdx);
                std::fill (lpZ.begin(), lpZ.end(), 0.0f);
            }
        }

        writeIdx = (writeIdx + 1) % ringSize;
    }
}
