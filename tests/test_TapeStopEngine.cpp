// Layer 1 — unit tests for TapeStopEngine.
// Pure DSP, no JUCE. Each test instantiates an engine, drives it through a
// short sequence of blocks, and asserts on observable behaviour.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TapeStopEngine.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

namespace
{
    // Small fixture: stereo, 256-sample blocks at 48 kHz. Tests can fill the
    // input with constant values, a sine, or a custom generator and call
    // process() to advance one block.
    struct Harness
    {
        static constexpr int    blockSize  = 256;
        static constexpr int    channels   = 2;
        static constexpr double sampleRate = 48000.0;

        TapeStopEngine eng;
        std::vector<std::vector<float>> input  { (size_t) channels, std::vector<float> ((size_t) blockSize, 0.0f) };
        std::vector<std::vector<float>> output { (size_t) channels, std::vector<float> ((size_t) blockSize, 0.0f) };
        std::vector<const float*> inPtrs;
        std::vector<float*>       outPtrs;

        Harness()
        {
            for (auto& v : input)  inPtrs .push_back (v.data());
            for (auto& v : output) outPtrs.push_back (v.data());
            eng.prepare (sampleRate, blockSize, channels);
        }

        void fill (float value)
        {
            for (auto& v : input) std::fill (v.begin(), v.end(), value);
        }

        void fill (std::function<float (int)> generator)
        {
            for (auto& v : input)
                for (int i = 0; i < blockSize; ++i)
                    v[i] = generator (i);
        }

        void process()
        {
            eng.processBlock (inPtrs.data(), outPtrs.data(), channels, blockSize);
        }

        // Run N blocks while writing samples i = block * blockSize + sample_in_block
        // through the supplied generator (continuous waveforms across blocks).
        void processContinuous (int numBlocks, std::function<float (int)> globalGenerator)
        {
            for (int b = 0; b < numBlocks; ++b)
            {
                const int base = b * blockSize;
                fill ([base, &globalGenerator] (int i) { return globalGenerator (base + i); });
                process();
            }
        }
    };

    float blockPeak (const std::vector<float>& v)
    {
        float p = 0.0f;
        for (float x : v) p = std::max (p, std::abs (x));
        return p;
    }
}


// --- Curve shapes ------------------------------------------------------------

TEST_CASE ("Curve shapes have correct endpoints and midpoints")
{
    using C = TapeStopEngine::Curve;

    REQUIRE (TapeStopEngine::applyCurve (0.0f, C::Linear)      == 0.0f);
    REQUIRE (TapeStopEngine::applyCurve (1.0f, C::Linear)      == 1.0f);
    REQUIRE (TapeStopEngine::applyCurve (0.5f, C::Linear)      == Catch::Approx (0.5f));

    REQUIRE (TapeStopEngine::applyCurve (0.0f, C::Exponential) == 0.0f);
    REQUIRE (TapeStopEngine::applyCurve (1.0f, C::Exponential) == 1.0f);
    REQUIRE (TapeStopEngine::applyCurve (0.5f, C::Exponential) == Catch::Approx (0.25f));  // ease-in

    REQUIRE (TapeStopEngine::applyCurve (0.0f, C::Logarithmic) == 0.0f);
    REQUIRE (TapeStopEngine::applyCurve (1.0f, C::Logarithmic) == 1.0f);
    REQUIRE (TapeStopEngine::applyCurve (0.5f, C::Logarithmic) == Catch::Approx (0.75f));  // ease-out

    REQUIRE (TapeStopEngine::applyCurve (0.0f, C::SCurve)      == 0.0f);
    REQUIRE (TapeStopEngine::applyCurve (1.0f, C::SCurve)      == 1.0f);
    REQUIRE (TapeStopEngine::applyCurve (0.5f, C::SCurve)      == Catch::Approx (0.5f));   // symmetric
}

