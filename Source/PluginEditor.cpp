#include "PluginEditor.h"

using namespace lfam;

static const float kBeatChoices[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
static juce::String beatsLabel (float b)
{
    if (b < 4.0f) return juce::String ((int) b) + (b == 1.0f ? " beat" : " beats");
    const float bars = b / 4.0f;
    return (bars == std::floor (bars) ? juce::String ((int) bars) : juce::String (bars, 1)) + (bars == 1.0f ? " bar" : " bars");
}

//==============================================================================
void StepGrid::paint (juce::Graphics& g)
{
    const float w = cellWidth();
    for (int i = 0; i < ca::kMaxSteps; ++i)
    {
        auto r = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (2.0f, 0.0f);
        const bool used = i < prog.numSteps;
        const bool cur = used && i == current;

        if (used) drawLcd (g, r);
        else
        {
            g.setColour (colours::well.withAlpha (0.6f)); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (colours::line); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        }

        if (cur)
        {
            g.setColour (colours::orange.withAlpha (0.22f)); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (colours::orange); g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 2.0f);
        }

        g.setColour (colours::textDim);
        g.setFont (font (10.0f, true));
        g.drawText (juce::String (i + 1), r.removeFromTop (14.0f).reduced (4.0f, 0.0f), juce::Justification::centredLeft);

        if (! used)
        {
            if (editable && i == prog.numSteps)
            {
                g.setColour (colours::textDim);
                g.setFont (font (20.0f, true));
                g.drawText ("+", r, juce::Justification::centred);
            }
            continue;
        }

        const auto& st = prog.steps[i];
        const auto q = (ca::Quality) st.quality;
        g.setColour (cur ? colours::lcdText : colours::lcdText.withAlpha (0.8f));
        g.setFont (font (r.getWidth() < 60 ? 15.0f : 17.0f, true));
        g.drawFittedText (juce::String (ca::chordName ((key + st.semis) % 12, q)), r.removeFromTop (r.getHeight() * 0.45f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
        g.setFont (font (12.0f));
        g.setColour (colours::lcdText.withAlpha (0.65f));
        g.drawFittedText (juce::String (ca::formatRoman (st.semis, q)), r.removeFromTop (r.getHeight() * 0.5f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
        g.setColour (colours::textDim);
        g.setFont (font (10.5f));
        g.drawText (beatsLabel (st.beats), r, juce::Justification::centred);
    }
}

void PatternView::paint (juce::Graphics& g)
{
    const auto& steps = ca::patterns()[(size_t) juce::jlimit (0, (int) ca::patterns().size() - 1, pattern)].steps;
    const float w = getWidth() / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        auto r = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (1.5f, 0.0f);
        const char c = steps[i];
        g.setColour ((i % 4 == 0) ? colours::panelHi.brighter (0.1f) : colours::panelHi);
        g.fillRoundedRectangle (r, 2.0f);
        auto ledR = r.reduced (r.getWidth() * 0.18f, getHeight() * 0.22f);
        juce::Colour col = colours::well;
        if (c == 'x') col = colours::led;
        else if (c == 'o') col = colours::orange;
        else if (c == '-') col = colours::orange.withAlpha (0.35f);
        if (dimmed) col = col.withMultipliedAlpha (0.35f);
        g.setColour (col);
        if (c == '-') g.fillRect (r.getX(), r.getCentreY() - 2.0f, r.getWidth() + 3.0f, 4.0f);
        else          g.fillRoundedRectangle (ledR, 2.0f);
        if (i == current && ! dimmed)
        {
            g.setColour (colours::text);
            g.drawRoundedRectangle (r, 2.0f, 1.5f);
        }
    }
}

//==============================================================================
CA2020Editor::CA2020Editor (CA2020Processor& p)
    : AudioProcessorEditor (p), proc (p), keyboard (p.keyboardState)
{
    setLookAndFeel (&laf);
    auto& ap = proc.apvts;

    auto combo = [&] (juce::ComboBox& c, const char* id)
    {
        if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter (id)))
            c.addItemList (prm->choices, 1);
        addAndMakeVisible (c);
        comboAtts.push_back (std::make_unique<ComboAtt> (ap, id, c));
    };
    auto knob = [&] (juce::Slider& s, const char* id, juce::Colour col)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        s.setColour (juce::Slider::rotarySliderFillColourId, col);
        addAndMakeVisible (s);
        sliderAtts.push_back (std::make_unique<SliderAtt> (ap, id, s));
    };
    auto toggle = [&] (juce::ToggleButton& b, const char* id)
    {
        addAndMakeVisible (b);
        buttonAtts.push_back (std::make_unique<ButtonAtt> (ap, id, b));
    };

    // header
    {
        juce::String lastGenre;
        const auto& st = ca::styles();
        for (int i = 0; i < (int) st.size(); ++i)
        {
            if (lastGenre != st[(size_t) i].genre) { styleBox.addSectionHeading (st[(size_t) i].genre); lastGenre = st[(size_t) i].genre; }
            styleBox.addItem (st[(size_t) i].name, i + 1);
        }
        styleBox.setTextWhenNothingSelected ("Choose a style...");
        styleBox.onChange = [this] { if (auto id = styleBox.getSelectedId(); id > 0) proc.applyStyle (id - 1); };
        addAndMakeVisible (styleBox);

        prevBtn.onClick = [this] { proc.applyStyle (juce::jmax (0, proc.currentStyle.load() - 1)); };
        nextBtn.onClick = [this] { proc.applyStyle (juce::jmin ((int) ca::styles().size() - 1, proc.currentStyle.load() + 1)); };
        saveBtn.onClick = [this] { savePreset(); };
        loadBtn.onClick = [this] { loadPreset(); };
        for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &loadBtn }) addAndMakeVisible (b);
        toggle (previewT, "preview");
        combo (patchBox, "previewPatch");
    }

    // mode
    const char* modeNames[] = { "CHORD", "PROGRESSION", "MEMORY", "THRU" };
    for (int i = 0; i < 4; ++i)
    {
        modeBtns[i].setButtonText (modeNames[i]);
        modeBtns[i].setColour (juce::TextButton::buttonOnColourId, colours::orange);
        modeBtns[i].onClick = [this, i] { setChoice ("mode", i); };
        addAndMakeVisible (modeBtns[i]);
    }
    combo (triggerBox, "trigger");
    learnBtn.setColour (juce::TextButton::buttonOnColourId, colours::led);
    learnBtn.setTooltip ("Play a chord on your keyboard, release it, and it is stored as the Memory chord");
    learnBtn.onClick = [this] { setChoice ("mode", ca::ModeMemory); proc.engine.learnRequest = true; };
    addAndMakeVisible (learnBtn);
    toggle (latchT, "latch");
    toggle (followT, "keyFollow");

    // key & chord
    combo (keyBox, "key"); combo (scaleBox, "scale"); combo (chordTypeBox, "chordType"); combo (extBox, "extension");

    // voicing
    knob (invK, "inversion", colours::blue); knob (octK, "octave", colours::blue); knob (strumK, "strum", colours::blue);
    combo (spreadBox, "spread"); combo (bassBox, "bass");
    toggle (leadT, "voiceLead");

    // feel
    knob (velK, "velocity", colours::teal); knob (humK, "humanize", colours::teal);
    knob (swingK, "swing", colours::teal); knob (tempoK, "tempo", colours::teal);
    combo (velModeBox, "velMode");

    // progression
    combo (progBox, "progression"); combo (speedBox, "progSpeed");
    grid.onCellClicked = [this] (int c) { editCell (c); };
    addAndMakeVisible (grid);
    addStepBtn.onClick = [this] { editCell (grid.prog.numSteps); };
    delStepBtn.onClick = [this]
    {
        const int nf = (int) ca::styles().size(), prog = intParam ("progression");
        if (prog < nf) return;
        auto pr = proc.getUserProgression (prog - nf);
        if (pr.numSteps > 1) { --pr.numSteps; proc.setUserProgression (prog - nf, pr); }
    };
    copyBtn.onClick = [this]
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Copy this progression to...");
        for (int i = 0; i < ca::kNumUserProgs; ++i)
            m.addItem (i + 1, "User " + juce::String (i + 1) + "  (" + juce::String (proc.getUserProgression (i).name) + ")");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&copyBtn), [this] (int r)
        {
            if (r <= 0) return;
            const int nf = (int) ca::styles().size();
            auto src = proc.engine.progressionFor (intParam ("progression"));
            proc.setUserProgression (r - 1, src);
            setChoice ("progression", nf + r - 1);
        });
    };
    renameBtn.onClick = [this] { renameUser(); };
    for (auto* b : { &addStepBtn, &delStepBtn, &copyBtn, &renameBtn }) addAndMakeVisible (b);

    // rhythm / arp
    const char* outNames[] = { "SUSTAIN", "RHYTHM", "ARP" };
    for (int i = 0; i < 3; ++i)
    {
        outBtns[i].setButtonText (outNames[i]);
        outBtns[i].setColour (juce::TextButton::buttonOnColourId, colours::red);
        outBtns[i].onClick = [this, i] { setChoice ("output", i); };
        addAndMakeVisible (outBtns[i]);
    }
    combo (patternBox, "pattern"); combo (arpModeBox, "arpMode"); combo (arpRateBox, "arpRate");
    addAndMakeVisible (patternView);
    knob (arpOctK, "arpOctaves", colours::red); knob (gateK, "gate", colours::red);

    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (16.0f);
    addAndMakeVisible (keyboard);

    setSize (1060, 690);
    refreshGrid();
    startTimerHz (30);
}

