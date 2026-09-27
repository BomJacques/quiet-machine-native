#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace
{
const juce::Colour graphite { 0xff232628 }, surface { 0xff2d3133 }, ink { 0xffefeee7 };
const juce::Colour secondary { 0xffa6aeae }, line { 0xff454b4d };
const juce::Colour yellow { 0xffe2d99a }, teal { 0xff85b7ad };

class NativeLook final : public juce::LookAndFeel_V4
{
public:
    NativeLook()
    {
        setColour(juce::Label::textColourId, ink);
        setColour(juce::Slider::textBoxTextColourId, ink);
        setColour(juce::Slider::textBoxBackgroundColourId, graphite);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId, teal.withAlpha(0.4f));
        setColour(juce::ComboBox::backgroundColourId, surface);
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::outlineColourId, line);
        setColour(juce::ComboBox::arrowColourId, secondary);
        setColour(juce::PopupMenu::backgroundColourId, surface);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, teal);
        setColour(juce::PopupMenu::highlightedTextColourId, graphite);
        setColour(juce::TextEditor::backgroundColourId, surface);
        setColour(juce::TextEditor::textColourId, ink);
        setColour(juce::TextEditor::outlineColourId, line);
        setColour(juce::TextEditor::focusedOutlineColourId, teal);
        setColour(juce::TextEditor::highlightColourId, teal.withAlpha(0.4f));
        setColour(juce::CaretComponent::caretColourId, ink);
        setColour(juce::TextButton::buttonColourId, surface);
        setColour(juce::TextButton::textColourOffId, ink);
        setColour(juce::TooltipWindow::backgroundColourId, ink);
        setColour(juce::TooltipWindow::textColourId, graphite);
        setColour(juce::TooltipWindow::outlineColourId, line);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float position, float start, float end, juce::Slider& slider) override
    {
        auto r = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(5);
        const auto radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre();
        const auto angle = start + position * (end - start);
        juce::Path track, value;
        track.addCentredArc(c.x, c.y, radius, radius, 0, start, end, true);
        value.addCentredArc(c.x, c.y, radius, radius, 0, start, angle, true);
        g.setColour(line); g.strokePath(track, juce::PathStrokeType(2));
        g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(value, juce::PathStrokeType(2.5f));
        g.setColour(surface);
        g.fillEllipse(c.x - radius + 5, c.y - radius + 5, (radius - 5) * 2, (radius - 5) * 2);
        g.setColour(ink);
        const auto a = angle - juce::MathConstants<float>::halfPi;
        g.drawLine(c.x + std::cos(a) * radius * 0.44f, c.y + std::sin(a) * radius * 0.44f,
                   c.x + std::cos(a) * radius * 0.76f, c.y + std::sin(a) * radius * 0.76f, 2);
    }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                          float, float, juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const auto cx = (float) x + (float) w * 0.5f;
        g.setColour(line); g.fillRect(cx - 1, (float) y, 2.0f, (float) h);
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRect(cx - 1, pos, 2.0f, (float) y + (float) h - pos);
        g.setColour(ink); g.fillRoundedRectangle(cx - 12, pos - 4, 24, 8, 2);
        g.setColour(graphite); g.drawHorizontalLine((int) pos, cx - 8, cx + 8);
    }
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(3);
        auto mark = r.removeFromLeft(18).withSizeKeepingCentre(14, 14);
        const auto on = b.getToggleState();
        g.setColour(on ? yellow : (over ? secondary : juce::Colour(0xff788080)));
        if (on) g.fillRoundedRectangle(mark, 2); else g.drawRoundedRectangle(mark, 2, 1);
        if (on)
        {
            g.setColour(graphite);
            g.drawLine(mark.getX() + 3, mark.getCentreY(), mark.getX() + 5, mark.getBottom() - 3, 1.5f);
            g.drawLine(mark.getX() + 5, mark.getBottom() - 3, mark.getRight() - 2, mark.getY() + 3, 1.5f);
        }
        g.setColour(on ? ink : secondary); g.setFont(juce::FontOptions(12));
        g.drawText(b.getButtonText(), r.withTrimmedLeft(5), juce::Justification::centredLeft);
    }
};

