// Durams Sn - velocity-driven one-shot snare re-synthesis engine.
// Pure C++17, no JUCE dependency, so it can be unit-tested stand-alone.
#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace durams
{

struct SampleData
{
    std::vector<float> left, right;   // equal length, mono is duplicated
    double sampleRate = 44100.0;
    int length() const { return (int) left.size(); }
};

struct Params
{
    float level  = 0.0f;   // dB   -36 .. +12
    float curve  = 0.0f;   // -1 .. 1   (>0 : need to hit harder for loud)
    float range  = 0.5f;   // 0 .. 1    dynamic range soft -> hard
    float tone   = 0.0f;   // -1 .. 1   static brightness
    float bright = 0.5f;   // 0 .. 1    how much velocity darkens soft hits
    float human  = 0.35f;  // 0 .. 1    per-hit random variation
    float pitch  = 0.0f;   // semitones -12 .. 12
    float snap   = 0.5f;   // 0 .. 1    stick attack
    float decay  = 1.0f;   // 0 .. 1    1 = natural length
    float wires  = 0.5f;   // 0 .. 1    0.5 = natural
    float body   = 0.5f;   // 0 .. 1    0.5 = natural
    float drive  = 0.2f;   // 0 .. 1    velocity dependent saturation
};

namespace detail
{
    inline double clampd (double v, double lo, double hi)
    {
        if (! std::isfinite (v)) return lo;
        return v < lo ? lo : (v > hi ? hi : v);
    }

    struct Rng
    {
        uint32_t s = 0x9E3779B9u;
        inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        inline double bi() { return (double) (next() >> 8) * (2.0 / 16777216.0) - 1.0; } // [-1,1)
    };

    // TPT state-variable filter, double precision
    struct Svf
    {
        double a1 = 0, a2 = 0, a3 = 0, k = 1.41421356, ic1 = 0, ic2 = 0;
        void set (double fc, double sr, double q)
        {
            fc = clampd (fc, 15.0, sr * 0.45);
            const double g = std::tan (3.14159265358979323846 * fc / sr);
            k = 1.0 / q;
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        inline void run (double x, double& lp, double& hp)
        {
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            lp = v2;
            hp = x - k * v1 - v2;
        }
    };

    inline float hermite (const float* b, int len, int i, float f)
    {
        auto at = [&] (int k) { return b[k < 0 ? 0 : (k >= len ? len - 1 : k)]; };
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
}

class SnareEngine
{
public:
    static constexpr int kMaxVoices = 32;
    static constexpr int kPolyCap   = 24;

    void prepare (double sr)
    {
        sampleRate = (sr > 1000.0 ? sr : 44100.0);
        levelCoef  = 1.0 - std::exp (-1.0 / (0.02 * sampleRate));
        fadeLen    = (int) (0.003 * sampleRate) + 1;
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = Voice();
        levelInit = false;
    }

    void setSample (const SampleData* s) { current = s; }

    void setParams (const Params& in)
    {
        using detail::clampd;
        p.level  = (float) clampd (in.level,  -36.0, 12.0);
        p.curve  = (float) clampd (in.curve,  -1.0, 1.0);
        p.range  = (float) clampd (in.range,   0.0, 1.0);
        p.tone   = (float) clampd (in.tone,   -1.0, 1.0);
        p.bright = (float) clampd (in.bright,  0.0, 1.0);
        p.human  = (float) clampd (in.human,   0.0, 1.0);
        p.pitch  = (float) clampd (in.pitch,  -12.0, 12.0);
        p.snap   = (float) clampd (in.snap,    0.0, 1.0);
        p.decay  = (float) clampd (in.decay,   0.0, 1.0);
        p.wires  = (float) clampd (in.wires,   0.0, 1.0);
        p.body   = (float) clampd (in.body,    0.0, 1.0);
        p.drive  = (float) clampd (in.drive,   0.0, 1.0);
        levelTarget = std::pow (10.0, p.level / 20.0);
        if (! levelInit) { levelCur = levelTarget; levelInit = true; }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active && ! v.dying) { v.dying = true; v.dyingGain = 1.0; }
    }

    // velocity 0..1 (MIDI vel / 127)
    void noteOn (float velocity)
    {
        const SampleData* s = current;
        if (s == nullptr || s->length() < 2 || (int) s->right.size() != s->length()) return;
        if (! std::isfinite (velocity) || velocity <= 0.0f) return;
        velocity = std::min (velocity, 1.0f);

        int act = 0; Voice* oldest = nullptr;
        for (auto& v : voices)
            if (v.active && ! v.dying)
            {
                ++act;
                if (oldest == nullptr || v.age < oldest->age) oldest = &v;
            }
        if (act >= kPolyCap && oldest != nullptr) { oldest->dying = true; oldest->dyingGain = 1.0; }

        Voice* slot = nullptr;
        for (auto& v : voices) if (! v.active) { slot = &v; break; }
        if (slot == nullptr)
        {
            slot = &voices[0];
            for (auto& v : voices) if (v.age < slot->age) slot = &v;
        }
        startVoice (*slot, *s, velocity);
    }

    // ADDS into L/R
    void render (float* L, float* R, int n)
    {
        for (auto& v : voices)
            if (v.active) renderVoice (v, L, R, n);

        for (int i = 0; i < n; ++i)
        {
            levelCur += (levelTarget - levelCur) * levelCoef;
            double l = L[i] * levelCur, r = R[i] * levelCur;
            if (! std::isfinite (l)) l = 0.0;
            if (! std::isfinite (r)) r = 0.0;
            L[i] = (float) std::max (-4.0, std::min (4.0, l));
            R[i] = (float) std::max (-4.0, std::min (4.0, r));
        }
    }

    int activeVoices() const { int c = 0; for (auto& v : voices) if (v.active) ++c; return c; }

private:
    struct Voice
    {
        bool active = false, dying = false;
        const SampleData* s = nullptr;
        double pos = 0, inc = 1, t = 0, dyingGain = 1;
        uint64_t age = 0;
        double gain = 1, env = 1, decayMul = 1, snapAmt = 0, snapEnv = 1, snapMul = 1, fadeInSec = 0.0001;
        double bodyGain = 0, wiresGain = 0, driveG = 1, driveMakeup = 1;
        double toneFc0 = 20000, tailMul = 1;
        int ctr = 0;
        detail::Svf body[2], wire[2], tone[2];
    };

    void startVoice (Voice& v, const SampleData& s, float vel)
    {
        using detail::clampd;
        v = Voice();
        v.active = true; v.s = &s; v.age = ++ageCounter;

        const double h = p.human;
        const double r1 = rng.bi(), r2 = rng.bi(), r3 = rng.bi(), r4 = rng.bi(), r5 = rng.bi(), r6 = rng.bi();

        // velocity curve + tiny human wobble in the "effective" force
        const double shaped = std::pow ((double) vel, std::pow (2.0, p.curve * 1.2));
        const double vp = clampd (shaped + r1 * 0.03 * h, 0.0, 1.0);

        // loudness: 0 dB at full force, down to -(4..42) dB for the softest touch
        const double gainDb = -(1.0 - vp) * (4.0 + p.range * 38.0) + r2 * 1.2 * h;
        v.gain = std::pow (10.0, gainDb / 20.0);

        // brightness: soft hits are darker
        const double fc = 20000.0 * std::pow (2.0, -(1.0 - vp) * p.bright * 4.5)
                                  * std::pow (2.0, p.tone * 1.5)
                                  * std::pow (2.0, r3 * 0.25 * h);
        v.toneFc0 = clampd (fc, 250.0, sampleRate * 0.45);
        v.tailMul = 0.35 + 0.65 * vp * vp;   // highs die faster on soft hits

        // pitch: harder hits tension the head a little
        const double semis = p.pitch + (vp - 0.7) * 0.5 + r4 * 0.12 * h;
        v.inc = clampd (std::pow (2.0, semis / 12.0) * s.sampleRate / sampleRate, 0.05, 16.0);

        // decay (only shortens, 1 = natural); soft hits ring a bit less
        double rate = 60.0 * (1.0 - p.decay) * (1.0 - p.decay) + p.range * (1.0 - vp) * (1.0 - vp) * 10.0;
        rate *= 1.0 + r5 * 0.2 * h;
        v.decayMul = std::exp (-std::max (rate, 0.0) / sampleRate);

        // stick attack
        v.snapAmt   = p.snap * (0.3 + 0.7 * vp) * 1.5;
        v.snapMul   = std::exp (-1.0 / (0.005 * sampleRate));
        v.fadeInSec = (1.0 - vp) * (1.0 - p.snap) * 0.003 + 0.0001;

        // shell body and snare wires
        v.bodyGain = (p.body - 0.5) * 2.0 * 0.9 * (0.4 + 0.6 * vp);
        const double w = (p.wires - 0.5) * 2.0;
        v.wiresGain = (w > 0.0 ? w * (0.25 + 0.9 * vp) : w * 0.8 * (0.3 + 0.7 * vp)) * (1.0 + r6 * 0.2 * h);

        // velocity dependent saturation
        const double d = p.drive * (0.2 + 0.8 * vp);
        v.driveG = 1.0 + 4.0 * d;
        v.driveMakeup = 1.0 + 0.6 * d;

        for (int c = 0; c < 2; ++c)
        {
            v.body[c].set (250.0, sampleRate, 0.707);
            v.wire[c].set (3500.0, sampleRate, 0.707);
            v.tone[c].set (v.toneFc0, sampleRate, 0.707);
        }
    }

    void renderVoice (Voice& v, float* L, float* R, int n)
    {
        const SampleData& s = *v.s;
        const int len = s.length();
        const float* bl = s.left.data();
        const float* br = s.right.data();
        const double dt = 1.0 / sampleRate;
        const bool useBody = std::fabs (v.bodyGain) > 1e-4, useWire = std::fabs (v.wiresGain) > 1e-4;
        const double fadeStep = 1.0 / (double) fadeLen;

        for (int i = 0; i < n; ++i)
        {
            if (v.pos >= (double) (len - 1)) { v.active = false; return; }
            const int i1 = (int) v.pos;
            const float f = (float) (v.pos - (double) i1);
            const double x[2] = { detail::hermite (bl, len, i1, f), detail::hermite (br, len, i1, f) };

            if (++v.ctr >= 16)
            {
                v.ctr = 0;
                const double fc = v.toneFc0 * (v.tailMul + (1.0 - v.tailMul) * std::exp (-v.t / 0.15));
                detail::Svf t = v.tone[0];
                t.set (fc, sampleRate, 0.707);
                for (int c = 0; c < 2; ++c)
                {
                    v.tone[c].a1 = t.a1; v.tone[c].a2 = t.a2; v.tone[c].a3 = t.a3; v.tone[c].k = t.k;
                }
            }

            const double fadeIn = v.t < v.fadeInSec ? 0.5 - 0.5 * std::cos (3.14159265358979 * v.t / v.fadeInSec) : 1.0;
            const double endFade = std::min (1.0, ((double) (len - 1) - v.pos) / (48.0 * v.inc));
            const double g = v.gain * v.env * fadeIn * endFade * (1.0 + v.snapAmt * v.snapEnv) * v.driveMakeup
                             * (v.dying ? v.dyingGain : 1.0);

            double out[2];
            for (int c = 0; c < 2; ++c)
            {
                double y = x[c], lp = 0.0, hp = 0.0;
                if (useBody) { v.body[c].run (x[c], lp, hp); y += v.bodyGain * lp; }
                if (useWire) { v.wire[c].run (x[c], lp, hp); y += v.wiresGain * hp; }
                v.tone[c].run (y, lp, hp);
                y = std::tanh (lp * v.driveG) / v.driveG;
                out[c] = y * g;
            }
            L[i] += (float) out[0];
            R[i] += (float) out[1];

            v.env *= v.decayMul;
            v.snapEnv *= v.snapMul;
            v.t += dt;
            v.pos += v.inc;
            if (v.dying) { v.dyingGain -= fadeStep; if (v.dyingGain <= 0.0) { v.active = false; return; } }
            if (v.env < 1e-6) { v.active = false; return; }
        }
    }

    Voice voices[kMaxVoices];
    Params p;
    const SampleData* current = nullptr;
    double sampleRate = 44100.0, levelCur = 1.0, levelTarget = 1.0, levelCoef = 0.01;
    bool levelInit = false;
    int fadeLen = 133;
    uint64_t ageCounter = 0;
    detail::Rng rng;
};

} // namespace durams
