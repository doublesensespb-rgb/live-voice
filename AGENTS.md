# LIVE VOICE (repo: live-voice) — guide for AI agents and developers

Read this whole file before changing anything. Current version: **0.16** (`set(LIVEVOICE_LABEL "0.16")` + `project(... VERSION 0.1.6)` in CMakeLists.txt).

## 1. Product

LIVE VOICE is a **Windows standalone app** (JUCE "Standalone" plugin format with a custom app class) that processes a
microphone live for a DJ / event host. It is a client commission of Andrey Ulyanov (artist name Thematic, manufacturer
"Thematic"), a producer and game sound designer. The client is NOT technical: everything must work by double-clicking the .exe.

Signal flow (all in `Source/VoiceDSP.h`, header-only, no JUCE, tested by `test/dsp_test.cpp`):

```
mic (L/R, picks the channel that carries signal) -> HPF -> GATE (threshold = room noise (rolling min over 1.6 s) +13 dB, 1:5,
  120 ms hold) -> AUTO LEVEL (slow, only while talking, -6..+12 dB)
  -> PITCH (Signalsmith Stretch, 30 ms block; above 4.5 kHz the ORIGINAL voice, LR4, latency-aligned) ─┐  crossfade to the DIRECT path (0 ms) when pitch = formant = 0
  -> BRUTAL mass layer (parallel shifter: same note, formants 5-7 lower, own 4:1 comp, same latency) ┘  and BRUTAL is off (300 ms hold, 25 ms fade)
  -> VOICE CHAIN: mud cut -> compressor -> body/presence/air -> de-esser -> warmth   (mode x STRENGTH)
  -> NOISE DUCK: the gate gain again (delayed by the path latency), so pauses end quieter than the input
  -> BRUTAL grit (harmonics-only parallel tube 150 Hz-2.5 kHz) + chest shelf, inside the voice chain
  -> EFFECTS together: WIDE (side-only doubler: two drifting copies, difference to L/-R + small centre copy; mono sum
     changes < 0.2 dB) + ECHO (ducked ping-pong) + SHIMMER (+12 st shifter into an 8-line FDN)
  -> CLIP (button, param `clipOn`: +8 dB into tanh with ADAA, -8+2 dB back, level-matched, 0 latency)
  -> OUTPUT gain -> MUTE (smoothed) -> limiter (-1 dBFS) + soft clip above 0.8
```
Audio is processed in 32-sample chunks. Modes: NATURAL, RADIO, CLUB, DEEP (`kModes`). SFX names in `kSfx`.

## 2. Owner's rules (important)
- The owner does not code. The agent does everything: code, build, check, deliver. Reply in Russian (he often writes with typos).
- When he sends change requests, FIRST agree on the changes in text, THEN implement. Visual change -> render a preview
  with the Snapshot tool and show it BEFORE building.
- Builds: the workflow runs on every push to `main` (and by hand). So push to `main` only after the owner approved and said
  to build; work on a branch before that.
- Version naming: 0.1, then 0.11, 0.12 ... Deliver files with the version in the name: `LIVE-VOICE-0.11-Windows.zip`.
- Always give links. **After EVERY build, without being asked:** send the versioned zip, the links (builds branch, README-RU.md,
  AGENTS.md, commit history) and a fresh offline archive `LIVE-VOICE-<version>-offline.zip` (clone of main with history +
  JUCE 8.0.10 + the build + handbook) so he can continue without the agent.
- Design = GRAIN FORM's **GRAPHITE** skin: dark graphite, warm white ink, copper accent `#d48c5c`, Inter font (embedded, OFL),
  flat knobs. The client is a serious person: icons are strict and geometric (no faces, no demons, no cartoon stars).
  NO violet, NO pink, NO neon.
- Latency matters a lot (live use). Never add processing that adds latency to the direct path.
- Never rename parameter IDs (`ParamID` in PluginProcessor.h): presets and the saved app state use them.
  NOTE: the UI "PITCH" knob = parameter `formant`; parameter `pitch` is the legacy note shift (ignored).

## 3. Code map
| File | What |
|---|---|
| `Source/VoiceDSP.h` | All DSP (Biquad, DelayLine, PitchShifter wrapper, FDN reverb, `VoiceEngine`), meters as atomics |
| `Source/PluginProcessor.*` | APVTS parameters, `kVoices` (the 10 preset tiles: mode, strength, pitch, clip, sfx), UndoManager + file-preset code (not used by the 0.12 UI, kept; factory files are no longer written) |
| `Source/PluginEditor.*` | UI 980x560 (0.13: presets + one PITCH/EFFECTS module that can be hidden): `LiveVoiceLook`, `Lamp`, `LevelMeter`, `VoiceTile` (10 presets from `kVoices` in PluginProcessor.h), `SfxButton` + `drawSfxIcon`, `LiveVoiceEditor` (CLIP/ON/AUDIO/MUTE, PITCH knob, effect buttons + amount bar, keys) |
| `Source/StandaloneApp.cpp` | Custom standalone app: mic never muted, audio window on first run, prefers "Windows Audio (Low Latency Mode)", settings in AppData/Roaming/LIVE VOICE |
| `tools/Snapshot.cpp` | Renders UI PNGs (CI + local previews) |
| `test/dsp_test.cpp` | DSP tests: NaN, clipping, latency (direct ~0, pitch ~30 ms), noise floor, pitch accuracy, live knob sweep clicks, CPU |
| `third_party/` | Signalsmith Stretch 1.3.2 + Signalsmith Linear 0.6.6 (MIT), vendored so offline builds work |
| `docs/README-RU.txt` | Client instructions, shipped inside the zip |
| `docs/OFFLINE-RU.txt` | Owner's full offline handbook (use, feedback, latency, offline build, continue with another AI). Update it every version and put a copy in the root of the offline archive |

