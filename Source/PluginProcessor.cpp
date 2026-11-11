#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

LiveVoiceProcessor::LiveVoiceProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Mic", AudioChannelSet::stereo(), true)
                        .withOutput ("Out", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "LIVEVOICE", createLayout())
{
    const char* ids[] = { ParamID::voiceOn, ParamID::mode, ParamID::strength, ParamID::pitch, ParamID::formant,
                          ParamID::sfxOn, ParamID::sfxType, ParamID::sfxAmount, ParamID::mute, ParamID::output, ParamID::clipOn,
                          ParamID::wideOn, ParamID::wideAmt, ParamID::shimOn, ParamID::shimAmt, ParamID::echoOn, ParamID::echoAmt,
                          ParamID::brutalOn, ParamID::brutalAmt, ParamID::voice };
    for (int i = 0; i < 20; ++i) p[i] = apvts.getRawParameterValue (ids[i]);
}

AudioProcessorValueTreeState::ParameterLayout LiveVoiceProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    StringArray modes, sfx;
    for (auto& m : lv::kModes) modes.add (m.name);
    for (auto& s : lv::kSfx) sfx.add (s.name);
    auto semis = [] (float x, int) { return (x > 0.04f ? "+" : x < -0.04f ? "-" : "") + String (std::abs (x), 1) + " st"; };
    v.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ParamID::voiceOn, 1 }, "Voice", true));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode, 1 }, "Mode", modes, 1));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::strength, 1 }, "Strength", NormalisableRange<float> (0.0f, 1.0f), 0.7f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x * 100)) + " %"; })));
    // 0.11: "pitch" (note shift) is legacy and ignored; the PITCH knob in the UI is the "formant" parameter (voice size)
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::pitch, 1 }, "Note Shift (legacy)", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction (semis)));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::formant, 1 }, "Pitch", NormalisableRange<float> (-6.0f, 6.0f, 0.01f), 0.0f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction (semis)));
    v.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ParamID::sfxOn, 1 }, "SFX", false));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::sfxType, 1 }, "SFX Type", sfx, 0));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::sfxAmount, 1 }, "SFX Amount", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x * 100)) + " %"; })));
    v.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ParamID::mute, 1 }, "Mute", false));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " dB"; })));
    v.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ParamID::clipOn, 1 }, "Clip", true));
    StringArray voices; for (auto& vp : kVoices) voices.add (vp.name);
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x * 100)) + " %"; });
    const char* fxNames[4] = { "Wide", "Shimmer", "Echo", "Brutal" };
    const float fxDef[4] = { 0.20f, 0.40f, 0.40f, 0.50f };
    for (int k = 0; k < 4; ++k)
    {
        v.push_back (std::make_unique<AudioParameterBool>  (ParameterID { kFxOn[k], 1 }, fxNames[k], k == 0));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { kFxAmt[k], 1 }, String (fxNames[k]) + " Amount", NormalisableRange<float> (0.0f, 1.0f), fxDef[k], pct));
    }
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::voice, 1 }, "Voice Preset", voices, 1));
    return { v.begin(), v.end() };
}

bool LiveVoiceProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo()) && out == AudioChannelSet::stereo();
}

void LiveVoiceProcessor::prepareToPlay (double sr, int block)
{
    engine.prepare (sr, jmax (block, 64));
    inCopy.assign ((size_t) jmax (block, 64) * 2 + 64, 0.0f);
    setLatencySamples (engine.latencySamples());
}

