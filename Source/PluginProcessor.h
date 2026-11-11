#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "VoiceDSP.h"

namespace ParamID
{
    // Never rename these: saved settings use them.
    inline constexpr const char* voiceOn   = "voiceOn";
    inline constexpr const char* mode      = "mode";
    inline constexpr const char* strength  = "strength";
    inline constexpr const char* pitch     = "pitch";
    inline constexpr const char* formant   = "formant";
    inline constexpr const char* sfxOn     = "sfxOn";
    inline constexpr const char* sfxType   = "sfxType";
    inline constexpr const char* sfxAmount = "sfxAmount";
    inline constexpr const char* mute      = "mute";
    inline constexpr const char* output    = "output";
    inline constexpr const char* clipOn    = "clipOn";   // 0.12
    inline constexpr const char* voice     = "voice";    // 0.12: the selected VOICE tile (sets mode, strength, pitch, clip)
    // 0.13: effects run together, each with its own switch + amount (sfxOn/sfxType/sfxAmount are legacy, ignored)
    inline constexpr const char* wideOn    = "wideOn";
    inline constexpr const char* wideAmt   = "wideAmt";
    inline constexpr const char* shimOn    = "shimOn";
    inline constexpr const char* shimAmt   = "shimAmt";
    inline constexpr const char* echoOn    = "echoOn";
    inline constexpr const char* echoAmt   = "echoAmt";
    inline constexpr const char* brutalOn  = "brutalOn";
    inline constexpr const char* brutalAmt = "brutalAmt";
}

// effect ids used by the UI / presets (order of the 4 buttons)
inline const char* const kFxOn[4]  = { ParamID::wideOn,  ParamID::shimOn,  ParamID::echoOn,  ParamID::brutalOn };
inline const char* const kFxAmt[4] = { ParamID::wideAmt, ParamID::shimAmt, ParamID::echoAmt, ParamID::brutalAmt };

// VOICE PRESET tile = finished "commercial" sound: character + how much lower + clip + effects (0 = effect off).
// 0.13: a little WIDE almost everywhere (mono-safe), CLEAN has none; BRUTAL only in the lowest voices; never more than 3 effects.
struct VoicePreset { const char* name; const char* hint; int mode; float strength; float pitch; bool clip; float fx[4]; };   // fx: WIDE, SHIMMER, ECHO, BRUTAL
inline constexpr int kNumVoices = 10;
inline const VoicePreset kVoices[kNumVoices] =
{
    { "CLEAN",    "natural, just better",   0, 0.60f,   0.0f, false, { 0.00f, 0.0f, 0.00f, 0.00f } },
    { "RADIO",    "dense FM host",          1, 0.85f,   0.0f, true,  { 0.20f, 0.0f, 0.00f, 0.00f } },
    { "CLUB MC",  "cuts through music",     2, 0.85f,   0.0f, true,  { 0.20f, 0.0f, 0.00f, 0.00f } },
    { "WARM",     "close & velvet",         3, 0.70f,  -1.0f, false, { 0.25f, 0.0f, 0.00f, 0.00f } },
    { "DEEP",     "deep announcer",         3, 0.80f,  -2.0f, true,  { 0.25f, 0.0f, 0.00f, 0.00f } },
    { "BIG HOST", "huge stadium voice",     1, 0.90f,  -3.0f, true,  { 0.30f, 0.0f, 0.00f, 0.00f } },
    { "GIANT",    "heavy & dark",           3, 0.85f,  -4.0f, true,  { 0.20f, 0.0f, 0.00f, 0.25f } },
    { "TITAN",    "lowest, massive",        1, 0.90f,  -6.0f, true,  { 0.20f, 0.0f, 0.00f, 0.35f } },
    { "TRAILER",  "movie trailer voice",    1, 0.90f,  -3.0f, true,  { 0.25f, 0.0f, 0.15f, 0.50f } },
    { "HYPE MC",  "club host + echo",       2, 0.85f,   0.0f, true,  { 0.25f, 0.0f, 0.40f, 0.00f } },
};

class LiveVoiceProcessor final : public juce::AudioProcessor
{
public:
    LiveVoiceProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LIVE VOICE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo;                         // UNDO button, Ctrl+Z / Ctrl+Shift+Z
    juce::AudioProcessorValueTreeState apvts;
    lv::VoiceEngine engine;

    // ---- presets (XML .lvpreset files in Documents/LIVE VOICE/Presets, sub-folders allowed) ----
    static juce::File presetRoot();
    juce::Array<juce::File> listPresets() const;                 // root first, then folders (A-Z)
    juce::StringArray listFolders() const;
    bool savePreset (const juce::String& name, const juce::String& folder);
    bool loadPreset (const juce::File&);
    void initSound();                                            // all parameters to default (one undo step)
    void installFactoryPresets();                                // writes the factory set once (missing files only)
    juce::String presetName { "INIT" };
    juce::File presetFile;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<float>* p[19] {};
    std::vector<float> inCopy;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveVoiceProcessor)
};
