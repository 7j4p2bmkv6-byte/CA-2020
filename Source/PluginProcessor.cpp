#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
juce::StringArray CA2020Processor::progressionNames()
{
    juce::StringArray n;
    for (auto& s : ca::styles()) n.add (juce::String (s.genre) + " - " + s.name);
    for (int i = 0; i < ca::kNumUserProgs; ++i) n.add ("User " + juce::String (i + 1));
    return n;
}

APVTS::ParameterLayout CA2020Processor::createLayout (bool standalone)
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto choice = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    auto integer = [&] (const char* id, const char* name, int lo, int hi, int def)
    { p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def)); };
    auto flt = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit = "")
    { p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
                                                                 juce::AudioParameterFloatAttributes().withLabel (unit))); };
    auto boolean = [&] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def)); };

    juce::StringArray keys, scales, chordTypes, patterns, arpModes, arpRates, patches;
    for (int i = 0; i < 12; ++i) keys.add (ca::noteName (i));
    for (int i = 0; i < (int) ca::Scale::Count; ++i) scales.add (ca::scaleName (i));
    chordTypes.add ("Auto (in key)");
    for (int i = 0; i < (int) ca::Quality::Count; ++i) chordTypes.add (ca::info ((ca::Quality) i).name);
    for (auto& pt : ca::patterns()) patterns.add (pt.name);
    for (int i = 0; i < 6; ++i) { arpModes.add (ca::arpModeName (i)); arpRates.add (ca::arpRateName (i)); }
    for (int i = 0; i < 5; ++i) patches.add (ca::previewPatchName (i));

    choice ("mode", "Mode", { "Chord", "Progression", "Memory", "Thru" }, 1);
    choice ("trigger", "Trigger", { "Hold", "Step", "Host sync" }, 0);
    boolean ("latch", "Latch", false);
    boolean ("keyFollow", "Key follow", true);
    choice ("key", "Key", keys, 0);
    choice ("scale", "Scale", scales, 0);
    choice ("chordType", "Chord type", chordTypes, 0);
    choice ("extension", "Extension", { "Triad", "7th", "9th" }, 0);
    choice ("progression", "Progression", progressionNames(), 0);
    choice ("progSpeed", "Progression speed", { "Half", "Normal", "Double" }, 1);
    choice ("output", "Output", { "Sustain", "Rhythm", "Arp" }, 1);
    choice ("pattern", "Rhythm pattern", patterns, 4);
    choice ("arpMode", "Arp mode", arpModes, 0);
    choice ("arpRate", "Arp rate", arpRates, 3);
    integer ("arpOctaves", "Arp octaves", 1, 4, 1);
    flt ("gate", "Gate", 0.05f, 1.0f, 0.8f);
    flt ("swing", "Swing", 0.0f, 1.0f, 0.0f);
    integer ("inversion", "Inversion", 0, 3, 0);
    choice ("spread", "Spread", { "Close", "Open", "Wide" }, 0);
    integer ("octave", "Octave", -2, 2, 0);
    choice ("bass", "Bass note", { "Off", "Root -1 oct", "Root -2 oct" }, 1);
    boolean ("voiceLead", "Voice leading", true);
    flt ("strum", "Strum", 0.0f, 100.0f, 0.0f, "ms");
    choice ("velMode", "Velocity mode", { "As played", "Fixed" }, 0);
    integer ("velocity", "Velocity", 1, 127, 100);
    flt ("humanize", "Humanize", 0.0f, 1.0f, 0.0f);
    flt ("tempo", "Internal tempo", 40.0f, 240.0f, 120.0f, "BPM");
    boolean ("preview", "Preview synth", standalone);
    choice ("previewPatch", "Preview sound", patches, 0);
    flt ("previewLevel", "Preview level", 0.0f, 1.0f, 0.7f);
    return { p.begin(), p.end() };
}

