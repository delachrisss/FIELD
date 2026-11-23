#include "PluginEditor.h"

namespace
{
    const juce::Colour bg      { 0xff0b0b0c };
    const juce::Colour halo    { 0xff6ea8ff };
    const juce::Colour ink     { 0xfff2f2f2 };
}

FieldAudioProcessorEditor::FieldAudioProcessorEditor (FieldAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      depthA (p.apvts, "depth", depth),
      widthA (p.apvts, "width", width),
      airA   (p.apvts, "air",   air)
{
    for (auto* s : { &depth, &width, &air })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setLookAndFeel (&laf);
        addAndMakeVisible (*s);
    }

    setSize (440, 620);
    startTimerHz (60);
}

FieldAudioProcessorEditor::~FieldAudioProcessorEditor()
{
    for (auto* s : { &depth, &width, &air })
        s->setLookAndFeel (nullptr);
}

void FieldAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().reduced (20);

    auto rows = b.removeFromBottom (3 * 34);
    readout = b.removeFromBottom (36);
    field = b.toFloat();

    juce::Slider* sliders[] = { &depth, &width, &air };
    for (int i = 0; i < 3; ++i)
    {
        auto row = rows.removeFromTop (34);
        labelRects[i] = row.removeFromLeft (70);
        sliders[i]->setBounds (row.reduced (0, 6));
    }
}

void FieldAudioProcessorEditor::timerCallback()
{
    const float lv = proc.level.exchange (0.0f);
    shownLevel += (juce::jmin (1.0f, lv) - shownLevel) * 0.3f;
    repaint();
}

void FieldAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    const float x   = proc.apvts.getRawParameterValue ("x")->load();
    const float d   = proc.apvts.getRawParameterValue ("dist")->load();
    const float dep = proc.apvts.getRawParameterValue ("depth")->load();

    // ---- field frame
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    g.fillRoundedRectangle (field, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (field, 10.0f, 1.0f);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (field, 10.0f);
        g.reduceClipRegion (clip);

        const auto in = inner();

        // ---- grid: distance lines + left/right lines
        for (int k = 0; k <= 4; ++k)
        {
            const float yy = in.getBottom() - in.getHeight() * (float) k / 4.0f;
            g.setColour (juce::Colours::white.withAlpha (k == 0 ? 0.14f : 0.06f));
            g.drawHorizontalLine ((int) yy, in.getX(), in.getRight());
        }
        for (int k = 0; k <= 4; ++k)
        {
            const float xx = in.getX() + in.getWidth() * (float) k / 4.0f;
            g.setColour (juce::Colours::white.withAlpha (k == 2 ? 0.14f : 0.05f));
            g.drawVerticalLine ((int) xx, in.getY(), in.getBottom());
        }

        // ---- listener
        const juce::Point<float> listener (in.getCentreX(), in.getBottom() + 16.0f);
        g.setColour (ink.withAlpha (0.55f));
        g.drawEllipse (listener.x - 6.0f, listener.y - 6.0f, 12.0f, 12.0f, 1.0f);

        // ---- source
        const juce::Point<float> c (in.getX() + (x + 1.0f) * 0.5f * in.getWidth(),
                                    in.getBottom() - d * in.getHeight());

        // line from listener to source
        g.setColour (ink.withAlpha (0.18f));
        g.drawLine (listener.x, listener.y - 6.0f, c.x, c.y, 1.0f);

        // depth halo = how much room around the source
        const float haloR = 20.0f + dep * (50.0f + 70.0f * d);
        juce::ColourGradient hg (halo.withAlpha (0.10f + 0.25f * shownLevel), c.x, c.y,
                                 halo.withAlpha (0.0f), c.x + haloR, c.y, true);
        g.setGradientFill (hg);
        g.fillEllipse (c.x - haloR, c.y - haloR, haloR * 2.0f, haloR * 2.0f);
        g.setColour (halo.withAlpha (0.18f));
        g.drawEllipse (c.x - haloR, c.y - haloR, haloR * 2.0f, haloR * 2.0f, 1.0f);

        // dot: small + soft when far, bigger + sharp when near, pulses with level
        const float r = 12.0f * (1.0f - 0.5f * d) + shownLevel * 5.0f;
        const float softR = r * (1.0f + 2.2f * d);
        juce::ColourGradient sg (ink.withAlpha (0.35f * d), c.x, c.y,
                                 ink.withAlpha (0.0f), c.x + softR, c.y, true);
        g.setGradientFill (sg);
        g.fillEllipse (c.x - softR, c.y - softR, softR * 2.0f, softR * 2.0f);

        g.setColour (ink.withAlpha (1.0f - 0.35f * d));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        // ---- axis labels
        g.setFont (juce::Font (11.0f));
        g.setColour (ink.withAlpha (0.4f));
        g.drawText ("FAR",  juce::Rectangle<float> (in.getX(), field.getY() + 8.0f, in.getWidth(), 16.0f),
                    juce::Justification::centred);
        g.drawText ("NEAR", juce::Rectangle<float> (in.getX(), in.getBottom() - 18.0f, in.getWidth(), 14.0f),
                    juce::Justification::centred);
        g.drawText ("L", juce::Rectangle<float> (field.getX() + 8.0f, in.getCentreY() - 8.0f, 16.0f, 16.0f),
                    juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (field.getRight() - 24.0f, in.getCentreY() - 8.0f, 16.0f, 16.0f),
                    juce::Justification::centred);
    }

    // ---- readout
    const int pan = (int) std::round (std::abs (x) * 100.0f);
    const juce::String side = pan < 2 ? juce::String ("C") : ((x < 0 ? "L " : "R ") + juce::String (pan));
    const juce::String text = side + "     " + juce::String (0.5f + d * 19.5f, 1) + " m     DEPTH "
                              + juce::String ((int) std::round (dep * 100.0f)) + "%";
    g.setColour (ink.withAlpha (0.85f));
    g.setFont (juce::Font (13.0f));
    g.drawText (text, readout, juce::Justification::centred);

    // ---- slider labels
    g.setColour (ink.withAlpha (0.45f));
    g.setFont (juce::Font (11.0f));
    const char* names[] = { "DEPTH", "WIDTH", "AIR" };
    for (int i = 0; i < 3; ++i)
        g.drawText (names[i], labelRects[i], juce::Justification::centredLeft);
}

void FieldAudioProcessorEditor::setParam (const char* id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void FieldAudioProcessorEditor::setFromMouse (juce::Point<float> pos)
{
    const auto in = inner();
    const float nx = juce::jlimit (-1.0f, 1.0f, (pos.x - in.getX()) / in.getWidth() * 2.0f - 1.0f);
    const float nd = juce::jlimit (0.0f, 1.0f, (in.getBottom() - pos.y) / in.getHeight());
    setParam ("x", nx);
    setParam ("dist", nd);
}

void FieldAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! field.contains (e.position)) return;
    dragging = true;
    proc.apvts.getParameter ("x")->beginChangeGesture();
    proc.apvts.getParameter ("dist")->beginChangeGesture();
    setFromMouse (e.position);
}

void FieldAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging) setFromMouse (e.position);
}

void FieldAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    proc.apvts.getParameter ("x")->endChangeGesture();
    proc.apvts.getParameter ("dist")->endChangeGesture();
}

void FieldAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! field.contains (e.position)) return;
    setParam ("x", 0.0f);
    setParam ("dist", 0.3f);
}
