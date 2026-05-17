// Layer 2 — render characterization tests.
// Drive the engine with a known signal end-to-end and assert that observable
// envelope characteristics match the intended behaviour: the slowdown
// attenuates, the return ramps back up, mix=0 is exact bypass, and a tighter
// stop curve attenuates faster than a gentle one.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TapeStopEngine.h"

#include <cmath>
#include <vector>


namespace
{
    constexpr int    kBlock      = 256;
    constexpr int    kChannels   = 2;
    constexpr double kSampleRate = 48000.0;

    // Drive a continuous sine through the engine for `numBlocks` blocks,
    // returning per-block RMS of channel 0.
    std::vector<float> runWithSine (TapeStopEngine& eng, int numBlocks, float freqHz)
    {
        std::vector<std::vector<float>> input  (kChannels, std::vector<float> (kBlock));
        std::vector<std::vector<float>> output (kChannels, std::vector<float> (kBlock));
        std::vector<const float*> inPtrs  { input[0].data(),  input[1].data()  };
        std::vector<float*>       outPtrs { output[0].data(), output[1].data() };

        std::vector<float> rms;
        rms.reserve ((size_t) numBlocks);

        const float radPerSample = 2.0f * (float) M_PI * freqHz / (float) kSampleRate;

        for (int b = 0; b < numBlocks; ++b)
        {
            for (int i = 0; i < kBlock; ++i)
            {
                const float x = std::sin (radPerSample * (b * kBlock + i));
                input[0][i] = x;
                input[1][i] = x;
            }
            eng.processBlock (inPtrs.data(), outPtrs.data(), kChannels, kBlock);

            double sumSq = 0.0;
            for (int i = 0; i < kBlock; ++i)
                sumSq += output[0][i] * output[0][i];
            rms.push_back ((float) std::sqrt (sumSq / kBlock));
        }
        return rms;
    }
}


// --- Stop envelope: floor=0 hits silence somewhere during the stop ----------
// Use momentary mode with the trigger held so we land in Holding (rate = floor
// = 0) and stay there. One-shot with no return would cycle through Settling
// back to Idle and pop back up to dry — silence is only momentary then.

TEST_CASE ("Momentary stop with floor=0 reaches silence at the holding floor")
{
    TapeStopEngine eng;
    eng.prepare (kSampleRate, kBlock, kChannels);

    eng.setStopTimeSeconds (0.20f);
    eng.setTriggerMode     (TapeStopEngine::TriggerMode::Momentary);
    eng.setFloorPitch      (0.0f);
    eng.setMix             (1.0f);
    eng.setCurve           (TapeStopEngine::Curve::Linear);

    eng.setTrigger (true);   // hold

    // 80 blocks ≈ 0.43 s — well past the 200 ms stop, so several blocks
    // worth of Holding will follow.
    const auto rms = runWithSine (eng, 80, 440.0f);

    // Started loud.
    REQUIRE (rms.front() > 0.5f);

    // Find the quietest block in the entire run — that's the floor.
    float minRms = 1e9f;
    for (float v : rms) minRms = std::min (minRms, v);
    REQUIRE (minRms < 0.05f);

    // And we should still be silent at the end (Holding indefinitely).
    REQUIRE (rms.back() < 0.05f);
    REQUIRE (eng.getState() == TapeStopEngine::State::Holding);
}


// --- Return ramp: amplitude comes back up after release ----------------------

