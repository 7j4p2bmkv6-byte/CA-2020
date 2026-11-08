// CA-2020 - small built-in poly synth so the standalone app makes sound on its own.
// Juno-flavoured: PolyBLEP saw + sub square, 24 dB resonant low-pass, ADSR, stereo BBD-style chorus.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace ca
{
struct SynthParams
{
    float cutoff = 0.45f, reso = 0.25f, envAmt = 0.35f;
    float attack = 0.004f, decay = 0.35f, sustain = 0.55f, release = 0.30f;
    float sub = 0.35f, chorus = 1.0f, level = 0.7f;
};

inline const char* previewPatchName (int i)
{
    static const char* n[] = { "Poly Stab", "Warm Pad", "Pluck", "Organ", "Brass" };
    return n[std::clamp (i, 0, 4)];
}

inline SynthParams previewPatch (int i)
{
    SynthParams p;
    switch (i)
    {
        case 1: p = { 0.38f, 0.15f, 0.20f, 0.45f, 1.2f, 0.85f, 1.10f, 0.30f, 1.0f, 0.62f }; break;
        case 2: p = { 0.30f, 0.30f, 0.55f, 0.002f, 0.22f, 0.0f, 0.25f, 0.25f, 1.0f, 0.75f }; break;
        case 3: p = { 0.70f, 0.05f, 0.00f, 0.004f, 0.10f, 1.0f, 0.06f, 0.50f, 1.0f, 0.55f }; break;
        case 4: p = { 0.32f, 0.20f, 0.45f, 0.06f, 0.50f, 0.70f, 0.25f, 0.20f, 1.0f, 0.62f }; break;
        default: break;
    }
    return p;
}

class PreviewSynth
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v = Voice();
        const int maxDelay = (int) (sr * 0.03f) + 4;
        dl.assign ((size_t) maxDelay, 0.0f); dr.assign ((size_t) maxDelay, 0.0f); wp = 0;
    }

    void setParams (const SynthParams& p) { prm = p; }

    void noteOn (int note, int vel)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.note == note && x.stage != Off) { v = &x; break; }
        if (! v) for (auto& x : voices) if (x.stage == Off) { v = &x; break; }
        if (! v) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
        v->note = note; v->vel = vel / 127.0f; v->stage = Attack; v->age = 0;
        v->inc = 440.0f * std::pow (2.0f, (note - 69) / 12.0f) / sr;
        v->detune = 1.0f + 0.0007f * (float) ((note * 7919) % 11 - 5);
    }

    void noteOff (int note)
    {
        for (auto& x : voices) if (x.note == note && x.stage != Off && x.stage != Release) x.stage = Release;
    }

    void allOff() { for (auto& x : voices) if (x.stage != Off) x.stage = Release; }

    /** Adds stereo output to L/R. */
    void render (float* L, float* R, int n)
    {
        const float aRate = 1.0f / std::max (1.0f, prm.attack * sr);
        const float dCoef = std::exp (-1.0f / std::max (1.0f, prm.decay * sr * 0.3f));
        const float rCoef = std::exp (-1.0f / std::max (1.0f, prm.release * sr * 0.3f));

        for (int i = 0; i < n; ++i)
        {
            float mono = 0.0f;
            for (auto& v : voices)
            {
                if (v.stage == Off) continue;
                ++v.age;
                switch (v.stage)
                {
                    case Attack:  v.env += aRate; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Decay; } break;
                    case Decay:   v.env = prm.sustain + (v.env - prm.sustain) * dCoef; break;
                    case Release: v.env *= rCoef; if (v.env < 0.0005f) { v.stage = Off; v.env = 0; continue; } break;
                    default: break;
                }
                const float inc = v.inc * v.detune;
                v.phase += inc; if (v.phase >= 1.0f) v.phase -= 1.0f;
                v.subPhase += inc * 0.5f; if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
                float saw = 2.0f * v.phase - 1.0f - blep (v.phase, inc);
                float sq  = (v.subPhase < 0.5f ? 1.0f : -1.0f) + blep (v.subPhase, inc * 0.5f)
                          - blep (std::fmod (v.subPhase + 0.5f, 1.0f), inc * 0.5f);
                float osc = saw * 0.6f + sq * prm.sub * 0.6f;

                float fc = 30.0f * std::pow (600.0f, std::clamp (prm.cutoff + prm.envAmt * v.env * 0.6f + (v.note - 60) * 0.004f, 0.0f, 1.0f));
                fc = std::min (fc, sr * 0.45f);
                const float g = std::tan (3.14159265f * fc / sr);
                const float k = 2.0f - 1.9f * prm.reso;
                float y = svf (v.s1, v.s2, osc, g, k);
                y = svf (v.s3, v.s4, y, g, 1.4f);
                mono += y * v.env * (0.35f + 0.65f * v.vel);
            }
            mono *= 0.22f * prm.level;

            // stereo chorus (two modulated delay taps, Juno I-style rate)
            float outL = mono, outR = mono;
            if (prm.chorus > 0.0f && ! dl.empty())
            {
                lfo += 0.5f / sr; if (lfo >= 1.0f) lfo -= 1.0f;
                const float tri = 4.0f * std::abs (lfo - 0.5f) - 1.0f;
                const int size = (int) dl.size();
                dl[(size_t) wp] = mono; dr[(size_t) wp] = mono;
                auto tap = [&] (std::vector<float>& d, float ms)
                {
                    float pos = (float) wp - ms * 0.001f * sr;
                    while (pos < 0) pos += (float) size;
                    int a = (int) pos; float f = pos - (float) a;
                    return d[(size_t) (a % size)] * (1.0f - f) + d[(size_t) ((a + 1) % size)] * f;
                };
                const float cl = tap (dl, 3.5f + 1.8f * tri), cr = tap (dr, 3.5f - 1.8f * tri);
                outL = mono * 0.7f + cl * 0.55f * prm.chorus;
                outR = mono * 0.7f + cr * 0.55f * prm.chorus;
                wp = (wp + 1) % size;
            }
            L[i] += std::tanh (outL);
            R[i] += std::tanh (outR);
        }
    }

private:
    enum Stage { Off, Attack, Decay, Release };
    struct Voice
    {
        int note = -1; float vel = 0, env = 0, phase = 0, subPhase = 0, inc = 0, detune = 1;
        float s1 = 0, s2 = 0, s3 = 0, s4 = 0; Stage stage = Off; int64_t age = 0;
    };

    static float blep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    static float svf (float& ic1, float& ic2, float x, float g, float k)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    float sr = 44100.0f, lfo = 0.0f;
    SynthParams prm;
    Voice voices[10];
    std::vector<float> dl, dr; int wp = 0;
};

} // namespace ca