class Dial final : public juce::Component
{
public:
    Dial(const juce::String& name, const juce::String& tip, bool fader = false)
    {
        label.setText(name, juce::dontSendNotification);
        label.setFont(juce::FontOptions(12)); label.setJustificationType(juce::Justification::centred);
        label.setTooltip(tip); addAndMakeVisible(label);
        slider.setSliderStyle(fader ? juce::Slider::LinearVertical : juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
        slider.setColour(juce::Slider::rotarySliderFillColourId, teal);
        slider.setColour(juce::Slider::trackColourId, yellow);
        slider.setTooltip(tip); slider.setName(name);
        slider.setNumDecimalPlacesToDisplay(2); slider.setScrollWheelEnabled(false);
        addAndMakeVisible(slider);
    }
    void attach(juce::AudioProcessorValueTreeState& state, const juce::String& id)
    {
        slider.setComponentID(id);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, slider);
        if (auto* p = state.getParameter(id)) slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
        const bool frequency = id.endsWith("Hz");
        juce::String unit;
        if (id == "ampAttack" || id == "ampDecay" || id == "ampRelease" || id == "glideSeconds") unit = " s";
        else if (id == "masterPitchSemitones") unit = " st";
        else if (id == "tapeSpeed") unit = " x";
        slider.textFromValueFunction = [unit, frequency](double v)
        {
            if (frequency) return v >= 1000 ? juce::String(v / 1000, 2) + " kHz"
                                            : juce::String(v, v >= 100 ? 0 : 2) + " Hz";
            return juce::String(v, 2) + unit;
        };
        slider.valueFromTextFunction = [frequency](const juce::String& text)
        { return text.getDoubleValue() * (frequency && text.containsIgnoreCase("k") ? 1000.0 : 1.0); };
        slider.updateText();
    }
    void resized() override
    {
        auto r = getLocalBounds(); label.setBounds(r.removeFromTop(20)); slider.setBounds(r);
    }
    juce::Slider slider;
private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// JUCE handles drag-note transitions and releases audition keys on focus loss.
class ChordKeyboard final : public juce::MidiKeyboardComponent, public juce::SettableTooltipClient
{
public:
    explicit ChordKeyboard(juce::MidiKeyboardState& state) : MidiKeyboardComponent(state, horizontalKeyboard)
    {
        setAvailableRange(48, 83); setLowestVisibleKey(48); setScrollButtonsVisible(false);
        clearKeyMappings();
        setKeyPressBaseOctave(4); setOctaveForMiddleC(4); setVelocity(0.75f, false);
        setWantsKeyboardFocus(true);
        setColour(whiteNoteColourId, ink); setColour(blackNoteColourId, graphite);
        setColour(keySeparatorLineColourId, surface); setColour(textLabelColourId, graphite);
        setColour(keyDownOverlayColourId, teal.withAlpha(0.85f));
        setColour(mouseOverKeyOverlayColourId, teal.withAlpha(0.25f));
        setTooltip("Latch: click to immediately save/remove a chord note. Audition or armed capture: click/drag or type A W S E D F T G Y H U J K O L P ;. Yellow dots mark saved notes; teal marks the selected Drone/Sub profile. HOLD is independent.");
    }
    bool mouseDownOnKey(int n, const juce::MouseEvent&) override
    { if (selectKey) selectKey(n - 48, editing); return ! editing; }
    bool mouseDraggedToKey(int n, const juce::MouseEvent&) override
    { if (! editing && selectKey) selectKey(n - 48, false); return ! editing; }
    bool keyStateChanged(bool down) override
    { return editing ? false : MidiKeyboardComponent::keyStateChanged(down); }
    void drawWhiteNote(int n, juce::Graphics& g, juce::Rectangle<float> r, bool down, bool over,
                       juce::Colour border, juce::Colour text) override
    { MidiKeyboardComponent::drawWhiteNote(n, g, r, down, over, border, text); drawMark(n, g, r, false); }
    juce::String getWhiteNoteText(int) override { return {}; }
    void drawBlackNote(int n, juce::Graphics& g, juce::Rectangle<float> r, bool down, bool over, juce::Colour fill) override
    { MidiKeyboardComponent::drawBlackNote(n, g, r, down, over, fill); drawMark(n, g, r, true); }
    void drawMark(int n, juce::Graphics& g, juce::Rectangle<float> r, bool black)
    {
        g.setColour(black ? ink : graphite); g.setFont(juce::FontOptions(black ? 9.0f : 10.0f));
        g.drawText(juce::MidiMessage::getMidiNoteName(n, true, true, 4),
                   r.withTop(r.getBottom() - 16), juce::Justification::centred);
        if (notes.contains(n - 48))
        {
            g.setColour(black ? yellow : juce::Colour(0xff8e8142));
            g.fillEllipse(r.getCentreX() - 3, r.getBottom() - 25.0f, 6, 6);
        }
        if (n - 48 == selected)
        { g.setColour(teal); g.fillRect(r.getX() + 1, r.getY(), r.getWidth() - 2, 4.0f); }
    }
    bool editing = true;
    int selected = 0;
    juce::Array<int> notes;
    std::function<void(int, bool)> selectKey;
};

class HelpOverlay final : public juce::Component
{
public:
    HelpOverlay()
    {
        setComponentID("helpOverlay"); setWantsKeyboardFocus(true);
        close.setButtonText("Close"); close.setComponentID("helpClose");
        close.onClick = [this] { if (dismiss) dismiss(); }; addAndMakeVisible(close);
        const char* names[] = { "Session", "Sources", "Tape", "Tone", "ADSR / Output", "Chord", "Key voice" };
        for (int i = 0; i < 7; ++i)
        {
            auto b = std::make_unique<juce::TextButton>(names[i]);
            b->onClick = [this, i] { selected = i; resized(); repaint(); };
            addAndMakeVisible(*b); tabs.push_back(std::move(b));
        }
    }
    void resized() override
    {
        panel = { (getWidth() - 740) / 2, selected >= 4 ? 112 : 420, 740, 304 };
        close.setBounds(panel.getRight() - 76, panel.getY() + 12, 64, 26);
        for (int i = 0; i < 7; ++i)
            tabs[(size_t) i]->setBounds(panel.getX() + 12 + i * 102, panel.getY() + 48, 98, 26);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(graphite.withAlpha(0.55f));
        if (juce::isPositiveAndBelow(selected, (int) regions.size()))
        { g.setColour(yellow); g.drawRect(regions[(size_t) selected], 2); }
        g.setColour(surface.withAlpha(0.97f)); g.fillRect(panel);
        g.setColour(teal); g.drawRect(panel, 1);
        g.setColour(ink); g.setFont(juce::FontOptions(18));
        g.drawText("QUIET MACHINE / CONTROL GUIDE", panel.reduced(16).removeFromTop(26), juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(15));
        g.drawFittedText(QuietMachineAudioProcessorEditor::helpText(selected),
                        panel.reduced(18).withTrimmedTop(68), juce::Justification::topLeft, 14);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (panel.contains(e.getPosition())) return;
        for (int i = 0; i < (int) regions.size(); ++i)
            if (regions[(size_t) i].contains(e.getPosition())) { selected = i; resized(); repaint(); break; }
    }
    bool keyPressed(const juce::KeyPress& k) override
    { if (k == juce::KeyPress::escapeKey && dismiss) dismiss(); return true; }
    bool keyStateChanged(bool) override { return true; }
    std::function<void()> dismiss;
    std::vector<juce::Rectangle<int>> regions;
private:
    int selected = 5;
    juce::Rectangle<int> panel;
    juce::TextButton close;
    std::vector<std::unique_ptr<juce::TextButton>> tabs;
};

juce::var readPatch(QuietMachineAudioProcessor& p)
{
    auto patch = juce::JSON::parse(p.getPatchStateForUi());
    if (patch.getDynamicObject() == nullptr) patch = new juce::DynamicObject();
    return patch;
}

}

