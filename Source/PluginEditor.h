#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LFamilyLook.h"

//==============================================================================
/** On-screen keyboard: click to play (input), output notes light up in orange. */
class OutputKeyboard : public juce::MidiKeyboardComponent
{
public:
    OutputKeyboard (juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent (s, horizontalKeyboard) {}
    void setOutputNotes (uint64_t lo, uint64_t hi) { if (lo != outLo || hi != outHi) { outLo = lo; outHi = hi; repaint(); } }

protected:
    bool isOut (int n) const { return n < 64 ? ((outLo >> n) & 1) != 0 : ((outHi >> (n - 64)) & 1) != 0; }

    void drawWhiteNote (int n, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override
    {
        juce::MidiKeyboardComponent::drawWhiteNote (n, g, area, isDown, isOver, lineColour, textColour);
        if (isOut (n))
        {
            g.setColour (lfam::colours::orange);
            g.fillRoundedRectangle (area.reduced (2.0f, 0.0f).removeFromBottom (area.getHeight() * 0.32f).reduced (0, 3), 2.0f);
        }
    }

    void drawBlackNote (int n, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour fill) override
    {
        juce::MidiKeyboardComponent::drawBlackNote (n, g, area, isDown, isOver, fill);
        if (isOut (n))
        {
            g.setColour (lfam::colours::orange);
            g.fillRoundedRectangle (area.reduced (2.0f).removeFromBottom (area.getHeight() * 0.35f), 2.0f);
        }
    }

private:
    uint64_t outLo = 0, outHi = 0;
};

//==============================================================================
/** The 16 progression step cells. */
class StepGrid : public juce::Component
{
public:
    std::function<void (int cell)> onCellClicked;
    ca::Progression prog;
    int key = 0, current = -1;
    bool editable = false;

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int cell = juce::jlimit (0, ca::kMaxSteps - 1, (int) (e.x / cellWidth()));
        if (onCellClicked) onCellClicked (cell);
    }
    float cellWidth() const { return getWidth() / (float) ca::kMaxSteps; }
};

/** 16-step rhythm pattern display with a running light. */
class PatternView : public juce::Component
{
public:
    int pattern = 0, current = -1;
    bool dimmed = false;
    void paint (juce::Graphics& g) override;
};

//==============================================================================
class CA2020Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CA2020Editor (CA2020Processor&);
    ~CA2020Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setChoice (const char* id, int value);
    int  intParam (const char* id) const;
    void editCell (int cell);
    void refreshGrid();
    void renameUser();
    void savePreset();
    void loadPreset();

    CA2020Processor& proc;
    lfam::LookAndFeel laf;

    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<ComboAtt>>  comboAtts;
    std::vector<std::unique_ptr<SliderAtt>> sliderAtts;
    std::vector<std::unique_ptr<ButtonAtt>> buttonAtts;

    // header
    juce::ComboBox styleBox, patchBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
    juce::ToggleButton previewT { "PREVIEW SYNTH" };

    // mode
    juce::TextButton modeBtns[4];
    juce::ComboBox triggerBox;
    juce::TextButton learnBtn { "LEARN" };
    juce::ToggleButton latchT { "LATCH" }, followT { "KEY FOLLOW" };

    // key & chord
    juce::ComboBox keyBox, scaleBox, chordTypeBox, extBox;

    // voicing
    juce::Slider invK, octK, strumK;
    juce::ComboBox spreadBox, bassBox;
    juce::ToggleButton leadT { "VOICE LEADING" };

    // feel
    juce::Slider velK, humK, swingK, tempoK;
    juce::ComboBox velModeBox;

    // progression
    juce::ComboBox progBox, speedBox;
    juce::TextButton addStepBtn { "+ STEP" }, delStepBtn { "- STEP" }, copyBtn { "COPY TO USER" }, renameBtn { "RENAME" };
    StepGrid grid;

    // rhythm / arp
    juce::TextButton outBtns[3];
    juce::ComboBox patternBox, arpModeBox, arpRateBox;
    PatternView patternView;
    juce::Slider arpOctK, gateK;

    OutputKeyboard keyboard;

    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;
    juce::Rectangle<int> secMode, secKey, secVoice, secFeel, secProg, secRhythm, lcdArea, logoArea, memArea;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> renameWindow;
    int lastUserVersion = -1, lastProg = -1, flash = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CA2020Editor)
};
