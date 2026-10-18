// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Core/Rack.h"
#include "Core/Recorder.h"

#include <array>

//
// The rack, as a JUCE processor - built only as a standalone app for now. It has no
// parameters for a host to see: the panel talks to the rack directly, and the whole of it,
// patterns and all, is saved as one tree.
//
class DebirthProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    DebirthProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MzDebirth"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void* data, int size) override;

    // the whole rack, as saved in a file
    juce::ValueTree save() const;
    void load(const juce::ValueTree&);
    void startAfresh(bool factoryGrooves);   // every pattern cleared - or filled with the factory grooves

    // The KEY section: the bank it works on - the one the Mz303 shows - and its two buttons.
    int keyBank() const { return rack.mz303.track.selected.load() / debirth::patternsPerBank; }
    void generateInKey();
    void transposeToKey();

    // bumped by every load, so the panel knows to read itself back
    int loadCount() const { return loads.load(); }

    debirth::Rack rack;

    // The song the Arrange window edits - message thread only. Every change reaches the
    // rack as a new Playback.
    debirth::SongDocument song;

    //
    // RECORDING from the rack into the song - see Recorder. REC in the transport makes the
    // next PLAY, from either transport, a take: the machines play their patterns from the top,
    // and what every one plays goes into the song from the cursor's bar, there to be seen
    // as it is played. STOP, or REC again, ends it; one undo takes the whole take back.
    //
    bool recordReady = false;
    void play();
    void endTake();
    bool taking() const { return rack.recording.load(); }
    int takeReached() const { return take.reached(); }   // where in the song a take has got to

    // which view the window shows, the rack or the Arrange window - saved with the rest
    enum class View { Rack, Arrange };
    View view = View::Rack;

    // which machines have their step rolls folded away in the rack - saved with the rest
    std::array<bool, debirth::Rack::channels> folded {};

    // The window's size, as the app last left it; 0 until it has been. Kept with the app's own
    // state and not in a rack's file: loading a rack does not move anybody's window.
    int windowWidth = 0, windowHeight = 0;

private:
    void timerCallback() override;   // the sampler's patch, handed over and collected
    void writeFactoryGrooves();
    void drainTake();   // the steps logged so far, into the take and the song

    debirth::Recorder take;
    debirth::Song beforeTake;
    bool takeShown = false;    // the take is in the song, as an edit to undo
    int takeVersion = -1;      // the song as the take last left it
    int modeAfterTake = -1;    // the mode to go back to once the take is over and stopped

    std::atomic<int> loads { 0 };
    std::vector<float> spare;   // a right channel to render into, when there is only a left

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebirthProcessor)
};
