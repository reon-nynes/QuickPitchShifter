#include "../src/PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace
{
thread_local bool watchAllocations = false;
thread_local size_t allocationCount = 0;
}

void* operator new(std::size_t size)
{
    if (watchAllocations)
        ++allocationCount;
    if (auto* result = std::malloc(size == 0 ? 1 : size))
        return result;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace
{
constexpr double pi = 3.14159265358979323846;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void setParameter(QuickPitchShifterProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void checkedProcess(QuickPitchShifterProcessor& processor, juce::AudioBuffer<float>& buffer, bool hostBypass = false)
{
    juce::MidiBuffer midi;
    allocationCount = 0;
    watchAllocations = true;
    if (hostBypass)
        processor.processBlockBypassed(buffer, midi);
    else
        processor.processBlock(buffer, midi);
    watchAllocations = false;
    require(allocationCount == 0, "C++ allocation occurred on the audio callback");
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            require(std::isfinite(buffer.getSample(c, i)), "Non-finite audio output");
}

double spectralPower(const std::vector<float>& samples, double frequency, double rate)
{
    double real = 0.0, imaginary = 0.0;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * static_cast<double>(i)
                                                / static_cast<double>(samples.size() - 1));
        const double angle = 2.0 * pi * frequency * static_cast<double>(i) / rate;
        real += window * samples[i] * std::cos(angle);
        imaginary += window * samples[i] * std::sin(angle);
    }
    return real * real + imaginary * imaginary;
}

void testPitch(float shift, int channelCount, bool preserveFormants)
{
    QuickPitchShifterProcessor processor;
    processor.setPlayConfigDetails(channelCount, channelCount, 48000.0, 256);
    require(processor.getTotalNumInputChannels() == channelCount, "Channel setup failed");
    setParameter(processor, "pitch", shift);
    setParameter(processor, "formants", preserveFormants ? 1.0f : 0.0f);
    processor.prepareToPlay(48000.0, 256);
    require(processor.getLatencySamples() == 7200, "Default latency should be 150 ms at 48 kHz");
    std::vector<float> output;
    output.reserve(24000);
    double stereoError = 0.0, signalEnergy = 0.0;
    const int sizes[] { 1, 127, 256, 511, 4097, 64 };
    int position = 0, iteration = 0;
    while (position < 96000)
    {
        const int count = std::min(sizes[iteration++ % 6], 96000 - position);
        juce::AudioBuffer<float> buffer(channelCount, count);
        for (int i = 0; i < count; ++i)
        {
            const float sample = static_cast<float>(0.25 * std::sin(2.0 * pi * 440.0 * (position + i) / 48000.0));
            for (int c = 0; c < channelCount; ++c)
                buffer.setSample(c, i, sample);
        }
        checkedProcess(processor, buffer);
        for (int i = 0; i < count; ++i)
        {
            if (position + i >= 72000)
            {
                output.push_back(buffer.getSample(0, i));
                const double sample = buffer.getSample(0, i);
                signalEnergy += sample * sample;
                if (channelCount == 2)
                {
                    const double difference = sample - buffer.getSample(1, i);
                    stereoError += difference * difference;
                }
            }
        }
        position += count;
    }
    const double expected = 440.0 * std::pow(2.0, shift / 12.0);
    double bestFrequency = 0.0, bestPower = -1.0;
    for (double frequency = expected - 4.0; frequency <= expected + 4.0; frequency += 0.25)
    {
        const double power = spectralPower(output, frequency, 48000.0);
        if (power > bestPower)
        {
            bestPower = power;
            bestFrequency = frequency;
        }
    }
    std::cout << "Measured pitch " << bestFrequency << " Hz; expected " << expected << " Hz\n";
    const double errorCents = 1200.0 * std::log2(bestFrequency / expected);
    require(std::abs(errorCents) < 10.0, "Pitch differs from requested interval by more than 10 cents");
    if (shift == -12.0f)
    {
        // Compare the wrapper with a standalone upstream processor, including its
        // inherent spectral approximation rather than assuming exact resampling.
        signalsmith::stretch::SignalsmithStretch<float> reference;
        reference.configure(1, 5760, 1440, true);
        reference.reset();
        reference.setTransposeSemitones(shift, 8000.0f / 48000.0f);
        std::array<float, 64> in {}, out {};
        const float* inputs[] { in.data() };
        float* outputs[] { out.data() };
        std::vector<float> referenceOutput;
        referenceOutput.reserve(24000);
        for (int start = 0; start < 96000; start += 64)
        {
            for (int i = 0; i < 64; ++i)
                in[static_cast<size_t>(i)] = static_cast<float>(0.25 * std::sin(2.0 * pi * 440 * (start + i) / 48000.0));
            reference.process(inputs, 64, outputs, 64);
            if (start >= 72000)
                referenceOutput.insert(referenceOutput.end(), out.begin(), out.end());
        }
        double referencePeak = 0.0, referencePower = -1.0;
        for (double frequency = expected - 4; frequency <= expected + 4; frequency += 0.25)
        {
            const double power = spectralPower(referenceOutput, frequency, 48000.0);
            if (power > referencePower)
            {
                referencePower = power;
                referencePeak = frequency;
            }
        }
        std::cout << "Standalone upstream reference: " << referencePeak << " Hz\n";
        require(std::abs(referencePeak - bestFrequency) <= 0.25, "Wrapper differs from standalone upstream pitch behavior");
    }
    require(bestPower > 100.0, "Pitch-shifted output is silent or too weak");
    if (channelCount == 2)
    {
        std::cout << "Stereo difference RMS / signal RMS: " << std::sqrt(stereoError / signalEnergy) << '\n';
        require(stereoError / signalEnergy < 0.0001, "Identical stereo channels lost coherence");
    }
    if (shift != 0.0f)
        require(bestPower > spectralPower(output, 440.0, 48000.0) * 20.0, "Unshifted tone dominates output");
    std::cout << "PASS pitch " << shift << " st, " << channelCount << " channels, formants "
              << preserveFormants << ": " << bestFrequency << " Hz\n";
}

void testBypassLatency(double rate, ProcessingSettings settings)
{
    QuickPitchShifterProcessor processor;
    processor.applyProcessingSettings(settings);
    setParameter(processor, "bypass", 1.0f);
    setParameter(processor, "pitch", 12.0f);
    processor.prepareToPlay(rate, 128);
    const int latency = processor.getLatencySamples();
    const int block = static_cast<int>(std::lround(rate * settings.blockMs / 1000.0));
    require(latency == block + static_cast<int>(std::lround(block / settings.overlap)), "Wrong configured latency");
    int position = 0;
    while (position < latency + 400)
    {
        const int count = std::min(127, latency + 400 - position);
        juce::AudioBuffer<float> buffer(2, count);
        buffer.clear();
        if (position == 0)
        {
            buffer.setSample(0, 0, 0.75f);
            buffer.setSample(1, 0, -0.25f);
        }
        checkedProcess(processor, buffer);
        for (int i = 0; i < count; ++i)
        {
            require(std::abs(buffer.getSample(0, i) - (position + i == latency ? 0.75f : 0.0f)) < 1.0e-6f,
                    "Bypass dry signal is not exactly delayed by reported latency");
            require(std::abs(buffer.getSample(1, i) - (position + i == latency ? -0.25f : 0.0f)) < 1.0e-6f,
                    "Bypass channels are not independent");
        }
        position += count;
    }
    std::cout << "PASS bypass latency " << rate << " Hz: " << latency << " samples\n";
}

void testState()
{
    QuickPitchShifterProcessor original;
    setParameter(original, "pitch", -7.25f);
    setParameter(original, "tonality", 9123.0f);
    setParameter(original, "formants", 1.0f);
    setParameter(original, "formantBase", 123.0f);
    setParameter(original, "bypass", 1.0f);
    original.applyProcessingSettings({ 90, 5.5f });
    juce::MemoryBlock state;
    original.getStateInformation(state);
    QuickPitchShifterProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    restored.prepareToPlay(48000.0, 256);
    for (const auto* id : { "pitch", "tonality", "formants", "formantBase", "bypass" })
        require(original.parameters.getRawParameterValue(id)->load() == restored.parameters.getRawParameterValue(id)->load(),
                "Parameter state did not round-trip");
    require(restored.activeSettings().encode() == ProcessingSettings { 90, 5.5f }.encode(), "Applied settings not restored");
    restored.setStateInformation(nullptr, 0);
    require(restored.appliedSettings().encode() == ProcessingSettings { 90, 5.5f }.encode(), "Invalid state altered configuration");
    auto corrupt = juce::ValueTree::readFromData(state.getData(), state.getSize());
    corrupt.setProperty("blockMs", 100000000.0, nullptr);
    corrupt.setProperty("overlap", -1, nullptr);
    juce::MemoryBlock corruptData;
    juce::MemoryOutputStream stream(corruptData, false);
    corrupt.writeToStream(stream);
    restored.setStateInformation(corruptData.getData(), static_cast<int>(corruptData.getSize()));
    require(restored.appliedSettings().encode() == ProcessingSettings { 180, 2.0f }.encode(), "Corrupt settings not clamped");
    std::cout << "PASS state round-trip, invalid state, configuration bounds\n";
}

void testUnpreparedLifecycle()
{
    QuickPitchShifterProcessor processor;
    processor.releaseResources();
    processor.reset();
    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();
    checkedProcess(processor, buffer);
    processor.prepareToPlay(48000.0, 256);
    checkedProcess(processor, buffer);
    processor.releaseResources();
    processor.releaseResources();
    processor.prepareToPlay(44100.0, 127);
    checkedProcess(processor, buffer);
    std::cout << "PASS release/reset/process before prepare, repeated release, sample-rate restart\n";
}

class ConfigurationWait final : private juce::Timer
{
public:
    ConfigurationWait(QuickPitchShifterProcessor& p, int expectedConfig)
        : processor(p), expected(expectedConfig), deadline(juce::Time::getMillisecondCounterHiRes() + 5000.0)
    {
        startTimer(5);
    }
    ~ConfigurationWait() override { stopTimer(); }
    bool complete = false;
private:
    void timerCallback() override
    {
        juce::AudioBuffer<float> buffer(2, 511);
        buffer.clear();
        checkedProcess(processor, buffer);
        complete = !processor.isReconfiguring() && processor.activeSettings().encode() == expected
                   && processor.getLatencySamples() == processor.activeLatencySamples();
        if (complete || juce::Time::getMillisecondCounterHiRes() > deadline)
            juce::MessageManager::getInstance()->stopDispatchLoop();
    }
    QuickPitchShifterProcessor& processor;
    int expected;
    double deadline;
};

void testReconfiguration()
{
    QuickPitchShifterProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    processor.applyProcessingSettings({ 180, 2 });
    processor.applyProcessingSettings({ 50, 8 });
    ConfigurationWait wait(processor, ProcessingSettings { 50, 8 }.encode());
    juce::MessageManager::getInstance()->runDispatchLoop();
    require(wait.complete, "Worker configuration/host latency update timed out");
    require(processor.getLatencySamples() == 2700, "Wrong latency after worker reconfiguration");
    require(!processor.hasConfigurationError(), "Worker configuration failed");
    std::cout << "PASS rapid Apply, live processing during worker configuration, host latency update\n";
}

void testAutomationAndHostBypass()
{
    QuickPitchShifterProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    for (int blockIndex = 0; blockIndex < 500; ++blockIndex)
    {
        if (blockIndex % 20 == 0)
        {
            setParameter(processor, "pitch", static_cast<float>((blockIndex % 25) - 12));
            setParameter(processor, "tonality", static_cast<float>(2000 + blockIndex * 30));
            setParameter(processor, "formants", blockIndex % 40 == 0 ? 1.0f : 0.0f);
            setParameter(processor, "formantBase", static_cast<float>(50 + blockIndex % 450));
            setParameter(processor, "bypass", blockIndex % 40 == 0 ? 1.0f : 0.0f);
        }
        for (int i = 0; i < 256; ++i)
        {
            const auto sample = static_cast<float>(0.2 * std::sin(2 * pi * 200 * (blockIndex * 256 + i) / 48000.0));
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample * 0.5f);
        }
        checkedProcess(processor, buffer, blockIndex % 3 == 0);
    }
    std::cout << "PASS live parameter changes and host bypass, finite output, no C++ callback allocations\n";
}

