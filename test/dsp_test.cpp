// Plain C++ tests of the voice engine (no JUCE):
//   g++ -std=c++17 -O2 -ISource -Ithird_party test/dsp_test.cpp -o dsptest && ./dsptest
// Checks: no NaN/Inf, output never above 0 dBFS, latency of the direct path = 0, pitch path ~30 ms,
// all modes x all SFX x several sample rates and random block sizes, random parameter jumps, silence stays silent,
// pitch shift really moves the pitch, CPU load.
#include "VoiceDSP.h"
#include <cstdio>
#include <chrono>
#include <random>

static int fails = 0;
#define CHECK(c, ...) do { if (! (c)) { ++fails; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

struct FakeVoice
{
    double sr, ph = 0; long t = 0; float f0 = 120.0f; lv::Biquad f1, f2, f3;
    explicit FakeVoice (double s) : sr (s) { f1.bp (sr, 700, 3); f2.bp (sr, 1500, 4); f3.bp (sr, 2600, 5); }
    float next()
    {
        ph += f0 / sr; float pulse = 0; if (ph >= 1) { ph -= 1; pulse = 1; }
        const float syll = std::sin (lv::kPi * (float) std::fmod (t / sr * 3.0, 1.0));
        ++t;
        return (f1.process (pulse) * 3.0f + f2.process (pulse) * 2.0f + f3.process (pulse) * 1.0f) * syll * 1.2f;
    }
};

// rough pitch estimate by autocorrelation (60..600 Hz)
static float estimateF0 (const std::vector<float>& x, double sr)
{
    const int n = (int) x.size(); float best = 0; int bestLag = 0;
    for (int lag = (int) (sr / 600); lag < (int) (sr / 60); ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int i = 0; i + lag < n; ++i) { s += x[i] * x[i + lag]; e1 += x[i] * x[i]; e2 += x[i + lag] * x[i + lag]; }
        const float c = (float) (s / std::sqrt (e1 * e2 + 1e-12));
        if (c > best) { best = c; bestLag = lag; }
    }
    // prefer the shortest lag whose correlation is close to the best (avoid octave/sub-harmonic errors)
    for (int lag = (int) (sr / 600); lag < bestLag; ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int i = 0; i + lag < n; ++i) { s += x[i] * x[i + lag]; e1 += x[i] * x[i]; e2 += x[i + lag] * x[i + lag]; }
        if (s / std::sqrt (e1 * e2 + 1e-12) > best * 0.9) { std::printf ("    (lag %d corr %.2f vs best lag %d %.2f)\n", lag, s / std::sqrt (e1 * e2 + 1e-12), bestLag, best); break; }
    }
    return bestLag > 0 ? (float) (sr / bestLag) : 0.0f;
}

