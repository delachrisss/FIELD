#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Thin-line sliders, nothing else.
struct MinimalLookAndFeel : public juce::LookAndFeel_V4
{
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRect ((float) x, cy - 0.5f, (float) w, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect ((float) x, cy - 0.5f, pos - (float) x, 1.0f);
        g.fillEllipse (pos - 4.0f, cy - 4.0f, 8.0f, 8.0f);
    }
};

class FieldAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit FieldAudioProcessorEditor (FieldAudioProcessor&);
    ~FieldAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setFromMouse (juce::Point<float> p);
    void setParam (const char* id, float value);
    juce::Rectangle<float> inner() const { return field.reduced (28.0f, 34.0f); }

    FieldAudioProcessor& proc;
    MinimalLookAndFeel laf;

    juce::Slider depth, width, air;
    juce::AudioProcessorValueTreeState::SliderAttachment depthA, widthA, airA;

    juce::Rectangle<float> field;
    juce::Rectangle<int> readout, labelRects[3];
    float shownLevel = 0.0f;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FieldAudioProcessorEditor)
};
