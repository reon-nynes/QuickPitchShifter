#include "PluginEditor.h"

namespace
{
const juce::Colour background { 0xff10151d }, panel { 0xff19212c }, border { 0xff2a3544 };
const juce::Colour text { 0xffedf3fb }, muted { 0xff8d9db3 };
const juce::Colour teal { 0xff55dfcc }, purple { 0xffb49aff }, amber { 0xffedbf77 }, green { 0xff9dd398 };

void drawText(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> bounds,
              float size, juce::Colour colour, bool bold = false)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value, bounds, juce::Justification::centredLeft);
}
}

PitchLookAndFeel::PitchLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, background);
    setColour(juce::Slider::textBoxOutlineColourId, border);
    setColour(juce::Slider::textBoxHighlightColourId, teal.withAlpha(0.25f));
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, background);
    setColour(juce::TextButton::buttonColourId, border);
    setColour(juce::TextButton::buttonOnColourId, amber);
    setColour(juce::TooltipWindow::backgroundColourId, panel.brighter(0.1f));
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, border);
    setColour(juce::Label::textColourId, muted);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::backgroundColourId, background);
    setColour(juce::TextEditor::highlightColourId, teal.withAlpha(0.25f));
}

void PitchLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPosition, float, float,
                                       juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float start = static_cast<float>(x), end = static_cast<float>(x + width);
    const auto accent = slider.findColour(juce::Slider::thumbColourId)
                             .withMultipliedAlpha(slider.isEnabled() ? 1.0f : 0.3f);
    g.setColour(border);
    g.drawLine(start, centreY, end, centreY, 5.0f);
    float origin = start;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
    {
        origin = start + static_cast<float>(slider.valueToProportionOfLength(0.0)) * static_cast<float>(width);
        g.setColour(muted.withAlpha(0.5f));
        g.drawLine(origin, centreY - 8.0f, origin, centreY + 8.0f, 1.0f);
    }
    g.setColour(accent.withAlpha(0.75f));
    g.drawLine(origin, centreY, sliderPosition, centreY, 5.0f);
    g.setColour(background);
    g.fillEllipse(sliderPosition - 9.0f, centreY - 9.0f, 18.0f, 18.0f);
    g.setColour(accent);
    g.fillEllipse(sliderPosition - 7.0f, centreY - 7.0f, 14.0f, 14.0f);
}

void PitchLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& colour, bool hover, bool down)
{
    auto fill = button.getToggleState() ? button.findColour(juce::TextButton::buttonOnColourId) : colour;
    if (hover)
        fill = fill.brighter(0.12f);
    if (down)
        fill = fill.darker(0.12f);
    g.setColour(fill.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.35f));
    g.fillRoundedRectangle(button.getLocalBounds().toFloat().reduced(0.5f), 7.0f);
}

void PitchLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool hover, bool)
{
    const auto bounds = button.getLocalBounds();
    const juce::Rectangle<float> box { 0.0f, static_cast<float>(bounds.getCentreY()) - 9.0f, 18.0f, 18.0f };
    g.setColour(button.getToggleState() ? amber : border.brighter(hover ? 0.2f : 0.0f));
    g.fillRoundedRectangle(box, 5.0f);
    if (button.getToggleState())
    {
        juce::Path check;
        check.startNewSubPath(4.0f, box.getCentreY());
        check.lineTo(8.0f, box.getCentreY() + 4.0f);
        check.lineTo(14.0f, box.getCentreY() - 4.0f);
        g.setColour(background);
        g.strokePath(check, juce::PathStrokeType(2.0f));
    }
    drawText(g, button.getButtonText(), bounds.withTrimmedLeft(28), 14.0f, text);
}

void QuickPitchShifterEditor::configureSlider(juce::Slider& slider, double minimum, double maximum,
                                             double interval, double initial, const juce::String& suffix,
                                             juce::Colour accent, const juce::String& tooltip)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 100, 32);
    slider.setRange(minimum, maximum, interval);
    slider.setValue(initial, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, initial);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::thumbColourId, accent);
    slider.setTooltip(tooltip);
    slider.setName(tooltip);
    addAndMakeVisible(slider);
}

