#include "PluginEditor.h"

namespace
{
    const juce::Colour bg     { 0xff16181d };
    const juce::Colour panel  { 0xff20232a };
    const juce::Colour text   { 0xffe8e8ea };
    const juce::Colour dim    { 0xff8a8f98 };
    const juce::Colour accent { 0xff4fc3f7 };
    const juce::Colour hot    { 0xffef5350 };
    const juce::Colour warm   { 0xffffb74d };

    constexpr int pad = 12, topH = 46, rollH = 290, sectionHeader = 26, knobH = 96,
                  knobW = 82, comboW = 128, bottomH = 58;
}

// ============================================================================
//  DragOutBox
// ============================================================================
juce::File DragOutBox::makeExportFile()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("SubdivFX")
               .getChildFile ("SubdivFX_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".mid");
}

void DragOutBox::paint (juce::Graphics& g)
{
    const bool hasNotes = ! proc.getNotes().empty();
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (panel.brighter (0.06f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (hasNotes ? accent : dim.withAlpha (0.5f));
    const float dash[] = { 5.0f, 4.0f };
    juce::Path outline, dashed;
    outline.addRoundedRectangle (r, 6.0f);
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dash, 2);
    g.fillPath (dashed);

    g.setColour (hasNotes ? text : dim);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (hasNotes ? "Arrastra esto a tu pista" : "Sin notas para arrastrar",
                getLocalBounds(), juce::Justification::centred);
}

void DragOutBox::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || proc.getNotes().empty())
        return;

    const auto file = makeExportFile();
    if (! proc.exportMidi (file))
        return;

    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (file.getFullPathName()), false, this,
                                                                [safe = juce::Component::SafePointer<DragOutBox> (this)]
                                                                {
                                                                    if (safe != nullptr) safe->dragging = false;
                                                                });
}

// ============================================================================
//  Editor
// ============================================================================
SubdivFXEditor::SubdivFXEditor (SubdivFXProcessor& p)
    : AudioProcessorEditor (&p), proc (p), pianoRoll (p), dragOut (p)
{
    lnf.setColour (juce::Slider::textBoxTextColourId,      text);
    lnf.setColour (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
    lnf.setColour (juce::ComboBox::backgroundColourId,     panel.brighter (0.1f));
    lnf.setColour (juce::ComboBox::textColourId,           text);
    lnf.setColour (juce::ComboBox::outlineColourId,        juce::Colours::transparentBlack);
    lnf.setColour (juce::PopupMenu::backgroundColourId,    panel);
    lnf.setColour (juce::Label::textColourId,              dim);
    lnf.setColour (juce::TextButton::buttonColourId,       panel.brighter (0.12f));
    lnf.setColour (juce::TextButton::textColourOffId,      text);
    lnf.setColour (juce::TextButton::textColourOnId,       juce::Colours::white);
    setLookAndFeel (&lnf);

    // --- Barra superior ---
    for (auto* b : { &captureBtn, &openBtn, &selectAllBtn, &undoBtn, &clearBtn })
        addAndMakeVisible (*b);

    captureBtn.setColour (juce::TextButton::buttonOnColourId, hot);
    captureBtn.onClick = [this]
    {
        if (proc.isCapturing()) proc.stopCapture();
        else                    proc.startCapture();
    };

    openBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Abrir archivo MIDI", juce::File(), "*.mid;*.midi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.loadMidiFile (f);
                              });
    };

    selectAllBtn.onClick = [this] { proc.selectAll (proc.numSelected() < (int) proc.getNotes().size()); };
    undoBtn.onClick      = [this] { proc.undo(); };
    clearBtn.onClick     = [this] { proc.clearNotes(); };

    // --- Piano roll ---
    addAndMakeVisible (pianoRoll);

    // --- Controles ---
    addSection ("SUBDIVISION", accent, { "mode", "rate", "parts", "rhythm", "rhythmAmt", "gate", "pitchStep" });
    addSection ("VELOCITY",    warm,   { "velMode", "velDepth", "velSteps", "velScale", "humanize" });

    // --- Barra inferior ---
    generateBtn.setColour (juce::TextButton::buttonColourId, accent.darker (0.25f));
    generateBtn.onClick = [this] { proc.generate(); };
    addAndMakeVisible (generateBtn);
    addAndMakeVisible (dragOut);

    saveBtn.onClick = [this]
    {
        if (proc.getNotes().empty()) return;
        chooser = std::make_unique<juce::FileChooser> ("Guardar MIDI", DragOutBox::makeExportFile(), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) proc.exportMidi (f.withFileExtension (".mid"));
                              });
    };
    addAndMakeVisible (saveBtn);

    status.setJustificationType (juce::Justification::centredRight);
    status.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (status);

    // Tamaño: la sección más ancha manda
    int maxW = 0;
    for (auto& s : sections)
    {
        int w = 0;
        for (auto& c : s.controls) w += c->combo ? comboW : knobW;
        maxW = juce::jmax (maxW, w);
    }

    setWantsKeyboardFocus (true);
    proc.addChangeListener (this);
    setSize (juce::jmax (760, pad * 2 + maxW + 16),
             topH + rollH + pad + (int) sections.size() * (sectionHeader + knobH + pad) + bottomH);
    refresh();
}