## 4. Build pipeline
`.github/workflows/build.yml`: `test` (ubuntu, g++ DSP tests) -> `windows` (MSVC: app + Snapshot + DspTest, screenshots,
zip `LIVE-VOICE-<label>-Windows.zip` with the exe + README-RU.txt) -> `publish` (branch `builds`, zip + `shots/*.png`).
JUCE 8.0.10 is cloned by CI. Fetch results: `git clone --depth 1 --branch builds https://github.com/doublesensespb-rgb/live-voice`.
LOOK at the screenshots before delivering.

Local Linux preview (no CI): compile JUCE modules with `-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0` (+ juce_core_CompilationTime.cpp,
juce_graphics_Harfbuzz.cpp, juce_graphics_Sheenbidi.c), make a BinaryData stand-in for the two Inter fonts, compile
PluginProcessor.cpp, PluginEditor.cpp, tools/Snapshot.cpp with `-Ithird_party`, link with freetype, fontconfig, pthread, dl, rt,
run under `Xvfb :99`.

## 5. History
v0.1: first build. 4 voice modes, PITCH ±12 + FORMANT, SFX WIDE/SHIMMER/ECHO THROW/BRUTAL, zero-latency direct path,
auto level, presets (6 factory), UNDO, GRAPHITE design.
v0.11: the big PITCH knob now drives the parameter `formant` (voice size: the owner found it does the "lower voice" he wants);
the note shift (param `pitch`) is legacy: kept for old states, forced to 0 in processBlock, not in the UI (on speech it barely
changed anything and sounded hissy with the 30 ms block). FORMANT row removed. Factory presets are rewritten when their
`version` differs from the app's label.
v0.12: client asked for a super simple UI: 10 "commercial" VOICE PRESET tiles (param `voice`; a tile writes mode, strength,
formant, clipOn, sfx*), PITCH knob, 4 effect buttons + amount bar, CLIP button (param `clipOn`, ADAA tanh, level matched).
Removed from UI: modes, STRENGTH, SAVE/UNDO/preset bar. Noise fix (owner: presets made the hiss louder): adaptive gate +
post-compressor duck, AGC max +12, less air; test 7 checks pauses are quieter than the input. Highs kept when pitching
(test 8: 8 kHz level at PITCH -6/-12).
v0.13: effects together (params wideOn/wideAmt, shimOn/shimAmt, echoOn/echoAmt, brutalOn/brutalAmt; sfxOn/sfxType/sfxAmount
legacy, ignored), `kVoices[].fx[4]`; mono-safe WIDE (test 9); BRUTAL rebuilt (no octave-down; test 10); PITCH + EFFECTS in one
module `controlsPanel` with HIDE (top-right) / vertical SHOW tab, key H, state property `showControls` (default false =
presets only on first start).
v0.14 (client: presets lowered the voice too hard into the lows): param `formant` range -6..+6; presets WARM -1, DEEP -2,
TRAILER -3, BIG HOST -3, GIANT -4, TITAN -6; DEEP mode body +2.5 dB / mud -3.5 / HPF 80; HPF += 5 Hz per semitone down;
BRUTAL chest +3 dB, mass layer 3..4.5 st lower; tiles show only name + hint (no numbers, no depth bar - client). Keep presets 'commercial'.
v0.15 (owner video: CLEAN sounded lowered, some presets 'overloaded / hum'): ON/OFF = full bypass (`Params::bypass`, raw mic,
0 ms, test 11); the selected tile is re-applied on every start (`applyVoicePreset` in setStateInformation; a PITCH left
turned last time had lowered CLEAN invisibly) and MUTE resets; hidden tab shows a PITCH badge when it differs from the
preset. Softer sound: modes retuned (lower ratios, almost no warmth), comp attack 8 ms, makeup x0.4, AGC max +8, CLIP
+4 dB (was +8) and only in RADIO/CLUB MC/HYPE MC, BRUTAL mass x1.0 / grit x0.6, mud cut -0.5 dB per semitone down.
Measured THD at a loud 220 Hz tone: TRAILER 10.2% -> 2.0%, TITAN 6.0 -> 1.0, DEEP 3.9 -> 0.1. (0.15 was never released.)
v0.16 (client after 0.13: CLEAN / RADIO / CLUB MC / HYPE MC are good; the lowered ones are 'too fierce, like a filter on the
voice', want organic and subtle but audible): modes 0-3 restored to the EXACT 0.13 values (ModeSpec got attackMs / makeupK /
agcMaxDb per mode; CLIP +8 dB again, only on those presets) - measured identical to 0.13. New soft modes 4 HOST LOW and
5 DEEP SOFT for lowered presets; lowering WARM -1, DEEP -1.5, BIG HOST -2, TRAILER -2.5, GIANT -3, TITAN -4; no clip;
BRUTAL 10/15/30 %; presence +0.4 dB per semitone down (clarity band now equal to the good presets instead of -3..-4 dB).
THD of the lowered presets 3-10 % -> 0.1-0.9 %. CLEAN has WIDE 15 %. Knob stays -6..+6.
DO NOT change modes 0-3 or the CLEAN/RADIO/CLUB MC/HYPE MC tiles without the owner asking.

## 6. How it was made
Built by the owner talking to Claude (Anthropic) in a claude.ai session; the owner described, Claude wrote the code,
built in GitHub Actions and delivered. Session: https://claude.ai/code/session_01Hux4b5APwPNLszPe84Rua1 (owner's account only).
JUCE: https://github.com/juce-framework/JUCE/tree/8.0.10 · https://docs.juce.com
