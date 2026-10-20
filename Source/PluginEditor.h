#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SubdivFXEditor : public juce::AudioProcessorEditor
{
public:
    explicit SubdivFXEditor (SubdivFXProcessor&);
    ~SubdivFXEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Control
    {
        juce::String paramID;
        std::unique_ptr<juce::Label>    label;
        std::unique_ptr<juce::Slider>   slider;
        std::unique_ptr<juce::ComboBox> combo;
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

    SubdivFXProcessor& processor;
    juce::LookAndFeel_V4 lnf;
    std::vector<Section> sections;

    void addSection (const juce::String& title, juce::Colour colour, const juce::StringArray& ids);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubdivFXEditor)
};
