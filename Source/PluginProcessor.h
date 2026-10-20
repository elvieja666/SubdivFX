#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// ============================================================================
//  SubdivFX — efecto MIDI que subdivide notas (repeticiones sincronizadas al
//  tempo) y les aplica curvas de velocity. Sólo procesa las notas que pasan el
//  filtro "Destino" (canal, rango de notas, rango de velocity); el resto pasa
//  sin cambios.
// ============================================================================
class SubdivFXProcessor : public juce::AudioProcessor
{
public:
    SubdivFXProcessor();
    ~SubdivFXProcessor() override = default;

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

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct Voice
    {
        bool active = false;
        bool isOn   = false;   // ¿la subdivisión actual está sonando?
        int  channel = 1;
        int  note    = 60;
        int  baseVelocity = 100;
        int  step    = 0;      // índice de subdivisión (0 = la primera)
        int  offIn   = -1;     // muestras hasta el note-off de la subdivisión (-1 = nada)
        int  nextIn  = -1;     // muestras hasta la próxima subdivisión (-1 = terminó)
    };

    static constexpr int maxVoices = 64;
    std::array<Voice, maxVoices> voices;

    juce::Random random;
    double currentSampleRate = 44100.0;
    bool   wasPlaying = false;

    // Parámetros (punteros cacheados, lectura lock-free en el audio thread)
    std::atomic<float>* pRate     = nullptr;
    std::atomic<float>* pGate     = nullptr;
    std::atomic<float>* pRepeats  = nullptr;
    std::atomic<float>* pVelMode  = nullptr;
    std::atomic<float>* pVelDepth = nullptr;
    std::atomic<float>* pVelSteps = nullptr;
    std::atomic<float>* pVelScale = nullptr;
    std::atomic<float>* pHumanize = nullptr;
    std::atomic<float>* pChannel  = nullptr;
    std::atomic<float>* pNoteLow  = nullptr;
    std::atomic<float>* pNoteHigh = nullptr;
    std::atomic<float>* pVelLow   = nullptr;
    std::atomic<float>* pVelHigh  = nullptr;

    bool  matchesTarget (const juce::MidiMessage&) const;
    int   velocityForStep (int baseVelocity, int step);
    Voice* findVoice (int channel, int note);
    Voice* freeVoice();
    void  killAllVoices (juce::MidiBuffer& out, int samplePos);
    void  startSubdivision (Voice&, juce::MidiBuffer& out, int samplePos, int gateSamples, int intervalSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubdivFXProcessor)
};