QuickPitchShifterEditor::QuickPitchShifterEditor(QuickPitchShifterProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    setLookAndFeel(&theme);
    configureSlider(pitch, -12, 12, 0, 0, " st", teal,
                    "Drag in whole semitones, or type a fractional value. Double-click to reset.");
    pitch.setComponentID("pitch");
    configureSlider(tonality, 2000, 20000, 1, 8000, " Hz", purple,
                    "Changes high-frequency pitch mapping to retain timbre. This is not a filter cutoff.");
    configureSlider(formantBase, 50, 500, 1, 200, " Hz", amber,
                    "Expected fundamental for formant analysis: lower for low voices, higher for high voices.");
    const auto settings = processor.appliedSettings();
    lastApplied = settings.encode();
    configureSlider(block, 50, 180, 1, settings.blockMs, " ms", green,
                    "Analysis window. Shorter windows reduce latency and may reduce quality. Press Apply to use changes.");
    configureSlider(overlap, 2, 8, 0.1, settings.overlap, " x", green,
                    "Analysis block divided by processing interval. Larger overlap usually uses more CPU. Press Apply.");
    block.setDoubleClickReturnValue(true, 120);
    overlap.setDoubleClickReturnValue(true, 4);
    block.setComponentID("analysisBlock");
    overlap.setComponentID("overlap");
    apply.setComponentID("apply");

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    pitchAttachment = std::make_unique<SliderAttachment>(processor.parameters, "pitch", pitch);
    tonalityAttachment = std::make_unique<SliderAttachment>(processor.parameters, "tonality", tonality);
    baseAttachment = std::make_unique<SliderAttachment>(processor.parameters, "formantBase", formantBase);
    addAndMakeVisible(formants);
    addAndMakeVisible(bypass);
    addAndMakeVisible(apply);
    bypass.setClickingTogglesState(true);
    bypass.setTooltip("Unshifted audio with the processing delay retained. Switching is crossfaded.");
    formants.setTooltip("Compensate for pitch changes to retain the source's formant character.");
    apply.setTooltip("Apply both processing settings. Rebuilding may briefly interrupt audio.");
    apply.setColour(juce::TextButton::buttonColourId, green.darker(0.65f));
    formantAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "formants", formants);
    bypassAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "bypass", bypass);
    apply.onClick = [this] { processor.applyProcessingSettings(pendingSettings()); refreshStatus(); };
    block.onValueChange = [this] { refreshStatus(); };
    overlap.onValueChange = [this] { refreshStatus(); };
    for (auto* label : { &latency, &status })
    {
        addAndMakeVisible(*label);
        label->setFont(juce::FontOptions(13.0f));
    }
    latency.setTooltip("Effect latency only; host and audio-device buffering add further delay.");
    setSize(720, 600);
    refreshStatus();
    startTimerHz(15);
}

QuickPitchShifterEditor::~QuickPitchShifterEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

ProcessingSettings QuickPitchShifterEditor::pendingSettings() const
{
    return { static_cast<int>(block.getValue()), static_cast<float>(overlap.getValue()) };
}

void QuickPitchShifterEditor::refreshStatus()
{
    const int applied = processor.appliedSettings().encode();
    if (applied != lastApplied)
    {
        const auto settings = processor.appliedSettings();
        block.setValue(settings.blockMs, juce::dontSendNotification);
        overlap.setValue(settings.overlap, juce::dontSendNotification);
        lastApplied = applied;
    }
    const bool pending = pendingSettings().encode() != applied;
    const bool busy = processor.isReconfiguring();
    const bool failed = processor.hasConfigurationError();
    apply.setEnabled(!busy && (pending || failed));
    apply.setButtonText(busy ? "Applying..." : "Apply");
    const auto active = processor.activeSettings();
    const double latencyMs = 1000.0 * processor.activeLatencySamples() / processor.processingSampleRate();
    latency.setText("Latency  " + juce::String(latencyMs, 1) + " ms", juce::dontSendNotification);
    if (failed)
        status.setText("Configuration failed. Press Apply to retry.", juce::dontSendNotification);
    else if (busy)
        status.setText("Applying processing settings...", juce::dontSendNotification);
    else if (pending)
        status.setText("Pending changes. Apply may briefly interrupt audio.", juce::dontSendNotification);
    else
        status.setText("Active: " + juce::String(active.blockMs) + " ms / " + juce::String(active.overlap, 1)
                       + " overlap", juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, failed ? juce::Colours::salmon : pending ? amber : muted);
    if (formantBase.isEnabled() != formants.getToggleState())
    {
        formantBase.setEnabled(formants.getToggleState());
        repaint();
    }
}

void QuickPitchShifterEditor::timerCallback()
{
    refreshStatus();
}

void QuickPitchShifterEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    drawText(g, "Quick Pitch Shifter", { 28, 18, 500, 34 }, 27.0f, text, true);
    drawText(g, "NYTERA  /  LIVE AUDIO  /  0.1.0", { 29, 54, 500, 20 }, 11.0f, muted);
    for (const auto area : { juce::Rectangle<int>(20, 90, 680, 118),
                             juce::Rectangle<int>(20, 220, 680, 166),
                             juce::Rectangle<int>(20, 398, 680, 146) })
    {
        g.setColour(panel);
        g.fillRoundedRectangle(area.toFloat(), 12.0f);
        g.setColour(border.withAlpha(0.7f));
        g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 12.0f, 1.0f);
    }
    drawText(g, "PITCH", { 36, 106, 160, 22 }, 12.0f, teal, true);
    drawText(g, "One octave down or up", { 36, 128, 300, 20 }, 12.0f, muted);
    drawText(g, "Tonality limit", { 36, 233, 180, 32 }, 14.0f, text);
    drawText(g, "Formant base (Hz)", { 36, 326, 180, 34 }, 14.0f,
             formants.getToggleState() ? text : muted);
    drawText(g, "PROCESSING", { 36, 411, 160, 26 }, 12.0f, green, true);
    drawText(g, "Analysis block", { 36, 450, 166, 34 }, 14.0f, text);
    drawText(g, "Overlap", { 36, 493, 166, 34 }, 14.0f, text);
    drawText(g, "Powered by Signalsmith Stretch", { 28, 558, 310, 24 }, 12.0f, muted);
}

void QuickPitchShifterEditor::resized()
{
    bypass.setBounds(588, 28, 104, 34);
    pitch.setBounds(36, 151, 648, 40);
    tonality.setBounds(212, 231, 472, 38);
    formants.setBounds(36, 282, 230, 32);
    formantBase.setBounds(212, 325, 472, 38);
    apply.setBounds(588, 409, 96, 30);
    latency.setBounds(208, 410, 360, 30);
    block.setBounds(204, 448, 480, 38);
    overlap.setBounds(204, 490, 480, 38);
    status.setBounds(342, 556, 350, 28);
}
