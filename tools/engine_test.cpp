// Off-line tests + demo renderer for the CA-2020 engine (g++ -O2 -std=c++17)
#include "../Source/engine/ChordArpEngine.h"
#include "../Source/engine/PreviewSynth.h"
#include <cstdio>
#include <cassert>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
using namespace ca;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf ("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

struct Sim
{
    Engine e; double sr = 48000; int block = 256;
    std::vector<MidiEv> in; std::vector<MidiEv> out;
    int sounding[128] {}; int maxPoly = 0; int ons = 0;
    std::vector<std::pair<long,std::vector<int>>> onsets; long t = 0;
    bool hostPlay = false; double hostPpq = 0; double bpm = 120; bool hasHost = false;
    PreviewSynth* synth = nullptr; std::vector<float>* L = nullptr; std::vector<float>* R = nullptr;

    Sim (const Settings& s) { e.prepare (sr); e.setSettings (s); }
    void run (int samples, std::vector<MidiEv> events = {})
    {
        int done = 0;
        while (done < samples)
        {
            int n = std::min (block, samples - done);
            in.clear();
            for (auto& ev : events) if (ev.offset >= done && ev.offset < done + n) in.push_back ({ ev.offset - done, ev.b0, ev.b1, ev.b2 });
            out.clear();
            HostInfo h; h.hasBpm = hasHost; h.bpm = bpm; h.hasPpq = hasHost; h.playing = hostPlay; h.ppq = hostPpq;
            e.process (n, h, in.data(), (int) in.size(), out);
            int k = 0;
            for (int i = 0; i < n; ++i)
            {
                while (k < (int) out.size() && out[(size_t)k].offset <= i)
                {
                    auto& m = out[(size_t)k++];
                    if ((m.b0 & 0xF0) == 0x90 && m.b2 > 0) { CHECK (sounding[m.b1] == 0); sounding[m.b1] = 1; ++ons;
                        if (onsets.empty() || onsets.back().first != t + i) onsets.push_back ({ t + i, {} }); onsets.back().second.push_back (m.b1);
                        if (synth) synth->noteOn (m.b1, m.b2); }
                    else if ((m.b0 & 0xF0) == 0x80) { sounding[m.b1] = 0; if (synth) synth->noteOff (m.b1); }
                }
                if (synth) synth->render (L->data() + t + i, R->data() + t + i, 1);
            }
            int poly = 0; for (int x : sounding) poly += x; maxPoly = std::max (maxPoly, poly);
            done += n; t += n;
            if (hostPlay) hostPpq += n * bpm / 60.0 / sr;
        }
    }
    int poly() const { int p = 0; for (int x : sounding) p += x; return p; }
};

static MidiEv on (int off, int n, int v = 100) { return { off, 0x90, (uint8_t) n, (uint8_t) v }; }
static MidiEv offm (int off, int n) { return { off, 0x80, (uint8_t) n, 0 }; }

int main (int argc, char** argv)
{
    // --- theory
    ChordRef c;
    CHECK (parseRoman ("bVImaj7", c) && c.semis == 8 && c.quality == Quality::Maj7);
    CHECK (parseRoman ("i9", c) && c.semis == 0 && c.quality == Quality::Min9);
    CHECK (parseRoman ("V7sus4", c) && c.semis == 7 && c.quality == Quality::Dom7Sus4);
    CHECK (parseRoman ("bVII5", c) && c.semis == 10 && c.quality == Quality::Power);
    CHECK (parseRoman ("ii7", c) && c.semis == 2 && c.quality == Quality::Min7);
    CHECK (! parseRoman ("X7", c));
    for (int q = 0; q < (int) Quality::Count; ++q)
        for (int sm = 0; sm < 12; ++sm)
        {
            ChordRef r; auto txt = formatRoman (sm, (Quality) q);
            bool ok = parseRoman (txt, r) && r.semis == sm && r.quality == (Quality) q;
            if (! ok) std::printf ("  roundtrip failed: %s\n", txt.c_str());
            CHECK (ok);
        }
    for (size_t i = 0; i < styles().size(); ++i)
    {
        std::istringstream ss (styles()[i].chords); int tokens = 0; std::string t; while (ss >> t) ++tokens;
        CHECK (styleProgression ((int) i).numSteps == tokens);
    }
    { int d; CHECK (snapToScale (61, 0, Scale::Major, d) == 60 && d == 0); }
    CHECK (qualityFromIntervals (diatonicIntervals (1, Scale::Major, 1)) == Quality::Min7);
    CHECK (qualityFromIntervals (diatonicIntervals (4, Scale::Major, 1)) == Quality::Dom7);
    CHECK (qualityFromIntervals (diatonicIntervals (6, Scale::Major, 1)) == Quality::HalfDim);
    CHECK (qualityFromIntervals (diatonicIntervals (0, Scale::Minor, 2)) == Quality::Min9);

    // --- one-finger chord, sustain: D in C major -> D minor (D F A)
    {
        Settings s; s.mode = ModeChord; s.output = OutSustain; s.key = 0; s.scale = 0; s.voiceLead = false;
        Sim sim (s);
        sim.run (48000, { on (100, 62), offm (24000, 62) });
        CHECK (! sim.onsets.empty());
        auto ch = sim.onsets.front().second; std::sort (ch.begin(), ch.end());
        CHECK ((ch == std::vector<int> { 62, 65, 69 }));
        CHECK (sim.onsets.front().first == 100);               // immediate response
        CHECK (sim.poly() == 0);
    }
    // --- progression Hold + rhythm: Pop Anthem in C, 4 bars, chords I V vi IV
    {
        Settings s; applyStyle (s, 0); s.keyFollow = false; s.voiceLead = false; s.bass = 0;
        Sim sim (s);
        const int bar = (int) (48000 * 60.0 / s.internalBpm * 4);
        sim.run (bar * 4 + 4800, { on (0, 60), offm (bar * 4, 60) });
        std::set<int> roots;
        std::vector<int> firstOfBar;
        for (int b = 0; b < 4; ++b)
            for (auto& o : sim.onsets) if (o.first >= b * bar && o.first < b * bar + 2000) { int lo = *std::min_element (o.second.begin(), o.second.end()); firstOfBar.push_back (lo % 12); break; }
        CHECK ((firstOfBar == std::vector<int> { 0, 7, 9, 5 }));
        CHECK (sim.poly() == 0);
        std::printf ("pop anthem: %d note-ons, max poly %d\n", sim.ons, sim.maxPoly);
    }
    // --- key follow: holding A plays the progression in A
    {
        Settings s; applyStyle (s, 0); s.keyFollow = true; s.bass = 0; s.output = OutSustain;
        Sim sim (s);
        sim.run (20000, { on (0, 57), offm (19000, 57) });
        auto ch = sim.onsets.front().second; std::set<int> pcs; for (int n : ch) pcs.insert (n % 12);
        CHECK ((pcs == std::set<int> { 9, 1, 4 }));          // A C# E
    }
    // --- arpeggiator up, 1/16 at 120 -> 6000 samples per step
    {
        Settings s; s.mode = ModeChord; s.output = OutArp; s.arpMode = 0; s.arpRate = 3; s.arpOctaves = 2; s.voiceLead = false;
        Sim sim (s);
        sim.run (48000, { on (0, 60), offm (47000, 60) });
        std::vector<int> seq; for (auto& o : sim.onsets) seq.push_back (o.second[0]);
        CHECK (seq.size() >= 7);
        CHECK ((std::vector<int> (seq.begin(), seq.begin() + 7) == std::vector<int> { 60, 64, 67, 72, 76, 79, 60 }));
        CHECK (sim.onsets[1].first == 6000);
        CHECK (sim.poly() == 0);
    }
    // --- step trigger: each press advances
    {
        Settings s; applyStyle (s, 0); s.trigger = TrigStep; s.output = OutSustain; s.keyFollow = false; s.bass = 0; s.voiceLead = false;
        Sim sim (s);
        sim.run (48000, { on (0, 60), offm (5000, 60), on (10000, 60), offm (15000, 60), on (20000, 60), offm (25000, 60) });
        std::vector<int> roots; for (auto& o : sim.onsets) roots.push_back (*std::min_element (o.second.begin(), o.second.end()) % 12);
        CHECK ((roots == std::vector<int> { 0, 7, 9 }));
        CHECK (sim.poly() == 0);
    }
    // --- chord memory learn
    {
        Settings s; s.mode = ModeMemory; s.output = OutSustain;
        Sim sim (s);
        sim.e.learnRequest = true;
        sim.run (30000, { on (0, 48), on (10, 55), on (20, 58), on (30, 62), offm (5000, 48), offm (5000, 55), offm (5000, 58), offm (5000, 62),
                          on (10000, 50), offm (20000, 50) });
        CHECK (sim.e.memCount == 4);
        auto ch = sim.onsets.front().second; std::sort (ch.begin(), ch.end());
        CHECK ((ch == std::vector<int> { 50, 57, 60, 64 }));
        CHECK (sim.poly() == 0);
    }
    // --- host sync: progression follows song position; stopping transport releases
    {
        Settings s; applyStyle (s, 5); s.trigger = TrigHost;
        Sim sim (s); sim.hasHost = true; sim.hostPlay = true; sim.bpm = 122;
        sim.run (48000 * 8);
        CHECK (sim.ons > 20);
        sim.hostPlay = false; sim.run (4800);
        CHECK (sim.poly() == 0);
    }
    // --- fuzz: random settings and input never leave stuck notes
    {
        std::mt19937 r (7);
        for (int iter = 0; iter < 60; ++iter)
        {
            Settings s; s.mode = r() % 4; s.trigger = r() % 3; s.latch = false; s.output = r() % 3; s.pattern = r() % 16;
            s.progression = r() % 40; s.arpMode = r() % 6; s.arpRate = r() % 6; s.arpOctaves = 1 + r() % 4;
            s.gate = 0.05f + (r() % 100) / 100.0f; s.swing = (r() % 100) / 100.0f; s.strumMs = (float) (r() % 80);
            s.spread = r() % 3; s.bass = r() % 3; s.inversion = r() % 4; s.voiceLead = r() & 1; s.humanize = (r() % 100) / 100.0f;
            s.chordType = r() % 23; s.extension = r() % 3; s.octave = (int) (r() % 5) - 2;
            Sim sim (s);
            sim.e.userProgs[0] = makeProgression ("u", "I IV:2 V:2");
            std::vector<MidiEv> ev;
            for (int k = 0; k < 30; ++k) { int n = 40 + r() % 40; int t0 = r() % 90000; ev.push_back (on (t0, n)); ev.push_back (offm (t0 + 100 + r() % 20000, n)); }
            std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.offset < b.offset; });
            sim.run (130000, ev);
            if (sim.poly() != 0) std::printf ("  stuck notes in fuzz iter %d (mode %d out %d trig %d)\n", iter, s.mode, s.output, s.trigger);
            CHECK (sim.poly() == 0);
        }
    }
    std::printf (failures ? "%d FAILURES\n" : "ALL TESTS PASSED\n", failures);
    if (failures) return 1;

    // ===================== demo render ===========================
    if (argc < 2) return 0;
    const double sr = 48000;
    struct Section { int style; int patch; int bars; int rootNote; int output; int pattern; int bpm; bool drums; int padPatch = -1; };
    std::vector<Section> secs = {
        { 5, 0, 8, 53, -1, -1, 122, true },     // Deep House Stabs (F minor) - offbeat stabs
        { 0, 4, 8, 60, -1, -1, 118, true },     // Pop Anthem - pop push rhythm
        { 17, 2, 8, 50, -1, -1, 104, true, 1 }, // Synthwave Night Drive - 1/16 arp over a sustained pad
        { 19, 1, 8, 53, -1, -1, 84, false },    // Lo-fi ii-V-I - strum + swing
    };
    std::vector<float> L, R;
    long pos = 0;
    for (auto& sc : secs)
    {
        Settings s; applyStyle (s, sc.style); s.internalBpm = sc.bpm;
        const long bar = (long) (sr * 60.0 / sc.bpm * 4);
        const long len = bar * sc.bars;
        const long tail = (long) (sr * 1.2);
        L.resize ((size_t) (pos + len + tail), 0.0f); R.resize (L.size(), 0.0f);
        std::vector<float> l2 ((size_t) (len + tail), 0.0f), r2 ((size_t) (len + tail), 0.0f);
        PreviewSynth syn; syn.prepare (sr); syn.setParams (previewPatch (sc.patch));
        Sim sim (s); sim.synth = &syn; sim.L = &l2; sim.R = &r2;
        sim.run ((int) (len + tail), { on (0, sc.rootNote, 105), offm ((int) len - 200, sc.rootNote) });
        if (sc.padPatch >= 0)
        {
            Settings ps = s; ps.output = OutSustain; ps.octave = -1; ps.bass = 1;
            PreviewSynth pad; pad.prepare (sr); auto pp = previewPatch (sc.padPatch); pp.level = 0.45f; pad.setParams (pp);
            Sim ps2 (ps); ps2.synth = &pad; ps2.L = &l2; ps2.R = &r2;
            ps2.run ((int) (len + tail), { on (0, sc.rootNote, 90), offm ((int) len - 200, sc.rootNote) });
        }
        { double e = 0; for (long i = 0; i < len; ++i) e += l2[(size_t) i] * l2[(size_t) i];
          const float gn = (float) (0.17 / std::sqrt (e / len + 1e-12)); for (auto& x : l2) x *= gn; for (auto& x : r2) x *= gn; }
        // simple drums for groove context
        if (sc.drums)
            for (long b = 0; b < sc.bars * 4; ++b)
            {
                long t0 = (long) (b * sr * 60.0 / sc.bpm);
                for (int i = 0; i < (int) (sr * 0.35) && t0 + i < len; ++i)
                {
                    double tt = i / sr; double f = 50 + 90 * std::exp (-tt * 30);
                    float k = (float) (std::sin (2 * M_PI * f * tt) * std::exp (-tt * 9) * 0.55);
                    l2[(size_t) (t0 + i)] += k; r2[(size_t) (t0 + i)] += k;
                }
                long th = t0 + (long) (sr * 60.0 / sc.bpm / 2);
                std::mt19937 nr ((unsigned) b);
                for (int i = 0; i < (int) (sr * 0.05) && th + i < len; ++i)
                {
                    float n = ((nr() % 2000) / 1000.0f - 1.0f) * std::exp (-i / (sr * 0.012)) * 0.12f;
                    l2[(size_t) (th + i)] += n * 0.8f; r2[(size_t) (th + i)] += n;
                }
            }
        for (size_t i = 0; i < l2.size(); ++i) { L[(size_t) pos + i] += l2[i]; R[(size_t) pos + i] += r2[i]; }
        std::printf ("section %-22s %ld samples, %d note-ons, max poly %d\n", styles()[(size_t) sc.style].name, len, sim.ons, sim.maxPoly);
        pos += len;
    }
    float peak = 0; for (size_t i = 0; i < L.size(); ++i) peak = std::max ({ peak, std::abs (L[i]), std::abs (R[i]) });
    const float g = 0.89f / std::max (peak, 1e-6f);
    std::ofstream f (argv[1], std::ios::binary);
    auto w32 = [&] (uint32_t v) { f.write ((char*) &v, 4); }; auto w16 = [&] (uint16_t v) { f.write ((char*) &v, 2); };
    const uint32_t frames = (uint32_t) L.size();
    f.write ("RIFF", 4); w32 (36 + frames * 4); f.write ("WAVEfmt ", 8); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (frames * 4);
    for (uint32_t i = 0; i < frames; ++i) { w16 ((uint16_t) (int16_t) std::lrint (L[i] * g * 32767)); w16 ((uint16_t) (int16_t) std::lrint (R[i] * g * 32767)); }
    std::printf ("peak %.2f, wrote %u frames (%.1f s)\n", peak, frames, frames / sr);
    return 0;
}