void testFractionalPitchTextbox()
{
    QuickPitchShifterProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* pitch = dynamic_cast<juce::Slider*>(editor->findChildWithID("pitch"));
    require(pitch != nullptr, "Pitch control missing");
    juce::Label* textbox = nullptr;
    for (auto* child : pitch->getChildren())
        if (auto* label = dynamic_cast<juce::Label*>(child))
            textbox = label;
    require(textbox != nullptr, "Pitch textbox missing");

    textbox->setText("+1.25 st", juce::sendNotificationSync);
    require(std::abs(processor.parameters.getRawParameterValue("pitch")->load() - 1.25f) < 1.0e-5f,
            "Fractional textbox input rounded before reaching the processor");
    require(textbox->getText() == "+1.25 st", "Fractional pitch text did not display correctly");
    textbox->setText("-3.125", juce::sendNotificationSync);
    require(std::abs(pitch->getValue() + 3.125) < 1.0e-5, "Negative fractional input lost precision");
    require(pitch->snapValue(1.25, juce::Slider::absoluteDrag) == 1.0
         && pitch->snapValue(-3.125, juce::Slider::velocityDrag) == -3.0,
            "Pitch dragging no longer snaps to whole semitones");

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    QuickPitchShifterProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    require(std::abs(restored.parameters.getRawParameterValue("pitch")->load() + 3.125f) < 1.0e-5f,
            "Saved fractional pitch was rounded on restoration");
    setParameter(processor, "pitch", 2.375f);
    require(std::abs(pitch->getValue() - 2.375) < 1.0e-5, "Host fractional pitch was rounded in the editor");
    textbox->setText("99", juce::sendNotificationSync);
    require(pitch->getValue() == 12.0, "Pitch textbox exceeded upper range");
    textbox->setText("-99", juce::sendNotificationSync);
    require(pitch->getValue() == -12.0, "Pitch textbox exceeded lower range");
    std::cout << "PASS fractional pitch textbox, whole-semitone dragging, state/host precision, bounds\n";
}