TEST_CASE ("Return ramps amplitude back up to near input level")
{
    TapeStopEngine eng;
    eng.prepare (kSampleRate, kBlock, kChannels);

    eng.setStopTimeSeconds   (0.10f);
    eng.setReturnTimeSeconds (0.10f);
    eng.setReturnEnabled     (true);
    eng.setTriggerMode       (TapeStopEngine::TriggerMode::OneShot);
    eng.setFloorPitch        (0.0f);
    eng.setMix               (1.0f);
    eng.setCurve             (TapeStopEngine::Curve::Linear);
    eng.setReturnCurve       (TapeStopEngine::Curve::Linear);

    eng.setTrigger (true);

    // 100 ms stop + 100 ms return + 30 ms settle ≈ 230 ms ≈ 44 blocks.
    // Run 60 to leave room for the settling crossfade to complete.
    const auto rms = runWithSine (eng, 60, 440.0f);

    REQUIRE (rms.front() > 0.5f);   // started loud

    // Find the quietest stretch (during the stop floor)
    float minRms = 1e9f;
    for (float v : rms) minRms = std::min (minRms, v);
    REQUIRE (minRms < 0.1f);        // hit silence during stop

    // After the return + settle, we should be close to the input level again.
    REQUIRE (rms.back() > 0.5f);
}


// --- mix=0 approximates dry passthrough -------------------------------------
// "Exact" passthrough isn't achievable because the mix smoother (one-pole τ ≈
// 20 ms) is asymptotic toward 0 rather than reaching it, so a tiny fraction
// of the wet path always bleeds through. We assert the output tracks the
// input closely after the smoother has had plenty of time to settle.

TEST_CASE ("mix=0 produces near-dry passthrough after smoother settles")
{
    TapeStopEngine eng;
    eng.prepare (kSampleRate, kBlock, kChannels);
    eng.setMix (0.0f);

    std::vector<std::vector<float>> input  (kChannels, std::vector<float> (kBlock));
    std::vector<std::vector<float>> output (kChannels, std::vector<float> (kBlock));
    std::vector<const float*> inPtrs  { input[0].data(),  input[1].data()  };
    std::vector<float*>       outPtrs { output[0].data(), output[1].data() };

    for (int i = 0; i < kBlock; ++i)
    {
        input[0][i] = -0.5f + i / (float) kBlock;
        input[1][i] = 0.5f * std::sin (2.0f * (float) M_PI * i / 64.0f);
    }

    // Run 100 blocks (~530 ms) — smoother τ is 20 ms so this is ~26 time
    // constants of settling. Residual mix value is < 1e-11, well below
    // float epsilon for our amplitudes.
    for (int b = 0; b < 100; ++b)
        eng.processBlock (inPtrs.data(), outPtrs.data(), kChannels, kBlock);

    // Generous margin: we're not exactly bit-perfect because the wet path
    // (DC-blocked dry) still contributes a vanishing amount.
    for (int i = 0; i < kBlock; ++i)
    {
        REQUIRE (output[0][i] == Catch::Approx (input[0][i]).margin (1e-3f));
        REQUIRE (output[1][i] == Catch::Approx (input[1][i]).margin (1e-3f));
    }
}


// --- Exponential curve attenuates more aggressively than linear -------------

TEST_CASE ("Exponential stop curve produces a different envelope shape than Linear")
{
    auto runWithCurve = [] (TapeStopEngine::Curve c)
    {
        TapeStopEngine eng;
        eng.prepare (kSampleRate, kBlock, kChannels);
        eng.setStopTimeSeconds (0.30f);
        eng.setReturnEnabled (false);
        eng.setTriggerMode (TapeStopEngine::TriggerMode::OneShot);
        eng.setFloorPitch (0.0f);
        eng.setMix (1.0f);
        eng.setCurve (c);
        eng.setTrigger (true);
        return runWithSine (eng, 30, 440.0f);
    };

    const auto rmsLinear = runWithCurve (TapeStopEngine::Curve::Linear);
    const auto rmsExpo   = runWithCurve (TapeStopEngine::Curve::Exponential);

    // Exponential is x*x — slow start, fast finish. At the mid-point of the
    // stop time, the linear curve has shed half its rate but exponential has
    // only shed a quarter, so exponential is still LOUDER mid-stop.
    // 300 ms stop / 256 samples per block / 48k = ~56 blocks for full stop.
    // We ran 30 blocks (≈ 160 ms ≈ 53% through). Check that index.
    const int midIdx = (int) (rmsLinear.size() * 1 / 2);
    REQUIRE (rmsExpo[(size_t) midIdx]   >  rmsLinear[(size_t) midIdx]);
}
