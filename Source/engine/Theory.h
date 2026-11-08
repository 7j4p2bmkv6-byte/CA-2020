// CA-2020 Chord Arpeggiator - music theory helpers (pure C++, no JUCE)
#pragma once
#include <array>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

namespace ca
{
//==============================================================================
// Chord qualities
enum class Quality : int
{
    Maj = 0, Min, Dom7, Maj7, Min7, Dim, HalfDim, Dim7, Aug, Sus2, Sus4, Dom7Sus4,
    Power, Add9, MinAdd9, Maj6, Min6, Dom9, Maj9, Min9, Min11, MinMaj7,
    Count
};

struct QualityInfo
{
    const char* name;      // shown in menus
    const char* suffix;    // chord-symbol suffix (Am7, Fmaj7 ...)
    const char* roman;     // roman-numeral suffix
    bool minor;            // lower-case roman numeral
    int numNotes;
    int iv[6];
};

inline const QualityInfo& info (Quality q)
{
    static const QualityInfo table[(int) Quality::Count] = {
        { "Major",        "",        "",      false, 3, { 0, 4, 7 } },
        { "Minor",        "m",       "",      true,  3, { 0, 3, 7 } },
        { "Dominant 7",   "7",       "7",     false, 4, { 0, 4, 7, 10 } },
        { "Major 7",      "maj7",    "maj7",  false, 4, { 0, 4, 7, 11 } },
        { "Minor 7",      "m7",      "7",     true,  4, { 0, 3, 7, 10 } },
        { "Diminished",   "dim",     "dim",   true,  3, { 0, 3, 6 } },
        { "Half-dim 7",   "m7b5",    "m7b5",  true,  4, { 0, 3, 6, 10 } },
        { "Diminished 7", "dim7",    "dim7",  true,  4, { 0, 3, 6, 9 } },
        { "Augmented",    "aug",     "+",     false, 3, { 0, 4, 8 } },
        { "Sus2",         "sus2",    "sus2",  false, 3, { 0, 2, 7 } },
        { "Sus4",         "sus4",    "sus4",  false, 3, { 0, 5, 7 } },
        { "7sus4",        "7sus4",   "7sus4", false, 4, { 0, 5, 7, 10 } },
        { "Power (5)",    "5",       "5",     false, 3, { 0, 7, 12 } },
        { "Add9",         "add9",    "add9",  false, 4, { 0, 4, 7, 14 } },
        { "Minor add9",   "m(add9)", "add9",  true,  4, { 0, 3, 7, 14 } },
        { "Major 6",      "6",       "6",     false, 4, { 0, 4, 7, 9 } },
        { "Minor 6",      "m6",      "6",     true,  4, { 0, 3, 7, 9 } },
        { "Dominant 9",   "9",       "9",     false, 5, { 0, 4, 7, 10, 14 } },
        { "Major 9",      "maj9",    "maj9",  false, 5, { 0, 4, 7, 11, 14 } },
        { "Minor 9",      "m9",      "9",     true,  5, { 0, 3, 7, 10, 14 } },
        { "Minor 11",     "m11",     "11",    true,  6, { 0, 3, 7, 10, 14, 17 } },
        { "Minor maj7",   "m(maj7)", "(maj7)",true,  4, { 0, 3, 7, 11 } },
    };
    int i = std::clamp ((int) q, 0, (int) Quality::Count - 1);
    return table[i];
}

inline const char* noteName (int pc)
{
    static const char* n[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return n[((pc % 12) + 12) % 12];
}

inline std::string chordName (int rootPc, Quality q)
{
    return std::string (noteName (rootPc)) + info (q).suffix;
}

//==============================================================================
// Scales (used by one-finger "Auto" chords)
enum class Scale : int { Major = 0, Minor, Dorian, Mixolydian, HarmonicMinor, Phrygian, Lydian, Count };

inline const char* scaleName (int s)
{
    static const char* n[] = { "Major", "Minor", "Dorian", "Mixolydian", "Harmonic minor", "Phrygian", "Lydian" };
    return n[std::clamp (s, 0, (int) Scale::Count - 1)];
}

inline const std::array<int, 7>& scaleSteps (Scale s)
{
    static const std::array<int, 7> t[] = {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 },
        { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 6, 7, 9, 11 } };
    return t[std::clamp ((int) s, 0, (int) Scale::Count - 1)];
}

/** Snaps a MIDI note down to the nearest note of the scale and returns its degree (0-6). */
inline int snapToScale (int note, int key, Scale s, int& degreeOut)
{
    const auto& st = scaleSteps (s);
    for (int down = 0; down < 12; ++down)
    {
        int n = note - down;
        int rel = (((n - key) % 12) + 12) % 12;
        for (int d = 0; d < 7; ++d)
            if (st[(size_t) d] == rel) { degreeOut = d; return n; }
    }
    degreeOut = 0;
    return note;
}

/** Stacks thirds on a scale degree. extension: 0 triad, 1 seventh, 2 ninth. */
inline std::vector<int> diatonicIntervals (int degree, Scale s, int extension)
{
    const auto& st = scaleSteps (s);
    auto tone = [&] (int idx) { return st[(size_t) (idx % 7)] + 12 * (idx / 7); };
    const int root = tone (degree);
    std::vector<int> iv;
    const int count = 3 + std::clamp (extension, 0, 2);
    for (int i = 0; i < count; ++i)
        iv.push_back (tone (degree + 2 * i) - root);
    return iv;
}

/** Names the diatonic chord built by diatonicIntervals (for display). */
inline Quality qualityFromIntervals (const std::vector<int>& iv)
{
    for (int q = 0; q < (int) Quality::Count; ++q)
    {
        const auto& qi = info ((Quality) q);
        if (qi.numNotes != (int) iv.size()) continue;
        bool same = true;
        for (int i = 0; i < qi.numNotes; ++i) if (qi.iv[i] != iv[(size_t) i]) { same = false; break; }
        if (same) return (Quality) q;
    }
    return Quality::Maj;
}

//==============================================================================
// Roman numerals: "bVImaj7", "ii7", "V7sus4", "i9" (= m9), "viidim", "bVII5" ...
struct ChordRef { int semis = 0; Quality quality = Quality::Maj; };

inline bool parseRoman (const std::string& text, ChordRef& out)
{
    size_t p = 0;
    int acc = 0;
    while (p < text.size() && (text[p] == 'b' || text[p] == '#'))
        acc += (text[p++] == 'b' ? -1 : 1);

    size_t start = p;
    while (p < text.size() && (text[p] == 'I' || text[p] == 'V' || text[p] == 'i' || text[p] == 'v')) ++p;
    std::string numeral = text.substr (start, p - start);
    if (numeral.empty()) return false;

    const bool lower = std::islower ((unsigned char) numeral[0]) != 0;
    std::string up;
    for (char c : numeral) up += (char) std::toupper ((unsigned char) c);

    static const char* romans[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const int degSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };
    int deg = -1;
    for (int i = 0; i < 7; ++i) if (up == romans[i]) deg = i;
    if (deg < 0) return false;

    const std::string sfx = text.substr (p);
    Quality q = lower ? Quality::Min : Quality::Maj;
    if      (sfx.empty())                         q = lower ? Quality::Min : Quality::Maj;
    else if (sfx == "7")                          q = lower ? Quality::Min7 : Quality::Dom7;
    else if (sfx == "maj7")                       q = Quality::Maj7;
    else if (sfx == "(maj7)")                     q = Quality::MinMaj7;
    else if (sfx == "9")                          q = lower ? Quality::Min9 : Quality::Dom9;
    else if (sfx == "maj9")                       q = Quality::Maj9;
    else if (sfx == "11")                         q = Quality::Min11;
    else if (sfx == "add9")                       q = lower ? Quality::MinAdd9 : Quality::Add9;
    else if (sfx == "6")                          q = lower ? Quality::Min6 : Quality::Maj6;
    else if (sfx == "sus2")                       q = Quality::Sus2;
    else if (sfx == "sus4")                       q = Quality::Sus4;
    else if (sfx == "7sus4")                      q = Quality::Dom7Sus4;
    else if (sfx == "5")                          q = Quality::Power;
    else if (sfx == "dim" || sfx == "o")          q = Quality::Dim;
    else if (sfx == "dim7" || sfx == "o7")        q = Quality::Dim7;
    else if (sfx == "m7b5")                       q = Quality::HalfDim;
    else if (sfx == "+" || sfx == "aug")          q = Quality::Aug;
    else return false;

    out.semis = (((degSemis[deg] + acc) % 12) + 12) % 12;
    out.quality = q;
    return true;
}

inline std::string formatRoman (int semis, Quality q)
{
    static const char* base[12] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
    std::string s = base[((semis % 12) + 12) % 12];
    if (info (q).minor)
        for (auto& c : s) if (c == 'I' || c == 'V') c = (char) std::tolower ((unsigned char) c);
    return s + info (q).roman;
}

} // namespace ca