class QuietMachineAudioProcessorEditor::Interface final : public juce::Component,
    private juce::Timer, private juce::MidiKeyboardState::Listener, private juce::ValueTree::Listener
{
public:
    explicit Interface(QuietMachineAudioProcessor& p) : processor(p), keyboard(keyState)
    {
        setLookAndFeel(&look); setOpaque(true);
        for (size_t i = 0; i < qm::sourceNames.size(); ++i)
        {
            const auto id = juce::String(qm::sourceNames[i]);
            const auto name = i < 5 ? "Tape " + juce::String((int) i + 1) : id.substring(0, 1).toUpperCase() + id.substring(1);
            sources.push_back(makeDial(id + "Gain", name, name + " source level. Double-click to reset.", true));
            mutes.push_back(makeToggle(id + "Mute", "M", "Mute " + name + "."));
        }
        const auto add = [this](auto& list, const char* id, const char* name, const char* tip)
        { list.push_back(makeDial(id, name, tip)); };
        add(tape, "tapeSpeed", "Speed", "Tape speed, 0.25x to 2x.");
        add(tape, "tapeAttack", "Attack", "Tape onset softness, independent of the overall envelope.");
        add(tape, "wowRateHz", "Wow rate", "Wow modulation rate in Hz.");
        add(tape, "wowDepth", "Wow", "Tape wow pitch modulation depth.");
        add(tape, "warpDepth", "Warp", "Tape warp intensity.");
        add(tape, "tapeWear", "Wear", "Tape wear and degradation.");
        add(tape, "tapeDrive", "Drive", "Tape saturation drive.");
        add(tone, "lowpassHz", "Low pass", "Low-pass cutoff in Hz.");
        add(tone, "lowpassQ", "Resonance", "Low-pass resonance (Q).");
        add(tone, "filterSweepDepth", "Sweep", "Filter modulation depth.");
        add(tone, "highpassHz", "High pass", "High-pass cutoff in Hz.");
        add(tone, "masterPitchSemitones", "Pitch", "Master tuning in semitones.");
        add(tone, "glideSeconds", "Glide", "Pitch glide in seconds.");
        add(envelope, "ampAttack", "Attack", "Overall amplitude attack in seconds.");
        add(envelope, "ampDecay", "Decay", "Overall amplitude decay in seconds.");
        add(envelope, "ampSustain", "Sustain", "Overall amplitude sustain level.");
        add(envelope, "ampRelease", "Release", "Overall amplitude release in seconds.");
        add(output, "masterGain", "Master", "Master output level.");
        add(output, "reverbMix", "Reverb", "Reverb wet/dry mix.");
        delay = makeToggle("delayEnabled", "Delay", "Enable master delay.");
        freeze = makeToggle("freezeEnabled", "Freeze", "Hold a captured audio texture.");
        hold = makeToggle("generatorEnabled", "HOLD", "Continuously hold the HOLD chord.");
        legato = makeToggle("ampLegato", "Legato", "ON: overlapping MIDI notes keep the overall envelope running. OFF: each incoming note retriggers Attack from the current level, without an abrupt cut. Applies to the whole output; not pitch glide.");
        midiChord.setComponentID("midiChordMode");
        midiChord.setName("MIDI chord per note");
        midiChord.setClickingTogglesState(true);
        midiChord.setColour(juce::TextButton::buttonOnColourId, yellow);
        midiChord.setColour(juce::TextButton::textColourOnId, graphite);
        midiChord.setTooltip("ON: each incoming MIDI note plays the selected MIDI chord shape, moved to that note. OFF: incoming notes play individually. Does not enable HOLD.");
        midiChord.onStateChange = [this]
        { midiChord.setButtonText(midiChord.getToggleState() ? "MIDI chord per note: ON" : "MIDI chord per note: OFF"); };
        midiChord.onClick = [this]
        {
            disarmCapture(); releaseKeyboard();
            if (midiChord.getToggleState()) chordTarget.setSelectedId(2, juce::sendNotificationSync);
        };
        midiChord.onStateChange();
        addAndMakeVisible(midiChord);
        toggles.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters, "midiChordMode", midiChord));
        waveform.addItemList({ "Saw", "Square", "Triangle", "Sine" }, 1);
        waveform.setComponentID("oscillatorWave");
        waveform.setTooltip("Oscillator waveform."); waveform.setName("Oscillator waveform"); addAndMakeVisible(waveform);
        waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "oscillatorWave", waveform);
        for (const auto& entry : qm::factory::presets()) preset.addItem(entry.name, preset.getNumItems() + 1);
        preset.setComponentID("factoryPreset");
        preset.setTextWhenNothingSelected("Sound presets");
        preset.setTooltip("Apply a factory sound and chord; stops HOLD and active notes first.");
        preset.onChange = [this] { if (preset.getSelectedId() > 0) applyPreset(preset.getSelectedId()); };
        addAndMakeVisible(preset);
        setupButton(save, "Save", "Save full processor state and chord/key profiles to a .quiet file.", [this] { chooseFile(true); });
        setupButton(load, "Load", "Load a saved .quiet processor state.", [this] { chooseFile(false); });
        setupButton(panic, "Stop", "Release audition notes, disable HOLD and send all-sound-off.", [this]
        { disarmCapture(); releaseKeyboard(); setParameter("generatorEnabled", 0); processor.panicFromUi(); });
        setupButton(helpToggle, "?", "Open the interactive control guide. Editing and typing are blocked until Close or Escape.", [this] { showHelp(! helpOverlay.isVisible()); });
        helpToggle.setComponentID("helpToggle");
        helpOverlay.dismiss = [this] { showHelp(false); };
        addChildComponent(helpOverlay);

        chordTarget.addItemList({ "HOLD chord", "MIDI shape" }, 1); chordTarget.setSelectedId(1);
        chordTarget.setComponentID("chordTarget");
        chordTarget.setTooltip("Choose HOLD's fixed chord or the transposable MIDI chord shape. C3-B5, up to 16 notes.");
        chordTarget.onChange = [this] { disarmCapture(); releaseKeyboard(); refreshChord(); }; addAndMakeVisible(chordTarget);
        keyboardMode.setComponentID("keyboardMode");
        keyboardMode.addItemList({ "Latch", "Audition" }, 1); keyboardMode.setSelectedId(1);
        keyboardMode.setTooltip("Latch: clicks immediately save chord membership until toggled off. Audition: play without editing. Neither mode enables HOLD.");
        keyboardMode.onChange = [this] { disarmCapture(); releaseKeyboard(); keyboard.editing = keyboardMode.getSelectedId() == 1; };
        addAndMakeVisible(keyboardMode);
        chordNoteNames.setComponentID("chordNoteNames"); chordNoteNames.setFont(juce::FontOptions(12));
        addAndMakeVisible(chordNoteNames);
        captureChord.setComponentID("captureChord"); captureChord.setButtonText("Hold chord to apply");
        captureChord.setTooltip("Arm, then change the held MIDI/typing notes and hold steady for 600 ms. Pre-held notes alone cannot apply. C3-B5 only, up to 16 notes. Applies once to the selected target; HOLD stays unchanged.");
        captureChord.onClick = [this]
        {
            if (! captureChord.getToggleState()) { disarmCapture(); return; }
            releaseKeyboard(); captureArmed = true; captureChanged = false;
            armMask = candidateMask = processor.getHeldInputChord(); stableSince = juce::Time::getMillisecondCounterHiRes();
            keyboard.editing = false; grabKeyboardFocus();
            status.setText("Capture armed: release/replay held notes, then hold C3-B5 steady for 600 ms", juce::dontSendNotification);
        };
        addAndMakeVisible(captureChord);
        setupButton(copyChord, "To MIDI", "Replace the MIDI chord with this HOLD chord, after confirmation.", [this]
        {
            const bool toMidi = chordTarget.getSelectedId() == 1;
            const auto targetName = juce::String(toMidi ? "MIDI" : "HOLD");
            const auto notes = chordVar();
            disarmCapture(); releaseKeyboard();
            juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Replace " + targetName + " chord?")
                .withMessage("The current " + targetName + " chord will be replaced by the selected chord.")
                .withButton("Replace").withButton("Cancel").withAssociatedComponent(this),
                [safe = juce::Component::SafePointer<Interface>(this), toMidi, targetName, notes](int result)
                {
                    if (result != 1 || safe == nullptr) return;
                    auto patch = readPatch(safe->processor);
                    patch.getDynamicObject()->setProperty(toMidi ? "midiChordNotes" : "autoNotes", notes);
                    safe->commitPatch(patch); safe->refreshChord();
                    safe->status.setText(targetName + " chord replaced", juce::dontSendNotification);
                });
        });
        copyChord.setComponentID("copyChord");
        for (int i = 0; i < 36; ++i)
            selectedKey.addItem(juce::MidiMessage::getMidiNoteName(i + 48, true, true, 4), i + 1);
        selectedKey.setComponentID("selectedKey");
        selectedKey.setSelectedId(1); selectedKey.setTooltip("Select the Drone/Sub per-key voice profile (C3-B5), independent of chord membership.");
        selectedKey.onChange = [this] { refreshVoice(); }; addAndMakeVisible(selectedKey);
        const char* names[] = { "Detune", "Pan", "Oscillators", "Spread" };
        const char* tips[] = { "Drone/Sub per-key tuning in cents (-50 to 50).", "Drone/Sub per-key pan (-1 left, +1 right).", "Drone/Sub per-key oscillator count (1-4).", "Drone/Sub per-key oscillator spread in cents (0-30)." };
        const double mins[] = { -50, -1, 1, 0 }, maxs[] = { 50, 1, 4, 30 }, steps[] = { 0.1, 0.01, 1, 0.1 }, defaults[] = { 0, 0, 2, 7 };
        for (int i = 0; i < 4; ++i)
        {
            auto dial = std::make_unique<Dial>(names[i], tips[i]);
            dial->slider.setRange(mins[i], maxs[i], steps[i]);
            dial->slider.setNumDecimalPlacesToDisplay(i == 2 ? 0 : (i == 1 ? 2 : 1));
            if (i == 0 || i == 3) dial->slider.setTextValueSuffix(" ct");
            dial->slider.setDoubleClickReturnValue(true, defaults[i]);
            dial->slider.onValueChange = [this] { if (! refreshing) writeVoice(); };
            addAndMakeVisible(*dial); voice.push_back(std::move(dial));
        }
        keyboard.selectKey = [this](int offset, bool toggle)
        {
            selectedKey.setSelectedId(offset + 1, juce::sendNotificationSync);
            if (toggle)
            {
                if (keyboard.notes.contains(offset)) keyboard.notes.removeFirstMatchingValue(offset);
                else if (keyboard.notes.size() < 16) keyboard.notes.add(offset);
                else { status.setText("Chord limit: 16 notes", juce::dontSendNotification); return; }
                keyboard.notes.sort(); writeChord();
            }
        };
        keyboard.setComponentID("chordKeyboard");
        addAndMakeVisible(keyboard); keyState.addListener(this);
        status.setFont(juce::FontOptions(11)); status.setColour(juce::Label::textColourId, secondary); addAndMakeVisible(status);
        processor.parameters.state.addListener(this);
        refreshChord(); refreshVoice(); lastPatch = processor.getPatchStateForUi(); startTimerHz(24);
    }
    ~Interface() override
    {
        stopTimer(); processor.parameters.state.removeListener(this);
        disarmCapture(); chooser.reset(); releaseKeyboard(); keyState.removeListener(this); setLookAndFeel(nullptr);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(graphite); g.setColour(ink); g.setFont(juce::FontOptions(27));
        g.drawText("QUIET MACHINE", 24, 17, 290, 35, juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11)); g.setColour(secondary);
        g.drawText("NATIVE  /  HELD TONES & TAPE", 26, 53, 280, 18, juce::Justification::centredLeft);
        section(g, "01   SOURCES", 24, 87, getWidth() - 48, 288, juce::Colour(0xff9dbad4));
        section(g, "02   TAPE", 24, 296, tapeSplit() - 40, 418, yellow);
        section(g, "03   TONE", tapeSplit() + 12, 296, getWidth() - tapeSplit() - 36, 418, teal);
        section(g, "04   OVERALL ADSR", 24, 426, envelopeEnd() - 40, 548, juce::Colour(0xffe3a48f));
        section(g, "05   OUTPUT", envelopeEnd() + 12, 426, getWidth() - envelopeEnd() - 36, 548, juce::Colour(0xffb2c9dc));
        section(g, "06   CHORD", 24, 554, keyboardEnd() - 40, 727, juce::Colour(0xffb8acd6));
        section(g, "KEY VOICE  /  DRONE + SUB", keyboardEnd() + 12, 554, getWidth() - keyboardEnd() - 36, 727, juce::Colour(0xffc7a8bd));
        g.setColour(line); g.drawHorizontalLine(getHeight() - 27, 24.0f, (float) getWidth() - 24);
        auto meter = meterBounds().toFloat();
        g.setColour(line); g.fillRect(meter);
        const auto db = juce::Decibels::gainToDecibels(peak, -60.0f);
        g.setColour(peak >= 1 ? juce::Colour(0xffdf9b85) : teal);
        g.fillRect(meter.withWidth(meter.getWidth() * juce::jlimit(0.0f, 1.0f, (db + 60) / 60)));
        g.setColour(secondary); g.setFont(juce::FontOptions(10));
        g.drawText("OUT", meterBounds().translated(0, -18).withHeight(14), juce::Justification::centredLeft);
    }
    void resized() override
    {
        const int width = getWidth() - 48;
        preset.setBounds(getWidth() - 550, 28, 204, 30); load.setBounds(getWidth() - 336, 28, 62, 30);
        save.setBounds(getWidth() - 266, 28, 62, 30);
        helpToggle.setBounds(324, 28, 30, 30);
        hold->setBounds(getWidth() - 194, 28, 90, 30); panic.setBounds(getWidth() - 96, 28, 72, 30);
        for (int i = 0; i < 12; ++i)
        {
            const int left = 24 + i * width / 12, right = 24 + (i + 1) * width / 12;
            sources[(size_t) i]->setBounds(left + 4, 110, right - left - 8, 147);
            mutes[(size_t) i]->setBounds((left + right) / 2 - 22, 259, 44, 25);
        }
        layoutDials(tape, { 24, 319, tapeSplit() - 40, 95 });
        layoutDials(tone, { tapeSplit() + 12, 319, getWidth() - tapeSplit() - 36, 95 });
        layoutDials(envelope, { 24, 449, envelopeEnd() - 40, 95 });
        legato->setBounds(envelopeEnd() - 100, 424, 84, 23);
        layoutDials(output, { envelopeEnd() + 12, 449, 172, 95 });
        const int switches = envelopeEnd() + 196;
        waveform.setBounds(switches, 461, 112, 28);
        delay->setBounds(switches + 122, 455, 88, 28); freeze->setBounds(switches + 122, 493, 88, 28);
        const auto keyWidth = keyboardEnd() - 40;
        midiChord.setBounds(24, 581, 228, 28);
        chordTarget.setBounds(262, 581, 136, 28); keyboardMode.setBounds(408, 581, 96, 28);
        copyChord.setBounds(keyboardEnd() - 96, 581, 80, 28);
        captureChord.setBounds(24, 613, 190, 30);
        chordNoteNames.setBounds(224, 613, keyWidth - 200, 30);
        keyboard.setBounds(24, 651, keyWidth, 71); keyboard.setKeyWidth((float) keyWidth / 21);
        selectedKey.setBounds(keyboardEnd() + 12, 581, getWidth() - keyboardEnd() - 36, 28);
        layoutDials(voice, { keyboardEnd() + 12, 621, getWidth() - keyboardEnd() - 36, 101 });
        status.setBounds(24, getHeight() - 25, getWidth() - 48, 22);
        helpOverlay.regions = { { 17, 12, getWidth() - 34, 64 }, { 17, 85, width + 14, 203 },
            { 17, 294, tapeSplit() - 26, 124 }, { tapeSplit() + 5, 294, getWidth() - tapeSplit() - 22, 124 },
            { 17, 424, width + 14, 124 }, { 17, 552, keyboardEnd() - 26, 175 },
            { keyboardEnd() + 5, 552, getWidth() - keyboardEnd() - 22, 175 } };
        helpOverlay.setBounds(getLocalBounds()); helpOverlay.toFront(false);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (helpOverlay.isVisible()) { if (key == juce::KeyPress::escapeKey) showHelp(false); return true; }
        if (! typingAllowed()) return false;
        const auto character = juce::CharacterFunctions::toUpperCase((juce::juce_wchar) key.getKeyCode());
        if (! juce::String(typingKeys).containsChar(character)) return false;
        updateTyping(true);
        return true;
    }
    bool keyStateChanged(bool) override { return updateTyping(true); }