CA2020Editor::~CA2020Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void CA2020Editor::setChoice (const char* id, int value)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) value));
        prm->endChangeGesture();
    }
}

int CA2020Editor::intParam (const char* id) const
{
    if (auto* v = proc.apvts.getRawParameterValue (id)) return (int) std::lround (v->load());
    return 0;
}

void CA2020Editor::refreshGrid()
{
    const int prog = intParam ("progression");
    const int nf = (int) ca::styles().size();
    grid.prog = proc.engine.progressionFor (prog);
    if (prog >= nf) grid.prog = proc.getUserProgression (prog - nf);
    grid.editable = prog >= nf;
    grid.key = intParam ("key");
    for (auto* b : { &addStepBtn, &delStepBtn, &renameBtn }) b->setEnabled (grid.editable);
    grid.repaint();
}

void CA2020Editor::editCell (int cell)
{
    const int nf = (int) ca::styles().size();
    const int prog = intParam ("progression");

    if (prog < nf)
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Factory progressions are read-only");
        for (int i = 0; i < ca::kNumUserProgs; ++i) m.addItem (i + 1, "Copy to User " + juce::String (i + 1) + " and edit");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&grid), [this, nf, prog] (int r)
        {
            if (r <= 0) return;
            proc.setUserProgression (r - 1, proc.engine.progressionFor (prog));
            setChoice ("progression", nf + r - 1);
        });
        return;
    }

    const int slot = prog - nf;
    auto pr = proc.getUserProgression (slot);

    if (cell >= pr.numSteps)
    {
        if (pr.numSteps >= ca::kMaxSteps) return;
        pr.steps[pr.numSteps] = pr.numSteps > 0 ? pr.steps[pr.numSteps - 1] : ca::ProgStep {};
        ++pr.numSteps;
        proc.setUserProgression (slot, pr);
        return;
    }

    const auto st = pr.steps[cell];
    const int key = intParam ("key");
    juce::PopupMenu roots, quals, lens, m;
    for (int s = 0; s < 12; ++s)
        roots.addItem (100 + s, juce::String (ca::formatRoman (s, (ca::Quality) st.quality)) + "   (" + juce::String (ca::chordName ((key + s) % 12, (ca::Quality) st.quality)) + ")", true, s == st.semis);
    for (int q = 0; q < (int) ca::Quality::Count; ++q)
        quals.addItem (200 + q, juce::String (ca::info ((ca::Quality) q).name) + "   (" + juce::String (ca::chordName ((key + st.semis) % 12, (ca::Quality) q)) + ")", true, q == st.quality);
    for (int i = 0; i < 8; ++i)
        lens.addItem (300 + i, beatsLabel (kBeatChoices[i]), true, std::abs (kBeatChoices[i] - st.beats) < 0.01f);

    m.addSectionHeader ("Step " + juce::String (cell + 1) + ":  " + juce::String (ca::chordName ((key + st.semis) % 12, (ca::Quality) st.quality)));
    m.addSubMenu ("Root (degree)", roots);
    m.addSubMenu ("Chord type", quals);
    m.addSubMenu ("Length", lens);
    m.addSeparator();
    m.addItem (402, "Duplicate step", pr.numSteps < ca::kMaxSteps);
    m.addItem (400, "Insert new step after", pr.numSteps < ca::kMaxSteps);
    m.addItem (401, "Delete step", pr.numSteps > 1);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&grid), [this, slot, cell] (int r)
    {
        if (r <= 0) return;
        auto p = proc.getUserProgression (slot);
        if (cell >= p.numSteps) return;
        if (r >= 100 && r < 112)      p.steps[cell].semis = r - 100;
        else if (r >= 200 && r < 300) p.steps[cell].quality = r - 200;
        else if (r >= 300 && r < 400) p.steps[cell].beats = kBeatChoices[r - 300];
        else if (r == 400 || r == 402)
        {
            if (p.numSteps >= ca::kMaxSteps) return;
            for (int k = p.numSteps; k > cell + 1; --k) p.steps[k] = p.steps[k - 1];
            p.steps[cell + 1] = r == 402 ? p.steps[cell] : ca::ProgStep { 0, 0, 4.0f };
            ++p.numSteps;
        }
        else if (r == 401)
        {
            if (p.numSteps <= 1) return;
            for (int k = cell; k < p.numSteps - 1; ++k) p.steps[k] = p.steps[k + 1];
            --p.numSteps;
        }
        proc.setUserProgression (slot, p);
    });
}

