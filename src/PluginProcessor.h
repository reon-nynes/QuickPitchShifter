#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchEngine.h"

class QuickPitchShifterProcessor final : public juce::AudioProcessor,
                                         private juce::Thread,
                                         private juce::AsyncUpdater
{
public:
    QuickPitchShifterProcessor();
    ~QuickPitchShifterProcessor() override;

    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Quick Pitch Shifter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState parameters;
    void applyProcessingSettings(ProcessingSettings);
    ProcessingSettings appliedSettings() const;
    ProcessingSettings activeSettings() const;
    bool isReconfiguring() const;
    bool hasConfigurationError() const { return configurationError.load(); }
    int activeLatencySamples() const { return engineLatency.load(); }
    double processingSampleRate() const { return currentRate.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    PitchControls readControls(bool forceBypass = false) const;
    void process(juce::AudioBuffer<float>&, bool forceBypass);
    void configureEngine();
    void run() override;
    void handleAsyncUpdate() override;

    std::atomic<float>* pitchValue = nullptr;
    std::atomic<float>* tonalityValue = nullptr;
    std::atomic<float>* formantValue = nullptr;
    std::atomic<float>* baseValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;
    std::atomic<int> requestedConfig { ProcessingSettings{}.encode() };
    std::atomic<int> activeConfig { ProcessingSettings{}.encode() };
    std::atomic<int> engineLatency { 0 };
    std::atomic<double> currentRate { 48000.0 };
    std::atomic<bool> configuring { false }, configurationError { false };
    std::atomic<bool> playbackPrepared { false };
    std::atomic<bool> forceReconfigure { false };
    juce::SpinLock engineLock;
    PitchEngine engine;
    bool prepared = false;
    int channels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickPitchShifterProcessor)
};