private:
    using Dials = std::vector<std::unique_ptr<Dial>>;
    int tapeSplit() const { return 24 + (getWidth() - 48) * 7 / 13; }
    int envelopeEnd() const { return 24 + (getWidth() - 48) * 38 / 100; }
    int keyboardEnd() const { return 24 + (getWidth() - 48) * 66 / 100; }
    juce::Rectangle<int> meterBounds() const { return { getWidth() - 674, 49, 96, 4 }; }
    static void section(juce::Graphics& g, const juce::String& title, int x, int y, int width,
                        int bottom, juce::Colour accent)
    {
        g.setColour(accent.withAlpha(0.65f));
        g.drawRect(juce::Rectangle<float>((float) x - 7.0f, (float) y - 2.0f,
                                          (float) width + 14.0f, (float) (bottom - y) + 2.0f), 1.0f);
        g.setColour(accent); g.setFont(juce::FontOptions(12));
        g.drawText(title, x, y, width, 18, juce::Justification::centredLeft);
        g.setColour(accent.withAlpha(0.3f));
        g.drawHorizontalLine(y + 21, (float) x, (float) (x + width));
    }
    static void layoutDials(Dials& dials, juce::Rectangle<int> r)
    {
        const auto n = (int) dials.size();
        for (int i = 0; i < n; ++i)
            dials[(size_t) i]->setBounds(r.getX() + i * r.getWidth() / n + 2, r.getY(),
                (i + 1) * r.getWidth() / n - i * r.getWidth() / n - 4, r.getHeight());
    }
    std::unique_ptr<Dial> makeDial(const juce::String& id, const juce::String& name, const juce::String& tip, bool fader = false)
    {
        auto d = std::make_unique<Dial>(name, tip, fader); d->attach(processor.parameters, id); addAndMakeVisible(*d); return d;
    }
    std::unique_ptr<juce::ToggleButton> makeToggle(const juce::String& id, const juce::String& name, const juce::String& tip)
    {
        auto b = std::make_unique<juce::ToggleButton>(name); b->setTooltip(tip); b->setName(tip); addAndMakeVisible(*b);
        b->setComponentID(id);
        toggles.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, id, *b)); return b;
    }
    void setupButton(juce::TextButton& b, const juce::String& name, const juce::String& tip, std::function<void()> action)
    { b.setButtonText(name); b.setTooltip(tip); b.onClick = std::move(action); addAndMakeVisible(b); }
    void setParameter(const juce::String& id, float value)
    { processor.beginParameterGesture(id); processor.setParameterFromUi(id, value); processor.endParameterGesture(id); }
    void releaseKeyboard()
    {
        keyboard.focusLost(juce::Component::focusChangedDirectly);
        typedNotes.fill(false);
        keyState.allNotesOff(1);
    }
    bool typingAllowed() const
    {
        if (helpOverlay.isVisible() || keyboard.editing || ! hasKeyboardFocus(true) || ! juce::Process::isForegroundProcess()
            || isCurrentlyBlockedByAnotherModalComponent()) return false;
        const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
        if (mods.isCtrlDown() || mods.isAltDown() || mods.isCommandDown()) return false;
        for (auto* c = juce::Component::getCurrentlyFocusedComponent(); c != nullptr && c != this; c = c->getParentComponent())
            if (dynamic_cast<juce::TextEditor*>(c) != nullptr || dynamic_cast<juce::ComboBox*>(c) != nullptr) return false;
        return true;
    }
    bool updateTyping(bool allowNewNotes)
    {
        const bool allowed = typingAllowed();
        bool changed = false;
        for (size_t i = 0; i < typedNotes.size(); ++i)
        {
            const bool down = allowed && juce::KeyPress::isKeyCurrentlyDown(typingKeys[i]);
            if (typedNotes[i] && ! down)
            { typedNotes[i] = false; keyState.noteOff(1, 48 + (int) i, 0); changed = true; }
            else if (allowNewNotes && down && ! typedNotes[i])
            { typedNotes[i] = true; keyState.noteOn(1, 48 + (int) i, 0.75f); changed = true; }
        }
        return changed;
    }
    const char* chordProperty() const { return chordTarget.getSelectedId() == 1 ? "autoNotes" : "midiChordNotes"; }
    juce::var chordVar() const
    { juce::Array<juce::var> notes; for (auto n : keyboard.notes) notes.add(n); return notes; }
    void commitPatch(const juce::var& patch)
    { lastPatch = juce::JSON::toString(patch, true); processor.setPatchStateFromUi(lastPatch); }
    void refreshChord()
    {
        const bool toMidi = chordTarget.getSelectedId() == 1;
        copyChord.setButtonText(toMidi ? "To MIDI" : "To HOLD");
        copyChord.setTooltip(toMidi ? "Replace the MIDI chord with this HOLD chord, after confirmation."
                                   : "Replace the HOLD chord with this MIDI chord, after confirmation.");
        auto patch = readPatch(processor); auto value = patch.getProperty(chordProperty(), juce::var());
        if (! value.isArray()) value = patch.getProperty("autoNotes", juce::var());
        keyboard.notes.clear();
        if (auto* notes = value.getArray())
        { for (const auto& n : *notes) if (keyboard.notes.size() < 16) keyboard.notes.addIfNotAlreadyThere(juce::jlimit(0, 35, (int) n)); }
        else for (auto n : { 0, 7, 14, 19 }) keyboard.notes.add(n);
        keyboard.notes.sort(); juce::StringArray text;
        for (auto n : keyboard.notes) text.add(juce::MidiMessage::getMidiNoteName(n + 48, true, true, 4));
        chordNoteNames.setText(text.isEmpty() ? "No notes selected" : text.joinIntoString("  "), juce::dontSendNotification);
        chordNoteNames.setTooltip(text.joinIntoString(", ")); keyboard.repaint();
    }
    void writeChord()
    {
        auto patch = readPatch(processor);
        // Older patches omit MIDI shape and inherit HOLD. Materialise it before editing HOLD.
        if (! patch.getProperty("midiChordNotes", juce::var()).isArray())
        {
            auto inherited = patch.getProperty("autoNotes", juce::var());
            if (! inherited.isArray()) inherited = juce::Array<juce::var> { 0, 7, 14, 19 };
            patch.getDynamicObject()->setProperty("midiChordNotes", inherited);
        }
        patch.getDynamicObject()->setProperty(chordProperty(), chordVar());
        commitPatch(patch); refreshChord(); status.setText("Chord updated", juce::dontSendNotification);
    }
    void disarmCapture()
    {
        captureArmed = false; captureChanged = false; candidateMask = 0;
        captureChord.setToggleState(false, juce::dontSendNotification);
        keyboard.editing = keyboardMode.getSelectedId() == 1;
    }
    void showHelp(bool visible)
    {
        disarmCapture(); releaseKeyboard(); helpOverlay.setVisible(visible);
        if (visible) { helpOverlay.toFront(true); helpOverlay.grabKeyboardFocus(); }
        else grabKeyboardFocus();
    }
    void pollCapture()
    {
        if (! captureArmed) return;
        const uint64_t mask = processor.getHeldInputChord();
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (mask != armMask) captureChanged = true;
        if (mask != candidateMask) { candidateMask = mask; stableSince = now; }
        if ((mask & (uint64_t(1) << 63)) != 0)
        {
            stableSince = now;
            status.setText("Outside C3-B5: chord unchanged. Release out-of-range notes and replay.", juce::dontSendNotification); return;
        }
        if (! captureChanged || mask == 0) { stableSince = now; return; }
        juce::Array<int> notes;
        for (int i = 0; i < 36; ++i) if ((mask & (uint64_t(1) << i)) != 0) notes.add(i);
        if (notes.size() > 16)
        { stableSince = now; status.setText("Capture limit: 16 notes. Chord unchanged.", juce::dontSendNotification); return; }
        if (now - stableSince < 600.0) return;
        keyboard.notes = notes; disarmCapture(); writeChord();
        status.setText("Captured chord applied. HOLD unchanged.", juce::dontSendNotification);
    }
    void refreshVoice()
    {
        juce::ScopedValueSetter<bool> guard(refreshing, true); keyboard.selected = selectedKey.getSelectedId() - 1;
        const auto profiles = readPatch(processor).getProperty("keyVoices", juce::var()); juce::var profile;
        if (auto* a = profiles.getArray()) if (juce::isPositiveAndBelow(keyboard.selected, a->size())) profile = (*a)[keyboard.selected];
        const char* props[] = { "detuneCents", "pan", "oscillatorCount", "oscillatorSpreadCents" };
        const double defaults[] = { 0, 0, 2, 7 };
        for (int i = 0; i < 4; ++i) voice[(size_t) i]->slider.setValue((double) profile.getProperty(props[i], defaults[i]), juce::dontSendNotification);
        keyboard.repaint();
    }
    void writeVoice()
    {
        auto patch = readPatch(processor); auto profiles = patch.getProperty("keyVoices", juce::var());
        if (! profiles.isArray()) profiles = juce::Array<juce::var>();
        auto* a = profiles.getArray(); while (a->size() < 36) a->add(juce::var(new juce::DynamicObject()));
        auto& profile = a->getReference(juce::jlimit(0, 35, selectedKey.getSelectedId() - 1));
        if (profile.getDynamicObject() == nullptr) profile = new juce::DynamicObject();
        const char* props[] = { "detuneCents", "pan", "oscillatorCount", "oscillatorSpreadCents" };
        for (int i = 0; i < 4; ++i) profile.getDynamicObject()->setProperty(props[i], voice[(size_t) i]->slider.getValue());
        patch.getDynamicObject()->setProperty("keyVoices", profiles); commitPatch(patch);
    }
    void timerCallback() override
    {
        // Poll only releases: a key held while entering the editor must not start a note.
        updateTyping(false);
        if (! hasKeyboardFocus(true) || ! juce::Process::isForegroundProcess())
            keyboard.focusLost(juce::Component::focusChangedDirectly);
        const auto nextPeak = processor.getOutputPeak();
        if (nextPeak != peak) { peak = nextPeak; repaint(meterBounds()); }
        const auto latest = processor.getPatchStateForUi();
        if (stateRecalled.exchange(false) || latest != lastPatch)
        { disarmCapture(); releaseKeyboard(); lastPatch = latest; refreshChord(); refreshVoice(); }
        pollCapture();
    }
    void handleNoteOn(juce::MidiKeyboardState*, int, int n, float v) override { processor.noteOnFromUi(n, v); }
    void handleNoteOff(juce::MidiKeyboardState*, int, int n, float) override { processor.noteOffFromUi(n); }
    void valueTreeRedirected(juce::ValueTree&) override { stateRecalled.store(true); }
    void applyPreset(int index)
    {
        if (! juce::isPositiveAndBelow(index - 1, (int) qm::factory::presets().size())) return;
        disarmCapture(); releaseKeyboard(); processor.panicFromUi(); setParameter("generatorEnabled", 0);
        // Reset only the sound controls owned by this editor; leave MIDI routing alone.
        const char* soundIds[] = { "glideSeconds", "oscillatorWave", "masterPitchSemitones", "lowpassHz", "lowpassQ",
            "filterSweepDepth", "highpassHz", "tapeSpeed", "tapeAttack", "wowRateHz", "wowDepth", "warpDepth", "tapeWear",
            "tapeDrive", "reverbMix", "delayEnabled", "freezeEnabled", "masterGain", "ampAttack", "ampDecay", "ampSustain", "ampRelease" };
        const auto reset = [this](const juce::String& id)
        { if (auto* p = processor.parameters.getParameter(id)) setParameter(id, p->convertFrom0to1(p->getDefaultValue())); };
        for (auto* id : soundIds) reset(id);
        for (auto* source : qm::sourceNames) { reset(juce::String(source) + "Gain"); reset(juce::String(source) + "Mute"); }
        setParameter("masterGain", 0.65f);
        auto patch = readPatch(processor); auto* o = patch.getDynamicObject();
        o->setProperty("keyVoices", juce::Array<juce::var>());
        const auto& entry = qm::factory::presets()[(size_t) index - 1];
        for (const auto& [id, value] : entry.values) setParameter(id, value);
        juce::Array<juce::var> notes;
        for (const auto note : entry.notes) notes.add(note);
        o->setProperty("autoNotes", notes); o->setProperty("midiChordNotes", notes); commitPatch(patch);
        refreshChord(); refreshVoice(); status.setText(preset.getText(), juce::dontSendNotification);
    }
    void chooseFile(bool saving)
    {
        if (chooser != nullptr) return;
        disarmCapture(); releaseKeyboard();
        chooser = std::make_unique<juce::FileChooser>(saving ? "Save Quiet Machine sound" : "Load Quiet Machine sound",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Quiet Machine.quiet"), "*.quiet", true);
        const auto chooserFlags = saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting
                                 : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(chooserFlags, [safe = juce::Component::SafePointer<Interface>(this), saving](const juce::FileChooser& dialog)
        {
            if (safe == nullptr) return;
            const auto file = dialog.getResult();
            if (file != juce::File()) safe->processFile(file, saving);
            // The chooser owns this callback; destruction must wait until it unwinds.
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->chooser.reset(); });
        });
    }
    void processFile(const juce::File& file, bool saving)
    {
        juce::MemoryBlock data;
        if (saving)
        {
            processor.getStateInformation(data); juce::TemporaryFile temporary(file); bool written = false;
            {
                juce::FileOutputStream stream(temporary.getFile());
                if (stream.openedOk()) { written = stream.write(data.getData(), data.getSize()); stream.flush(); written = written && stream.getStatus().wasOk(); }
            }
            const auto ok = written && temporary.overwriteTargetFileWithTemporary();
            status.setText(ok ? "Saved: " + file.getFileName() : "Save failed: check folder permissions", juce::dontSendNotification); return;
        }
        if (file.getSize() <= 0 || file.getSize() > 4 * 1024 * 1024 || ! file.loadFileAsData(data))
        { status.setText("Load failed: unreadable or oversized file", juce::dontSendNotification); return; }
        auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), (int) data.getSize());
        const auto state = xml != nullptr ? juce::ValueTree::fromXml(*xml) : juce::ValueTree();
        if (! state.isValid() || state.getType() != processor.parameters.copyState().getType())
        { status.setText("Load failed: not a Quiet Machine state", juce::dontSendNotification); return; }
        releaseKeyboard(); processor.panicFromUi(); processor.setStateInformation(data.getData(), (int) data.getSize());
        lastPatch = processor.getPatchStateForUi(); refreshChord(); refreshVoice();
        preset.setSelectedId(0, juce::dontSendNotification); status.setText("Loaded: " + file.getFileName(), juce::dontSendNotification);
    }
    QuietMachineAudioProcessor& processor;
    NativeLook look;
    juce::TooltipWindow tooltips { this, 650 };
    Dials sources, tape, tone, envelope, output, voice;
    std::vector<std::unique_ptr<juce::ToggleButton>> mutes;
    std::unique_ptr<juce::ToggleButton> delay, freeze, hold, legato;
    juce::TextButton midiChord;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggles;
    juce::ComboBox waveform, preset, chordTarget, selectedKey, keyboardMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;
    juce::TextButton save, load, panic, copyChord, helpToggle;
    juce::ToggleButton captureChord;
    juce::Label status, chordNoteNames;
    HelpOverlay helpOverlay;
    juce::MidiKeyboardState keyState;
    ChordKeyboard keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastPatch;
    bool refreshing = false;
    static constexpr char typingKeys[] = "AWSEDFTGYHUJKOLP;";
    std::array<bool, sizeof(typingKeys) - 1> typedNotes {};
    float peak = 0;
    bool captureArmed = false, captureChanged = false;
    uint64_t armMask = 0, candidateMask = 0;
    double stableSince = 0;
    std::atomic<bool> stateRecalled { false };
};

