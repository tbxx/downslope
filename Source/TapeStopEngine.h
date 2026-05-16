#pragma once

#include <vector>
#include <cstddef>

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
    void setMix               (float m) { mix           = m; }

    void setTrigger (bool t) { triggerHigh = t; }

    void processBlock (const float* const* inputs,
                       float* const* outputs,
                       int numChannels,
                       int numSamples);

private:
    float applyCurve (float x, Curve c) const;
    float interpolateRead (int channel, double pos) const;

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

    float stopTimeSec   = 1.0f;
    float returnTimeSec = 0.25f;
    float floorPitch    = 0.0f;
    Curve stopCurve     = Curve::Exponential;
    Curve returnCurve   = Curve::Linear;
    TriggerMode triggerMode = TriggerMode::Momentary;
    bool  returnEnabled = true;
    bool  toneRolloff   = true;
    float mix           = 1.0f;
};