int main()
{
    std::mt19937 rng (7);
    // ---- 1. all modes x all SFX x sample rates x random block sizes ----
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        lv::VoiceEngine e; e.prepare (sr, 2048);
        FakeVoice v (sr);
        std::vector<float> in (2048), l (2048), r (2048);
        for (int cfg = 0; cfg < 24; ++cfg)
        {
            lv::Params p; p.mode = cfg % 4; p.pitch = (float) ((cfg * 5) % 25 - 12); p.formant = (float) ((cfg * 3) % 9 - 4);
            p.sfxOn = cfg >= 4; p.sfxType = (cfg / 4) % 4; p.sfxAmount = (float) (cfg % 5) / 4.0f; p.strength = (float) (cfg % 3) / 2.0f;
            e.setParams (p);
            double peak = 0, sum = 0; bool bad = false; long total = 0;
            auto t0 = std::chrono::steady_clock::now();
            while (total < (long) (sr * 1.5))
            {
                const int blk = 1 + (int) (rng() % 2048);
                for (int i = 0; i < blk; ++i) in[i] = v.next();
                e.process (in.data(), nullptr, l.data(), r.data(), blk);
                for (int i = 0; i < blk; ++i)
                {
                    if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) bad = true;
                    peak = std::max (peak, (double) std::max (std::abs (l[i]), std::abs (r[i])));
                    sum += l[i] * l[i];
                }
                total += blk;
            }
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            const double cpu = ms / 1500.0 * 100.0;
            if (cfg < 8 || bad || peak > 1.0)
                std::printf ("sr %.0f cfg %2d %-7s %-10s pitch %+5.1f: peak %.3f rms %.3f cpu %.1f%%\n", sr, cfg, lv::kModes[p.mode].name,
                             p.sfxOn ? lv::kSfx[p.sfxType].name : "-", p.pitch, peak, std::sqrt (sum / total), cpu);
            CHECK (! bad, "NaN/Inf at sr %.0f cfg %d", sr, cfg);
            CHECK (peak <= 1.0, "clipping %.3f at sr %.0f cfg %d", peak, sr, cfg);
            CHECK (std::sqrt (sum / total) > 0.01, "output too quiet at sr %.0f cfg %d", sr, cfg);
            CHECK (cpu < 25.0, "cpu %.1f%% at sr %.0f cfg %d", cpu, sr, cfg);
        }
    }

    // ---- 2. latency: impulse through the direct path and through the pitch path ----
    for (int pathPitch = 0; pathPitch < 2; ++pathPitch)
    {
        const double sr = 48000; lv::VoiceEngine e; e.prepare (sr, 256);
        lv::Params p; p.voiceOn = false; p.pitch = pathPitch ? 0.01f : 0.0f; e.setParams (p); e.reset();
        std::vector<float> in (256, 0.0f), l (256), r (256);
        // let the path settle (direct-path hold / crossfade)
        for (int b = 0; b < 200; ++b) e.process (in.data(), nullptr, l.data(), r.data(), 256);
        int found = -1; float best = 0;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 256; ++i) { const long t = (long) b * 256 + i; in[i] = (t % 4800 < 2400) ? std::sin (t * 0.05f) * 0.5f * (t < 2400 ? 1.0f : 0.0f) : 0.0f; }
            e.process (in.data(), nullptr, l.data(), r.data(), 256);
            for (int i = 0; i < 256; ++i) if (std::abs (l[i]) > 0.05f && found < 0) found = b * 256 + i;
            (void) best;
        }
        std::printf ("%s path: first output after %d samples (%.1f ms)\n", pathPitch ? "pitch" : "direct", found, found * 1000.0 / sr);
        if (pathPitch) CHECK (found > 0 && found < sr * 0.040, "pitch path latency %d", found);
        else CHECK (found >= 0 && found < 8, "direct path latency %d (must be ~0)", found);
    }

    // ---- 3. silence in -> silence out (no hiss from auto level / effects) ----
    {
        const double sr = 48000; lv::VoiceEngine e; e.prepare (sr, 512);
        lv::Params p; p.sfxOn = true; p.sfxType = 1; p.sfxAmount = 1.0f; e.setParams (p);
        std::vector<float> in (512), l (512), r (512);
        std::mt19937 nr (3); std::normal_distribution<float> nd (0.0f, 1.0f);
        double peak = 0;
        for (int b = 0; b < 400; ++b)
        {
            for (auto& x : in) x = nd (nr) * 0.0003f;   // -70 dBFS mic hiss
            e.process (in.data(), nullptr, l.data(), r.data(), 512);
            if (b > 100) for (int i = 0; i < 512; ++i) peak = std::max (peak, (double) std::abs (l[i]));
        }
        std::printf ("mic hiss -70 dBFS -> output peak %.1f dBFS\n", 20 * std::log10 (peak + 1e-12));
        CHECK (peak < 0.003, "noise floor raised to %.4f", peak);
    }

    // ---- 4. pitch really moves: a 300 Hz tone must come out at 300 * 2^(st/12) ----
    for (float st : { -12.0f, -7.0f, -5.0f, 5.0f, 12.0f })
    {
        const double sr = 48000; lv::VoiceEngine e; e.prepare (sr, 256);
        lv::Params p; p.voiceOn = false; p.pitch = st; e.setParams (p); e.reset();
        std::vector<float> in (256), l (256), r (256), cap;
        long t = 0;
        for (int b = 0; b < 300; ++b)
        {
            for (int i = 0; i < 256; ++i, ++t) in[i] = 0.4f * std::sin (2.0f * lv::kPi * 300.0f * (float) t / (float) sr);
            e.process (in.data(), nullptr, l.data(), r.data(), 256);
            if (b > 150 && cap.size() < 4800) cap.insert (cap.end(), l.begin(), l.end());
        }
        float bestM = 0, bestF = 0;
        for (float f = 60; f < 1400; f += 0.5f)
        {
            double re = 0, im = 0; const int n = (int) cap.size();
            for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / n); re += cap[i] * w * std::cos (2 * M_PI * f * i / sr); im += cap[i] * w * std::sin (2 * M_PI * f * i / sr); }
            if (re * re + im * im > bestM) { bestM = (float) (re * re + im * im); bestF = f; }
        }
        const float want = 300.0f * std::pow (2.0f, st / 12.0f);
        std::printf ("pitch %+5.1f st: 300 Hz -> %.1f Hz (want %.1f)\n", st, bestF, want);
        CHECK (std::abs (bestF / want - 1.0f) < 0.03f, "pitch %+.1f gave %.1f Hz", st, bestF);
    }

    // ---- 5. live knob play: sweep pitch every block, mute, mode and sfx switching ----
    {
        const double sr = 48000; lv::VoiceEngine e; e.prepare (sr, 128);
        FakeVoice v (sr); std::vector<float> in (128), l (128), r (128);
        float prev = 0, maxJump = 0; bool bad = false;
        for (int b = 0; b < 3000; ++b)
        {
            lv::Params p; p.pitch = 12.0f * std::sin (b * 0.01f); p.mode = (b / 300) % 4; p.sfxOn = (b / 500) % 2; p.sfxType = (b / 1000) % 4;
            p.mute = (b / 700) % 5 == 4; p.voiceOn = (b / 900) % 3 != 2; e.setParams (p);
            for (int i = 0; i < 128; ++i) in[i] = v.next();
            e.process (in.data(), nullptr, l.data(), r.data(), 128);
            for (int i = 0; i < 128; ++i) { if (! std::isfinite (l[i])) bad = true; maxJump = std::max (maxJump, std::abs (l[i] - prev)); prev = l[i]; }
        }
        std::printf ("live sweep: max sample jump %.3f\n", maxJump);
        CHECK (! bad, "NaN in live sweep");
        CHECK (maxJump < 0.9f, "click: jump %.3f", maxJump);
    }

    // ---- 6. CLIP button: level-matched (+-3 dB rms), lower crest factor, no latency, no NaN ----
    {
        const double sr = 48000;
        double rms[2] = {}, crest[2] = {};
        for (int c = 0; c < 2; ++c)
        {
            lv::VoiceEngine e; e.prepare (sr, 256);
            lv::Params p; p.clipOn = c == 1; e.setParams (p); e.reset();
            FakeVoice v (sr); std::vector<float> in (256), l (256), r (256);
            double sum = 0, pk = 0; long n = 0; bool bad = false;
            for (int b = 0; b < 600; ++b)
            {
                for (int i = 0; i < 256; ++i) in[i] = v.next();
                e.process (in.data(), nullptr, l.data(), r.data(), 256);
                if (b > 200) for (int i = 0; i < 256; ++i) { if (! std::isfinite (l[i])) bad = true; sum += l[i] * l[i]; pk = std::max (pk, (double) std::abs (l[i])); ++n; }
            }
            rms[c] = std::sqrt (sum / n); crest[c] = pk / rms[c];
            CHECK (! bad, "NaN with clip %d", c);
            CHECK (pk < 1.0, "clip %d peak %.3f", c, pk);
        }
        const double diff = 20 * std::log10 (rms[1] / rms[0]);
        std::printf ("CLIP: rms change %+.1f dB, crest %.2f -> %.2f\n", diff, crest[0], crest[1]);
        CHECK (std::abs (diff) < 3.0, "clip changes level by %.1f dB", diff);
        CHECK (crest[1] < crest[0], "clip does not reduce peaks");
        // direct path latency with CLIP on
        lv::VoiceEngine e; e.prepare (sr, 256); lv::Params p; p.voiceOn = false; p.clipOn = true; e.setParams (p); e.reset();
        std::vector<float> in (256, 0.0f), l (256), r (256);
        for (int b = 0; b < 100; ++b) e.process (in.data(), nullptr, l.data(), r.data(), 256);
        in[0] = 0.5f; e.process (in.data(), nullptr, l.data(), r.data(), 256);
        int first = -1; for (int i = 0; i < 256; ++i) if (std::abs (l[i]) > 0.05f) { first = i; break; }
        std::printf ("CLIP direct path: impulse after %d samples\n", first);
        CHECK (first >= 0 && first <= 2, "clip adds latency (%d)", first);
    }

    std::printf (fails ? "\n%d CHECK(S) FAILED\n" : "\nALL TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