void LiveVoiceProcessor::processBlock (AudioBuffer<float>& buf, MidiBuffer&)
{
    ScopedNoDenormals nd;
    lv::Params q;
    q.voiceOn   = true;                     // the voice chain always runs ...
    q.bypass    = p[0]->load() < 0.5f;      // ... and ON/OFF is a full bypass to the raw mic (0.15)
    q.mode      = (int) p[1]->load();
    q.strength  = p[2]->load();
    q.pitch     = 0.0f;   // legacy note shift: removed in 0.11 (sounded hissy on speech), PITCH = voice size (formant)
    q.formant   = p[4]->load();
    q.sfxOn     = p[5]->load() > 0.5f;
    q.sfxType   = (int) p[6]->load();
    q.sfxAmount = p[7]->load();
    q.mute      = p[8]->load() > 0.5f;
    q.outDb     = p[9]->load();
    q.clipOn    = p[10]->load() > 0.5f;
    q.wideOn    = p[11]->load() > 0.5f;  q.wideAmt   = p[12]->load();
    q.shimOn    = p[13]->load() > 0.5f;  q.shimAmt   = p[14]->load();
    q.echoOn    = p[15]->load() > 0.5f;  q.echoAmt   = p[16]->load();
    q.brutalOn  = p[17]->load() > 0.5f;  q.brutalAmt = p[18]->load();
    q.sfxOn     = false;   // legacy single effect
    engine.setParams (q);

    const int n = buf.getNumSamples();
    const int ins = getTotalNumInputChannels();
    // copy the inputs: in/out share the same buffer
    float* l = buf.getWritePointer (0);
    float* r = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : l;
    if ((int) inCopy.size() < n * 2) return;   // host sent a bigger block than announced (never in the app)
    float* tmp = inCopy.data();
    const float* inL = nullptr; const float* inR = nullptr;
    if (ins > 0) { FloatVectorOperations::copy (tmp, buf.getReadPointer (0), n); inL = tmp; }
    if (ins > 1) { FloatVectorOperations::copy (tmp + n, buf.getReadPointer (1), n); inR = tmp + n; }
    engine.process (inL, inR, l, r, n);
}

void LiveVoiceProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("pluginVersion", LIVEVOICE_LABEL, nullptr);
    st.setProperty ("presetName", presetName, nullptr);
    st.setProperty ("presetFile", presetFile.getFullPathName(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void LiveVoiceProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = ValueTree::fromXml (*xml);
            presetName = st.getProperty ("presetName", "INIT").toString();
            const String pf = st.getProperty ("presetFile", "").toString();
            presetFile = pf.isNotEmpty() && File::isAbsolutePath (pf) ? File (pf) : File();
            apvts.replaceState (st);
            // 0.15: what is lit is what you hear - every start re-applies the selected VOICE tile completely
            // (a PITCH knob left turned last time must not secretly lower CLEAN), and never starts muted
            applyVoicePreset (jlimit (0, kNumVoices - 1, (int) p[19]->load()));
            if (auto* m = apvts.getParameter (ParamID::mute)) m->setValueNotifyingHost (0.0f);
            undo.clearUndoHistory();
        }
}

// ============================================================================ presets
namespace
{
    const char* kPresetExt = ".lvpreset";
    const char* kParamIds[] = { ParamID::voiceOn, ParamID::mode, ParamID::strength, ParamID::pitch, ParamID::formant,
                                ParamID::sfxOn, ParamID::sfxType, ParamID::sfxAmount, ParamID::output, ParamID::clipOn };   // mute is never stored
    String safeName (const String& s)
    {
        return File::createLegalFileName (s.trim()).substring (0, 60);
    }
}

File LiveVoiceProcessor::presetRoot()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("LIVE VOICE").getChildFile ("Presets");
}

StringArray LiveVoiceProcessor::listFolders() const
{
    StringArray out;
    for (auto& d : presetRoot().findChildFiles (File::findDirectories, false))
        out.add (d.getFileName());
    out.sortNatural();
    return out;
}

Array<File> LiveVoiceProcessor::listPresets() const
{
    Array<File> out;
    auto root = presetRoot();
    auto add = [&out] (const File& dir)
    {
        auto files = dir.findChildFiles (File::findFiles, false, String ("*") + kPresetExt);
        std::sort (files.begin(), files.end(), [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        out.addArray (files);
    };
    add (root);
    for (auto& f : listFolders()) add (root.getChildFile (f));
    return out;
}

bool LiveVoiceProcessor::savePreset (const String& nameIn, const String& folder)
{
    const String name = safeName (nameIn);
    if (name.isEmpty()) return false;
    auto dir = folder.trim().isEmpty() ? presetRoot() : presetRoot().getChildFile (safeName (folder));
    if (! dir.createDirectory()) return false;
    XmlElement xml ("LIVEVOICEPRESET");
    xml.setAttribute ("version", LIVEVOICE_LABEL);
    xml.setAttribute ("name", name);
    for (auto* id : kParamIds)
        if (auto* prm = apvts.getParameter (id))
            xml.setAttribute (id, prm->convertFrom0to1 (prm->getValue()));
    auto f = dir.getChildFile (name + kPresetExt);
    if (! xml.writeTo (f)) return false;
    presetName = name; presetFile = f;
    return true;
}

bool LiveVoiceProcessor::loadPreset (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("LIVEVOICEPRESET")) return false;
    undo.beginNewTransaction ("Load preset");
    for (auto* id : kParamIds)
        if (auto* prm = apvts.getParameter (id))
        {
            const float def = prm->convertFrom0to1 (prm->getDefaultValue());
            const float v = (float) xml->getDoubleAttribute (id, def);
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));   // no gesture: the whole preset is ONE undo step
        }
    presetName = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
    presetFile = f;
    return true;
}

