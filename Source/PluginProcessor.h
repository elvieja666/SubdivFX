#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "NoteModel.h"

// ============================================================================
//  SubdivFX — generador de clips de subdivisión.
//  Se ajustan los parámetros y el clip resultante se arrastra a la pista.
//  "Probar" lo toca por la salida MIDI (el instrumento de la pista suena).
//  El MIDI que entra al plugin pasa sin cambios.
// ============================================================================
class SubdivFXProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener
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

    // --- Clip (message thread) ---
    NoteList buildPattern() const;
    double   patternLength() const;          // largo del clip en beats
    bool     exportMidi (const juce::File&) const;
    void     newVariation();                 // nueva semilla para Aleatorio / Humanizar
    int      getVersion() const { return version.load(); }

    // --- Probar ---
    void startPreview();
    void stopPreview();
    bool isPreviewing() const { return previewActive.load(); }

private:
    struct PrevEv { double beat; int pitch; int velocity; bool isOn; };

    juce::SpinLock previewLock;
    std::vector<PrevEv> pending, playing;    // pending: lo pide la UI; playing: audio thread
    bool hasPending = false, stopRequested = false;
    size_t playIdx = 0;
    double playBeat = 0.0;
    std::array<bool, 128> active {};
    std::atomic<bool> previewActive { false };

    std::atomic<double> bpm { 120.0 };
    double sampleRateHz = 44100.0;

    std::atomic<juce::int64> seed { 20261020 };
    std::atomic<int> version { 0 };

    void parameterChanged (const juce::String&, float) override { ++version; }
    void releaseActive (juce::MidiBuffer&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubdivFXProcessor)
};
