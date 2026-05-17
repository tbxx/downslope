#pragma once

#include <JuceHeader.h>

class MinimalistLookAndFeel : public juce::LookAndFeel_V4
{
public:
    struct Palette
    {
        juce::Colour bg        = juce::Colour (0xffECE8DD);   // travertine cream
        juce::Colour surface   = juce::Colour (0xffE2DCCB);   // muted surface
        juce::Colour ink       = juce::Colour (0xff1A1A1A);   // near-black
        juce::Colour inkMuted  = juce::Colour (0xff6B6B6B);
        juce::Colour hairline  = juce::Colour (0x55000000);
        juce::Colour accent    = juce::Colour (0xffB22222);   // Le Corbusier red
    };
    Palette p;

    MinimalistLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId,         p.bg);
        setColour (juce::Label::textColourId,                          p.ink);

        setColour (juce::Slider::textBoxTextColourId,                  p.ink);
        setColour (juce::Slider::textBoxBackgroundColourId,            juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,               juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId,             p.accent);
        setColour (juce::Slider::rotarySliderOutlineColourId,          p.ink);

        setColour (juce::ComboBox::backgroundColourId,                 juce::Colours::transparentBlack);
        setColour (juce::ComboBox::outlineColourId,                    juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId,                       p.ink);
        setColour (juce::ComboBox::arrowColourId,                      p.ink);

        setColour (juce::TextButton::buttonColourId,                   juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonOnColourId,                 p.accent);
        setColour (juce::TextButton::textColourOffId,                  p.ink);
        setColour (juce::TextButton::textColourOnId,                   p.bg);

        setColour (juce::ToggleButton::textColourId,                   p.ink);
        setColour (juce::ToggleButton::tickColourId,                   p.accent);
        setColour (juce::ToggleButton::tickDisabledColourId,           p.inkMuted);

        setColour (juce::PopupMenu::backgroundColourId,                p.bg);
        setColour (juce::PopupMenu::textColourId,                      p.ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId,     p.ink);
        setColour (juce::PopupMenu::highlightedTextColourId,           p.bg);
    }

    // --- Fonts --------------------------------------------------------------

    static juce::Font caps (float size, float kerning = 0.18f, bool bold = false)
    {
        auto opts = juce::FontOptions (size).withKerningFactor (kerning);
        if (bold) opts = opts.withStyle ("Bold");
        return juce::Font (opts);
    }

    juce::Font getLabelFont (juce::Label&) override                  { return caps (13.5f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override            { return caps (15.5f, 0.08f); }
    juce::Font getPopupMenuFont() override                           { return juce::Font (juce::FontOptions (15.5f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override   { return caps (16.0f, 0.30f, true); }

    juce::Font getSliderPopupFont (juce::Slider&) override           { return juce::Font (juce::FontOptions (15.5f)); }

    // Knob value text — legibility over architectural austerity here. Slightly
    // bolder than the surrounding labels so the *value* is what reads first.
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::outlineWhenEditingColourId,    juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::textColourId, p.ink);
        return l;
    }

    // --- Rotary knob: hollow ring + single accent tick ----------------------

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& s) override
    {
        const float diameter = (float) juce::jmin (w, h) - 14.0f;
        const float radius   = diameter * 0.5f;
        const float cx = x + w * 0.5f;
        const float cy = y + h * 0.5f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        const float alpha = s.isEnabled() ? 1.0f : 0.28f;

        // soft inner fill so the ring sits on a slight tonal disc
        g.setColour (p.surface.withAlpha (alpha));
        g.fillEllipse (cx - radius, cy - radius, diameter, diameter);

        // outer ring, hairline weight
        g.setColour (p.ink.withAlpha (alpha));
        g.drawEllipse (cx - radius, cy - radius, diameter, diameter, 1.2f);

        // value tick — accent when enabled, muted ink when not
        const float ti = radius * 0.30f;
        const float to = radius - 2.5f;
        juce::Path tick;
        tick.startNewSubPath (cx + std::sin (angle) * ti, cy - std::cos (angle) * ti);
        tick.lineTo         (cx + std::sin (angle) * to, cy - std::cos (angle) * to);
        g.setColour (s.isEnabled() ? p.accent : p.inkMuted.withAlpha (0.5f));
        g.strokePath (tick, juce::PathStrokeType (2.6f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // --- Combo box: just a hairline underline + small chevron ---------------

    void drawComboBox (juce::Graphics& g, int w, int h,
                       bool /*isDown*/, int /*bX*/, int /*bY*/, int /*bW*/, int /*bH*/,
                       juce::ComboBox& box) override
    {
        const float alpha = box.isEnabled() ? 1.0f : 0.35f;

        // hairline rule beneath
        g.setColour (p.hairline.withAlpha (p.hairline.getFloatAlpha() * alpha));
        g.fillRect (0, h - 1, w, 1);

        // tiny chevron at right
        const float cx = (float) w - 11.0f;
        const float cy = (float) h * 0.5f - 2.0f;
        juce::Path chev;
        chev.startNewSubPath (cx - 4, cy - 2);
        chev.lineTo (cx,     cy + 2);
        chev.lineTo (cx + 4, cy - 2);
        g.setColour (p.ink.withAlpha (alpha));
        g.strokePath (chev, juce::PathStrokeType (1.2f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (0, 0, box.getWidth() - 18, box.getHeight() - 4);
        label.setFont (getComboBoxFont (box));
    }

    // --- Buttons: outline rectangle, solid red when on ----------------------

    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool isOver, bool isDown) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        if (on)
        {
            g.setColour (p.accent);
            g.fillRect (r);
        }
        else
        {
            g.setColour (p.ink);
            g.drawRect (r, 1.5f);
            if (isOver || isDown)
            {
                g.setColour (p.ink.withAlpha (isDown ? 0.12f : 0.06f));
                g.fillRect (r);
            }
        }
    }

    // --- Toggle button: square checkbox, accent fill ------------------------

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                           bool /*isOver*/, bool /*isDown*/) override
    {
        const float boxSize = 17.0f;
        const float bx = 2.0f;
        const float by = (b.getHeight() - boxSize) * 0.5f;

        g.setColour (p.ink);
        g.drawRect (juce::Rectangle<float> (bx, by, boxSize, boxSize), 1.4f);

        if (b.getToggleState())
        {
            g.setColour (p.accent);
            g.fillRect (juce::Rectangle<float> (bx + 4.0f, by + 4.0f,
                                                 boxSize - 8.0f, boxSize - 8.0f));
        }

        g.setColour (p.ink);
        g.setFont (caps (14.0f, 0.18f));
        g.drawText (b.getButtonText().toUpperCase(),
                    (int) (bx + boxSize + 12), 0,
                    b.getWidth() - (int) (bx + boxSize + 12), b.getHeight(),
                    juce::Justification::centredLeft);
    }

    // --- PopupMenu: flat, square corners ------------------------------------

    int getPopupMenuBorderSize() override { return 6; }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (p.bg);
        g.setColour (p.ink);
        g.drawRect (0, 0, w, h, 1);
    }
};
