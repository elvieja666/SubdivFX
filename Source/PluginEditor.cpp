#include "PluginEditor.h"

namespace
{
    const juce::Colour bg      { 0xff16181d };
    const juce::Colour panel   { 0xff20232a };
    const juce::Colour text    { 0xffe8e8ea };
    const juce::Colour dim     { 0xff8a8f98 };
    const juce::Colour accent  { 0xff4fc3f7 };
    const juce::Colour warm    { 0xffffb74d };
    const juce::Colour green   { 0xff81c784 };
    const juce::Colour pink    { 0xfff06292 };

    constexpr int pad = 12, topH = 46, clipH = 160, sectionHeader = 26, knobH = 96,
                  knobW = 82, comboW = 128, bottomH = 58, sectionInset = 12;
}

// ============================================================================
//  ClipView
// ============================================================================
ClipView::ClipView (SubdivFXProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::Colour ClipView::velocityColour (int velocity)
{
    // como en Logic: azul = suave, rojo = fuerte
    const float t = juce::jlimit (0.0f, 1.0f, (float) (velocity - 1) / 126.0f);
    return juce::Colour::fromHSV (0.62f * (1.0f - t), 0.68f, 0.92f, 1.0f);
}

juce::File ClipView::makeExportFile()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
               .getChildFile ("SubdivFX")
               .getChildFile ("SubdivFX_" + juce::Time::getCurrentTime().formatted ("%H%M%S") + ".mid");
}

void ClipView::paint (juce::Graphics& g)
{
    const auto notes  = proc.buildPattern();
    const double len  = proc.patternLength();
    auto bounds       = getLocalBounds().toFloat();

    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    auto area = bounds.reduced (14.0f, 16.0f).withTrimmedBottom (14.0f);

    // Rango de pitch
    int lo = 127, hi = 0;
    for (const auto& n : notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
    if (notes.empty()) { lo = hi = 60; }
    const int rows = juce::jmax (7, hi - lo + 3);
    const int top  = hi + (rows - (hi - lo)) / 2;
    const float rowH = area.getHeight() / (float) rows;

    auto beatToX  = [&] (double b) { return area.getX() + (float) (b / len) * area.getWidth(); };
    auto pitchToY = [&] (int p)    { return area.getY() + (float) (top - p) * rowH; };

    // Grilla: compases, negras y semicorcheas según el largo
    const double step = len <= 1.0 ? 0.0625 : (len <= 4.0 ? 0.25 : 1.0);
    for (double b = 0.0; b <= len + 1e-9; b += step)
    {
        const bool isBar  = std::abs (std::fmod (b, 4.0)) < 1e-6;
        const bool isBeat = std::abs (b - std::round (b)) < 1e-6;
        g.setColour (isBar ? juce::Colour (0xff4a4f59) : (isBeat ? juce::Colour (0xff353a43) : juce::Colour (0xff2a2e35)));
        g.drawVerticalLine ((int) beatToX (b), area.getY(), area.getBottom());
    }

    // Notas
    for (const auto& n : notes)
    {
        const float x0 = beatToX (n.start), x1 = beatToX (n.end());
        juce::Rectangle<float> r (x0, pitchToY (n.pitch) + 1.0f, juce::jmax (2.0f, x1 - x0 - 1.0f), juce::jmax (3.0f, rowH - 2.0f));
        g.setColour (velocityColour (n.velocity));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
    }

    // Indicación de arrastre
    g.setColour (hover ? text : dim);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText ("Arrastra este clip a tu pista", bounds.reduced (14.0f, 6.0f).removeFromBottom (16.0f),
                juce::Justification::centredRight);

    g.setColour (hover ? accent : accent.withAlpha (0.35f));
    const float dash[] = { 5.0f, 4.0f };
    juce::Path outline, dashed;
    outline.addRoundedRectangle (bounds.reduced (1.0f), 8.0f);
    juce::PathStrokeType (hover ? 1.6f : 1.1f).createDashedStroke (dashed, outline, dash, 2);
    g.fillPath (dashed);
}

void ClipView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5)
        return;

    const auto file = makeExportFile();
    if (! proc.exportMidi (file))
        return;

    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (file.getFullPathName()), false, this,
        [safe = juce::Component::SafePointer<ClipView> (this)]
        {
            if (safe != nullptr) safe->dragging = false;
        });
}

