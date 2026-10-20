#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ============================================================================
//  Mini piano roll: muestra las notas del plugin y permite seleccionarlas.
//  · Clic en una nota: la selecciona (Shift+clic suma / quita).
//  · Arrastrar en un lugar vacío: selección con rectángulo (Shift suma).
//  · Clic en un lugar vacío: deselecciona todo.
// ============================================================================
class PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (SubdivFXProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    void setDropHighlight (bool shouldHighlight);
    static juce::Colour velocityColour (int velocity);

private:
    SubdivFXProcessor& proc;

    // Vista (se recalcula a partir de las notas en cada repaint)
    double beatStart = 0.0, beatEnd = 8.0;
    int    lowPitch = 48, highPitch = 72;

    bool marqueeActive = false;
    juce::Rectangle<int> marquee;
    std::vector<bool> selectionAtDragStart;
    bool dropHighlight = false;

    static constexpr int keyW = 38, rulerH = 18, velLaneH = 48;

    void updateView();
    juce::Rectangle<int> rollArea() const;
    juce::Rectangle<int> velArea() const;
    float beatToX (double beat) const;
    float pitchToY (int pitch) const;     // borde superior de la fila
    float rowHeight() const;
    juce::Rectangle<float> noteRect (const Note&) const;
    int noteAt (juce::Point<int>) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