TEST_CASE ("Curves are monotonic non-decreasing on [0,1]")
{
    using C = TapeStopEngine::Curve;
    for (auto c : { C::Linear, C::Exponential, C::Logarithmic, C::SCurve })
    {
        float prev = -1.0f;
        for (int i = 0; i <= 100; ++i)
        {
            const float x = i / 100.0f;
            const float y = TapeStopEngine::applyCurve (x, c);
            REQUIRE (y >= prev);
            prev = y;
        }
    }
}

TEST_CASE ("Curve input is clamped to [0,1]")
{
    using C = TapeStopEngine::Curve;
    REQUIRE (TapeStopEngine::applyCurve (-0.5f, C::Linear) == 0.0f);
    REQUIRE (TapeStopEngine::applyCurve ( 1.5f, C::Linear) == 1.0f);
}


// --- State machine -----------------------------------------------------------

TEST_CASE ("Engine starts in Idle and trigger rising edge enters Stopping")
{
    Harness h;
    REQUIRE (h.eng.getState() == TapeStopEngine::State::Idle);

    h.process();
    REQUIRE (h.eng.getState() == TapeStopEngine::State::Idle);

    h.eng.setTrigger (true);
    h.process();
    REQUIRE (h.eng.getState() == TapeStopEngine::State::Stopping);
}

TEST_CASE ("Momentary mode holds at floor while trigger stays high")
{
    Harness h;
    h.eng.setStopTimeSeconds (0.02f);   // 20 ms ≈ 4 blocks
    h.eng.setTriggerMode (TapeStopEngine::TriggerMode::Momentary);
    h.eng.setFloorPitch (0.5f);

    h.eng.setTrigger (true);
    for (int i = 0; i < 20; ++i) h.process();   // well past stop time

    REQUIRE (h.eng.getState() == TapeStopEngine::State::Holding);
}

TEST_CASE ("Momentary trigger release while Holding transitions to Returning")
{
    Harness h;
    h.eng.setStopTimeSeconds (0.02f);
    h.eng.setReturnTimeSeconds (0.05f);
    h.eng.setReturnEnabled (true);
    h.eng.setTriggerMode (TapeStopEngine::TriggerMode::Momentary);

    h.eng.setTrigger (true);
    for (int i = 0; i < 20; ++i) h.process();
    REQUIRE (h.eng.getState() == TapeStopEngine::State::Holding);

    h.eng.setTrigger (false);
    h.process();
    REQUIRE (h.eng.getState() == TapeStopEngine::State::Returning);
}

TEST_CASE ("One-shot trigger runs the full envelope and returns to Idle")
{
    Harness h;
    h.eng.setStopTimeSeconds (0.05f);
    h.eng.setReturnTimeSeconds (0.02f);
    h.eng.setReturnEnabled (true);
    h.eng.setTriggerMode (TapeStopEngine::TriggerMode::OneShot);

    h.eng.setTrigger (true);
    h.process();
    h.eng.setTrigger (false); // one-shot ignores release

    // 50 ms stop + 20 ms return + ~30 ms settling ≈ 100 ms ≈ 19 blocks.
    // Run plenty extra and assert we eventually land back in Idle.
    for (int i = 0; i < 40; ++i) h.process();

    REQUIRE (h.eng.getState() == TapeStopEngine::State::Idle);
}

TEST_CASE ("Return disabled goes straight from Stopping to Settling to Idle")
{
    Harness h;
    h.eng.setStopTimeSeconds (0.02f);
    h.eng.setReturnEnabled (false);
    h.eng.setTriggerMode (TapeStopEngine::TriggerMode::OneShot);

    h.eng.setTrigger (true);
    h.process();
    h.eng.setTrigger (false);

    for (int i = 0; i < 30; ++i) h.process();
    REQUIRE (h.eng.getState() == TapeStopEngine::State::Idle);
}


// --- Click safety at trigger onset ------------------------------------------

