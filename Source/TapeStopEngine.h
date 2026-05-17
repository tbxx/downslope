#pragma once

#include <vector>
#include <cstddef>
#include <cmath>

class TapeStopEngine
{
public:
    enum class Curve       { Linear = 0, Exponential, Logarithmic, SCurve };
    enum class TriggerMode { Momentary = 0, OneShot };
    enum class State       { Idle, Stopping, Holding, Returning, Settling };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setStopTimeSeconds   (float s) { stopTimeSec   = s; }
    void setReturnTimeSeconds (float s) { returnTimeSec = s; }
    void setFloorPitch        (float p) { floorPitch    = p; }
    void setCurve             (Curve c) { stopCurve     = c; }
    void setReturnCurve       (Curve c) { returnCurve   = c; }
    void setTriggerMode       (TriggerMode m) { triggerMode = m; }
    void setReturnEnabled     (bool e)  { returnEnabled = e; }
    void setToneRolloff       (bool e)  { toneRolloff   = e; }
    void setToneAmount        (float a) { toneAmount    = a; }   // 0..1; 0.5 = current behaviour
    void setMix               (float m) { mix           = m; }

    void setTrigger (bool t) { triggerHigh = t; }

    void processBlock (const float* const* inputs,
                       float* const* outputs,
                       int numChannels,
                       int numSamples);

    // --- Test accessors -----------------------------------------------------
    // Exposed so tests can assert state-machine behaviour without poking at
    // internal members. applyCurve is stateless — promoted to public static
    // so unit tests can exercise the curve shapes directly.
    State getState() const { return state; }
    static float applyCurve (float x, Curve c);

private:
    // Shared constants — keep all "magic numbers" gathered here.
    static constexpr double settleCrossfadeSeconds = 0.03;  // 30 ms blend back to dry on release
    static constexpr float  ampFadeThreshold       = 0.15f; // amp fades to 0 over the last 15% of slowdown
    static constexpr int    interpLatencySamples   = 4;     // read head sits this many samples behind the write head at trigger
    static constexpr float  dcBlockerCornerHz      = 5.0f;
    static constexpr float  smoothingTimeMs        = 20.0f; // one-pole smoother on mix/floor
    static constexpr float  ringBufferSeconds      = 10.0f;

    float interpolateRead (int channel, double pos) const;
    void  enterSettling();

    double fs = 44100.0;
    int    nCh = 2;

    std::vector<std::vector<float>> ring;
    int    ringSize = 0;
    int    writeIdx = 0;
    double readPos  = 0.0;

    State  state         = State::Idle;
    bool   triggerHigh   = false;
    bool   prevTrigger   = false;

    double phase         = 0.0;
    double settleSamples = 0.0;
    double settleCounter = 0.0;

    std::vector<float> lpZ;

    // DC blocker state (per channel)
    std::vector<float> dcX1, dcY1;
    float dcR = 0.999f;

    // One-pole smoothers for zipper-noise-free user params
    struct Smoother { float cur = 0.0f, coeff = 1.0f;
        void prepare (double sr, float ms) {
            const double tau = static_cast<double> (ms) * 0.001;
            coeff = static_cast<float> (1.0 - std::exp (-1.0 / (tau * sr)));
        }
        void snap (float v) { cur = v; }
        float next (float t) { cur += coeff * (t - cur); return cur; }
    };
    Smoother mixSm, floorSm;

    float stopTimeSec   = 1.0f;
    float returnTimeSec = 0.25f;
    float floorPitch    = 0.0f;
    Curve stopCurve     = Curve::Exponential;
    Curve returnCurve   = Curve::Linear;
    TriggerMode triggerMode = TriggerMode::Momentary;
    bool  returnEnabled = true;
    bool  toneRolloff   = true;
    float toneAmount    = 0.5f;
    float mix           = 1.0f;
};