//==============================================================================
CA2020Processor::CA2020Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CA2020", createLayout (wrapperType == wrapperType_Standalone))
{
    auto bind = [this] (P& p, const char* id) { p.v = apvts.getRawParameterValue (id); jassert (p.v != nullptr); };
    bind (pMode, "mode"); bind (pTrigger, "trigger"); bind (pLatch, "latch"); bind (pFollow, "keyFollow");
    bind (pKey, "key"); bind (pScale, "scale"); bind (pChordType, "chordType"); bind (pExt, "extension");
    bind (pProg, "progression"); bind (pSpeed, "progSpeed"); bind (pOutput, "output"); bind (pPattern, "pattern");
    bind (pArpMode, "arpMode"); bind (pArpRate, "arpRate"); bind (pArpOct, "arpOctaves"); bind (pGate, "gate");
    bind (pSwing, "swing"); bind (pInv, "inversion"); bind (pSpread, "spread"); bind (pOct, "octave");
    bind (pBass, "bass"); bind (pLead, "voiceLead"); bind (pStrum, "strum"); bind (pVelMode, "velMode");
    bind (pVel, "velocity"); bind (pHuman, "humanize"); bind (pTempo, "tempo"); bind (pPreview, "preview");
    bind (pPatch, "previewPatch"); bind (pLevel, "previewLevel");

    // Starter user slots so the editor has something to show
    userProgs[0] = ca::makeProgression ("My Progression", "I V vi IV");
    userProgs[1] = ca::makeProgression ("Minor Groove", "i7 bVImaj7 bIIImaj7 bVII");
    for (int i = 2; i < ca::kNumUserProgs; ++i)
        userProgs[i] = ca::makeProgression (("User " + std::to_string (i + 1)).c_str(), "I IV V I");

    inEvents.reserve (512); outEvents.reserve (2048);
}

int CA2020Processor::getNumPrograms() { return (int) ca::styles().size(); }

const juce::String CA2020Processor::getProgramName (int index)
{
    const auto& s = ca::styles();
    return juce::isPositiveAndBelow (index, (int) s.size()) ? juce::String (s[(size_t) index].genre) + " - " + s[(size_t) index].name : juce::String();
}

bool CA2020Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
}

void CA2020Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    synth.prepare (sampleRate);
    scratchRight.assign ((size_t) juce::jmax (4096, samplesPerBlock * 2), 0.0f);
    lastPatch = -1;
    keyboardState.reset();
}

