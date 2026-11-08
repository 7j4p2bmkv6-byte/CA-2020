// CA-2020 Chord Arpeggiator - the MIDI engine (pure C++, no JUCE, so it can be tested on its own)
#pragma once
#include "Styles.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace ca
{
enum Mode    { ModeChord = 0, ModeProgression = 1, ModeMemory = 2, ModeThru = 3 };
enum Trigger { TrigHold = 0, TrigStep = 1, TrigHost = 2 };

struct Settings
{
    int   mode = ModeProgression, trigger = TrigHold;
    bool  latch = false, keyFollow = true;
    int   key = 0, scale = 0;
    int   chordType = 0;           // 0 = Auto (diatonic), otherwise Quality index + 1
    int   extension = 0;           // 0 triad, 1 seventh, 2 ninth (Auto chords)
    int   progression = 0;         // 0..styles-1 = factory, then user slots
    int   progSpeed = 1;           // 0 half, 1 normal, 2 double
    int   output = OutRhythm, pattern = 0;
    int   arpMode = 0, arpRate = 3, arpOctaves = 1;
    float gate = 0.8f, swing = 0.0f;
    int   inversion = 0, spread = 0, octave = 0, bass = 0;
    bool  voiceLead = true;
    float strumMs = 0.0f;
    int   velMode = 0, velocity = 100;
    float humanize = 0.0f;
    double internalBpm = 120.0;
};

struct MidiEv   { int offset; uint8_t b0, b1, b2; };
struct HostInfo { bool hasPpq = false, playing = false, hasBpm = false; double ppq = 0, bpm = 120; };

class Engine
{
public:
    Engine()
    {
        pending.reserve (2048); held.reserve (32);
        for (int i = 0; i < (int) styles().size(); ++i) factory.push_back (styleProgression (i));
    }

    //==========================================================================
    void prepare (double newSampleRate)
    {
        sr = newSampleRate > 0 ? newSampleRate : 44100.0;
        pending.clear();
        for (auto& n : noteOn) n = false;
        for (auto& g : noteGen) g = 0;
        held.clear(); physical = 0; chordNotes.clear(); currentChord.clear(); prevVoicing.clear();
        wasActive = false; usingHost = false; stepIdx = -1; now = 0;
    }

    void setSettings (const Settings& newSettings) { next = newSettings; }
    Progression userProgs[kNumUserProgs];

    // chord memory (written by the audio thread when learning, read by the UI / state saving)
    std::atomic<int> memCount { 0 };
    std::atomic<int> memIv[8] {};
    std::atomic<bool> learnRequest { false }, learning { false };

    // UI feedback
    std::atomic<int>  uiTick { -1 }, uiStep { -1 }, uiRootPc { -1 }, uiQuality { 0 }, uiChordStyle { -1 };
    std::atomic<uint64_t> uiNotesLo { 0 }, uiNotesHi { 0 };
    std::atomic<bool> uiActive { false };

    void setMemory (const std::vector<int>& iv)
    {
        int n = (int) std::min<size_t> (iv.size(), 8);
        for (int i = 0; i < n; ++i) memIv[i] = iv[(size_t) i];
        memCount = n;
    }

    std::vector<int> getMemory() const
    {
        std::vector<int> v;
        for (int i = 0; i < memCount.load(); ++i) v.push_back (memIv[i].load());
        return v;
    }

    const Progression& progressionFor (int index) const
    {
        const int nf = (int) factory.size();
        if (index >= nf) return userProgs[std::clamp (index - nf, 0, kNumUserProgs - 1)];
        return factory[(size_t) std::max (0, index)];
    }

    //==========================================================================
    void process (int numSamples, const HostInfo& host, const MidiEv* in, int numIn, std::vector<MidiEv>& out)
    {
        applySettingsChange();

        if (learnRequest.exchange (false)) { learning = true; learnNotes.clear(); learnHeld = 0; }

        const double bpm = host.hasBpm && host.bpm > 1.0 ? host.bpm : s.internalBpm;
        pps = bpm / 60.0 / sr;

        const bool hostRunning = host.playing && host.hasPpq;
        if (hostRunning)
        {
            const bool jumped = ! usingHost || std::abs (host.ppq - expectedHostPpq) > 0.01;
            usingHost = true;
            ppqStart = host.ppq;
            if (jumped) resyncTicks (ppqStart);
            expectedHostPpq = host.ppq + numSamples * pps;
        }
        else
        {
            if (usingHost) { usingHost = false; releaseAll (now); wasActive = false; }
            ppqStart = internalPpq;
        }

        curPpq = ppqStart;
        updateActive (now, ppqStart);

        int ev = 0;
        for (int i = 0; i < numSamples; ++i)
        {
            const int64_t T = now + i;
            curPpq = ppqStart + i * pps;

            while (ev < numIn && in[ev].offset <= i)
                handleInput (in[ev++], T, curPpq, out, i);

            const double ppq = ppqStart + i * pps;
            curPpq = ppq;

            if (clockRunning())
            {
                const int64_t t16 = (int64_t) std::floor (ppq / 0.25 + 1e-9);
                if (t16 != last16) { last16 = t16; onTick16 (t16, ppq, T); }

                if (s.output == OutArp)
                {
                    const double len = arpRateBeats (s.arpRate);
                    const int64_t ta = (int64_t) std::floor (ppq / len + 1e-9);
                    if (ta != lastArp) { lastArp = ta; onArpTick (ta, T); }
                }
            }

            flushDue (T, i, out);
        }

        if (! usingHost && wasActive) internalPpq = ppqStart + numSamples * pps;
        now += numSamples;

        uint64_t lo = 0, hi = 0;
        for (int n = 0; n < 128; ++n)
            if (noteOn[n]) { if (n < 64) lo |= (1ull << n); else hi |= (1ull << (n - 64)); }
        uiNotesLo = lo; uiNotesHi = hi;
        uiActive = wasActive;
    }

    /** Ends every note immediately (used on bypass / reset / transport stop). */
    void panic (std::vector<MidiEv>& out)
    {
        pending.clear();
        for (int n = 0; n < 128; ++n)
            if (noteOn[n]) { out.push_back ({ 0, 0x80, (uint8_t) n, 0 }); noteOn[n] = false; }
        held.clear(); physical = 0; chordNotes.clear(); currentChord.clear(); wasActive = false;
    }

private:
    struct Pending { int64_t time; bool on; int note, vel; uint32_t gen; int64_t offAt; };

    Settings s, next;
    double sr = 44100.0, pps = 0.0, ppqStart = 0.0, internalPpq = 0.0, expectedHostPpq = 0.0, curPpq = 0.0;
    double progStartPpq = 0.0;
    bool usingHost = false, wasActive = false;
    int64_t now = 0, last16 = INT64_MIN, lastArp = INT64_MIN;
    int64_t arpIdx = 0;
    int stepIdx = -1, physical = 0, lastVel = 100, rhythmHit = 0;

    struct Held { int note, vel; };
    std::vector<Held> held;
    std::vector<int> learnNotes; int learnHeld = 0;

    bool noteOn[128] {};
    uint32_t noteGen[128] {};
    std::vector<Pending> pending;
    std::vector<int> chordNotes;      // notes started by Sustain/Rhythm output
    std::vector<int> currentChord;    // latest voiced target chord (arp pool source)
    std::vector<int> prevVoicing;     // for voice leading
    std::mt19937 rng { 2020 };
    std::vector<Progression> factory;

    //==========================================================================
    void applySettingsChange()
    {
        const bool structural = next.mode != s.mode || next.output != s.output || next.progression != s.progression
                             || next.trigger != s.trigger || next.key != s.key;
        if (next.latch != s.latch && ! next.latch && physical == 0) held.clear();
        if (next.progression != s.progression || next.trigger != s.trigger) stepIdx = -1;
        s = next;
        if (structural) { pendingRelease = true; prevVoicing.clear(); }
    }
    bool pendingRelease = false;

    bool clockRunning() const { return usingHost || wasActive; }

    bool isActive() const
    {
        if (! held.empty()) return true;
        return s.mode == ModeProgression && s.trigger == TrigHost && usingHost;
    }

    void resyncTicks (double ppq)
    {
        auto prevTick = [ppq] (double len)
        {
            const double x = ppq / len;
            const double f = std::floor (x + 1e-9);
            return (x - f) < 1e-6 ? (int64_t) f - 1 : (int64_t) f;   // fire now only if exactly on the grid
        };
        last16 = prevTick (0.25);
        lastArp = prevTick (arpRateBeats (s.arpRate));
    }

    void updateActive (int64_t T, double ppq)
    {
        if (pendingRelease) { pendingRelease = false; releaseAll (T); currentChord.clear(); }

        const bool a = isActive();
        if (a == wasActive) return;
        wasActive = a;

        if (a)
        {
            arpIdx = 0; rhythmHit = 0;
            if (usingHost) { progStartPpq = std::floor (ppq); }
            else
            {
                ppqStart -= curPpq; curPpq = 0.0; internalPpq = 0.0;   // the internal clock restarts at the key press
                progStartPpq = 0.0;
                last16 = INT64_MIN; lastArp = INT64_MIN;
            }
        }
        else
        {
            releaseAll (T);
            currentChord.clear(); prevVoicing.clear();
            uiStep = -1; uiRootPc = -1; uiTick = -1;
        }
    }

    //==========================================================================
    void handleInput (const MidiEv& e, int64_t T, double ppq, std::vector<MidiEv>& out, int offset)
    {
        const int type = e.b0 & 0xF0;
        const bool isOn  = type == 0x90 && e.b2 > 0;
        const bool isOff = type == 0x80 || (type == 0x90 && e.b2 == 0);

        if (! isOn && ! isOff)
        {
            if (type == 0xB0 && (e.b1 == 123 || e.b1 == 120)) { held.clear(); physical = 0; updateActive (T, ppq); }
            out.push_back ({ offset, e.b0, e.b1, e.b2 });     // pass CC, pitch-bend etc. straight through
            return;
        }

        if (learning)
        {
            if (isOn) { learnNotes.push_back (e.b1); ++learnHeld; }
            else if (--learnHeld <= 0)
            {
                learnHeld = 0;
                if (! learnNotes.empty())
                {
                    std::sort (learnNotes.begin(), learnNotes.end());
                    learnNotes.erase (std::unique (learnNotes.begin(), learnNotes.end()), learnNotes.end());
                    std::vector<int> iv;
                    for (int n : learnNotes) iv.push_back (n - learnNotes.front());
                    setMemory (iv);
                }
                learning = false;
            }
            return;
        }

        if (isOn)
        {
            if (s.latch && physical == 0) held.clear();
            if (physical == 0 && s.trigger == TrigStep) ++stepIdx;
            held.erase (std::remove_if (held.begin(), held.end(), [&] (const Held& h) { return h.note == e.b1; }), held.end());
            held.push_back ({ e.b1, e.b2 });
            ++physical;
            lastVel = e.b2;
        }
        else
        {
            physical = std::max (0, physical - 1);
            if (! s.latch)
                held.erase (std::remove_if (held.begin(), held.end(), [&] (const Held& h) { return h.note == e.b1; }), held.end());
        }

        updateActive (T, ppq);
        if (! wasActive) return;

        // Immediate response for held chords and arp pool, so playing feels direct.
        if (s.output == OutSustain)
            sustainUpdate (T, s.trigger == TrigStep && isOn);
        else if (s.output == OutArp || (s.trigger == TrigStep && isOn))
            currentChord = target (curPpq);
    }

    //==========================================================================
    // Building the chord to play right now
    std::vector<int> voice (int root, std::vector<int> iv, bool allowLead)
    {
        std::vector<int> base;
        for (int x : iv) base.push_back (root + x);
        std::sort (base.begin(), base.end());

        auto applySpread = [this] (std::vector<int> v)
        {
            std::sort (v.begin(), v.end());
            if (s.spread == 1 && v.size() >= 3) { v[v.size() - 2] -= 12; }
            else if (s.spread == 2) for (size_t i = 1; i < v.size(); i += 2) v[i] += 12;
            std::sort (v.begin(), v.end());
            return v;
        };
        auto invert = [] (std::vector<int> v, int k)
        {
            for (int i = 0; i < k; ++i) { std::sort (v.begin(), v.end()); v[0] += 12; }
            std::sort (v.begin(), v.end());
            return v;
        };
        auto mean = [] (const std::vector<int>& v) { double m = 0; for (int x : v) m += x; return v.empty() ? 0.0 : m / v.size(); };

        std::vector<int> best;
        if (allowLead && s.voiceLead && ! prevVoicing.empty())
        {
            const double pm = mean (prevVoicing);
            double bestCost = 1e9;
            for (int k = 0; k < (int) base.size(); ++k)
                for (int sh = -12; sh <= 12; sh += 12)
                {
                    auto c = applySpread (invert (base, k));
                    for (auto& x : c) x += sh;
                    if (c.front() < 28 || c.back() > 100) continue;
                    double cost = std::abs (mean (c) - pm) + 0.25 * std::abs (c.front() - prevVoicing.front())
                                + 0.25 * std::abs (c.back() - prevVoicing.back());
                    if (cost < bestCost) { bestCost = cost; best = c; }
                }
        }
        if (best.empty())
            best = applySpread (invert (base, std::min (s.inversion, (int) base.size() - 1)));

        if (allowLead) prevVoicing = best;

        if (s.bass > 0) best.insert (best.begin(), root - 12 * s.bass);
        std::vector<int> outv;
        for (int x : best) if (x >= 0 && x <= 127 && std::find (outv.begin(), outv.end(), x) == outv.end()) outv.push_back (x);
        return outv;
    }

    std::vector<int> chordForNote (int note, bool lead)
    {
        if (s.chordType == 0)
        {
            int deg = 0;
            const int root = snapToScale (note, s.key, (Scale) s.scale, deg);
            auto iv = diatonicIntervals (deg, (Scale) s.scale, s.extension);
            uiRootPc = root % 12; uiQuality = (int) qualityFromIntervals (iv);
            return voice (root + 12 * s.octave, iv, lead);
        }
        const auto q = (Quality) (s.chordType - 1);
        const auto& qi = info (q);
        uiRootPc = note % 12; uiQuality = (int) q;
        return voice (note + 12 * s.octave, std::vector<int> (qi.iv, qi.iv + qi.numNotes), lead);
    }

    int lowestHeld() const
    {
        int lo = 128; for (auto& h : held) lo = std::min (lo, h.note);
        return lo;
    }

    std::vector<int> target (double ppq)
    {
        std::vector<int> result;
        auto addUnique = [&result] (const std::vector<int>& v)
        { for (int x : v) if (std::find (result.begin(), result.end(), x) == result.end()) result.push_back (x); };

        switch (s.mode)
        {
            case ModeThru:
                for (auto& h : held) result.push_back (std::clamp (h.note + 12 * s.octave, 0, 127));
                uiRootPc = held.empty() ? -1 : held.back().note % 12; uiQuality = -1;
                return result;

            case ModeChord:
                for (size_t i = 0; i < held.size(); ++i) addUnique (chordForNote (held[i].note, held.size() == 1));
                return result;

            case ModeMemory:
            {
                const auto mem = getMemory();
                if (mem.empty()) { for (auto& h : held) addUnique (chordForNote (h.note, held.size() == 1)); return result; }
                for (auto& h : held)
                {
                    std::vector<int> v;
                    for (int x : mem) v.push_back (x);
                    // memory keeps its own voicing: only octave + bass apply
                    for (auto& x : v) x += h.note + 12 * s.octave;
                    if (s.bass > 0) v.insert (v.begin(), h.note + 12 * s.octave - 12 * s.bass);
                    for (int x : v) if (x >= 0 && x <= 127 && std::find (result.begin(), result.end(), x) == result.end()) result.push_back (x);
                }
                uiRootPc = held.empty() ? -1 : held.front().note % 12; uiQuality = -2;
                return result;
            }

            default: break;
        }

        // Progression
        const Progression& p = progressionFor (s.progression);
        if (p.numSteps <= 0) return result;

        int step = 0;
        if (s.trigger == TrigStep)
        {
            step = std::max (0, stepIdx) % p.numSteps;
        }
        else
        {
            static const double speed[] = { 0.5, 1.0, 2.0 };
            const double mul = speed[std::clamp (s.progSpeed, 0, 2)];
            const double total = p.totalBeats();
            double pos = (s.trigger == TrigHost && usingHost ? ppq : ppq - progStartPpq) * mul;
            pos = std::fmod (pos, total); if (pos < 0) pos += total;
            double acc = 0;
            for (int i = 0; i < p.numSteps; ++i) { acc += p.steps[i].beats; if (pos < acc - 1e-9) { step = i; break; } }
        }

        const bool follow = s.keyFollow && ! held.empty();
        const int tonic = follow ? lowestHeld() % 12 : s.key;
        const int baseOct = follow ? std::clamp (lowestHeld() / 12, 3, 6) : 4;   // 48 = C3 region
        const auto& st = p.steps[step];
        const int rootPc = (tonic + st.semis) % 12;
        int root = baseOct * 12 + rootPc;
        if (root > baseOct * 12 + 6) root -= 12;               // keep roots around the tonic
        root += 12 * s.octave;

        const auto& qi = info ((Quality) st.quality);
        uiStep = step; uiRootPc = rootPc; uiQuality = st.quality;
        return voice (root, std::vector<int> (qi.iv, qi.iv + qi.numNotes), true);
    }

    //==========================================================================
    int velocityFor (float scale)
    {
        int v = (s.velMode == 0 && ! held.empty()) ? lastVel : s.velocity;
        v = (int) std::round (v * scale);
        if (s.humanize > 0.0f)
        {
            std::uniform_real_distribution<float> d (-1.0f, 1.0f);
            v += (int) std::round (d (rng) * 18.0f * s.humanize);
        }
        return std::clamp (v, 1, 127);
    }

    int64_t humanDelay()
    {
        if (s.humanize <= 0.0f) return 0;
        std::uniform_real_distribution<float> d (0.0f, 1.0f);
        return (int64_t) (d (rng) * s.humanize * 0.012 * sr);
    }

    int64_t beatsToSamples (double beats) const { return (int64_t) std::llround (beats / pps); }

    void schedule (int64_t time, int note, int vel, int64_t offAt)
    {
        pending.push_back ({ time, true, note, vel, 0, offAt });
    }

    void playChord (const std::vector<int>& notes, int64_t T, int vel, int64_t offAt, bool downStrum)
    {
        const int64_t strum = (int64_t) (s.strumMs * 0.001 * sr);
        std::vector<int> order = notes;
        std::sort (order.begin(), order.end());
        if (downStrum) std::reverse (order.begin(), order.end());
        const int64_t h = humanDelay();
        for (size_t i = 0; i < order.size(); ++i)
        {
            schedule (T + h + (int64_t) i * strum, order[i], vel, offAt > 0 ? offAt + (int64_t) i * strum : 0);
            chordNotes.push_back (order[i]);
        }
    }

    void releaseChord (int64_t T)
    {
        for (int n : chordNotes)
        {
            pending.erase (std::remove_if (pending.begin(), pending.end(), [n] (const Pending& p) { return p.note == n; }), pending.end());
            pending.push_back ({ T, false, n, 0, 0xFFFFFFFFu, 0 });
        }
        chordNotes.clear();
    }

    void releaseAll (int64_t T)
    {
        pending.clear();
        for (int n = 0; n < 128; ++n)
            if (noteOn[n]) pending.push_back ({ T, false, n, 0, 0xFFFFFFFFu, 0 });
        chordNotes.clear();
    }

    void sustainUpdate (int64_t T, bool force)
    {
        auto t = target (curPpq);
        if (! force && t == currentChord) return;
        releaseChord (T);
        currentChord = t;
        if (! t.empty()) playChord (t, T, velocityFor (1.0f), 0, false);
    }

    //==========================================================================
    void onTick16 (int64_t t, double ppq, int64_t T)
    {
        if (! wasActive) return;
        uiTick = (int) (((t % 16) + 16) % 16);
        const bool odd = (t & 1) != 0;
        const int64_t swingDelay = odd ? beatsToSamples (s.swing * 0.5 * 0.25) : 0;

        if (s.output == OutSustain)
        {
            if (s.trigger != TrigStep) sustainUpdate (T, false);
            return;
        }

        if (s.output == OutArp)
        {
            if (s.trigger != TrigStep || currentChord.empty()) currentChord = target (ppq);
            return;
        }

        // Rhythm
        const auto& pat = patterns()[(size_t) std::clamp (s.pattern, 0, (int) patterns().size() - 1)].steps;
        const int idx = (int) (((t % 16) + 16) % 16);
        uiTick = idx;
        const char c = pat[idx];
        auto holdSteps = [&] (int from)
        {
            int n = 1;
            while (n < 16 && pat[(from + n) % 16] == '-') ++n;
            return n;
        };

        auto t2 = (s.trigger == TrigStep && ! currentChord.empty()) ? currentChord : target (ppq);

        if (c == 'x' || c == 'o')
        {
            releaseChord (T + swingDelay);
            currentChord = t2;
            const int len = holdSteps (idx);
            const int64_t start = T + swingDelay;
            const int64_t offAt = start + std::max<int64_t> (8, beatsToSamples (0.25 * len * s.gate));
            playChord (t2, start, velocityFor (c == 'x' ? 1.0f : 0.82f), offAt, (rhythmHit++ & 1) != 0);
        }
        else if (c == '-' && t2 != currentChord && ! chordNotes.empty())
        {
            releaseChord (T);
            currentChord = t2;
            int remaining = 1;
            while (remaining < 16 && pat[(idx + remaining) % 16] == '-') ++remaining;
            const int64_t offAt = T + std::max<int64_t> (8, beatsToSamples (0.25 * remaining * s.gate));
            playChord (t2, T, velocityFor (0.82f), offAt, false);
        }
    }

    void onArpTick (int64_t t, int64_t T)
    {
        if (! wasActive || currentChord.empty()) return;

        std::vector<int> pool;
        std::vector<int> base = currentChord;
        if (! (s.arpMode == 5)) std::sort (base.begin(), base.end());
        for (int o = 0; o < std::clamp (s.arpOctaves, 1, 4); ++o)
            for (int n : base) if (n + 12 * o <= 127) pool.push_back (n + 12 * o);
        if (pool.empty()) return;

        const int64_t size = (int64_t) pool.size();
        int64_t i = 0;
        switch (s.arpMode)
        {
            case 0: case 5: i = arpIdx % size; break;
            case 1: i = size - 1 - (arpIdx % size); break;
            case 2: case 3:
            {
                const int64_t period = size > 1 ? 2 * size - 2 : 1;
                const int64_t p = arpIdx % period;
                i = p < size ? p : period - p;
                if (s.arpMode == 3) i = size - 1 - i;
                break;
            }
            default: { std::uniform_int_distribution<int64_t> d (0, size - 1); i = d (rng); break; }
        }
        ++arpIdx;

        const double len = arpRateBeats (s.arpRate);
        const bool straight = s.arpRate == 1 || s.arpRate == 3;
        const int64_t delay = (straight && (t & 1)) ? beatsToSamples (s.swing * 0.5 * len) : 0;
        const int64_t start = T + delay + humanDelay();
        const int64_t offAt = start + std::max<int64_t> (8, beatsToSamples (len * s.gate));
        const float accent = (arpIdx % 4 == 1) ? 1.0f : 0.85f;
        schedule (start, pool[(size_t) i], velocityFor (accent), offAt);
    }

    //==========================================================================
    void flushDue (int64_t T, int offset, std::vector<MidiEv>& out)
    {
        if (pending.empty()) return;
        // offs first, then ons, so a re-struck note is cleanly retriggered
        for (int pass = 0; pass < 2; ++pass)
        {
            for (size_t k = 0; k < pending.size();)
            {
                auto p = pending[k];
                if (p.time > T || p.on != (pass == 1)) { ++k; continue; }
                pending.erase (pending.begin() + (long) k);

                if (! p.on)
                {
                    if (noteOn[p.note] && (p.gen == 0xFFFFFFFFu || p.gen == noteGen[p.note]))
                    { out.push_back ({ offset, 0x80, (uint8_t) p.note, 0 }); noteOn[p.note] = false; }
                }
                else
                {
                    if (noteOn[p.note]) out.push_back ({ offset, 0x80, (uint8_t) p.note, 0 });
                    out.push_back ({ offset, 0x90, (uint8_t) p.note, (uint8_t) p.vel });
                    noteOn[p.note] = true;
                    const uint32_t g = ++noteGen[p.note];
                    if (p.offAt > 0) pending.push_back ({ std::max (p.offAt, T + 1), false, p.note, 0, g, 0 });
                }
            }
        }
    }
};

} // namespace ca

namespace ca
{
/** Copies a style preset's musical settings into Settings (used by the demo renderer and tests). */
inline void applyStyle (Settings& s, int index)
{
    const auto& st = styles()[(size_t) std::clamp (index, 0, (int) styles().size() - 1)];
    s.mode = ModeProgression; s.progression = index; s.key = st.key; s.scale = st.scale;
    s.internalBpm = st.bpm; s.output = st.output; s.pattern = st.pattern;
    s.arpMode = st.arpMode; s.arpRate = st.arpRate; s.arpOctaves = st.arpOct;
    s.gate = st.gate; s.swing = st.swing; s.octave = st.octave; s.spread = st.spread;
    s.bass = st.bass; s.strumMs = st.strumMs;
}
} // namespace ca