void CA2020Editor::renameUser()
{
    const int nf = (int) ca::styles().size(), prog = intParam ("progression");
    if (prog < nf) return;
    const int slot = prog - nf;
    renameWindow = std::make_unique<juce::AlertWindow> ("Rename progression", "Name for User " + juce::String (slot + 1) + ":",
                                                        juce::MessageBoxIconType::NoIcon);
    renameWindow->addTextEditor ("name", proc.getUserProgression (slot).name);
    renameWindow->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    renameWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    renameWindow->setLookAndFeel (&laf);
    juce::Component::SafePointer<CA2020Editor> safeThis (this);
    renameWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this, slot, safeThis] (int r)
    {
        if (r == 1 && renameWindow != nullptr)
        {
            auto p = proc.getUserProgression (slot);
            std::memset (p.name, 0, sizeof (p.name));
            renameWindow->getTextEditorContents ("name").substring (0, 38).copyToUTF8 (p.name, sizeof (p.name) - 1);
            proc.setUserProgression (slot, p);
        }
        juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->renameWindow.reset(); });
    }), false);
}

void CA2020Editor::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Save CA-2020 preset", CA2020Processor::presetFolder().getChildFile ("My preset.ca2020"), "*.ca2020");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != juce::File()) proc.savePreset (f.withFileExtension ("ca2020"));
                          });
}

