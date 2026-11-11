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
    struct Shot { const char* name; int voice; float knob; bool hidden; };
    const Shot shots[] = { { "ui-default", 1, 0.0f, false }, { "ui-trailer", 8, -3.0f, false }, { "ui-big-host", 5, -3.0f, false },
                           { "ui-titan-played", 7, -4.5f, false }, { "ui-hype", 9, 0.0f, false }, { "ui-hidden", 4, -2.0f, true }, { "ui-hidden-changed", 0, -4.0f, true } };
    for (auto& s : shots)
    {
        LiveVoiceProcessor proc;
        const auto& v = kVoices[s.voice];
        setP (proc, ParamID::voice, (float) s.voice); setP (proc, ParamID::mode, (float) v.mode); setP (proc, ParamID::strength, v.strength);
        setP (proc, ParamID::formant, s.knob); setP (proc, ParamID::clipOn, v.clip ? 1.0f : 0.0f);
        for (int k = 0; k < 4; ++k) { setP (proc, kFxOn[k], v.fx[k] > 0 ? 1.0f : 0.0f); if (v.fx[k] > 0) setP (proc, kFxAmt[k], v.fx[k]); }
        proc.apvts.state.setProperty ("showControls", ! s.hidden, nullptr);
        proc.engine.inLevel = 0.35f; proc.engine.outLevel = 0.6f; proc.engine.sfxActivity = 0.6f;
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
