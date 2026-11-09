#pragma once
// LIVE VOICE - real-time voice DSP (header-only, no JUCE dependency so it can be tested with plain g++).
//
// Signal flow (mono inside, stereo out):
//   input -> HPF -> GATE -> PITCH (Signalsmith Stretch, formant kept) ─┬─> sum -> VOICE CHAIN -> SFX -> OUTPUT gain -> LIMITER
//                              └─> BRUTAL layer (pitch - 12 st, same latency) ┘
//   VOICE CHAIN = mud cut -> COMPRESSOR -> body / presence / air EQ -> DE-ESSER -> WARMTH (saturation)
//   SFX = WIDE (stereo doubler) | SHIMMER (octave-up reverb) | ECHO THROW (ducked ping-pong) | BRUTAL (octave-down layer)
//   CLIP (button): level-matched tanh clipper with ADAA before the output gain, 0 latency.
//   ZERO LATENCY: when PITCH = 0, FORMANT = 0 and BRUTAL is off, the voice skips the pitch shifter (crossfade), so the
//   only delay is the sound card buffer. The shifter adds ~30 ms only while the pitch/formant is really moved.
//   AUTO LEVEL: a slow input gain rider (only while the host talks) brings quiet USB mics up to a good level.

#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <atomic>
#include "signalsmith-stretch/signalsmith-stretch.h"

