// CA-2020 - factory progressions, rhythm patterns and style presets
#pragma once
#include "Theory.h"
#include <cstring>
#include <sstream>

namespace ca
{
constexpr int kMaxSteps = 16;
constexpr int kNumUserProgs = 8;

struct ProgStep { int semis = 0; int quality = 0; float beats = 4.0f; };

struct Progression
{
    char name[40] = {};
    int numSteps = 0;
    ProgStep steps[kMaxSteps];

    float totalBeats() const
    {
        float t = 0; for (int i = 0; i < numSteps; ++i) t += steps[i].beats;
        return t;
    }
};

/** Builds a progression from text such as "i7:4 bVImaj7:4 bIII:2 bVII:2" (":beats", default 4). */
inline Progression makeProgression (const char* name, const char* text)
{
    Progression p;
    std::strncpy (p.name, name, sizeof (p.name) - 1);
    std::istringstream in (text);
    std::string tok;
    while (in >> tok && p.numSteps < kMaxSteps)
    {
        float beats = 4.0f;
        auto colon = tok.find (':');
        if (colon != std::string::npos) { beats = std::stof (tok.substr (colon + 1)); tok = tok.substr (0, colon); }
        ChordRef c;
        if (! parseRoman (tok, c)) continue;
        p.steps[p.numSteps++] = { c.semis, (int) c.quality, beats };
    }
    return p;
}

//==============================================================================
// 16-step rhythm patterns: x = accented hit, o = hit, - = hold, . = rest
struct Pattern { const char* name; const char* steps; };

inline const std::vector<Pattern>& patterns()
{
    static const std::vector<Pattern> p = {
        { "Sustain (bar)",      "x---------------" },
        { "Ballad halves",      "x-------o-------" },
        { "Quarters",           "x---o---o---o---" },
        { "Eighths",            "x-o-o-o-x-o-o-o-" },
        { "Pop push",           "x-o-o--ox-o-o--o" },
        { "Rock drive",         "x-o-x-o-x-o-x-o-" },
        { "House offbeat",      "..x...x...x...x." },
        { "Piano house",        "x..x..x.x..x..x." },
        { "Deep stab",          "..x..x.x..x..x.." },
        { "Dotted 3-3-2",       "x--o--o-x--o--o-" },
        { "Charleston",         "x-----o-x-----o-" },
        { "Reggae skank",       "....x.......x..." },
        { "Trance gate",        "xoxoxoxoxoxoxoxo" },
        { "Funk 16ths",         "x..o..o.o.x..o.o" },
        { "Future chops",       "x--.o.o--.o.x-.o" },
        { "Lo-fi lazy",         "x------.....o---" },
    };
    return p;
}

inline const char* arpModeName (int i)
{
    static const char* n[] = { "Up", "Down", "Up/Down", "Down/Up", "Random", "As played" };
    return n[std::clamp (i, 0, 5)];
}

inline double arpRateBeats (int i)
{
    static const double r[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return r[std::clamp (i, 0, 5)];
}

inline const char* arpRateName (int i)
{
    static const char* n[] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    return n[std::clamp (i, 0, 5)];
}

//==============================================================================
// Style presets. Each one sets the progression plus the musical feel around it.
enum Output { OutSustain = 0, OutRhythm = 1, OutArp = 2 };

struct Style
{
    const char* genre;
    const char* name;
    const char* chords;
    int key;          // 0 = C
    int scale;        // ca::Scale
    int bpm;          // suggested tempo (used when no host tempo)
    int output;       // Output
    int pattern;      // index into patterns()
    int arpMode, arpRate, arpOct;
    float gate, swing;
    int octave;       // -2..+2
    int spread;       // 0 close, 1 open, 2 wide
    int bass;         // 0 off, 1 root -1 oct, 2 root -2 oct
    float strumMs;
};

inline const std::vector<Style>& styles()
{
    static const std::vector<Style> s = {
        // genre       name                   chords                                         key sc bpm out pat  am ar ao gate  swing oct sp bs strum
        { "Pop",       "Pop Anthem",          "I V vi IV",                                   0, 0, 118, 1,  4,  0, 3, 1, 0.80f, 0.00f, 0, 0, 1, 0 },
        { "Pop",       "Pop Ballad",          "vi IV I V",                                   7, 0,  76, 1,  1,  0, 3, 1, 0.90f, 0.00f, 0, 1, 1, 18 },
        { "Pop",       "50s Doo-Wop",         "I vi IV V",                                   5, 0, 100, 1,  9,  0, 3, 1, 0.80f, 0.00f, 0, 0, 1, 0 },
        { "Pop",       "Feel-Good Pop",       "I iii vi IV",                                 2, 0, 112, 2,  0,  2, 3, 2, 0.60f, 0.00f, 0, 0, 0, 0 },
        { "Pop",       "Royal Road",          "IVmaj7 V7 iii7 vi",                           0, 0, 104, 1,  3,  0, 3, 1, 0.80f, 0.00f, 0, 1, 1, 0 },
        { "House",     "Deep House Stabs",    "i9 iv9",                                      5, 1, 122, 1,  6,  0, 3, 1, 0.50f, 0.10f, 0, 0, 0, 0 },
        { "House",     "Classic Piano House", "vi IV I V",                                  10, 0, 124, 1,  7,  0, 3, 1, 0.70f, 0.00f, 0, 0, 0, 0 },
        { "House",     "Garage Organ",        "ii7 V7 Imaj7 vi7",                            3, 0, 128, 1,  8,  0, 3, 1, 0.50f, 0.25f, 0, 0, 0, 0 },
        { "House",     "Melodic House",       "i bVI bIII bVII",                             9, 1, 122, 2,  0,  2, 3, 2, 0.55f, 0.00f, 0, 1, 1, 0 },
        { "House",     "Nu-Disco",            "Imaj7 vi7 ii7 V7",                            7, 0, 118, 1, 13,  0, 3, 1, 0.60f, 0.15f, 0, 0, 0, 0 },
        { "Techno",    "Minor Stab Techno",   "i7 i7 bVII bVI",                              4, 1, 130, 1,  6,  0, 3, 1, 0.35f, 0.00f, 0, 0, 0, 0 },
        { "Trance",    "Epic Trance",         "i bVII bVI bVII",                             9, 1, 138, 2,  0,  0, 3, 2, 0.50f, 0.00f, 0, 0, 1, 0 },
        { "Trance",    "Uplifting Gate",      "bVI bVII i i",                                1, 1, 138, 1, 12,  0, 3, 1, 0.50f, 0.00f, 0, 1, 1, 0 },
        { "Rock",      "Classic Rock",        "I5 bVII5 IV5 I5",                             4, 0, 120, 1,  5,  0, 3, 1, 0.85f, 0.00f,-1, 0, 0, 0 },
        { "Rock",      "Stadium Rock",        "I IV vi V",                                   7, 0, 126, 1,  3,  0, 3, 1, 0.85f, 0.00f, 0, 1, 1, 8 },
        { "Rock",      "12-Bar Blues",        "I7 I7 I7 I7 IV7 IV7 I7 I7 V7 IV7 I7 V7",      9, 6, 108, 1, 10,  0, 3, 1, 0.70f, 0.45f, 0, 0, 1, 0 },
        { "Rock",      "Pop-Punk",            "I V vi IV",                                   2, 0, 168, 1,  5,  0, 3, 1, 0.90f, 0.00f, 0, 0, 1, 0 },
        { "Synthwave", "Night Drive",         "i bVImaj7 bVII i",                            2, 1, 100, 2,  0,  0, 3, 2, 0.60f, 0.00f, 0, 1, 1, 0 },
        { "Synthwave", "Outrun Sunset",       "bVI bVII i i",                                4, 1, 110, 2,  0,  2, 1, 2, 0.70f, 0.00f, 0, 1, 1, 0 },
        { "Lo-fi",     "Lo-fi ii-V-I",        "ii7 V7 Imaj7 Imaj7",                          5, 0,  80, 1, 15,  0, 3, 1, 0.90f, 0.35f, 0, 1, 1, 30 },
        { "Lo-fi",     "Neo-Soul",            "IVmaj7 iii7 vi9 V7sus4",                      3, 0,  86, 1, 15,  0, 3, 1, 0.90f, 0.30f, 0, 1, 1, 35 },
        { "Jazz",      "Jazz Turnaround",     "Imaj7:2 vi7:2 ii7:2 V7:2",                   10, 0, 120, 1,  2,  0, 3, 1, 0.75f, 0.40f, 0, 1, 1, 12 },
        { "Jazz",      "Bossa",               "ii7 V7 Imaj7 VI7",                            0, 0, 130, 1, 10,  0, 3, 1, 0.60f, 0.00f, 0, 1, 1, 0 },
        { "R&B",       "Smooth R&B",          "Imaj9 iii7 vi9 IVmaj7",                       1, 0,  92, 0,  0,  0, 3, 1, 1.00f, 0.00f, 0, 1, 1, 40 },
        { "R&B",       "Gospel Lift",         "IVmaj7 V7 iii7 vi7",                          8, 0,  84, 1,  1,  0, 3, 1, 0.90f, 0.00f, 0, 1, 1, 25 },
        { "EDM",       "Future Bass",         "IVmaj7 V vi7 iii7",                           6, 0, 150, 1, 14,  0, 3, 1, 0.70f, 0.00f, 0, 2, 1, 0 },
        { "EDM",       "Festival Anthem",     "vi IV I V",                                   5, 0, 128, 1, 12,  0, 3, 1, 0.55f, 0.00f, 0, 1, 1, 0 },
        { "Funk",      "Dorian Funk Vamp",    "i7 IV7",                                      4, 2, 104, 1, 13,  0, 3, 1, 0.45f, 0.20f, 0, 0, 0, 0 },
        { "Reggae",    "One Drop",            "I IV V IV",                                   7, 0,  76, 1, 11,  0, 3, 1, 0.40f, 0.00f, 0, 0, 0, 0 },
        { "Cinematic", "Epic Cinematic",      "i bVI iv V",                                  2, 4,  90, 0,  0,  0, 3, 1, 1.00f, 0.00f, 0, 2, 2, 0 },
        { "Cinematic", "Andalusian",          "i bVII bVI V",                                9, 4,  96, 2,  0,  2, 4, 2, 0.60f, 0.00f, 0, 0, 1, 0 },
        { "Classical", "Canon",               "I:2 V:2 vi:2 iii:2 IV:2 I:2 IV:2 V:2",        2, 0,  90, 2,  0,  0, 3, 2, 0.70f, 0.00f, 0, 0, 1, 0 },
    };
    return s;
}

inline Progression styleProgression (int i)
{
    const auto& s = styles();
    const auto& st = s[(size_t) std::clamp (i, 0, (int) s.size() - 1)];
    return makeProgression (st.name, st.chords);
}

} // namespace ca