//==============================================================================
void CA2020Processor::setParam (const juce::String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void CA2020Processor::applyStyle (int index)
{
    const auto& all = ca::styles();
    if (! juce::isPositiveAndBelow (index, (int) all.size())) return;
    const auto& st = all[(size_t) index];
    currentStyle = index;
    setParam ("mode", ca::ModeProgression);
    setParam ("progression", (float) index);
    setParam ("key", (float) st.key);
    setParam ("scale", (float) st.scale);
    setParam ("output", (float) st.output);
    setParam ("pattern", (float) st.pattern);
    setParam ("arpMode", (float) st.arpMode);
    setParam ("arpRate", (float) st.arpRate);
    setParam ("arpOctaves", (float) st.arpOct);
    setParam ("gate", st.gate);
    setParam ("swing", st.swing);
    setParam ("octave", (float) st.octave);
    setParam ("spread", (float) st.spread);
    setParam ("bass", (float) st.bass);
    setParam ("strum", st.strumMs);
    setParam ("tempo", (float) st.bpm);
    setParam ("progSpeed", 1.0f);
}

ca::Progression CA2020Processor::getUserProgression (int slot) const
{
    std::lock_guard<std::mutex> lk (userMutex);
    return userProgs[juce::jlimit (0, ca::kNumUserProgs - 1, slot)];
}

void CA2020Processor::setUserProgression (int slot, const ca::Progression& p)
{
    {
        std::lock_guard<std::mutex> lk (userMutex);
        userProgs[juce::jlimit (0, ca::kNumUserProgs - 1, slot)] = p;
    }
    ++userVersion;
}

ca::Settings CA2020Processor::readSettings() const
{
    ca::Settings s;
    auto i = [] (const P& p) { return (int) std::lround (p.get()); };
    s.mode = i (pMode); s.trigger = i (pTrigger); s.latch = pLatch.get() > 0.5f; s.keyFollow = pFollow.get() > 0.5f;
    s.key = i (pKey); s.scale = i (pScale); s.chordType = i (pChordType); s.extension = i (pExt);
    s.progression = i (pProg); s.progSpeed = i (pSpeed); s.output = i (pOutput); s.pattern = i (pPattern);
    s.arpMode = i (pArpMode); s.arpRate = i (pArpRate); s.arpOctaves = i (pArpOct);
    s.gate = pGate.get(); s.swing = pSwing.get();
    s.inversion = i (pInv); s.spread = i (pSpread); s.octave = i (pOct); s.bass = i (pBass);
    s.voiceLead = pLead.get() > 0.5f; s.strumMs = pStrum.get();
    s.velMode = i (pVelMode); s.velocity = i (pVel); s.humanize = pHuman.get();
    s.internalBpm = pTempo.get();
    return s;
}

//==============================================================================
void CA2020Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // on-screen / computer keyboard notes join the incoming MIDI
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    inEvents.clear();
    for (const auto meta : midi)
    {
        const auto* d = meta.data;
        if (meta.numBytes < 1 || meta.numBytes > 3 || d[0] >= 0xF0) continue;   // no sysex / realtime
        inEvents.push_back ({ juce::jlimit (0, n - 1, meta.samplePosition), d[0],
                              (uint8_t) (meta.numBytes > 1 ? d[1] : 0), (uint8_t) (meta.numBytes > 2 ? d[2] : 0) });
    }

    // user progression edits -> engine (never block the audio thread)
    if (engineUserVersion != userVersion.load())
    {
        std::unique_lock<std::mutex> lk (userMutex, std::try_to_lock);
        if (lk.owns_lock())
        {
            for (int i = 0; i < ca::kNumUserProgs; ++i) engine.userProgs[i] = userProgs[i];
            engineUserVersion = userVersion.load();
        }
    }

    ca::HostInfo host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())          { host.hasBpm = true; host.bpm = *bpm; }
            if (auto ppq = pos->getPpqPosition())  { host.hasPpq = true; host.ppq = *ppq; }
            host.playing = pos->getIsPlaying();
        }
    lastBpm = host.hasBpm ? host.bpm : (double) pTempo.get();
    hostPlaying = host.playing;

    engine.setSettings (readSettings());
    outEvents.clear();
    engine.process (n, host, inEvents.data(), (int) inEvents.size(), outEvents);

    midi.clear();
    for (auto& e : outEvents)
    {
        const uint8_t bytes[3] = { e.b0, e.b1, e.b2 };
        const int len = juce::jlimit (1, 3, juce::MidiMessage::getMessageLengthFromFirstByte (e.b0));
        midi.addEvent (bytes, len, juce::jlimit (0, n - 1, e.offset));
    }

    // built-in preview synth (standalone, or when you want to audition without an instrument)
    const bool preview = pPreview.get() > 0.5f;
    if (! preview)
    {
        if (lastPreview) synth.allOff();
        lastPreview = false;
        return;
    }
    lastPreview = true;

    const int patch = (int) std::lround (pPatch.get());
    lastPatch = patch;
    auto prm = ca::previewPatch (patch);
    prm.level = pLevel.get();
    synth.setParams (prm);

    if (buffer.getNumChannels() == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R == nullptr)
    {
        if ((int) scratchRight.size() < n) scratchRight.resize ((size_t) n);
        std::fill (scratchRight.begin(), scratchRight.begin() + n, 0.0f);
        R = scratchRight.data();
    }

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { synth.render (L + pos, R + pos, at - pos); pos = at; }
        const auto m = meta.getMessage();
        if (m.isNoteOn())       synth.noteOn (m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff()) synth.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allOff();
    }
    if (pos < n) synth.render (L + pos, R + pos, n - pos);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