SubdivFXEditor::~SubdivFXEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void SubdivFXEditor::addSection (const juce::String& title, juce::Colour colour, const juce::StringArray& ids)
{
    Section section;
    section.title  = title;
    section.colour = colour;

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

// ----------------------------------------------------------------------------
void SubdivFXEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void SubdivFXEditor::refresh()
{
    const bool cap = proc.isCapturing();
    captureBtn.setToggleState (cap, juce::dontSendNotification);
    captureBtn.setButtonText (cap ? "Detener captura" : "Capturar");
    undoBtn.setEnabled (proc.canUndo());

    const int total = (int) proc.getNotes().size();
    const int sel   = proc.numSelected();
    status.setText (total == 0 ? juce::String()
                               : juce::String (total) + " notas  |  " + juce::String (sel) + " seleccionadas",
                    juce::dontSendNotification);
    generateBtn.setButtonText (sel > 0 || total == 0 ? "GENERAR" : "GENERAR (todo)");

    pianoRoll.repaint();
    dragOut.repaint();
}

bool SubdivFXEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)) { proc.selectAll (true); return true; }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo(); return true; }
    if (key == juce::KeyPress::returnKey)                                    { proc.generate(); return true; }
    return false;
}

// ----------------------------------------------------------------------------
bool SubdivFXEditor::isMidiFile (const juce::String& path)
{
    return path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi");
}

bool SubdivFXEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isMidiFile (f)) return true;
    return false;
}

void SubdivFXEditor::fileDragEnter (const juce::StringArray&, int, int) { pianoRoll.setDropHighlight (true); }
void SubdivFXEditor::fileDragExit  (const juce::StringArray&)           { pianoRoll.setDropHighlight (false); }

void SubdivFXEditor::filesDropped (const juce::StringArray& files, int, int)
{
    pianoRoll.setDropHighlight (false);
    for (auto& f : files)
        if (isMidiFile (f))
        {
            proc.loadMidiFile (juce::File (f));
            break;
        }
}

// ----------------------------------------------------------------------------
void SubdivFXEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    g.setColour (text);
    g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    g.drawText ("SubdivFX", pad, 0, 140, topH, juce::Justification::centredLeft);

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

    // Barra superior
    auto top = area.removeFromTop (topH).reduced (pad, 8);
    clearBtn.setBounds     (top.removeFromRight (78));  top.removeFromRight (6);
    undoBtn.setBounds      (top.removeFromRight (84));  top.removeFromRight (6);
    selectAllBtn.setBounds (top.removeFromRight (130)); top.removeFromRight (6);
    openBtn.setBounds      (top.removeFromRight (96));  top.removeFromRight (6);
    captureBtn.setBounds   (top.removeFromRight (130));

    // Piano roll
    pianoRoll.setBounds (area.removeFromTop (rollH).reduced (pad, 0));
    area.removeFromTop (pad);

    // Secciones de controles
    for (auto& s : sections)
    {
        auto sb = area.removeFromTop (sectionHeader + knobH).reduced (pad, 0);
        s.bounds = sb;
        area.removeFromTop (pad);

        int x = sb.getX() + 12;
        const int y = sb.getY() + sectionHeader - 4;
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

    // Barra inferior
    auto bottom = area.removeFromTop (bottomH).reduced (pad, 10);
    generateBtn.setBounds (bottom.removeFromLeft (150));  bottom.removeFromLeft (10);
    dragOut.setBounds     (bottom.removeFromLeft (230));  bottom.removeFromLeft (10);
    saveBtn.setBounds     (bottom.removeFromLeft (110));
    status.setBounds      (bottom);
}
