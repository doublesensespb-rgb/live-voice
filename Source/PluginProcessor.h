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
}

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
    std::atomic<float>* p[10] {};
    std::vector<float> inCopy;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveVoiceProcessor)
};
