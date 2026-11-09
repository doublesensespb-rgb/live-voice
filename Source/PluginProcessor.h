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
}

// 0.12 simple UI: one VOICE PRESET tile = finished "commercial" sound: character + how much lower + clip (+ optional SFX).
struct VoicePreset { const char* name; const char* hint; int mode; float strength; float pitch; bool clip; int sfx; float sfxAmount; };   // sfx -1 = none
inline constexpr int kNumVoices = 10;
inline const VoicePreset kVoices[kNumVoices] =
{
    { "CLEAN",    "natural, just better",   0, 0.60f,   0.0f, false, -1, 0.50f },
    { "RADIO",    "dense FM host",          1, 0.85f,   0.0f, true,  -1, 0.50f },
    { "CLUB MC",  "cuts through music",     2, 0.85f,   0.0f, true,  -1, 0.50f },
    { "WARM",     "close & velvet",         3, 0.70f,  -2.0f, false, -1, 0.50f },
    { "DEEP",     "deep announcer",         3, 0.80f,  -4.0f, true,  -1, 0.50f },
    { "BIG HOST", "huge stadium voice",     1, 0.90f,  -6.0f, true,   0, 0.35f },
    { "GIANT",    "heavy & dark",           3, 0.85f,  -9.0f, true,  -1, 0.50f },
    { "TITAN",    "lowest, massive",        1, 0.90f, -12.0f, true,  -1, 0.50f },
    { "TRAILER",  "movie trailer voice",    1, 0.90f,  -5.0f, true,   3, 0.55f },
    { "HYPE MC",  "club host + echo",       2, 0.85f,   0.0f, true,   2, 0.40f },
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
    std::atomic<float>* p[11] {};
    std::vector<float> inCopy;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveVoiceProcessor)
};