void LiveVoiceProcessor::applyVoicePreset (int i)
{
    i = jlimit (0, kNumVoices - 1, i);
    const auto& v = kVoices[i];
    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    set (ParamID::voice, (float) i);
    set (ParamID::mode, (float) v.mode);
    set (ParamID::strength, v.strength);
    set (ParamID::formant, v.pitch);
    set (ParamID::clipOn, v.clip ? 1.0f : 0.0f);
    for (int k = 0; k < 4; ++k)
    {
        if (v.fx[k] > 0.0f) set (kFxAmt[k], v.fx[k]);
        set (kFxOn[k], v.fx[k] > 0.0f ? 1.0f : 0.0f);
    }
}

void LiveVoiceProcessor::initSound()
{
    undo.beginNewTransaction ("Init");
    for (auto* id : kParamIds)
        if (auto* prm = apvts.getParameter (id))
        {
            prm->setValueNotifyingHost (prm->getDefaultValue());
        }
    presetName = "INIT"; presetFile = File();
}

void LiveVoiceProcessor::installFactoryPresets()
{
    struct F { const char* name; int mode; float strength, pitch, formant; bool sfx; int type; float amount; bool clip; };
    // modes: 0 NATURAL 1 RADIO 2 CLUB 3 DEEP / sfx: 0 WIDE 1 SHIMMER 2 ECHO THROW 3 BRUTAL
    static const F factory[] = {
        { "Clean Host",       0, 0.60f,  0.0f,  0.0f, false, 0, 0.50f, false },
        { "Radio Night",      1, 0.85f,  0.0f,  0.0f, false, 0, 0.50f, true  },
        { "Club MC",          2, 0.85f,  0.0f,  0.0f, true,  2, 0.30f, true  },
        { "Deep Voice",       3, 0.75f,  0.0f, -4.0f, false, 0, 0.50f, false },
        { "Brutal Announcer", 1, 0.90f,  0.0f, -5.0f, true,  3, 0.55f, true  },
        { "Echo Hype",        2, 0.80f,  0.0f,  0.0f, true,  2, 0.70f, false },
    };
    auto dir = presetRoot().getChildFile ("Factory");
    if (! dir.createDirectory()) return;
    for (auto& f : factory)
    {
        auto file = dir.getChildFile (String (f.name) + kPresetExt);
        if (file.existsAsFile())
            if (auto old = XmlDocument::parse (file))
                if (old->getStringAttribute ("version") == LIVEVOICE_LABEL) continue;   // factory files are refreshed on update
        XmlElement xml ("LIVEVOICEPRESET");
        xml.setAttribute ("version", LIVEVOICE_LABEL);
        xml.setAttribute ("name", f.name);
        xml.setAttribute (ParamID::voiceOn, 1.0);
        xml.setAttribute (ParamID::mode, f.mode);
        xml.setAttribute (ParamID::strength, f.strength);
        xml.setAttribute (ParamID::pitch, f.pitch);
        xml.setAttribute (ParamID::formant, f.formant);
        xml.setAttribute (ParamID::sfxOn, f.sfx ? 1.0 : 0.0);
        xml.setAttribute (ParamID::sfxType, f.type);
        xml.setAttribute (ParamID::sfxAmount, f.amount);
        xml.setAttribute (ParamID::output, 0.0);
        xml.setAttribute (ParamID::clipOn, f.clip ? 1.0 : 0.0);
        xml.writeTo (file);
    }
}

AudioProcessorEditor* LiveVoiceProcessor::createEditor() { return new LiveVoiceEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LiveVoiceProcessor(); }
