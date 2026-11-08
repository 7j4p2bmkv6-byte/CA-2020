// L-Family look & feel - one shared visual language for L-DAW, CA-2020 and future L-family tools.
// Charcoal panels, Roland-style coloured section stripes, amber LCDs, cream legends, cap-style faders.
// Drop this same file into L-DAW/Source and CA-2020/Source - keep both copies identical.
#pragma once
#include <JuceHeader.h>

namespace lfam
{
//==============================================================================
namespace colours
{
    const juce::Colour bg         { 0xff17181b };   // window background
    const juce::Colour panel      { 0xff24252a };   // section face
    const juce::Colour panelHi    { 0xff2e3036 };   // raised parts / strips
    const juce::Colour well       { 0xff111214 };   // slots, fields, meters
    const juce::Colour line       { 0xff3a3c43 };
    const juce::Colour text       { 0xffe9e4d8 };   // cream legend
    const juce::Colour textDim    { 0xff9a968c };
    const juce::Colour lcdBg      { 0xff1a1408 };
    const juce::Colour lcdText    { 0xffffb547 };   // amber LCD
    const juce::Colour led        { 0xffff4a3d };

    // section stripes (Juno / JNO-600 family)
    const juce::Colour red        { 0xffd9453b };
    const juce::Colour orange     { 0xffe8743b };
    const juce::Colour yellow     { 0xffe5b33a };
    const juce::Colour blue       { 0xff3e8ed0 };
    const juce::Colour teal       { 0xff2fb3a3 };
    const juce::Colour purple     { 0xff8f6bd1 };
    const juce::Colour cream      { 0xffd8d2c2 };

