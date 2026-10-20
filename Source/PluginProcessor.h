#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "NoteModel.h"

// ============================================================================
//  SubdivFX — editor de subdivisiones MIDI.
//  · Entrada: arrastrar un .mid / región al plugin, o capturar lo que suena.
//  · Se seleccionan notas en el mini piano roll y se aplica "Generar".
//  · Salida: arrastrar el resultado de vuelta al DAW, o guardarlo como .mid.
//  El MIDI que pasa por el plugin no se modifica (la pista suena normal).
// ============================================================================
class SubdivFXProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster,
                          private juce::Timer
{
public:
    SubdivFXProcessor();
    ~SubdivFXProcessor() override;

    // --- AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SubdivFX"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Parámetros ---
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // --- Notas (sólo desde el message thread) ---
    NoteList& getNotes() { return notes; }
    void notesChanged();                 // avisar a la UI después de tocar la selección
    void loadMidiFile (const juce::File&);
    void generate();
    void clearNotes();
    void undo();
    bool canUndo() const { return ! undoStack.empty(); }
    void selectAll (bool shouldSelect);
    int  numSelected() const;
    bool exportMidi (const juce::File&, bool onlySelected = false);
    double getBpm() const { return bpm.load(); }

    // --- Captura ---
    void startCapture();
    void stopCapture();
    bool isCapturing() const { return capturing.load(); }

private:
    struct CapEvent { double beat; int channel, pitch, velocity; bool isOn; };

    static constexpr int fifoSize = 2048;
    juce::AbstractFifo fifo { fifoSize };
    std::array<CapEvent, fifoSize> fifoData;

    std::atomic<bool>   capturing { false };
    std::atomic<double> bpm { 120.0 };
    double sampleRateHz = 44100.0;

    std::array<double, 16 * 128> openStart;   // inicio de notas abiertas (-1 = cerrada)
    std::array<int,    16 * 128> openVel;

    NoteList notes;
    std::vector<NoteList> undoStack;
    juce::Random random;

    void pushUndo();
    GenSettings currentSettings() const;
    void timerCallback() override;           // pasa lo capturado al modelo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubdivFXProcessor)
};
