// LIVE VOICE - the Windows app (JUCE custom standalone).
// Differences from JUCE's default standalone window:
//  * the microphone is NEVER muted (JUCE mutes the input by default to avoid feedback),
//  * the audio window (microphone / speakers) opens by itself on the very first start,
//  * the AUDIO button inside the UI opens the same window,
//  * the chosen devices and the last sound are remembered (AppData/Roaming/LIVE VOICE).
#include "PluginEditor.h"
#include <juce_audio_plugin_client/detail/juce_CreatePluginFilter.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

using namespace juce;

class LiveVoiceApp final : public JUCEApplication
{
public:
    LiveVoiceApp()
    {
        PropertiesFile::Options o;
        o.applicationName     = "LIVE VOICE";
        o.filenameSuffix      = ".settings";
        o.folderName          = "LIVE VOICE";
        o.osxLibrarySubFolder = "Application Support";
        props.setStorageParameters (o);
    }

    const String getApplicationName() override           { return "LIVE VOICE"; }
    const String getApplicationVersion() override        { return LIVEVOICE_LABEL; }
    bool moreThanOneInstanceAllowed() override           { return false; }   // two copies would fight for the microphone
    void anotherInstanceStarted (const String&) override { if (window) window->toFront (true); }

    void initialise (const String&) override
    {
        auto* settings = props.getUserSettings();
        const bool firstRun = ! settings->containsKey ("audioSetup");
        settings->setValue ("shouldMuteInput", false);   // read by StandalonePluginHolder when it opens the devices

        window = std::make_unique<StandaloneFilterWindow> ("LIVE VOICE  " + String (LIVEVOICE_LABEL),
                                                           Colour (0xff1c1e20), settings, false);
        auto& holder = *window->pluginHolder;
        holder.getMuteInputValue().setValue (false);
        LiveVoiceEditor::onAudioSettings = [this]
        {
            if (window != nullptr) window->pluginHolder->showAudioSettingsDialog();
        };
        window->setVisible (true);

        if (firstRun)
        {
            // Windows: prefer WASAPI low-latency mode (smaller buffers) if this computer's devices accept it
            auto& dm = holder.deviceManager;
            const String lowLat = "Windows Audio (Low Latency Mode)";
            for (auto* t : dm.getAvailableDeviceTypes())
                if (t->getTypeName() == lowLat)
                {
                    const String before = dm.getCurrentAudioDeviceType();
                    dm.setCurrentAudioDeviceType (lowLat, true);
                    if (dm.getCurrentAudioDevice() == nullptr)
                        dm.setCurrentAudioDeviceType (before, true);   // not supported here -> back to normal Windows Audio
                    break;
                }
        }
        if (firstRun)
            Timer::callAfterDelay (600, [this] { if (window != nullptr) window->pluginHolder->showAudioSettingsDialog(); });

        // keep the mic un-muted even if the checkbox in the audio window is ticked by accident
        holder.getMuteInputValue().addListener (&unmute);
    }

    void shutdown() override
    {
        LiveVoiceEditor::onAudioSettings = nullptr;
        if (window != nullptr)
        {
            window->pluginHolder->getMuteInputValue().removeListener (&unmute);
            window->pluginHolder->savePluginState();
        }
        window = nullptr;
        props.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (window != nullptr) window->pluginHolder->savePluginState();
        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
            Timer::callAfterDelay (100, [] { if (auto* app = JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        else
            quit();
    }

private:
    struct Unmute final : Value::Listener
    {
        void valueChanged (Value& v) override { if ((bool) v.getValue()) v.setValue (false); }
    } unmute;

    ApplicationProperties props;
    std::unique_ptr<StandaloneFilterWindow> window;
};

JUCEApplicationBase* juce_CreateApplication() { return new LiveVoiceApp(); }