void testPendingEditorSettings()
{
    QuickPitchShifterProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* block = dynamic_cast<juce::Slider*>(editor->findChildWithID("analysisBlock"));
    auto* overlap = dynamic_cast<juce::Slider*>(editor->findChildWithID("overlap"));
    require(block != nullptr && overlap != nullptr, "Processing controls missing");
    block->setValue(90, juce::sendNotificationSync);
    overlap->setValue(5.5, juce::sendNotificationSync);
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    auto state = juce::ValueTree::readFromData(saved.getData(), saved.getSize());
    require(static_cast<int>(state["blockMs"]) == 120 && static_cast<double>(state["overlap"]) == 4,
            "Unapplied editor settings leaked into saved state");
    editor.reset();
    editor.reset(processor.createEditor());
    block = dynamic_cast<juce::Slider*>(editor->findChildWithID("analysisBlock"));
    overlap = dynamic_cast<juce::Slider*>(editor->findChildWithID("overlap"));
    require(block->getValue() == 120 && overlap->getValue() == 4, "Unapplied changes survived editor close");
    block->setValue(90, juce::sendNotificationSync);
    overlap->setValue(5.5, juce::sendNotificationSync);
    auto* apply = dynamic_cast<juce::TextButton*>(editor->findChildWithID("apply"));
    require(apply != nullptr && apply->isEnabled(), "Apply button did not enable for pending changes");
    apply->onClick();
    processor.getStateInformation(saved);
    state = juce::ValueTree::readFromData(saved.getData(), saved.getSize());
    require(static_cast<int>(state["blockMs"]) == 90 && static_cast<double>(state["overlap"]) == 5.5,
            "Apply did not commit both settings to saved state");
    std::cout << "PASS pending settings excluded from state, editor reopen, explicit Apply\n";
}

void writeEditorSnapshot(const juce::File& destination)
{
    QuickPitchShifterProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.5f);
    auto stream = destination.createOutputStream();
    require(stream != nullptr, "Cannot create editor preview file");
    stream->setPosition(0);
    stream->truncate();
    require(juce::PNGImageFormat().writeImageToStream(image, *stream), "Cannot encode editor preview");
    std::cout << "PASS editor creation/snapshot: " << destination.getFullPathName() << '\n';
}
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    try
    {
        testUnpreparedLifecycle();
        testPitch(0, 2, false);
        testPitch(12, 2, false);
        testPitch(-12, 1, false);
        testPitch(7, 2, true);
        testPitch(1.25f, 2, false);
        testBypassLatency(44100, { 120, 4 });
        testBypassLatency(48000, { 50, 8 });
        testBypassLatency(96000, { 180, 2 });
        testState();
        testAutomationAndHostBypass();
        testFractionalPitchTextbox();
        testPendingEditorSettings();
        testReconfiguration();
        if (argc == 3 && juce::String(argv[1]) == "--snapshot")
            writeEditorSnapshot(juce::File(juce::String(argv[2])));
        std::cout << "All checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        watchAllocations = false;
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
