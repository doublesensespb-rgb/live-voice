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
    struct Shot { const char* name; int mode; float pitch, formant; bool sfx; int type; float amt; };
    const Shot shots[] = {
        { "ui-default", 1,  0.0f, 0.0f, false, 0, 0.5f },
        { "ui-brutal", 3, -7.0f, -2.0f, true, 3, 0.65f },
        { "ui-club-shimmer", 2, 5.0f, 0.0f, true, 1, 0.8f },
        { "ui-echo", 0, -12.0f, 0.0f, true, 2, 0.4f },
        { "ui-wide", 1, -3.0f, 3.0f, true, 0, 0.55f },
        { "ui-save", 1, -4.0f, -1.0f, true, 3, 0.55f },
    };
    for (auto& s : shots)
    {
        LiveVoiceProcessor proc;
        setP (proc, ParamID::mode, (float) s.mode); setP (proc, ParamID::formant, s.pitch);
        setP (proc, ParamID::sfxOn, s.sfx ? 1.0f : 0.0f); setP (proc, ParamID::sfxType, (float) s.type); setP (proc, ParamID::sfxAmount, s.amt);
        if (String (s.name) == "ui-brutal") setP (proc, ParamID::clipOn, 1.0f);
        proc.engine.inLevel = 0.35f; proc.engine.outLevel = 0.6f; proc.engine.sfxActivity = s.sfx ? 0.6f : 0.0f;
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        auto* e = dynamic_cast<LiveVoiceEditor*> (ed.get());
        proc.presetName = String (s.name) == "ui-brutal" || String (s.name) == "ui-save" ? "Brutal Announcer" : String (s.name) == "ui-default" ? "INIT" : "Radio Night";
        if (String (s.name) == "ui-save") e->openSaveForSnapshot();
        for (int i = 0; i < 12; ++i) e->tick();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        FileOutputStream os (dir.getChildFile (String (s.name) + ".png"));
        os.setPosition (0); os.truncate();
        PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s\n", dir.getChildFile (String (s.name) + ".png").getFullPathName().toRawUTF8());
    }
    return 0;
}