void CA2020Processor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    outEvents.clear();
    engine.panic (outEvents);          // never leave the instrument with hanging notes
    for (auto& e : outEvents)
    {
        const uint8_t bytes[3] = { e.b0, e.b1, e.b2 };
        midi.addEvent (bytes, 3, 0);
    }
    synth.allOff();
}

//==============================================================================
juce::ValueTree CA2020Processor::buildFullState()
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("USERPROGS"), nullptr);
    state.removeChild (state.getChildWithName ("MEMORY"), nullptr);

    juce::ValueTree progs ("USERPROGS");
    for (int i = 0; i < ca::kNumUserProgs; ++i)
    {
        const auto p = getUserProgression (i);
        juce::StringArray steps;
        for (int k = 0; k < p.numSteps; ++k)
            steps.add (juce::String (p.steps[k].semis) + "," + juce::String (p.steps[k].quality) + "," + juce::String (p.steps[k].beats));
        progs.appendChild (juce::ValueTree ("PROG", { { "name", juce::String (p.name) }, { "steps", steps.joinIntoString (";") } }), nullptr);
    }
    state.appendChild (progs, nullptr);

    juce::StringArray mem;
    for (int v : engine.getMemory()) mem.add (juce::String (v));
    state.appendChild (juce::ValueTree ("MEMORY", { { "intervals", mem.joinIntoString (",") } }), nullptr);
    state.setProperty ("style", currentStyle.load(), nullptr);
    return state;
}

void CA2020Processor::restoreFullState (const juce::ValueTree& tree)
{
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType())) return;
    apvts.replaceState (tree.createCopy());
    currentStyle = (int) tree.getProperty ("style", -1);

    auto progs = tree.getChildWithName ("USERPROGS");
    for (int i = 0; i < juce::jmin (ca::kNumUserProgs, progs.getNumChildren()); ++i)
    {
        auto c = progs.getChild (i);
        ca::Progression p;
        auto name = c.getProperty ("name").toString();
        name.copyToUTF8 (p.name, sizeof (p.name) - 1);
        auto steps = juce::StringArray::fromTokens (c.getProperty ("steps").toString(), ";", "");
        for (auto& s : steps)
        {
            auto f = juce::StringArray::fromTokens (s, ",", "");
            if (f.size() < 3 || p.numSteps >= ca::kMaxSteps) continue;
            p.steps[p.numSteps++] = { juce::jlimit (0, 11, f[0].getIntValue()),
                                      juce::jlimit (0, (int) ca::Quality::Count - 1, f[1].getIntValue()),
                                      juce::jlimit (0.25f, 32.0f, f[2].getFloatValue()) };
        }
        setUserProgression (i, p);
    }

    std::vector<int> mem;
    for (auto& s : juce::StringArray::fromTokens (tree.getChildWithName ("MEMORY").getProperty ("intervals").toString(), ",", ""))
        if (s.isNotEmpty()) mem.push_back (s.getIntValue());
    engine.setMemory (mem);
}

void CA2020Processor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = buildFullState().createXml()) copyXmlToBinary (*xml, dest);
}

void CA2020Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size)) restoreFullState (juce::ValueTree::fromXml (*xml));
}

juce::File CA2020Processor::presetFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("CA-2020 Presets");
    f.createDirectory();
    return f;
}

bool CA2020Processor::savePreset (const juce::File& file)
{
    if (auto xml = buildFullState().createXml()) return xml->writeTo (file);
    return false;
}

bool CA2020Processor::loadPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file)) { restoreFullState (juce::ValueTree::fromXml (*xml)); return true; }
    return false;
}

juce::AudioProcessorEditor* CA2020Processor::createEditor() { return new CA2020Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()   { return new CA2020Processor(); }
