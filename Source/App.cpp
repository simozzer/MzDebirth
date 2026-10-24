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
                std::make_unique<juce::StandalonePluginHolder>(properties.getUserSettings(), false, juce::String(),
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
