#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout QuickPitchShifterProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "pitch", 1 }, "Pitch",
        juce::NormalisableRange<float> { -12.0f, 12.0f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")
            .withStringFromValueFunction([](float value, int maximumLength) {
                const auto text = (value > 0 ? "+" : "")
                    + juce::String(value, 6).trimCharactersAtEnd("0").trimCharactersAtEnd(".");
                return maximumLength > 0 ? text.substring(0, maximumLength) : text;
            })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "tonality", 1 }, "Tonality limit",
        juce::NormalisableRange<float> { 2000.0f, 20000.0f, 1.0f }, 8000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "formants", 1 }, "Preserve formants", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "formantBase", 1 }, "Formant base",
        juce::NormalisableRange<float> { 50.0f, 500.0f, 1.0f }, 200.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "bypass", 1 }, "Bypass", false,
        juce::AudioParameterBoolAttributes().withCategory(juce::AudioProcessorParameter::genericParameter)));
    return layout;
}

QuickPitchShifterProcessor::QuickPitchShifterProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      Thread("QuickPitchShifter configuration"),
      parameters(*this, nullptr, "QuickPitchShifterState", makeParameters())
{
    pitchValue = parameters.getRawParameterValue("pitch");
    tonalityValue = parameters.getRawParameterValue("tonality");
    formantValue = parameters.getRawParameterValue("formants");
    baseValue = parameters.getRawParameterValue("formantBase");
    bypassValue = parameters.getRawParameterValue("bypass");
    startThread();
}

QuickPitchShifterProcessor::~QuickPitchShifterProcessor()
{
    stopThread(-1);
    cancelPendingUpdate();
}

PitchControls QuickPitchShifterProcessor::readControls(bool forceBypass) const
{
    return { pitchValue->load(), tonalityValue->load(), formantValue->load() >= 0.5f,
             baseValue->load(), forceBypass || bypassValue->load() >= 0.5f };
}

void QuickPitchShifterProcessor::configureEngine()
{
    const int config = requestedConfig.load();
    forceReconfigure.store(false);
    configuring.store(true);
    configurationError.store(false);
    try
    {
        engine.configure(currentRate.load(), channels, ProcessingSettings::decode(config), readControls());
        activeConfig.store(config);
        engineLatency.store(engine.latencySamples());
        prepared = true;
    }
    catch (...)
    {
        prepared = false;
        configurationError.store(true);
    }
    configuring.store(false);
}

void QuickPitchShifterProcessor::prepareToPlay(double sampleRate, int)
{
    const juce::SpinLock::ScopedLockType lock(engineLock);
    currentRate.store(sampleRate);
    channels = getTotalNumInputChannels();
    configureEngine();
    playbackPrepared.store(true);
    setLatencySamples(engineLatency.load());
}

void QuickPitchShifterProcessor::releaseResources()
{
    const juce::SpinLock::ScopedLockType lock(engineLock);
    playbackPrepared.store(false);
    if (prepared)
        engine.reset();
    prepared = false;
}

void QuickPitchShifterProcessor::reset()
{
    const juce::SpinLock::ScopedTryLockType lock(engineLock);
    if (lock.isLocked() && prepared)
        engine.reset();
}

bool QuickPitchShifterProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto input = layout.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo())
        && input == layout.getMainOutputChannelSet();
}

void QuickPitchShifterProcessor::process(juce::AudioBuffer<float>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;
    const juce::SpinLock::ScopedTryLockType lock(engineLock);
    if (!lock.isLocked() || !prepared || buffer.getNumChannels() < channels)
    {
        buffer.clear();
        return;
    }
    for (int c = channels; c < buffer.getNumChannels(); ++c)
        buffer.clear(c, 0, buffer.getNumSamples());
    engine.process(buffer, readControls(forceBypass));
}

void QuickPitchShifterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process(buffer, false);
}

void QuickPitchShifterProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process(buffer, true);
}

juce::AudioProcessorParameter* QuickPitchShifterProcessor::getBypassParameter() const
{
    return parameters.getParameter("bypass");
}

double QuickPitchShifterProcessor::getTailLengthSeconds() const
{
    return static_cast<double>(engineLatency.load()) / currentRate.load();
}

void QuickPitchShifterProcessor::applyProcessingSettings(ProcessingSettings settings)
{
    requestedConfig.store(settings.encode());
    if (configurationError.exchange(false))
        forceReconfigure.store(true);
    if (playbackPrepared.load())
        notify();
}

ProcessingSettings QuickPitchShifterProcessor::appliedSettings() const
{
    return ProcessingSettings::decode(requestedConfig.load());
}

ProcessingSettings QuickPitchShifterProcessor::activeSettings() const
{
    return ProcessingSettings::decode(activeConfig.load());
}

bool QuickPitchShifterProcessor::isReconfiguring() const
{
    return configuring.load() || (!configurationError.load()
        && (forceReconfigure.load() || requestedConfig.load() != activeConfig.load()));
}

void QuickPitchShifterProcessor::run()
{
    while (!threadShouldExit())
    {
        wait(500);
        if (threadShouldExit())
            break;
        if (!playbackPrepared.load() || configurationError.load()
            || (requestedConfig.load() == activeConfig.load() && !forceReconfigure.load()))
            continue;
        {
            const juce::SpinLock::ScopedLockType lock(engineLock);
            if (!playbackPrepared.load()
                || (requestedConfig.load() == activeConfig.load() && !forceReconfigure.load()))
                continue;
            configureEngine();
        }
        triggerAsyncUpdate();
    }
}

void QuickPitchShifterProcessor::handleAsyncUpdate()
{
    setLatencySamples(engineLatency.load());
}

void QuickPitchShifterProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    const auto settings = appliedSettings();
    state.setProperty("schemaVersion", 1, nullptr);
    state.setProperty("blockMs", settings.blockMs, nullptr);
    state.setProperty("overlap", settings.overlap, nullptr);
    juce::MemoryOutputStream stream(destination, false);
    state.writeToStream(stream);
}

void QuickPitchShifterProcessor::setStateInformation(const void* data, int size)
{
    const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(juce::jmax(0, size)));
    if (!state.isValid() || !state.hasType("QuickPitchShifterState"))
        return;
    parameters.replaceState(state);
    const double block = static_cast<double>(state.getProperty("blockMs", 120));
    const double overlap = static_cast<double>(state.getProperty("overlap", 4.0));
    applyProcessingSettings({ std::isfinite(block) ? static_cast<int>(juce::jlimit(50.0, 180.0, block)) : 120,
                              std::isfinite(overlap) ? static_cast<float>(overlap) : 4.0f });
}

juce::AudioProcessorEditor* QuickPitchShifterProcessor::createEditor()
{
    return new QuickPitchShifterEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuickPitchShifterProcessor();
}
