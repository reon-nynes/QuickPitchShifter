#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <array>
#include <vector>

struct ProcessingSettings
{
    int blockMs = 120;
    float overlap = 4.0f;

    static ProcessingSettings sanitise(ProcessingSettings);
    static ProcessingSettings decode(int);
    int encode() const;
};

struct PitchControls
{
    float semitones = 0.0f;
    float tonalityHz = 8000.0f;
    bool preserveFormants = false;
    float formantBaseHz = 200.0f;
    bool bypass = false;
};

// Owned by the processor. Configuration and processing require its engine lock.
class PitchEngine
{
public:
    void configure(double sampleRate, int channels, ProcessingSettings, PitchControls);
    void reset();
    void process(juce::AudioBuffer<float>&, PitchControls);
    int latencySamples() const { return latency; }

private:
    static constexpr int chunkSize = 64;
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::array<std::array<float, chunkSize>, 2> input {}, output {}, dry {};
    std::array<const float*, 2> inputPointers { input[0].data(), input[1].data() };
    std::array<float*, 2> outputPointers { output[0].data(), output[1].data() };
    std::array<std::vector<float>, 2> dryDelay;
    juce::SmoothedValue<float> pitch, tonality, formantBase, bypassMix;
    double rate = 48000.0;
    int channelCount = 2, latency = 0, dryPosition = 0;
};
