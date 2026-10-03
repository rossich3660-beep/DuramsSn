#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include <vector>
#include "SnareEngine.h"

class DuramsProcessor : public juce::AudioProcessor,
                        public juce::ChangeBroadcaster
{
public:
    DuramsProcessor();
    ~DuramsProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Durams Sn"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message-thread API used by the editor
    bool loadSampleFromFile (const juce::File& file);
    std::shared_ptr<const durams::SampleData> getCurrentSample() const;
    void triggerAudition (float velocity01) { auditionVel.store (velocity01); }
    juce::StringArray getSupportedWildcards() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    durams::Params readParams() const;

    void installSample (std::shared_ptr<durams::SampleData> s, const juce::String& name, const juce::String& path, bool embed);
    bool loadSampleFromState();

    durams::SnareEngine engine;
    std::atomic<const durams::SampleData*> samplePtr { nullptr };
    std::atomic<float> auditionVel { -1.0f };

    // owned on the message thread only; old samples are kept alive for a while
    // because the audio thread may still be playing them
    juce::CriticalSection loadLock;
    std::shared_ptr<durams::SampleData> currentSample;
    struct Retired { std::shared_ptr<durams::SampleData> data; juce::uint32 when; };
    std::vector<Retired> retired;

    juce::AudioFormatManager formatManager;
    std::atomic<float>* prm[12] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DuramsProcessor)
};