void CA2020Editor::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Load CA-2020 preset", CA2020Processor::presetFolder(), "*.ca2020");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.existsAsFile()) { proc.loadPreset (f); lastUserVersion = -1; }
                          });
}

//==============================================================================
void CA2020Editor::timerCallback()
{
    const int mode = intParam ("mode"), output = intParam ("output"), prog = intParam ("progression");
    for (int i = 0; i < 4; ++i) modeBtns[i].setToggleState (i == mode, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) outBtns[i].setToggleState (i == output, juce::dontSendNotification);

    const bool learning = proc.engine.learning.load() || proc.engine.learnRequest.load();
    flash = (flash + 1) % 30;
    learnBtn.setToggleState (learning && flash < 15, juce::dontSendNotification);

    if (proc.userVersion.load() != lastUserVersion || prog != lastProg || grid.key != intParam ("key"))
    {
        lastUserVersion = proc.userVersion.load(); lastProg = prog;
        refreshGrid();
    }

    const int step = mode == ca::ModeProgression ? proc.engine.uiStep.load() : -1;
    if (step != grid.current) { grid.current = step; grid.repaint(); }

    const int st = proc.currentStyle.load();
    if (styleBox.getSelectedId() != st + 1) styleBox.setSelectedId (st + 1, juce::dontSendNotification);

    patternView.pattern = intParam ("pattern");
    patternView.dimmed = output != ca::OutRhythm;
    patternView.current = proc.engine.uiActive.load() ? proc.engine.uiTick.load() : -1;
    patternView.repaint();

    // modes that do not use a control are greyed out
    const bool prog_ = mode == ca::ModeProgression;
    for (juce::Component* c : { (juce::Component*) &progBox, (juce::Component*) &speedBox, (juce::Component*) &grid, (juce::Component*) &copyBtn })
        c->setAlpha (prog_ ? 1.0f : 0.45f);
    chordTypeBox.setAlpha (mode == ca::ModeChord ? 1.0f : 0.45f);
    extBox.setAlpha (mode == ca::ModeChord && intParam ("chordType") == 0 ? 1.0f : 0.45f);
    arpModeBox.setAlpha (output == ca::OutArp ? 1.0f : 0.45f);
    arpRateBox.setAlpha (output == ca::OutArp ? 1.0f : 0.45f);
    arpOctK.setAlpha (output == ca::OutArp ? 1.0f : 0.45f);
    patternBox.setAlpha (output == ca::OutRhythm ? 1.0f : 0.45f);

    keyboard.setOutputNotes (proc.engine.uiNotesLo.load(), proc.engine.uiNotesHi.load());
    repaint (lcdArea);
    repaint (memArea);
}

