#include "TapeStopEngine.h"

#include <algorithm>
#include <cmath>

void TapeStopEngine::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    fs  = sampleRate;
    nCh = std::max (1, numChannels);

    // 10 seconds of buffer comfortably covers any stop time we expose.
    ringSize = static_cast<int> (std::ceil (sampleRate * 10.0));
    ring.assign (static_cast<size_t> (nCh),
                 std::vector<float> (static_cast<size_t> (ringSize), 0.0f));
    lpZ.assign (static_cast<size_t> (nCh), 0.0f);

    reset();
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
    std::fill (lpZ.begin(), lpZ.end(), 0.0f);
}

float TapeStopEngine::applyCurve (float x, Curve c) const
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
    const int channels = std::min (numChannels, nCh);

    const bool rising  =  triggerHigh && ! prevTrigger;
    const bool falling = ! triggerHigh &&   prevTrigger;
    prevTrigger = triggerHigh;

    // Edge-driven state transitions
    if (rising && state == State::Idle)
    {
        state   = State::Stopping;
        phase   = 0.0;
        readPos = static_cast<double> (writeIdx);
    }

    if (falling && triggerMode == TriggerMode::Momentary
        && (state == State::Stopping || state == State::Holding))
    {
        if (returnEnabled)
        {
            state = State::Returning;
            phase = 0.0;
        }
        else
        {
            state         = State::Settling;
            settleSamples = std::max (1.0, fs * 0.03);
            settleCounter = 0.0;
        }
    }

    const double stopSamples   = std::max (1.0, static_cast<double> (stopTimeSec)   * fs);
    const double returnSamples = std::max (1.0, static_cast<double> (returnTimeSec) * fs);
    constexpr float ampFadeThreshold = 0.15f;

    for (int n = 0; n < numSamples; ++n)
    {
        // Always feed the ring with the live input
        for (int ch = 0; ch < channels; ++ch)
            ring[static_cast<size_t> (ch)][static_cast<size_t> (writeIdx)] = inputs[ch][n];

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
                    {
                        state = State::Holding;
                    }
                    else
                    {
                        if (returnEnabled) { state = State::Returning; phase = 0.0; }
                        else
                        {
                            state         = State::Settling;
                            settleSamples = std::max (1.0, fs * 0.03);
                            settleCounter = 0.0;
                        }
                    }
                }
                const float shaped = applyCurve (p, stopCurve);
                rate = 1.0f - shaped * (1.0f - floorPitch);
                phase += 1.0;
                break;
            }

            case State::Holding:
                rate = floorPitch;
                break;

            case State::Returning:
            {
                float p = static_cast<float> (phase / returnSamples);
                if (p >= 1.0f)
                {
                    p = 1.0f;
                    state         = State::Settling;
                    settleSamples = std::max (1.0, fs * 0.03);
                    settleCounter = 0.0;
                }
                const float shaped = applyCurve (p, returnCurve);
                rate = floorPitch + shaped * (1.0f - floorPitch);
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

        // Tone rolloff coefficient — onepole low-pass that closes as rate drops.
        const float tCoeff = toneRolloff ? std::max (0.02f, rate) : 1.0f;

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
                const float t  = std::min (1.0f, static_cast<float> (settleCounter / settleSamples));
                wet = wet * (1.0f - t) + dry * t;
            }

            outputs[ch][n] = dry * (1.0f - mix) + wet * mix;
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
