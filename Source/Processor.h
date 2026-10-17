// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Core/Rack.h"

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

    // which view the window shows, the rack or the Arrange window - saved with the rest
    enum class View { Rack, Arrange };
    View view = View::Rack;

private:
    void timerCallback() override;   // the sampler's patch, handed over and collected
    void writeFactoryGrooves();

    std::atomic<int> loads { 0 };
    std::vector<float> spare;   // a right channel to render into, when there is only a left

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebirthProcessor)
};
