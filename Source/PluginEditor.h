#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// GRAPHITE skin, the same as GRAIN FORM 0.92+: dark graphite, warm white ink, copper accent, Inter font.
namespace Palette
{
    inline const juce::Colour bgTop     { 0xff222426 };
    inline const juce::Colour bgBottom  { 0xff1c1e20 };
    inline const juce::Colour panel     { 0xff292b2e };
    inline const juce::Colour screen    { 0xff0e0f10 };
    inline const juce::Colour deep      { 0xff080909 };
    inline const juce::Colour button    { 0xff3a3d41 };
    inline const juce::Colour buttonHi  { 0xff464a4f };
    inline const juce::Colour text      { 0xffece6df };
    inline const juce::Colour textDim   { 0xff9b948c };
    inline const juce::Colour label     { 0xffd2cbc3 };
    inline const juce::Colour knob      { 0xff45484c };
    inline const juce::Colour knobDark  { 0xff2b2d30 };
    inline const juce::Colour track     { 0xffece6df };
    inline const juce::Colour accent    { 0xffd48c5c };
    inline const juce::Colour accentSoft{ 0xffb8774c };
    inline const juce::Colour glow      { 0xfff0b080 };
    inline const juce::Colour ledOff    { 0xff4a4d51 };
    inline const juce::Colour edge      { 0x0fffffff };
    inline const juce::Colour clip      { 0xffe0625a };
}

class LiveVoiceLook final : public juce::LookAndFeel_V4
{
public:
    LiveVoiceLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
};

// round on/off lamp (power)
class Lamp final : public juce::Button
{
public:
    Lamp() : juce::Button ("lamp") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class LevelMeter final : public juce::Component
{
public:
    void setLevel (float v) { if (std::abs (v - level) > 0.002f) { level = v; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float level = 0;
};

// one big VOICE PRESET tile
class VoiceTile final : public juce::Button
{
public:
    explicit VoiceTile (int i) : juce::Button (kVoices[i].name), index (i) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    const int index;
};

// square SFX button with its copper icon
class SfxButton final : public juce::Button
{
public:
    explicit SfxButton (int t) : juce::Button (lv::kSfx[t].name), type (t) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    const int type; float activity = 0;
};

class LiveVoiceEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LiveVoiceEditor (LiveVoiceProcessor&);
    ~LiveVoiceEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void tick() { timerCallback(); }   // used by the Snapshot tool
    static inline std::function<void()> onAudioSettings;   // set by the standalone app (opens the audio device window)

private:
    void timerCallback() override;
    void setParam (const char* id, float value);
    float getParam (const char* id) const;
    void selectVoice (int i);
    void refresh();

    LiveVoiceProcessor& proc;
    LiveVoiceLook look;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    LevelMeter inMeter, outMeter;
    Lamp powerBtn;
    juce::TextButton muteBtn { "MUTE" }, audioBtn { "AUDIO" }, clipBtn { "CLIP" };
    juce::OwnedArray<VoiceTile> tiles;
    juce::Slider pitchKnob;              // PITCH = voice size (parameter "formant")
    juce::OwnedArray<SfxButton> sfxBtns;
    juce::Slider amountBar;

    std::unique_ptr<SA> aPitch, aAmount;
    std::unique_ptr<BA> aMute, aPower, aClip;

    juce::Rectangle<int> voicePanel, pitchPanel, sfxPanel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveVoiceEditor)
};
