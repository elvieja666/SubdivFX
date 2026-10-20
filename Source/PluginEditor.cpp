#include "PluginEditor.h"

namespace
{
    const juce::Colour bg     { 0xff16181d };
    const juce::Colour panel  { 0xff20232a };
    const juce::Colour text   { 0xffe8e8ea };
    const juce::Colour dim    { 0xff8a8f98 };

    constexpr int knobW = 90, knobH = 110, header = 34, pad = 12, titleH = 46;
}

SubdivFXEditor::SubdivFXEditor (SubdivFXProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lnf.setColour (juce::Slider::textBoxTextColourId,    text);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::ComboBox::backgroundColourId,   panel.brighter (0.1f));
    lnf.setColour (juce::ComboBox::textColourId,         text);
    lnf.setColour (juce::ComboBox::outlineColourId,      juce::Colours::transparentBlack);
    lnf.setColour (juce::PopupMenu::backgroundColourId,  panel);
    lnf.setColour (juce::Label::textColourId,            dim);
    setLookAndFeel (&lnf);

    addSection ("SUBDIVISION", juce::Colour (0xff4fc3f7), { "rate", "gate", "repeats" });
    addSection ("VELOCITY",    juce::Colour (0xffffb74d), { "velMode", "velDepth", "velSteps", "velScale", "humanize" });
    addSection ("DESTINO",     juce::Colour (0xff81c784), { "channel", "noteLow", "noteHigh", "velLow", "velHigh" });

    int maxUnits = 0;
    for (auto& s : sections)
    {
        int units = 0;
        for (auto& c : s.controls) units += c->combo ? 2 : 1;
        maxUnits = juce::jmax (maxUnits, units);
    }

    setSize (pad * 2 + maxUnits * knobW + 8,
             titleH + (int) sections.size() * (header + knobH + pad) + pad);
}

void SubdivFXEditor::addSection (const juce::String& title, juce::Colour colour, const juce::StringArray& ids)
{
    Section section;
    section.title  = title;
    section.colour = colour;

    for (auto& id : ids)
    {
        auto* param = processor.apvts.getParameter (id);
        jassert (param != nullptr);

        auto c = std::make_unique<Control>();
        c->paramID = id;

        c->label = std::make_unique<juce::Label> (id, param->getName (32));
        c->label->setJustificationType (juce::Justification::centred);
        c->label->setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (*c->label);

        if (dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr)
        {
            c->combo = std::make_unique<juce::ComboBox>();
            c->combo->addItemList (param->getAllValueStrings(), 1);
            c->combo->setColour (juce::ComboBox::arrowColourId, colour);
            addAndMakeVisible (*c->combo);
            c->comboAttachment = std::make_unique<ComboAttachment> (processor.apvts, id, *c->combo);
        }
        else
        {
            c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                        juce::Slider::TextBoxBelow);
            c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW - 6, 18);
            c->slider->setColour (juce::Slider::rotarySliderFillColourId, colour);
            c->slider->setColour (juce::Slider::thumbColourId, colour.brighter (0.3f));
            c->slider->setColour (juce::Slider::rotarySliderOutlineColourId, panel.brighter (0.25f));
            c->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            c->slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (*c->slider);
            c->sliderAttachment = std::make_unique<SliderAttachment> (processor.apvts, id, *c->slider);
        }

        section.controls.push_back (std::move (c));
    }

    sections.push_back (std::move (section));
}

void SubdivFXEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    g.setColour (text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("SubdivFX", pad, 0, 200, titleH, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("subdivision + velocity MIDI FX", getWidth() - 260 - pad, 0, 260, titleH,
                juce::Justification::centredRight);

    for (auto& s : sections)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (s.bounds.toFloat(), 8.0f);
        g.setColour (s.colour);
        g.fillRoundedRectangle (s.bounds.withWidth (4).toFloat(), 2.0f);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.getX() + 14, s.bounds.getY(), 200, header,
                    juce::Justification::centredLeft);
    }
}

void SubdivFXEditor::resized()
{
    int y = titleH;
    for (auto& s : sections)
    {
        s.bounds = { pad, y, getWidth() - pad * 2, header + knobH };
        int x = pad + 8;
        for (auto& c : s.controls)
        {
            const int w = c->combo ? knobW * 2 : knobW;
            juce::Rectangle<int> cell (x, y + header - 6, w, knobH);
            c->label->setBounds (cell.removeFromTop (18));
            if (c->combo)
                c->combo->setBounds (cell.withSizeKeepingCentre (w - 24, 30).translated (0, -8));
            else
                c->slider->setBounds (cell.reduced (4, 0));
            x += w;
        }
        y += header + knobH + pad;
    }
}