juce::String QuietMachineAudioProcessorEditor::helpText(int section)
{
    switch (section)
    {
        case 0: return "Factory presets replace sound settings, both chords and per-key profiles; they stop notes and HOLD first. Save/Load stores/restores a complete .quiet sound.\n\nHOLD continuously sounds the HOLD chord. Selecting or capturing notes never turns HOLD on. Stop releases UI notes, cancels capture, disables HOLD and sends all-sound-off. OUT shows output level.\n\nClick a highlighted section or a tab to explore. Close or Escape returns to editing; help releases UI-held notes and blocks typing and edits.";
        case 1: return "Each fader sets one source level: Tape 1-5, Swell, Rumble, Piano, Drone, Sub, Static and Hiss. M mutes that source without changing its level.\n\nDrag vertically to mix sources. Double-click a sound control to restore its default; its readout shows the current value and can be edited. Detailed tooltips are available after closing help.\n\nDrone and Sub also use the selected note's Key voice settings. Other sources are not changed by those per-key controls.";
        case 2: return "Speed changes tape playback speed. Attack softens tape onset independently of the overall envelope.\n\nWow rate sets the modulation frequency; Wow sets its pitch depth. Warp adds tape instability. Wear controls degradation. Drive adds saturation.\n\nDrag a knob to change its value, double-click to reset, or edit its readout. These are sound controls, not chord-selection controls.";
        case 3: return "Low pass removes high frequencies; Resonance emphasizes its cutoff. Sweep controls filter modulation depth. High pass removes low frequencies.\n\nPitch tunes the overall sound in semitones without renaming the selected input notes. Glide sets the time taken to move between pitches.\n\nThese settings affect the sound globally. Per-note Drone/Sub detune and pan are in Key voice.";
        case 4: return "Overall ADSR: Attack fades in, Decay moves toward Sustain, Sustain sets the held level, and Release fades out after notes end.\n\nLegato ON: overlapping MIDI notes keep the envelope running. OFF: each incoming note retriggers Attack from the current level. This affects the whole output, including noise and effects; it is not pitch glide.\n\nMaster sets output level. Reverb sets wet/dry balance; Freeze holds a captured audio texture. Delay enables master delay. The waveform selector chooses Saw, Square, Triangle or Sine. Stop silences held sound.";
        case 5: return "MIDI chord per note ON: each incoming note plays the MIDI shape, rooted at that note. OFF: notes play individually. Clicking ON selects MIDI shape for editing; HOLD playback stays separate.\n\nTarget selects HOLD chord or MIDI shape. Latch clicks save/remove notes immediately. Audition plays without editing; drag keys or type A W S E D F T G Y H U J K O L P ;.\n\nHold chord to apply: arm, then change/replay MIDI or typing notes. Hold steady for 600 ms to capture once. Use 1-16 notes within C3-B5. Pre-held notes alone cannot apply.\n\nTo MIDI / To HOLD copies after confirmation. Capture and editing never enable HOLD. Stop, help, target/mode change, preset or load cancels capture.";
        case 6: return "The note selector chooses a Drone/Sub voice profile, independently of chord membership. Clicking a piano key also selects that profile; the teal key stripe marks it. All note names use C3-B5.\n\nDetune adjusts that note in cents. Pan moves it left/right. Oscillators sets its voice count; Spread separates those oscillators in cents.\n\nChanges save immediately per key. Double-click restores a control's default. Factory presets reset profiles; Save/Load preserves them with the chords and sound.";
        default: return {};
    }
}

QuietMachineAudioProcessorEditor::QuietMachineAudioProcessorEditor(QuietMachineAudioProcessor& p)
    : AudioProcessorEditor(&p), interface(std::make_unique<Interface>(p))
{
    addAndMakeVisible(*interface); setResizable(true, true); setResizeLimits(1080, 760, 1440, 760); setSize(1120, 760);
}
QuietMachineAudioProcessorEditor::~QuietMachineAudioProcessorEditor() = default;
void QuietMachineAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(graphite); }
void QuietMachineAudioProcessorEditor::resized()
{
    interface->setBounds(getLocalBounds().withSizeKeepingCentre(juce::jmin(1440, getWidth()), juce::jmin(760, getHeight())));
}
