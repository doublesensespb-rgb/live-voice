// Renders the editor to PNG without audio hardware (CI preview + local design previews).
// Usage: Snapshot <outdir>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

using namespace juce;

static void setP (LiveVoiceProcessor& p, const char* id, float v)
{
    auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v));
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File dir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    dir.createDirectory();
    struct Shot { const char* name; int voice; float knob; };
    const Shot shots[] = { { "ui-default", 1, 0.0f }, { "ui-trailer", 8, -5.0f }, { "ui-big-host", 5, -6.0f }, { "ui-titan-played", 7, -8.5f }, { "ui-hype", 9, 0.0f } };
    for (auto& s : shots)
    {
        LiveVoiceProcessor proc;
        const auto& v = kVoices[s.voice];
        setP (proc, ParamID::voice, (float) s.voice); setP (proc, ParamID::mode, (float) v.mode); setP (proc, ParamID::strength, v.strength);
        setP (proc, ParamID::formant, s.knob); setP (proc, ParamID::clipOn, v.clip ? 1.0f : 0.0f);
        setP (proc, ParamID::sfxOn, v.sfx >= 0 ? 1.0f : 0.0f); if (v.sfx >= 0) { setP (proc, ParamID::sfxType, (float) v.sfx); setP (proc, ParamID::sfxAmount, v.sfxAmount); }
        proc.engine.inLevel = 0.35f; proc.engine.outLevel = 0.6f; proc.engine.sfxActivity = v.sfx >= 0 ? 0.6f : 0.0f;
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        auto* e = dynamic_cast<LiveVoiceEditor*> (ed.get());
        for (int i = 0; i < 12; ++i) e->tick();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = dir.getChildFile (String (s.name) + ".png");
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}
