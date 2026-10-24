// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Processor.h"

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

//
// THE APP: JUCE's standalone one - the same window, the same audio settings - but closing
// asks. JUCE's own saves the rack as it stands whenever the window closes, so the next time
// the app opens, there it is, changes and all, saved or not. Here the panel asks first, if
// anything has changed since the rack was saved: and only a rack that was saved, or that
// has nothing unsaved in it, is what the app opens with next time.
//
namespace
{
    // THE DEFAULT AUDIO DEVICE ON A RASPBERRY PI
    // With no USB sound card attached, the Pi's HDMI output is the one that works. ALSA names
    // it "vc4-hdmi-1, MAI PCM i2s-hifi-0" - the number follows the HDMI port, so this is a
    // wildcard rather than that exact string. JUCE matches it case-insensitively as a wildcard
    // (AudioDeviceManager::initialiseDefault), and ONLY when there is no saved audio setup: it
    // picks the device on a first run and never overrides a device the player has chosen. A
    // pattern matching nothing falls back to the normal default, so this cannot strand anyone.
    juce::String preferredDefaultDevice()
    {
       #if JUCE_LINUX || JUCE_BSD
        return "vc4-hdmi*";
       #else
        return {};
       #endif
    }

    class Window final : public juce::StandaloneFilterWindow
    {
    public:
        using StandaloneFilterWindow::StandaloneFilterWindow;

        void closeButtonPressed() override
        {
            auto* processor = dynamic_cast<DebirthProcessor*>(getAudioProcessor());
            auto quit = [this] (bool keep)
            {
                if (keep) pluginHolder->savePluginState();   // the rack to reopen with
                juce::JUCEApplicationBase::quit();
            };

            if (processor != nullptr && processor->onCloseRequested)
                processor->onCloseRequested(quit);
            else
                quit(true);
        }
    };

    class App final : public juce::JUCEApplication
    {
    public:
        App()
        {
            juce::PropertiesFile::Options options;
            options.applicationName     = JucePlugin_Name;
            options.filenameSuffix      = ".settings";
            options.osxLibrarySubFolder = "Application Support";
           #if JUCE_LINUX || JUCE_BSD
            options.folderName          = "~/.config";
           #else
            options.folderName          = "";
           #endif
            properties.setStorageParameters(options);   // where JUCE's own app keeps it, so nothing moves
        }

        const juce::String getApplicationName() override    { return JucePlugin_Name; }
        const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
        bool moreThanOneInstanceAllowed() override          { return true; }
        void anotherInstanceStarted(const juce::String&) override {}

        void initialise(const juce::String&) override
        {
            window = std::make_unique<Window>(getApplicationName(),
                juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                std::make_unique<juce::StandalonePluginHolder>(properties.getUserSettings(), false, preferredDefaultDevice(),
                                                               nullptr, juce::Array<juce::StandalonePluginHolder::PluginInOuts>(),
                                                               false));
            window->setVisible(true);
        }

        void shutdown() override
        {
            window = nullptr;
            properties.saveIfNeeded();
        }

        // quitting from the system - the Mac's menu, a log-off - is the window closing
        void systemRequestedQuit() override
        {
            if (window != nullptr) window->closeButtonPressed();
            else                   quit();
        }

    private:
        juce::ApplicationProperties properties;
        std::unique_ptr<Window> window;
    };
}

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new App(); }