// ============================================================================
//  Editor
// ============================================================================
int SubdivFXEditor::Section::contentWidth() const
{
    int w = 0;
    for (auto& c : controls) w += c->combo ? comboW : knobW;
    return w + sectionInset * 2;
}

SubdivFXEditor::SubdivFXEditor (SubdivFXProcessor& p)
    : AudioProcessorEditor (&p), proc (p), clipView (p)
{
    lnf.setColour (juce::Slider::textBoxTextColourId,    text);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::ComboBox::backgroundColourId,   panel.brighter (0.1f));
    lnf.setColour (juce::ComboBox::textColourId,         text);
    lnf.setColour (juce::ComboBox::outlineColourId,      juce::Colours::transparentBlack);
    lnf.setColour (juce::PopupMenu::backgroundColourId,  panel);
    lnf.setColour (juce::Label::textColourId,            dim);
    lnf.setColour (juce::TextButton::buttonColourId,     panel.brighter (0.12f));
    lnf.setColour (juce::TextButton::textColourOffId,    text);
    lnf.setColour (juce::TextButton::textColourOnId,     juce::Colours::white);
    setLookAndFeel (&lnf);

    addAndMakeVisible (clipView);

    addSection ("CLIP",        green,  0, { "rootNote", "length", "velocity" });
    addSection ("PITCH",       pink,   0, { "pitchDir", "pitchStep" });
    addSection ("SUBDIVISION", accent, 1, { "mode", "rate", "parts", "rhythm", "rhythmAmt", "gate" });
    addSection ("VELOCITY",    warm,   2, { "velMode", "velDepth", "velSteps", "humanize" });

    previewBtn.setColour (juce::TextButton::buttonColourId, accent.darker (0.3f));
    previewBtn.setColour (juce::TextButton::buttonOnColourId, accent.darker (0.1f));
    previewBtn.onClick = [this]
    {
        if (proc.isPreviewing()) proc.stopPreview();
        else                     proc.startPreview();
    };
    variationBtn.setTooltip ("Nueva variacion para Aleatorio y Humanizar");
    variationBtn.onClick = [this] { proc.newVariation(); };

    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Guardar MIDI",
                     juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("SubdivFX.mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) proc.exportMidi (f.withFileExtension (".mid"));
                              });
    };

    for (auto* b : { &previewBtn, &variationBtn, &saveBtn })
        addAndMakeVisible (*b);

    status.setJustificationType (juce::Justification::centredRight);
    status.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (status);

    // Ancho según la fila más ancha
    int maxRow = 0;
    for (int row = 0; row < 3; ++row)
    {
        int w = 0, count = 0;
        for (auto& s : sections)
            if (s.row == row) { w += s.contentWidth(); ++count; }
        maxRow = juce::jmax (maxRow, w + (count - 1) * pad);
    }

    setSize (juce::jmax (680, maxRow + pad * 2),
             topH + clipH + pad + 3 * (sectionHeader + knobH + pad) + bottomH);
    startTimerHz (20);
    timerCallback();
}