namespace lv
{
constexpr float kPi = 3.14159265358979f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
// transparent below 0.8, rounds the top so nothing can ever pass 0.99
// tanh clipper with 1st-order antiderivative anti-aliasing (ADAA): no oversampling, no latency, clean highs
struct AdaaClipper
{
    float x1 = 0, f1 = 0;
    static float logCosh (float x)
    {
        const float a = std::abs (x);
        return a > 12.0f ? a - 0.69314718f : std::log (std::cosh (x));
    }
    void reset() { x1 = 0; f1 = 0; }
    inline float process (float x)
    {
        const float f = logCosh (x);
        const float d = x - x1;
        const float y = std::abs (d) > 1.0e-4f ? (f - f1) / d : std::tanh (0.5f * (x + x1));
        x1 = x; f1 = f;
        return y;
    }
};

inline float softClip (float x)
{
    const float a = std::abs (x);
    if (a < 0.8f) return x;
    return std::copysign (0.8f + 0.19f * std::tanh ((a - 0.8f) / 0.19f), x);
}

// one-pole smoothing coefficient for a time constant in ms
inline float coefMs (float ms, double sr) { return ms <= 0.0f ? 0.0f : std::exp (-1.0f / (float) (ms * 0.001 * sr)); }

// RBJ biquad
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    void hp (double sr, float f, float q = 0.7071f)
    {
        const float w = 2 * kPi * f / (float) sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void lp (double sr, float f, float q = 0.7071f)
    {
        const float w = 2 * kPi * f / (float) sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void bp (double sr, float f, float q)
    {
        const float w = 2 * kPi * f / (float) sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set (a, 0, -a, 1 + a, -2 * c, 1 - a);
    }
    void peak (double sr, float f, float q, float db)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / (float) sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
    void lowShelf (double sr, float f, float db)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / (float) sr, c = std::cos (w), s = std::sin (w);
        const float a = s / 2 * std::sqrt (2.0f), sq = 2 * std::sqrt (A) * a;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (double sr, float f, float db)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / (float) sr, c = std::cos (w), s = std::sin (w);
        const float a = s / 2 * std::sqrt (2.0f), sq = 2 * std::sqrt (A) * a;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

// simple fractional delay line
struct DelayLine
{
    std::vector<float> buf; int w = 0;
    void init (int maxSamples) { buf.assign ((size_t) std::max (4, maxSamples), 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    inline float read (float delay) const   // delay in samples, >= 1
    {
        const int n = (int) buf.size();
        float pos = (float) w - delay;
        while (pos < 0) pos += (float) n;
        const int i0 = (int) pos; const float fr = pos - (float) i0;
        const float a = buf[(size_t) (i0 % n)], b = buf[(size_t) ((i0 + 1) % n)];
        return a + (b - a) * fr;
    }
};

// ---------------------------------------------------------------------------------------------
// The four voice modes. Every number is the value at STRENGTH = 100 %; strength scales them.
struct ModeSpec
{
    const char* name; const char* hint;
    float hpf, mudDb, mudHz, thrDb, ratio, bodyDb, bodyHz, presDb, presHz, airDb, deess, warmth;
};
inline const ModeSpec kModes[4] =
{
    { "NATURAL", "clean & even",        90.0f, -2.0f, 320.0f, -22.0f, 2.5f, 1.0f, 140.0f, 2.5f, 4200.0f, 1.0f, 0.55f, 0.10f },
    { "RADIO",   "dense FM host",       80.0f, -3.5f, 340.0f, -30.0f, 5.0f, 3.5f, 130.0f, 3.5f, 3600.0f, 1.5f, 0.80f, 0.35f },
    { "CLUB",    "cuts through music", 140.0f, -4.0f, 300.0f, -32.0f, 6.0f, 0.0f, 150.0f, 5.0f, 3000.0f, 2.0f, 0.90f, 0.30f },
    { "DEEP",    "warm & low",          60.0f, -2.5f, 420.0f, -26.0f, 3.5f, 4.5f, 110.0f, 1.5f, 4000.0f, -1.0f, 0.60f, 0.25f },
};

struct SfxSpec { const char* name; const char* hint; };
inline const SfxSpec kSfx[4] =
{
    { "WIDE",       "huge stereo double" },
    { "SHIMMER",    "angelic octave halo" },
    { "ECHO THROW", "last word flies away" },
    { "BRUTAL",     "heavy low announcer" },
};

struct Params
{
    bool  voiceOn = true;
    int   mode = 1;           // RADIO
    float strength = 0.7f;    // 0..1
    float pitch = 0.0f;       // semitones -12..+12
    float formant = 0.0f;     // semitones -12..+12 (0 = natural formants)
    bool  sfxOn = false;
    int   sfxType = 0;
    float sfxAmount = 0.5f;   // 0..1
    bool  mute = false;
    float outDb = 0.0f;
    bool  clipOn = false;     // CLIP button: dense radio clipper, level-matched
};

// ---------------------------------------------------------------------------------------------
class PitchShifter
{
public:
    void prepare (double sr, int maxBlock)
    {
        // ~30 ms analysis block: a compromise between live latency and voice quality
        const int block = (int) (sr * 0.030), interval = block / 4;
        st.configure (1, block, interval, false);
        st.setFormantBase (0);
        in.assign ((size_t) maxBlock, 0.0f); out.assign ((size_t) maxBlock, 0.0f);
        sampleRate = sr;
        st.reset();
    }
    int latency() const { return st.inputLatency() + st.outputLatency(); }
    void setShift (float semis, float formantSemis)
    {
        // tonality limit keeps the breath / sibilance natural instead of chipmunky
        st.setTransposeSemitones (semis, (float) (8000.0 / sampleRate));
        st.setFormantSemitones (formantSemis, true);   // true = keep the original formants while the pitch moves
    }
    void process (const float* src, float* dst, int n)
    {
        const float* ins[1] = { src };
        float* outs[1] = { dst };
        st.process (ins, n, outs, n);
    }
    void reset() { st.reset(); }
private:
    signalsmith::stretch::SignalsmithStretch<float> st { 4242 };
    std::vector<float> in, out;
    double sampleRate = 48000.0;
};

// Small 8-line feedback delay network reverb (used by SHIMMER)
class Fdn
{
public:
    void prepare (double sr)
    {
        static const float ms[8] = { 37.1f, 41.3f, 47.9f, 53.3f, 59.7f, 67.1f, 73.9f, 83.3f };
        for (int i = 0; i < 8; ++i) { len[i] = (int) (ms[i] * 0.001 * sr * 1.6); d[i].init (len[i] + 4); lpz[i] = 0; }
        pre.init ((int) (sr * 0.05) + 4);
        sampleRate = sr;
    }
    void clear() { for (auto& x : d) x.clear(); pre.clear(); for (auto& z : lpz) z = 0; }
    // decay: seconds to -60 dB, damp: 0..1
    void process (float in, float decay, float damp, float& outL, float& outR)
    {
        pre.push (in);
        const float x = pre.read ((float) (sampleRate * 0.03));
        float v[8];
        for (int i = 0; i < 8; ++i) v[i] = d[i].read ((float) len[i]);
        // Householder mix
        float s = 0; for (float a : v) s += a; s *= 0.25f;
        for (int i = 0; i < 8; ++i)
        {
            const float g = std::pow (10.0f, -3.0f * (float) len[i] / (float) (decay * sampleRate));
            float y = (v[i] - s) * g;
            lpz[i] += (1.0f - damp) * (y - lpz[i]);   // damping
            d[i].push (lpz[i] + x * (i & 1 ? 0.35f : -0.35f));
        }
        outL = (v[0] + v[2] + v[4] + v[6]) * 0.5f;
        outR = (v[1] + v[3] + v[5] + v[7]) * 0.5f;
    }
private:
    DelayLine d[8], pre; int len[8] {}; float lpz[8] {}; double sampleRate = 48000.0;
};

// ---------------------------------------------------------------------------------------------
class VoiceEngine
{
public:
    std::atomic<float> inLevel { 0 }, outLevel { 0 }, gainReduction { 0 }, sfxActivity { 0 };

    void prepare (double sr, int maxBlock)
    {
        fs = sr; maxN = std::max (32, maxBlock);
        main.prepare (sr, maxN); demon.prepare (sr, maxN); shimmerPs.prepare (sr, maxN);
        gateBuf.assign ((size_t) maxN, 0);
        gateDl.init (main.latency() + 64); dryDl.init (main.latency() + 64);
        mono.assign ((size_t) maxN, 0); pitched.assign ((size_t) maxN, 0); demonBuf.assign ((size_t) maxN, 0);
        shimIn.assign ((size_t) maxN, 0); shimOut.assign ((size_t) maxN, 0);
        wideDl.init ((int) (sr * 0.06)); echoL.init ((int) (sr * 1.2)); echoR.init ((int) (sr * 1.2));
        fdn.prepare (sr);
        reset();
    }

    void reset()
    {
        main.reset(); demon.reset(); shimmerPs.reset();
        for (auto* b : { &hpf1, &hpf2, &mud, &body, &pres, &air, &deessDet, &deessSplit, &demonLp, &echoHpL, &echoLpL, &echoHpR, &echoLpR, &shimHp })
            b->reset();
        gateDl.clear(); dryDl.clear(); noiseFloor = 1.0e-3f; lvlEnv = 0;
        for (auto& m : segMin) m = 1.0f; segCur = 1.0f; segPos = segCount = 0;
        for (auto* b : { &xLp1, &xLp2, &xHp1, &xHp2 }) b->reset();
        wideDl.clear(); echoL.clear(); echoR.clear(); fdn.clear();
        gateEnv = 0; gateGain = 0; compEnv = 0; deessEnv = 0; limGain = 1; shimFbL = shimFbR = 0; voiceEnv = 0;
        pitchSm = cur.pitch; formantSm = cur.formant; muteGain = cur.mute ? 0.0f : 1.0f;
        voiceMix = cur.voiceOn ? 1.0f : 0.0f; sfxMix = 0; demonMix = 0;
        clipL.reset(); clipR.reset(); clipMix = cur.clipOn ? 1.0f : 0.0f;
        directMix = (std::abs (cur.pitch) < 0.005f && std::abs (cur.formant) < 0.005f) ? 1.0f : 0.0f; directHold = 0; agcEnv = 0;
        lastMode = -1;
    }

    int latencySamples() const { return main.latency(); }   // only while pitching; 0 in the direct path
    std::atomic<bool> pitchPathActive { false };

    void setParams (const Params& p) { cur = p; }

    // in: one or two input channels (any can be null), out: L/R
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        int done = 0;
        while (done < n)
        {
            const int k = std::min (32, n - done);
            processChunk (inL ? inL + done : nullptr, inR ? inR + done : nullptr, outL + done, outR + done, k);
            done += k;
        }
    }

private:
    double fs = 48000.0; int maxN = 512;
    Params cur;
    PitchShifter main, demon, shimmerPs;
    std::vector<float> mono, pitched, demonBuf, shimIn, shimOut;
    Biquad hpf1, hpf2, mud, body, pres, air, deessDet, deessSplit, demonLp, echoHpL, echoLpL, echoHpR, echoLpR, shimHp;
    DelayLine wideDl, echoL, echoR;
    Fdn fdn;
    float gateEnv = 0, gateGain = 0, compEnv = 0, deessEnv = 0, limGain = 1, voiceEnv = 0;
    float pitchSm = 0, formantSm = 0, muteGain = 1, voiceMix = 1, sfxMix = 0, demonMix = 0, shimFbL = 0, shimFbR = 0;
    float directMix = 1, directHold = 0;   // 1 = voice bypasses the pitch shifter (no latency)
    float agcDb = 0, agcEnv = 0;           // auto level
    int gateHold = 0;
    float noiseFloor = 1.0e-3f, lvlEnv = 0;  // room noise tracker (sets the gate threshold)
    float segMin[8] { 1, 1, 1, 1, 1, 1, 1, 1 }; float segCur = 1; int segPos = 0, segCount = 0;   // rolling minimum over ~1.6 s
    DelayLine gateDl, dryDl;                 // gate gain / dry voice delayed by the shifter latency
    Biquad xLp1, xLp2, xHp1, xHp2;           // 4.5 kHz Linkwitz-Riley crossover: highs of the original voice
    std::vector<float> gateBuf;
    AdaaClipper clipL, clipR; float clipMix = 0;
    float lfoPhase[4] { 0.0f, 0.25f, 0.5f, 0.75f };
    int lastMode = -1; float lastStrength = -1;
    float chL = 0, chR = 0;   // input channel energy (picks the live channel of a mono USB mic)
    float peakIn = 0, peakOut = 0; int meterCount = 0;

    void updateFilters()
    {
        const auto& m = kModes[std::clamp (cur.mode, 0, 3)];
        const float s = cur.strength;
        hpf1.hp (fs, m.hpf, 0.54f); hpf2.hp (fs, m.hpf, 1.31f);           // 4th order Butterworth
        mud.peak (fs, m.mudHz, 1.0f, m.mudDb * s);
        body.lowShelf (fs, m.bodyHz, m.bodyDb * s);
        pres.peak (fs, m.presHz, 0.8f, m.presDb * s);
        air.highShelf (fs, 10000.0f, m.airDb * s);
        deessDet.bp (fs, 6500.0f, 1.2f);
        deessSplit.hp (fs, 4500.0f);
        demonLp.lp (fs, 2600.0f);
        echoHpL.hp (fs, 450.0f); echoHpR.hp (fs, 450.0f);
        echoLpL.lp (fs, 4200.0f); echoLpR.lp (fs, 4200.0f);
        shimHp.hp (fs, 500.0f);
        xLp1.lp (fs, 4500.0f); xLp2.lp (fs, 4500.0f); xHp1.hp (fs, 4500.0f); xHp2.hp (fs, 4500.0f);
        lastMode = cur.mode; lastStrength = s;
    }

    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        if (cur.mode != lastMode || std::abs (cur.strength - lastStrength) > 0.001f) updateFilters();
        const auto& m = kModes[std::clamp (cur.mode, 0, 3)];
        const float s = cur.strength;

        const float cNoiseA = coefMs (5.0f, fs), cNoiseR = coefMs (60.0f, fs);
        const int segLen = (int) (fs * 0.2);
        const float cGateA = coefMs (1.0f, fs), cGateR = coefMs (40.0f, fs), cGgA = coefMs (2.0f, fs), cGgR = coefMs (70.0f, fs);
        const int holdLen = (int) (fs * 0.12);

        // ---- input: pick / average the channels that carry signal ----
        float eL = 0, eR = 0;
        for (int i = 0; i < n; ++i) { if (inL) eL += inL[i] * inL[i]; if (inR) eR += inR[i] * inR[i]; }
        chL = chL * 0.99f + eL; chR = chR * 0.99f + eR;
        const bool useL = inL && chL > 1e-9f, useR = inR && chR > 1e-9f;
        const float wl = useL && useR ? 0.5f : 1.0f;
        float pk = 0;
        for (int i = 0; i < n; ++i)
        {
            float x = (useL ? inL[i] : 0.0f) * wl + (useR ? inR[i] : 0.0f) * (useL && useR ? 0.5f : 1.0f);
            pk = std::max (pk, std::abs (x));
            // HPF only when the voice chain is on
            const float h = hpf2.process (hpf1.process (x));
            x = x + (h - x) * voiceMix;
            // NOISE TRACKER: the quietest moment of the last ~1.6 s (gaps between words) = room noise
            const float a = std::abs (x);
            lvlEnv = a > lvlEnv ? a + (lvlEnv - a) * cNoiseA : a + (lvlEnv - a) * cNoiseR;
            segCur = std::min (segCur, lvlEnv);
            if (++segCount >= segLen)
            {
                segMin[segPos] = segCur; segPos = (segPos + 1) & 7; segCur = 1.0f; segCount = 0;
                float mn = 1.0f; for (float v : segMin) mn = std::min (mn, v);
                noiseFloor = std::clamp (mn, 3.0e-5f, 0.006f);
            }
            // GATE (downward expander 1:5): threshold 13 dB over the room noise, never below -62 dBFS
            gateEnv = a > gateEnv ? a + (gateEnv - a) * cGateA : a + (gateEnv - a) * cGateR;
            const float thr = std::max (dbToGain (-62.0f), noiseFloor * 4.5f);   // +13 dB over the room noise
            if (gateEnv >= thr) gateHold = holdLen; else if (gateHold > 0) --gateHold;   // 120 ms hold keeps word endings
            float target = (gateEnv >= thr || gateHold > 0) ? 1.0f : std::max (0.003f, std::pow (gateEnv / thr, 4.0f));
            target = 1.0f + (target - 1.0f) * voiceMix;
            gateGain = target > gateGain ? target + (gateGain - target) * cGgA : target + (gateGain - target) * cGgR;
            x *= gateGain;
            gateBuf[(size_t) i] = gateGain;
            // AUTO LEVEL: measure only while the voice is clearly present, move slowly (max ~3 dB/s)
            if (gateEnv > thr * 6.0f)
            {
                agcEnv += (x * x - agcEnv) * 0.0005f;
                const float rmsDb = 10.0f * std::log10 (agcEnv + 1e-12f) + agcDb;
                const float want = std::clamp (-20.0f - (rmsDb - agcDb), -6.0f, 12.0f);
                agcDb += (want - agcDb) * (float) (3.0 / fs / 6.0);
            }
            x *= dbToGain (agcDb * voiceMix);
            mono[(size_t) i] = x;
        }
        peakIn = std::max (peakIn, pk);

        // ---- PITCH (glides so the knob can be played) ----
        const float gl = coefMs (60.0f, fs / n);
        pitchSm = cur.pitch + (pitchSm - cur.pitch) * gl;
        formantSm = cur.formant + (formantSm - cur.formant) * gl;
        main.setShift (pitchSm, formantSm);
        main.process (mono.data(), pitched.data(), n);
        {
            // keep the ORIGINAL highs (sibilance, air) when the voice is made lower/higher: only the body is shifted
            const float lat = (float) main.latency();
            for (int i = 0; i < n; ++i)
            {
                dryDl.push (mono[(size_t) i]);
                const float hiOrig = xHp2.process (xHp1.process (dryDl.read (lat)));
                const float loShift = xLp2.process (xLp1.process (pitched[(size_t) i]));
                pitched[(size_t) i] = loShift + hiOrig;   // LR4: low + high sum flat (in phase)
            }
        }

        // ---- BRUTAL layer (parallel, same latency as the main path) ----
        const bool demonActive = cur.sfxOn && cur.sfxType == 3;
        const float demonTarget = demonActive ? cur.sfxAmount : 0.0f;
        demonMix = demonTarget + (demonMix - demonTarget) * coefMs (40.0f, fs / n);
        if (demonMix > 0.001f || demonActive)
        {
            demon.setShift (pitchSm - 12.0f, formantSm - 3.0f - 3.0f * demonMix);
            demon.process (mono.data(), demonBuf.data(), n);
            const float drive = 1.5f + 5.0f * demonMix;
            for (int i = 0; i < n; ++i)
            {
                const float d = demonLp.process (std::tanh (demonBuf[(size_t) i] * drive) / std::tanh (drive) * 0.9f);
                pitched[(size_t) i] += d * demonMix * 1.3f;
            }
        }

        // ---- DIRECT PATH: no pitch change -> skip the shifter (no latency). Hold 300 ms so it does not flap. ----
        {
            const bool need = std::abs (cur.pitch) > 0.005f || std::abs (cur.formant) > 0.005f || std::abs (pitchSm) > 0.01f
                              || std::abs (formantSm) > 0.01f || demonActive || demonMix > 0.0005f;
            if (need) directHold = 0.3f; else directHold = std::max (0.0f, directHold - (float) n / (float) fs);
            const float dT = (need || directHold > 0.0f) ? 0.0f : 1.0f;
            const float cf = coefMs (25.0f, fs);
            for (int i = 0; i < n; ++i)
            {
                directMix = dT + (directMix - dT) * cf;
                pitched[(size_t) i] = pitched[(size_t) i] * (1.0f - directMix) + mono[(size_t) i] * directMix;
            }
            pitchPathActive.store (directMix < 0.5f);
        }

        // ---- VOICE CHAIN ----
        const float vTarget = cur.voiceOn ? 1.0f : 0.0f;
        const float thrDb = -10.0f + (m.thrDb + 10.0f) * s, ratio = 1.0f + (m.ratio - 1.0f) * s;
        const float makeup = dbToGain ((-thrDb) * (1.0f - 1.0f / ratio) * 0.5f);
        const float aC = coefMs (3.0f, fs), rC = coefMs (130.0f, fs);
        const float aD = coefMs (1.0f, fs), rD = coefMs (60.0f, fs);
        const float warm = m.warmth * s;
        float grMax = 0;
        for (int i = 0; i < n; ++i)
        {
            const float vm = voiceMix = vTarget + (voiceMix - vTarget) * 0.9995f;
            const float dry = pitched[(size_t) i];
            float x = mud.process (dry);
            // compressor (peak env, soft knee)
            const float a = std::abs (x);
            compEnv = a > compEnv ? a + (compEnv - a) * aC : a + (compEnv - a) * rC;
            const float envDb = gainToDb (compEnv);
            float over = envDb - thrDb, grDb = 0;
            if (over > -3.0f) grDb = over < 3.0f ? (1.0f / ratio - 1.0f) * (over + 3.0f) * (over + 3.0f) / 12.0f : (1.0f / ratio - 1.0f) * over;
            grMax = std::min (grMax, grDb);
            x *= dbToGain (grDb) * makeup;
            x = air.process (pres.process (body.process (x)));
            // de-esser: reduce only the highs when the sibilant band is loud
            const float det = std::abs (deessDet.process (x));
            deessEnv = det > deessEnv ? det + (deessEnv - det) * aD : det + (deessEnv - det) * rD;
            const float lvl = std::abs (x) * 0.5f + 1e-6f;
            float red = deessEnv > lvl ? std::pow (lvl / deessEnv, 0.8f * m.deess * s) : 1.0f;
            const float hi = deessSplit.process (x);
            x = (x - hi) + hi * red;
            // warmth
            if (warm > 0.001f)
            {
                const float dr = 1.0f + warm * 3.0f;
                x = x * (1.0f - warm) + std::tanh (x * dr) / dr * 1.15f * warm;
            }
            pitched[(size_t) i] = dry + (x - dry) * vm;
        }
        gainReduction.store (-grMax * voiceMix);

        // ---- NOISE DUCK after the compressor: the same gate again, so pauses end up quieter than the input ----
        {
            const float lat = (1.0f - directMix) * (float) main.latency();
            for (int i = 0; i < n; ++i)
            {
                gateDl.push (gateBuf[(size_t) i]);
                const float gg = lat < 1.0f ? gateBuf[(size_t) i] : gateDl.read (std::max (1.0f, lat));
                pitched[(size_t) i] *= 1.0f + (gg - 1.0f) * voiceMix;
            }
        }

        // ---- SFX (stereo) ----
        const float sfxTarget = cur.sfxOn ? 1.0f : 0.0f;
        const float amt = cur.sfxAmount;
        const int type = std::clamp (cur.sfxType, 0, 3);
        if (type == 1 && (cur.sfxOn || sfxMix > 0.001f))
        {
            // SHIMMER: octave-up pitch of (voice + reverb feedback) into a long reverb
            for (int i = 0; i < n; ++i) shimIn[(size_t) i] = shimHp.process (pitched[(size_t) i] * 0.6f + (shimFbL + shimFbR) * 0.5f * (0.35f + 0.4f * amt));
            shimmerPs.setShift (12.0f, 0.0f);
            shimmerPs.process (shimIn.data(), shimOut.data(), n);
        }
        float act = 0;
        const float out = dbToGain (cur.outDb);
        const float mT = cur.mute ? 0.0f : 1.0f;
        float pko = 0;
        for (int i = 0; i < n; ++i)
        {
            sfxMix = sfxTarget + (sfxMix - sfxTarget) * 0.9995f;
            const float v = pitched[(size_t) i];
            float l = v, r = v;
            voiceEnv = std::abs (v) > voiceEnv ? std::abs (v) : voiceEnv * 0.9997f;
            if (sfxMix > 0.0005f)
            {
                float wl2 = 0, wr2 = 0;
                if (type == 0)
                {
                    // WIDE: four slowly drifting copies (8..23 ms), opposite sides, a few cents apart
                    wideDl.push (v);
                    const float depth = (0.25f + 1.1f * amt) * 0.001f * (float) fs;
                    float taps[4];
                    for (int t = 0; t < 4; ++t)
                    {
                        lfoPhase[t] += (0.13f + 0.07f * (float) t) / (float) fs; if (lfoPhase[t] >= 1) lfoPhase[t] -= 1;
                        const float base = (8.0f + 5.0f * (float) t) * 0.001f * (float) fs;
                        taps[t] = wideDl.read (base + depth * (0.5f + 0.5f * std::sin (2 * kPi * lfoPhase[t])));
                    }
                    const float g = 0.55f + 0.5f * amt;
                    wl2 = (taps[0] * 0.9f + taps[2] * 0.6f - taps[3] * 0.25f) * g;
                    wr2 = (taps[1] * 0.9f + taps[3] * 0.6f - taps[2] * 0.25f) * g;
                    l = v * (1.0f - 0.25f * amt) + wl2; r = v * (1.0f - 0.25f * amt) + wr2;
                }
                else if (type == 1)
                {
                    float rl, rr;
                    fdn.process (shimOut[(size_t) i] + v * 0.25f, 2.5f + 5.0f * amt, 0.35f, rl, rr);
                    shimFbL = rl; shimFbR = rr;
                    const float w = 0.25f + 0.75f * amt;
                    wl2 = rl * w; wr2 = rr * w;
                    l = v + wl2; r = v + wr2;
                }
                else if (type == 2)
                {
                    // ECHO THROW: ping-pong, filtered repeats, ducked while the host is talking
                    const float tL = 0.375f * (float) fs, tR = 0.5625f * (float) fs;
                    const float dl = echoL.read (tL), dr = echoR.read (tR);
                    const float fb = 0.35f + 0.37f * amt;
                    echoL.push (echoLpL.process (echoHpL.process (v + dr * fb)));
                    echoR.push (echoLpR.process (echoHpR.process (dl * fb)));
                    const float duck = 1.0f / (1.0f + voiceEnv * 14.0f);
                    const float w = (0.45f + 0.75f * amt) * duck;
                    wl2 = dl * w; wr2 = dr * w;
                    l = v + wl2; r = v + wr2;
                }
                else
                {
                    l = r = v;   // BRUTAL is mixed in before the voice chain
                    wl2 = v * demonMix;
                }
                l = v + (l - v) * sfxMix; r = v + (r - v) * sfxMix;
                act = std::max (act, std::abs (wl2) + std::abs (wr2));
            }
            else
            {
                // keep the SFX lines fed / empty so turning on is clean
                if (type == 0) wideDl.push (v);
                shimFbL *= 0.99f; shimFbR *= 0.99f;
            }
            // CLIP: +8 dB into a smooth tanh, back down (+2 dB make-up) -> quiet parts keep their level, peaks get dense
            const float cT = cur.clipOn ? 1.0f : 0.0f;
            clipMix = cT + (clipMix - cT) * 0.9995f;
            {
                const float drive = 2.5f, back = 1.26f / drive;
                const float cl = clipL.process (l * drive) * back, cr = clipR.process (r * drive) * back;
                if (clipMix > 0.0005f) { l += (cl - l) * clipMix; r += (cr - r) * clipMix; }
            }
            // output gain + mute (smoothed) + limiter (ceiling -1 dBFS)
            muteGain = mT + (muteGain - mT) * 0.998f;
            l *= out * muteGain; r *= out * muteGain;
            const float peak = std::max (std::abs (l), std::abs (r));
            const float ceil = 0.89f;
            const float tg = peak > ceil ? ceil / peak : 1.0f;
            limGain = tg < limGain ? tg : tg + (limGain - tg) * 0.9993f;
            l = softClip (l * limGain); r = softClip (r * limGain);
            outL[i] = l; outR[i] = r;
            pko = std::max (pko, std::max (std::abs (l), std::abs (r)));
        }
        peakOut = std::max (peakOut, pko);
        sfxActivity.store (std::max (sfxActivity.load() * 0.97f, std::min (1.0f, act * 2.0f * sfxMix)));
        if (++meterCount >= 8)
        {
            inLevel.store (std::max (peakIn, inLevel.load() * 0.85f));
            outLevel.store (std::max (peakOut, outLevel.load() * 0.85f));
            peakIn = peakOut = 0; meterCount = 0;
        }
    }
};
} // namespace lv
