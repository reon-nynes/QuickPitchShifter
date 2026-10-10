#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>

class PitchLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PitchLookAndFeel();
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class PitchSlider final : public juce::Slider
{
public:
    double snapValue(double attempted, DragMode mode) override
    {
        return mode == notDragging ? attempted : std::round(attempted);
    }
};

class TonalitySlider final : public juce::Slider
{
public:
    double snapValue(double attempted, DragMode mode) override
    {
        return mode == notDragging ? std::round(attempted) : std::round(attempted / 100.0) * 100.0;
    }
};

class QuickPitchShifterEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit QuickPitchShifterEditor(QuickPitchShifterProcessor&);
    ~QuickPitchShifterEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshStatus();
    ProcessingSettings pendingSettings() const;
    void configureSlider(juce::Slider&, double, double, double, double, const juce::String&,
                         juce::Colour, const juce::String&);

    QuickPitchShifterProcessor& processor;
    PitchLookAndFeel theme;
    juce::Slider formantBase, block, overlap;
    PitchSlider pitch;
    TonalitySlider tonality;
    juce::ToggleButton formants { "Preserve formants" };
    juce::TextButton bypass { "Bypass" }, apply { "Apply" };
    juce::Label latency, status;
    juce::TooltipWindow tooltips { this, 650 };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment,
        tonalityAttachment, baseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formantAttachment, bypassAttachment;
    int lastApplied = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickPitchShifterEditor)
};
