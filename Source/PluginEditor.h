#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Vista previa del clip. No se edita: se agarra y se arrastra a la pista.
class ClipView : public juce::Component
{
public:
    explicit ClipView (SubdivFXProcessor&);

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDrag  (const juce::MouseEvent&) override;
    void mouseUp    (const juce::MouseEvent&) override { dragging = false; }

    static juce::Colour velocityColour (int velocity);
    static juce::File makeExportFile();

private:
    SubdivFXProcessor& proc;
    bool hover = false, dragging = false;
};

// ============================================================================
class SubdivFXEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit SubdivFXEditor (SubdivFXProcessor&);
    ~SubdivFXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

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
        int row = 0;
        std::vector<std::unique_ptr<Control>> controls;
        juce::Rectangle<int> bounds;
        int contentWidth() const;
    };

    SubdivFXProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    ClipView clipView;
    juce::TextButton previewBtn { "Probar" }, variationBtn { "Variar" }, saveBtn { "Guardar .mid" };
    juce::Label status;
    std::vector<Section> sections;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastVersion = -1;

    void addSection (const juce::String& title, juce::Colour colour, int row, const juce::StringArray& ids);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubdivFXEditor)
};
