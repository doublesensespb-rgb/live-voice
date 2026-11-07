# LIVE VOICE (repo: live-voice) — guide for AI agents and developers

Read this whole file before changing anything. Current version: **0.11** (`set(LIVEVOICE_LABEL "0.11")` + `project(... VERSION 0.1.1)` in CMakeLists.txt).

## 1. Product

LIVE VOICE is a **Windows standalone app** (JUCE "Standalone" plugin format with a custom app class) that processes a
microphone live for a DJ / event host. It is a client commission of Andrey Ulyanov (artist name Thematic, manufacturer
"Thematic"), a producer and game sound designer. The client is NOT technical: everything must work by double-clicking the .exe.

Signal flow (all in `Source/VoiceDSP.h`, header-only, no JUCE, tested by `test/dsp_test.cpp`):

```
mic (L/R, picks the channel that carries signal) -> HPF -> soft GATE -> AUTO LEVEL (slow, only while talking, -6..+18 dB)
  -> PITCH (Signalsmith Stretch, 30 ms block, formants kept)  ─┐  crossfade to the DIRECT path (0 ms) when pitch = formant = 0
  -> BRUTAL layer (parallel shifter at pitch-12, same latency) ┘  and BRUTAL is off (300 ms hold, 25 ms fade)
  -> VOICE CHAIN: mud cut -> compressor -> body/presence/air -> de-esser -> warmth   (mode x STRENGTH)
  -> SFX: WIDE (4-tap modulated doubler) | SHIMMER (+12 st shifter into an 8-line FDN) | ECHO THROW (ducked ping-pong) | BRUTAL
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
| `Source/PluginProcessor.*` | APVTS parameters, UndoManager, presets (`.lvpreset` XML in Documents/LIVE VOICE/Presets, folders, factory set written once into `Factory/`) |
| `Source/PluginEditor.*` | UI 1060x586: `LiveVoiceLook`, `Lamp`, `LevelMeter`, `SfxScreen` (icons), `SaveOverlay`, `LiveVoiceEditor` (preset bar, undo, shortcuts) |
| `Source/StandaloneApp.cpp` | Custom standalone app: mic never muted, audio window on first run, prefers "Windows Audio (Low Latency Mode)", settings in AppData/Roaming/LIVE VOICE |
| `tools/Snapshot.cpp` | Renders UI PNGs (CI + local previews) |
| `test/dsp_test.cpp` | DSP tests: NaN, clipping, latency (direct ~0, pitch ~30 ms), noise floor, pitch accuracy, live knob sweep clicks, CPU |
| `third_party/` | Signalsmith Stretch 1.3.2 + Signalsmith Linear 0.6.6 (MIT), vendored so offline builds work |
| `docs/README-RU.txt` | Client instructions, shipped inside the zip |

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

## 6. How it was made
Built by the owner talking to Claude (Anthropic) in a claude.ai session; the owner described, Claude wrote the code,
built in GitHub Actions and delivered. Session: https://claude.ai/code/session_01Hux4b5APwPNLszPe84Rua1 (owner's account only).
JUCE: https://github.com/juce-framework/JUCE/tree/8.0.10 · https://docs.juce.com
