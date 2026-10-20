#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PianoRoll.h"

// Zona desde la que se arrastra el resultado (.mid) hacia el DAW
class DragOutBox : public juce::Component
{
public:
    explicit DragOutBox (SubdivFXProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    static juce::File makeExportFile();

private:
    SubdivFXProcessor& proc;
    bool dragging = false;
};

// ============================================================================
class SubdivFXEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::ChangeListener
{
public:
    explicit SubdivFXEditor (SubdivFXProcessor&);
    ~SubdivFXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // Arrastrar archivos MIDI hacia el plugin
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Control
    {
        std::unique_ptr<juce::Label>      label;
        std::unique_ptr<juce::Slider>     slider;
        std::unique_ptr<juce::ComboBox>   combo;
        std::unique_ptr<SliderAttachment> sliderAttachment;
        std::unique_ptr<ComboAttachment>  comboAttachment;
    };

    struct Section
    {
        juce::String title;
        juce::Colour colour;
        std::vector<std::unique_ptr<Control>> controls;
        juce::Rectangle<int> bounds;
    };

    SubdivFXProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    juce::TextButton captureBtn { "Capturar" }, openBtn { "Abrir .mid" },
                     selectAllBtn { "Seleccionar todo" }, undoBtn { "Deshacer" }, clearBtn { "Limpiar" },
                     generateBtn { "GENERAR" }, saveBtn { "Guardar .mid" };
    PianoRoll  pianoRoll;
    DragOutBox dragOut;
    juce::Label status;
    std::vector<Section> sections;
    std::unique_ptr<juce::FileChooser> chooser;

    void addSection (const juce::String& title, juce::Colour colour, const juce::StringArray& ids);
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    static bool isMidiFile (const juce::String& path);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubdivFXEditor)
};