TEST_CASE ("No large discontinuity at trigger onset")
{
    Harness h;
    h.eng.setStopTimeSeconds (0.5f);

    // Fill the ring with a slow sine so adjacent samples differ only slightly.
    const auto sine = [] (int n) { return std::sin (2.0f * M_PI * n / 200.0f); };

    // Warm the ring with several blocks so trigger doesn't read uninitialised tail.
    h.processContinuous (5, sine);

    const float lastBeforeTrigger = h.output[0][Harness::blockSize - 1];

    // Trigger and process one more block of continuous waveform.
    h.eng.setTrigger (true);
    h.fill ([&] (int i) { return sine (5 * Harness::blockSize + i); });
    h.process();

    const float firstAfterTrigger = h.output[0][0];

    // Adjacent sine samples (period 200) differ by ~ sin(2π/200) ≈ 0.031.
    // Without the interp-latency fix the read would pull stale ring data
    // and produce a discontinuity >> 0.1. Generous threshold of 0.1 catches
    // the regression without false positives from amp-fade or LP filter
    // settling.
    REQUIRE (std::abs (firstAfterTrigger - lastBeforeTrigger) < 0.1f);
}


// --- NaN / Inf safety --------------------------------------------------------

TEST_CASE ("NaN input never propagates to output")
{
    Harness h;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    h.fill ([&] (int i) { return (i == 50) ? nan : (i == 100 ? inf : 0.25f); });
    h.eng.setTrigger (true);
    h.process();

    for (int i = 0; i < Harness::blockSize; ++i)
    {
        REQUIRE (std::isfinite (h.output[0][i]));
        REQUIRE (std::isfinite (h.output[1][i]));
    }
}


// --- Output is bounded -------------------------------------------------------

TEST_CASE ("Output stays bounded for unity-amplitude inputs across a full cycle")
{
    Harness h;
    h.eng.setStopTimeSeconds (0.1f);
    h.eng.setReturnTimeSeconds (0.05f);
    h.eng.setTriggerMode (TapeStopEngine::TriggerMode::OneShot);

    h.eng.setTrigger (true);

    const auto sine = [] (int n) { return std::sin (2.0f * M_PI * n / 120.0f); };

    for (int b = 0; b < 80; ++b)
    {
        h.fill ([b, sine] (int i) { return sine (b * Harness::blockSize + i); });
        h.process();
        REQUIRE (blockPeak (h.output[0]) <= 1.5f);  // cubic interp can overshoot slightly
        REQUIRE (blockPeak (h.output[1]) <= 1.5f);
    }
}


// --- Prepare guards ----------------------------------------------------------

TEST_CASE ("Calling processBlock before prepare doesn't crash")
{
    TapeStopEngine eng; // no prepare()
    std::vector<float> in (256, 0.5f), outA (256, 0.0f), outB (256, 0.0f);
    const float* inPtrs[]  = { in.data(),  in.data()  };
    float*       outPtrs[] = { outA.data(), outB.data() };

    eng.processBlock (inPtrs, outPtrs, 2, 256);  // must not crash or write garbage

    for (int i = 0; i < 256; ++i)
    {
        REQUIRE (outA[i] == 0.0f);
        REQUIRE (outB[i] == 0.0f);
    }
}

TEST_CASE ("Invalid sample rate / channel count is ignored by prepare")
{
    TapeStopEngine eng;
    eng.prepare ( 0.0, 256, 2);   // bad SR
    eng.prepare (-1.0, 256, 2);   // negative SR
    eng.prepare (48000.0, 256, 0); // zero channels

    // After three bad calls, a real prepare should still work
    eng.prepare (48000.0, 256, 2);

    std::vector<float> in (256, 0.0f), outA (256, 99.0f), outB (256, 99.0f);
    const float* inPtrs[]  = { in.data(),  in.data()  };
    float*       outPtrs[] = { outA.data(), outB.data() };
    eng.processBlock (inPtrs, outPtrs, 2, 256);
    // Should have produced finite output (likely all zeros from silent input)
    for (int i = 0; i < 256; ++i)
        REQUIRE (std::isfinite (outA[i]));
}