    // mixer channel-type colours (same stripes as the instruments)
    inline juce::Colour forChannel (int type)   // 0 instrument, 1 audio, 2 send/FX, 3 master, 4 MIDI FX
    {
        switch (type) { case 1: return teal; case 2: return purple; case 3: return red; case 4: return yellow; default: return orange; }
    }
}

inline juce::Font font (float height, bool bold = false)
{
   #if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
   #else
    return juce::Font (height, bold ? juce::Font::bold : juce::Font::plain);
   #endif
}

//==============================================================================
/** Panel with a coloured header stripe and a cream title (the JNO-600 section style). */
inline juce::Rectangle<int> drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title,
                                         juce::Colour stripe, int stripeHeight = 18)
{
    auto rf = r.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (rf, 4.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (rf.reduced (0.5f), 4.0f, 1.0f);

    auto head = rf.removeFromTop ((float) stripeHeight);
    juce::Path p;
    p.addRoundedRectangle (head.getX(), head.getY(), head.getWidth(), head.getHeight(), 4.0f, 4.0f, true, true, false, false);
    g.setColour (stripe);
    g.fillPath (p);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.setFont (font (11.5f, true));
    g.drawText (title.toUpperCase(), head.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
    return r.withTrimmedTop (stripeHeight);
}

/** Amber LCD window. */
inline void drawLcd (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (colours::lcdBg);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.5f);
    g.setColour (colours::lcdText.withAlpha (0.06f));
    for (float y = r.getY() + 2.0f; y < r.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, r.getX() + 2.0f, r.getRight() - 2.0f);
}

/** Segmented level meter (green / amber / red), value 0..1 (linear gain mapped by caller). */
inline void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float level, float peakHold = -1.0f)
{
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 2.0f);
    const int segs = juce::jmax (8, (int) (r.getHeight() / 4.0f));
    const float segH = r.getHeight() / (float) segs;
    const int lit = (int) std::round (juce::jlimit (0.0f, 1.0f, level) * segs);
    for (int i = 0; i < segs; ++i)
    {
        const float frac = (float) i / (float) segs;
        auto c = frac > 0.9f ? colours::led : frac > 0.72f ? colours::yellow : juce::Colour (0xff3fcf6a);
        auto seg = juce::Rectangle<float> (r.getX() + 1.5f, r.getBottom() - (i + 1) * segH + 0.6f, r.getWidth() - 3.0f, segH - 1.2f);
        g.setColour (i < lit ? c : c.withAlpha (0.10f));
        g.fillRect (seg);
    }
    if (peakHold > 0.0f)
    {
        g.setColour (colours::text);
        g.fillRect (r.getX() + 1.5f, r.getBottom() - peakHold * r.getHeight(), r.getWidth() - 3.0f, 1.5f);
    }
}

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::DocumentWindow::backgroundColourId, bg);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextButton::buttonColourId, panelHi);
        setColour (juce::TextButton::buttonOnColourId, orange);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, led);
        setColour (juce::ToggleButton::tickDisabledColourId, textDim);
        setColour (juce::ComboBox::backgroundColourId, well);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, orange);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::PopupMenu::headerTextColourId, yellow);
        setColour (juce::Slider::rotarySliderFillColourId, orange);
        setColour (juce::Slider::rotarySliderOutlineColourId, well);
        setColour (juce::Slider::thumbColourId, cream);
        setColour (juce::Slider::trackColourId, orange);
        setColour (juce::Slider::backgroundColourId, well);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, well);
        setColour (juce::Slider::textBoxOutlineColourId, line);
        setColour (juce::TextEditor::backgroundColourId, well);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, line);
        setColour (juce::TextEditor::focusedOutlineColourId, orange);
        setColour (juce::ScrollBar::thumbColourId, line);
        setColour (juce::ListBox::backgroundColourId, well);
        setColour (juce::TooltipWindow::backgroundColourId, panelHi);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, orange.withAlpha (0.55f));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, cream.withAlpha (0.25f));
        setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.4f));
    }

    // Knob: dark cap, coloured value arc, cream pointer
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float rad = size * 0.5f, ang = a0 + pos * (a1 - a0);
        const auto fill = s.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;  track.addCentredArc (c.x, c.y, rad - 1.5f, rad - 1.5f, 0, a0, a1, true);
        g.setColour (colours::well);  g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path val;    val.addCentredArc (c.x, c.y, rad - 1.5f, rad - 1.5f, 0, a0, ang, true);
        g.setColour (fill);           g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto cap = r.reduced (size * 0.17f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3d44), cap.getX(), cap.getY(), juce::Colour (0xff1b1c20), cap.getRight(), cap.getBottom(), false));
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (cap, 1.0f);

        const float pr = cap.getWidth() * 0.5f;
        juce::Line<float> pointer (c.getPointOnCircumference (pr * 0.25f, ang), c.getPointOnCircumference (pr * 0.92f, ang));
        g.setColour (colours::cream); g.drawLine (pointer, 2.2f);
    }

    // Fader: dark slot, cream cap with a black index line (JNO-600 / mixer)
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearHorizontal)
        { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s); return; }

        const bool vert = style == juce::Slider::LinearVertical;
        auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
        const auto accent = s.findColour (juce::Slider::trackColourId);

        if (vert)
        {
            auto slot = juce::Rectangle<float> (r.getCentreX() - 2.5f, r.getY(), 5.0f, r.getHeight());
            g.setColour (colours::well); g.fillRoundedRectangle (slot, 2.5f);
            g.setColour (colours::textDim.withAlpha (0.35f));
            for (int i = 0; i <= 10; ++i)
            {
                const float ty = r.getY() + r.getHeight() * i / 10.0f;
                g.drawHorizontalLine ((int) ty, r.getCentreX() - 11.0f, r.getCentreX() - 6.0f);
                g.drawHorizontalLine ((int) ty, r.getCentreX() + 6.0f, r.getCentreX() + 11.0f);
            }
            g.setColour (accent.withAlpha (0.8f));
            g.fillRoundedRectangle (slot.withTop (sliderPos), 2.5f);
            auto cap = juce::Rectangle<float> (juce::jmin (26.0f, r.getWidth() - 2.0f), 14.0f).withCentre ({ r.getCentreX(), sliderPos });
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (cap.translated (0, 2), 2.0f);
            g.setGradientFill (juce::ColourGradient (colours::cream, cap.getX(), cap.getY(), juce::Colour (0xffa9a394), cap.getX(), cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 2.0f);
            g.setColour (juce::Colours::black); g.drawHorizontalLine ((int) cap.getCentreY(), cap.getX() + 2, cap.getRight() - 2);
        }
        else
        {
            auto slot = juce::Rectangle<float> (r.getX(), r.getCentreY() - 2.5f, r.getWidth(), 5.0f);
            g.setColour (colours::well); g.fillRoundedRectangle (slot, 2.5f);
            g.setColour (accent.withAlpha (0.8f)); g.fillRoundedRectangle (slot.withRight (sliderPos), 2.5f);
            auto cap = juce::Rectangle<float> (12.0f, juce::jmin (22.0f, r.getHeight() - 2.0f)).withCentre ({ sliderPos, r.getCentreY() });
            g.setColour (colours::cream); g.fillRoundedRectangle (cap, 2.0f);
            g.setColour (juce::Colours::black); g.drawVerticalLine ((int) cap.getCentreX(), cap.getY() + 2, cap.getBottom() - 2);
        }
    }

    // Buttons: flat rectangular caps; "on" state glows in the button's on-colour
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto c = on ? b.findColour (juce::TextButton::buttonOnColourId) : bgColour;
        if (down) c = c.darker (0.25f); else if (over) c = c.brighter (0.08f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (0, 1.5f), 3.0f);
        g.setColour (c); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? c.brighter (0.3f) : colours::line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (13.0f, h * 0.55f), true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override                { return font (13.0f); }
    juce::Font getPopupMenuFont() override                                { return font (14.0f); }
    juce::Font getLabelFont (juce::Label& l) override                     { return font (juce::jmax (11.0f, l.getFont().getHeight())); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.hasKeyboardFocus (true) ? colours::orange : box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        juce::Path tri; const float cx = (float) w - 10.0f, cy = h * 0.5f;
        tri.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId)); g.fillPath (tri);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto ledR = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getX() + 8.0f, r.getCentreY() });
        const bool on = b.getToggleState();
        if (on) { g.setColour (colours::led.withAlpha (0.35f)); g.fillEllipse (ledR.expanded (3.0f)); }
        g.setColour (on ? colours::led : juce::Colour (0xff4a1e1b)); g.fillEllipse (ledR);
        g.setColour (juce::Colours::black); g.drawEllipse (ledR, 1.0f);
        g.setColour (over ? colours::text : colours::text.withAlpha (0.85f));
        g.setFont (font (12.5f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (20.0f), juce::Justification::centredLeft);
    }
};

} // namespace lfam