//==============================================================================
void CA2020Editor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    // brushed top strip
    auto top = getLocalBounds().removeFromTop (64).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2d32), 0, 0, juce::Colour (0xff1c1d20), 0, top.getBottom(), false));
    g.fillRect (top);
    g.setColour (colours::orange); g.fillRect (0.0f, top.getBottom() - 2.0f, top.getWidth(), 2.0f);

    // logo
    {
        auto logo = logoArea;
        g.setColour (colours::text);
        g.setFont (font (30.0f, true));
        g.drawText ("CA-2020", logo.removeFromTop (34), juce::Justification::bottomLeft);
        g.setColour (colours::orange);
        g.setFont (font (11.5f, true));
        g.drawText ("CHORD  ARPEGGIATOR", logo, juce::Justification::topLeft);
    }

    // LCD
    drawLcd (g, lcdArea.toFloat());
    {
        auto r = lcdArea.reduced (12, 4);
        const int rootPc = proc.engine.uiRootPc.load(), q = proc.engine.uiQuality.load();
        juce::String chord = "--";
        if (rootPc >= 0)
            chord = q >= 0 ? juce::String (ca::chordName (rootPc, (ca::Quality) q)) : q == -2 ? juce::String (ca::noteName (rootPc)) + " mem" : juce::String (ca::noteName (rootPc));

        const int mode = intParam ("mode");
        juce::String roman, stepTxt;
        const int step = proc.engine.uiStep.load();
        if (mode == ca::ModeProgression && step >= 0 && step < grid.prog.numSteps && proc.engine.uiActive.load())
        {
            roman = juce::String (ca::formatRoman (grid.prog.steps[step].semis, (ca::Quality) grid.prog.steps[step].quality));
            stepTxt = "STEP " + juce::String (step + 1) + "/" + juce::String (grid.prog.numSteps);
        }

        auto left = r.removeFromLeft (r.getWidth() / 2);
        g.setColour (colours::lcdText);
        g.setFont (font (30.0f, true));
        g.drawFittedText (proc.engine.uiActive.load() ? chord : juce::String ("--"), left, juce::Justification::centredLeft, 1, 0.6f);

        g.setFont (font (15.0f, true));
        g.drawText (roman, r.removeFromTop (r.getHeight() / 2), juce::Justification::centredRight);
        g.setFont (font (11.0f));
        g.setColour (colours::lcdText.withAlpha (0.75f));
        const juce::String clock = juce::String (proc.lastBpm.load(), 1) + " BPM  " + (proc.hostPlaying.load() ? "HOST" : "INT");
        g.drawText (stepTxt + (stepTxt.isEmpty() ? "" : "   ") + clock, r, juce::Justification::centredRight);
    }

    // sections
    drawSection (g, secMode, "Mode", colours::orange);
    drawSection (g, secKey, "Key & Chord", colours::yellow);
    drawSection (g, secVoice, "Voicing", colours::blue);
    drawSection (g, secFeel, "Feel", colours::teal);
    drawSection (g, secProg, "Progression", colours::cream);
    drawSection (g, secRhythm, "Rhythm & Arpeggio", colours::red);

    g.setFont (font (10.5f, true));
    for (auto& c : captions)
    {
        g.setColour (colours::textDim);
        g.drawText (c.second, c.first, juce::Justification::centredLeft);
    }

    // memory read-out
    {
        auto mem = proc.engine.getMemory();
        juce::String txt = "MEMORY: ";
        if (mem.empty()) txt << "empty - press LEARN";
        else for (int v : mem) txt << ca::noteName (v % 12) << " ";
        g.setColour (proc.engine.learning.load() ? colours::led : colours::textDim);
        g.drawText (proc.engine.learning.load() ? juce::String ("LEARNING: play a chord and release...") : txt, memArea, juce::Justification::centredLeft);
    }

    // progression name / hint
    {
        const int nf = (int) ca::styles().size(), prog = intParam ("progression");
        g.setColour (colours::textDim);
        g.setFont (font (11.0f));
        auto hint = secProg.withTrimmedTop (22).withHeight (26).withTrimmedLeft (760).withTrimmedRight (10);
        g.drawText (prog >= nf ? "\"" + juce::String (grid.prog.name) + "\" - click a step to edit"
                               : "Factory - click a step to copy & edit", hint, juce::Justification::centredRight);
    }
}

