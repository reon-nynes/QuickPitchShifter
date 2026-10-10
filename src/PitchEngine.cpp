#include "PitchEngine.h"
#include <algorithm>
#include <cmath>

ProcessingSettings ProcessingSettings::sanitise(ProcessingSettings settings)
{
    settings.blockMs = juce::jlimit(50, 180, settings.blockMs);
    if (!std::isfinite(settings.overlap))
        settings.overlap = 4.0f;
    settings.overlap = std::round(juce::jlimit(2.0f, 8.0f, settings.overlap) * 10.0f) / 10.0f;
    return settings;
}

int ProcessingSettings::encode() const
{
    const auto safe = sanitise(*this);
    return safe.blockMs * 100 + static_cast<int>(std::lround(safe.overlap * 10.0f));
}

ProcessingSettings ProcessingSettings::decode(int packed)
{
    return sanitise({ packed / 100, static_cast<float>(packed % 100) / 10.0f });
}

void PitchEngine::configure(double sampleRate, int channels, ProcessingSettings settings,
                            PitchControls controls)
{
    rate = sampleRate;
    channelCount = channels;
    settings = ProcessingSettings::sanitise(settings);
    const int block = static_cast<int>(std::lround(rate * settings.blockMs / 1000.0));
    const int interval = static_cast<int>(std::lround(static_cast<double>(block) / settings.overlap));
    stretch.configure(channelCount, block, interval, true);
    stretch.reset();
    latency = stretch.inputLatency() + stretch.outputLatency();
    for (auto& delay : dryDelay)
        delay.assign(static_cast<size_t>(latency), 0.0f);
    dryPosition = 0;
    pitch.reset(rate, 0.02);
    tonality.reset(rate, 0.02);
    formantBase.reset(rate, 0.02);
    bypassMix.reset(rate, 0.01);
    pitch.setCurrentAndTargetValue(controls.semitones);
    tonality.setCurrentAndTargetValue(controls.tonalityHz);
    formantBase.setCurrentAndTargetValue(controls.formantBaseHz);
    bypassMix.setCurrentAndTargetValue(controls.bypass ? 1.0f : 0.0f);
    stretch.setTransposeSemitones(controls.semitones, controls.tonalityHz / static_cast<float>(rate));
    stretch.setFormantSemitones(0.0f, controls.preserveFormants);
    stretch.setFormantBase(controls.formantBaseHz / static_cast<float>(rate));
}

void PitchEngine::reset()
{
    stretch.reset();
    for (auto& delay : dryDelay)
        std::fill(delay.begin(), delay.end(), 0.0f);
    dryPosition = 0;
}

void PitchEngine::process(juce::AudioBuffer<float>& buffer, PitchControls controls)
{
    pitch.setTargetValue(controls.semitones);
    tonality.setTargetValue(controls.tonalityHz);
    formantBase.setTargetValue(controls.formantBaseHz);
    bypassMix.setTargetValue(controls.bypass ? 1.0f : 0.0f);
    stretch.setFormantSemitones(0.0f, controls.preserveFormants);

    for (int offset = 0; offset < buffer.getNumSamples(); offset += chunkSize)
    {
        const int count = std::min(chunkSize, buffer.getNumSamples() - offset);
        for (int c = 0; c < channelCount; ++c)
            std::copy_n(buffer.getReadPointer(c, offset), count, input[static_cast<size_t>(c)].data());

        for (int i = 0; i < count; ++i)
        {
            for (int c = 0; c < channelCount; ++c)
            {
                auto& delay = dryDelay[static_cast<size_t>(c)];
                dry[static_cast<size_t>(c)][static_cast<size_t>(i)] = delay[static_cast<size_t>(dryPosition)];
                delay[static_cast<size_t>(dryPosition)] = input[static_cast<size_t>(c)][static_cast<size_t>(i)];
            }
            if (++dryPosition == latency)
                dryPosition = 0;
        }

        stretch.setTransposeSemitones(pitch.skip(count), tonality.skip(count) / static_cast<float>(rate));
        stretch.setFormantBase(formantBase.skip(count) / static_cast<float>(rate));
        stretch.process(inputPointers.data(), count, outputPointers.data(), count);

        for (int i = 0; i < count; ++i)
        {
            const float mix = bypassMix.getNextValue();
            for (int c = 0; c < channelCount; ++c)
            {
                const auto channel = static_cast<size_t>(c), sample = static_cast<size_t>(i);
                buffer.setSample(c, offset + i, output[channel][sample] * (1.0f - mix) + dry[channel][sample] * mix);
            }
        }
    }
}
