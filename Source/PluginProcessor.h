#pragma once
#include <JuceHeader.h>

// FIELD - spatial placement. Position a sound left/right and near/far,
// with room depth. Distance is rendered the way the ear expects it:
// quieter direct sound, duller highs, more reverb, longer pre-delay.
class FieldAudioProcessor : public juce::AudioProcessor
{
public:
    FieldAudioProcessor();
    ~FieldAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FIELD"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> level { 0.0f }; // read by the visualizer

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double sr = 44100.0;

    juce::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 4 };
    juce::dsp::StateVariableTPTFilter<float> lowpass;
    juce::AudioBuffer<float> wet; // ch0/1 = reverb in/out, ch2 = wet gain

    juce::SmoothedValue<float> xS, dS, depthS, widthS, airS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FieldAudioProcessor)
};
