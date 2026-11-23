#include "PluginProcessor.h"
#include "PluginEditor.h"

FieldAudioProcessor::FieldAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FieldAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<P> (juce::ParameterID { "x",     1 }, "Left / Right",   R (-1.0f, 1.0f, 0.001f), 0.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "dist",  1 }, "Near / Far",     R (0.0f, 1.0f, 0.001f),  0.3f));
    l.add (std::make_unique<P> (juce::ParameterID { "depth", 1 }, "Depth",          R (0.0f, 1.0f, 0.001f),  0.35f));
    l.add (std::make_unique<P> (juce::ParameterID { "width", 1 }, "Width",          R (0.0f, 1.5f, 0.001f),  1.0f));
    l.add (std::make_unique<P> (juce::ParameterID { "air",   1 }, "Air Absorption", R (0.0f, 1.0f, 0.001f),  0.7f));
    return l;
}

bool FieldAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void FieldAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    preDelay.setMaximumDelayInSamples ((int) (sampleRate * 0.06) + 4);
    preDelay.prepare (spec);
    preDelay.reset();

    lowpass.prepare (spec);
    lowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowpass.reset();

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    wet.setSize (3, samplesPerBlock);

    auto init = [this] (juce::SmoothedValue<float>& s, const char* id)
    {
        s.reset (sr, 0.05);
        s.setCurrentAndTargetValue (apvts.getRawParameterValue (id)->load());
    };
    init (xS, "x"); init (dS, "dist"); init (depthS, "depth"); init (widthS, "width"); init (airS, "air");
}

void FieldAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    if (wet.getNumSamples() < n)
        wet.setSize (3, n, false, false, true);

    xS.setTargetValue     (apvts.getRawParameterValue ("x")->load());
    dS.setTargetValue     (apvts.getRawParameterValue ("dist")->load());
    depthS.setTargetValue (apvts.getRawParameterValue ("depth")->load());
    widthS.setTargetValue (apvts.getRawParameterValue ("width")->load());
    airS.setTargetValue   (apvts.getRawParameterValue ("air")->load());

    float* L  = buffer.getWritePointer (0);
    float* R  = buffer.getWritePointer (1);
    float* wL = wet.getWritePointer (0);
    float* wR = wet.getWritePointer (1);
    float* wG = wet.getWritePointer (2);

    float peak = 0.0f, lastDepth = 0.0f, lastDist = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float x = xS.getNextValue();
        const float d = dS.getNextValue();
        const float dep = depthS.getNextValue();
        const float w = widthS.getNextValue();
        const float air = airS.getNextValue();
        lastDepth = dep; lastDist = d;

        // stereo width
        float l = L[i], r = R[i];
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r) * w;
        l = m + s;
        r = m - s;

        peak = std::max ({ peak, std::abs (l), std::abs (r) });

        // reverb send, delayed more the farther the source is
        preDelay.setDelay ((float) (sr * (0.002 + 0.038 * d)));
        preDelay.pushSample (0, 0.5f * (l + r));
        const float pd = preDelay.popSample (0);
        wL[i] = pd;
        wR[i] = pd;
        // far = mostly room, near = mostly direct
        wG[i] = dep * (0.2f + 0.8f * d) * 0.5f;

        // air absorption: highs fade with distance
        lowpass.setCutoffFrequency (20000.0f * std::pow (0.15f, d * air));
        l = lowpass.processSample (0, l);
        r = lowpass.processSample (1, r);

        // left / right placement (constant power)
        const float a  = (x + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float gl = std::min (1.0f, 1.4142f * std::cos (a));
        const float gr = std::min (1.0f, 1.4142f * std::sin (a));

        // distance attenuation
        const float g = juce::Decibels::decibelsToGain (-18.0f * d);

        L[i] = l * gl * g;
        R[i] = r * gr * g;
    }

    juce::Reverb::Parameters p;
    p.roomSize   = 0.3f + 0.65f * lastDepth;
    p.damping    = 0.3f + 0.5f * lastDist;
    p.wetLevel   = 1.0f;
    p.dryLevel   = 0.0f;
    p.width      = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);
    reverb.processStereo (wL, wR, n);

    for (int i = 0; i < n; ++i)
    {
        L[i] += wL[i] * wG[i];
        R[i] += wR[i] * wG[i];
    }

    level.store (std::max (level.load(), peak));
}

juce::AudioProcessorEditor* FieldAudioProcessor::createEditor()
{
    return new FieldAudioProcessorEditor (*this);
}

void FieldAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void FieldAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FieldAudioProcessor();
}