void CA2020Editor::resized()
{
    captions.clear();
    auto cap = [this] (int x, int y, int w, const juce::String& t) { captions.push_back ({ { x, y, w, 14 }, t }); };

    // header
    logoArea = { 18, 6, 230, 56 };
    lcdArea  = { 250, 8, 390, 48 };
    styleBox.setBounds (652, 10, 236, 24);
    prevBtn.setBounds (892, 10, 28, 24);
    nextBtn.setBounds (922, 10, 28, 24);
    saveBtn.setBounds (956, 10, 44, 24);
    loadBtn.setBounds (1004, 10, 44, 24);
    previewT.setBounds (652, 37, 130, 22);
    patchBox.setBounds (784, 37, 166, 22);

    // row A
    const int ay = 72, ah = 172;
    secMode  = { 12, ay, 250, ah };
    secKey   = { 270, ay, 250, ah };
    secVoice = { 528, ay, 270, ah };
    secFeel  = { 806, ay, 242, ah };

    {
        const int x = secMode.getX(), y = ay;
        for (int i = 0; i < 4; ++i) modeBtns[i].setBounds (x + 8 + (i % 2) * 121, y + 26 + (i / 2) * 32, 113, 26);
        cap (x + 8, y + 92, 110, "TRIGGER");
        triggerBox.setBounds (x + 8, y + 106, 113, 24);
        learnBtn.setBounds (x + 129, y + 106, 113, 24);
        latchT.setBounds (x + 8, y + 140, 110, 22);
        followT.setBounds (x + 129, y + 140, 118, 22);
    }
    {
        const int x = secKey.getX(), y = ay;
        cap (x + 8, y + 24, 110, "KEY");          keyBox.setBounds (x + 8, y + 38, 113, 24);
        cap (x + 129, y + 24, 110, "SCALE");      scaleBox.setBounds (x + 129, y + 38, 113, 24);
        cap (x + 8, y + 70, 110, "CHORD TYPE");   chordTypeBox.setBounds (x + 8, y + 84, 113, 24);
        cap (x + 129, y + 70, 110, "EXTENSION");  extBox.setBounds (x + 129, y + 84, 113, 24);
        memArea = { x + 8, y + 120, 234, 40 };
    }
    {
        const int x = secVoice.getX(), y = ay;
        cap (x + 14, y + 22, 70, "INVERSION"); invK.setBounds (x + 10, y + 34, 70, 66);
        cap (x + 112, y + 22, 70, "OCTAVE");   octK.setBounds (x + 100, y + 34, 70, 66);
        cap (x + 204, y + 22, 70, "STRUM");    strumK.setBounds (x + 190, y + 34, 70, 66);
        cap (x + 10, y + 102, 120, "SPREAD");  spreadBox.setBounds (x + 10, y + 116, 120, 22);
        cap (x + 140, y + 102, 120, "BASS");   bassBox.setBounds (x + 140, y + 116, 120, 22);
        leadT.setBounds (x + 10, y + 144, 160, 22);
    }
    {
        const int x = secFeel.getX(), y = ay;
        const char* names[] = { "VELOCITY", "HUMANIZE", "SWING", "TEMPO" };
        juce::Slider* ks[] = { &velK, &humK, &swingK, &tempoK };
        for (int i = 0; i < 4; ++i) { cap (x + 6 + i * 58, y + 22, 58, names[i]); ks[i]->setBounds (x + 4 + i * 58, y + 34, 58, 66); }
        cap (x + 8, y + 102, 120, "VELOCITY MODE");
        velModeBox.setBounds (x + 8, y + 116, 130, 22);
    }

    // row B - progression
    secProg = { 12, 252, 1036, 182 };
    {
        const int x = secProg.getX(), y = secProg.getY();
        progBox.setBounds (x + 8, y + 26, 300, 24);
        speedBox.setBounds (x + 316, y + 26, 96, 24);
        addStepBtn.setBounds (x + 420, y + 26, 64, 24);
        delStepBtn.setBounds (x + 488, y + 26, 64, 24);
        copyBtn.setBounds (x + 560, y + 26, 112, 24);
        renameBtn.setBounds (x + 676, y + 26, 72, 24);
        grid.setBounds (x + 6, y + 58, 1024, 116);
    }

    // row C - rhythm / arp
    secRhythm = { 12, 442, 1036, 140 };
    {
        const int x = secRhythm.getX(), y = secRhythm.getY();
        cap (x + 8, y + 22, 200, "OUTPUT");
        for (int i = 0; i < 3; ++i) outBtns[i].setBounds (x + 8 + i * 92, y + 36, 86, 28);
        cap (x + 8, y + 74, 200, "RHYTHM PATTERN");
        patternBox.setBounds (x + 8, y + 88, 270, 24);
        patternView.setBounds (x + 290, y + 40, 380, 70);
        cap (x + 690, y + 22, 120, "ARP MODE");  arpModeBox.setBounds (x + 690, y + 36, 120, 24);
        cap (x + 690, y + 70, 120, "ARP RATE");  arpRateBox.setBounds (x + 690, y + 84, 120, 24);
        cap (x + 832, y + 22, 70, "OCTAVES");    arpOctK.setBounds (x + 826, y + 34, 70, 66);
        cap (x + 916, y + 22, 70, "GATE");       gateK.setBounds (x + 906, y + 34, 70, 66);
    }

    keyboard.setBounds (12, 590, 1036, 92);
}