SubdivFXEditor::~SubdivFXEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SubdivFXEditor::addSection (const juce::String& title, juce::Colour colour, int row, const juce::StringArray& ids)
{
    Section section;
    section.title  = title;
    section.colour = colour;
    section.row    = row;

    for (auto& id : ids)
    {
        auto* param = proc.apvts.getParameter (id);
        jassert (param != nullptr);

        auto c = std::make_unique<Control>();
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
            c->comboAttachment = std::make_unique<ComboAttachment> (proc.apvts, id, *c->combo);
        }
        else
        {
            c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                        juce::Slider::TextBoxBelow);
            c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW - 4, 18);
            c->slider->setColour (juce::Slider::rotarySliderFillColourId, colour);
            c->slider->setColour (juce::Slider::thumbColourId, colour.brighter (0.3f));
            c->slider->setColour (juce::Slider::rotarySliderOutlineColourId, panel.brighter (0.25f));
            c->slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            c->slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (*c->slider);
            c->sliderAttachment = std::make_unique<SliderAttachment> (proc.apvts, id, *c->slider);
        }

        section.controls.push_back (std::move (c));
    }

    sections.push_back (std::move (section));
}

void SubdivFXEditor::timerCallback()
{
    const bool playing = proc.isPreviewing();
    previewBtn.setToggleState (playing, juce::dontSendNotification);
    previewBtn.setButtonText (playing ? "Detener" : "Probar");

    const int v = proc.getVersion();
    if (v == lastVersion) return;
    lastVersion = v;

    const auto notes = proc.buildPattern();
    int lo = 127, hi = 0;
    for (const auto& n : notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }

    juce::String s = juce::String ((int) notes.size()) + " notas";
    if (! notes.empty() && hi != lo)
        s << "  |  " << juce::MidiMessage::getMidiNoteName (notes.front().pitch, true, true, 3)
          << " a " << juce::MidiMessage::getMidiNoteName (notes.back().pitch, true, true, 3);
    status.setText (s, juce::dontSendNotification);

    clipView.repaint();
}

void SubdivFXEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    g.setColour (text);
    g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    g.drawText ("SubdivFX", pad, 0, 160, topH, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("generador de subdivisiones MIDI", getWidth() - 260 - pad, 0, 260, topH,
                juce::Justification::centredRight);

    for (auto& s : sections)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (s.bounds.toFloat(), 8.0f);
        g.setColour (s.colour);
        g.fillRoundedRectangle (s.bounds.withWidth (4).toFloat(), 2.0f);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.getX() + 14, s.bounds.getY() + 2, 200, sectionHeader,
                    juce::Justification::centredLeft);
    }
}

void SubdivFXEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (topH);

    clipView.setBounds (area.removeFromTop (clipH).reduced (pad, 0));
    area.removeFromTop (pad);

    for (int row = 0; row < 3; ++row)
    {
        auto rowArea = area.removeFromTop (sectionHeader + knobH).reduced (pad, 0);
        area.removeFromTop (pad);

        std::vector<Section*> inRow;
        for (auto& s : sections)
            if (s.row == row) inRow.push_back (&s);

        for (size_t i = 0; i < inRow.size(); ++i)
        {
            auto& s = *inRow[i];
            const bool last = i + 1 == inRow.size();
            s.bounds = last ? rowArea : rowArea.removeFromLeft (s.contentWidth());
            if (! last) rowArea.removeFromLeft (pad);

            int x = s.bounds.getX() + sectionInset;
            const int y = s.bounds.getY() + sectionHeader - 4;
            for (auto& c : s.controls)
            {
                const int w = c->combo ? comboW : knobW;
                juce::Rectangle<int> cell (x, y, w, knobH);
                c->label->setBounds (cell.removeFromTop (18));
                if (c->combo)
                    c->combo->setBounds (cell.withSizeKeepingCentre (w - 12, 28).translated (0, -10));
                else
                    c->slider->setBounds (cell.reduced (2, 0));
                x += w;
            }
        }
    }

    auto bottom = area.removeFromTop (bottomH).reduced (pad, 10);
    previewBtn.setBounds   (bottom.removeFromLeft (120)); bottom.removeFromLeft (10);
    variationBtn.setBounds (bottom.removeFromLeft (90));  bottom.removeFromLeft (10);
    saveBtn.setBounds      (bottom.removeFromLeft (120));
    status.setBounds       (bottom);
}
