#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Divisiones de compás disponibles y su duración en negras (beats)
    const juce::StringArray rateNames  { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
    const double            rateBeats[] { 1.0,   0.5,   1.0/3,  0.25,   1.0/6,   0.125,  1.0/12,  0.0625 };

    const juce::StringArray velModeNames { "Escalar", "Rampa sube", "Rampa baja", "Acento", "Aleatorio" };
    enum VelMode { Scale = 0, RampUp, RampDown, Accent, RandomMode };

    juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); }
}

// ----------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout SubdivFXProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withLabel ("%")
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    auto noteAttr = AudioParameterIntAttributes()
                        .withStringFromValueFunction ([] (int v, int) { return noteName (v); });

    // --- Subdivisión ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Division", rateNames, 3));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "gate", 1 }, "Gate",
                                                        NormalisableRange<float> (5.f, 100.f, 1.f), 75.f, pct));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { "repeats", 1 }, "Repeticiones", 0, 32, 0,
                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                        { return v == 0 ? String ("Mientras suene") : String (v); })));

    // --- Velocity ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "velMode", 1 }, "Curva vel.", velModeNames, 2));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "velDepth", 1 }, "Profundidad",
                                                        NormalisableRange<float> (0.f, 100.f, 1.f), 50.f, pct));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { "velSteps", 1 }, "Largo curva", 2, 16, 4));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "velScale", 1 }, "Escala vel.",
                                                        NormalisableRange<float> (10.f, 200.f, 1.f), 100.f, pct));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "humanize", 1 }, "Humanizar",
                                                        NormalisableRange<float> (0.f, 50.f, 1.f), 0.f, pct));

    // --- Destino (qué notas se procesan) ---
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "channel", 1 }, "Canal", 0, 16, 0,
                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                        { return v == 0 ? String ("Todos") : String (v); })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "noteLow", 1 },  "Nota min", 0, 127, 0,   noteAttr));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "noteHigh", 1 }, "Nota max", 0, 127, 127, noteAttr));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "velLow", 1 },   "Vel min",  1, 127, 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "velHigh", 1 },  "Vel max",  1, 127, 127));

    return layout;
}

// ----------------------------------------------------------------------------
SubdivFXProcessor::SubdivFXProcessor()
    : AudioProcessor (BusesProperties()),   // efecto MIDI: sin buses de audio
      apvts (*this, nullptr, "SubdivFX", createLayout())
{
    pRate     = apvts.getRawParameterValue ("rate");
    pGate     = apvts.getRawParameterValue ("gate");
    pRepeats  = apvts.getRawParameterValue ("repeats");
    pVelMode  = apvts.getRawParameterValue ("velMode");
    pVelDepth = apvts.getRawParameterValue ("velDepth");
    pVelSteps = apvts.getRawParameterValue ("velSteps");
    pVelScale = apvts.getRawParameterValue ("velScale");
    pHumanize = apvts.getRawParameterValue ("humanize");
    pChannel  = apvts.getRawParameterValue ("channel");
    pNoteLow  = apvts.getRawParameterValue ("noteLow");
    pNoteHigh = apvts.getRawParameterValue ("noteHigh");
    pVelLow   = apvts.getRawParameterValue ("velLow");
    pVelHigh  = apvts.getRawParameterValue ("velHigh");
}

void SubdivFXProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    for (auto& v : voices) v = Voice{};
    wasPlaying = false;
}

// ----------------------------------------------------------------------------
bool SubdivFXProcessor::matchesTarget (const juce::MidiMessage& m) const
{
    const int ch = (int) pChannel->load();
    if (ch != 0 && m.getChannel() != ch) return false;

    const int lo = (int) pNoteLow->load(), hi = (int) pNoteHigh->load();
    const int n  = m.getNoteNumber();
    if (n < juce::jmin (lo, hi) || n > juce::jmax (lo, hi)) return false;

    const int vlo = (int) pVelLow->load(), vhi = (int) pVelHigh->load();
    const int vel = m.getVelocity();
    return vel >= juce::jmin (vlo, vhi) && vel <= juce::jmax (vlo, vhi);
}

int SubdivFXProcessor::velocityForStep (int baseVelocity, int step)
{
    const int   steps = juce::jmax (2, (int) pVelSteps->load());
    const float depth = pVelDepth->load() / 100.f;
    const int   pos   = step % steps;
    const float t     = (float) pos / (float) (steps - 1);   // 0..1 dentro del ciclo

    float f = 1.f;
    switch ((int) pVelMode->load())
    {
        case RampUp:     f = 1.f - depth * (1.f - t);                 break;
        case RampDown:   f = 1.f - depth * t;                         break;
        case Accent:     f = (pos == 0) ? 1.f : 1.f - depth;          break;
        case RandomMode: f = 1.f - depth * random.nextFloat();        break;
        case Scale:
        default:         f = 1.f;                                     break;
    }

    float vel = (float) baseVelocity * f * (pVelScale->load() / 100.f);

    const float hum = pHumanize->load() / 100.f;
    if (hum > 0.f)
        vel *= 1.f + hum * (random.nextFloat() * 2.f - 1.f);

    return juce::jlimit (1, 127, juce::roundToInt (vel));
}

SubdivFXProcessor::Voice* SubdivFXProcessor::findVoice (int channel, int note)
{
    for (auto& v : voices)
        if (v.active && v.channel == channel && v.note == note)
            return &v;
    return nullptr;
}

SubdivFXProcessor::Voice* SubdivFXProcessor::freeVoice()
{
    for (auto& v : voices)
        if (! v.active)
            return &v;
    return nullptr;
}

void SubdivFXProcessor::killAllVoices (juce::MidiBuffer& out, int samplePos)
{
    for (auto& v : voices)
    {
        if (v.active && v.isOn)
            out.addEvent (juce::MidiMessage::noteOff (v.channel, v.note), samplePos);
        v = Voice{};
    }
}

void SubdivFXProcessor::startSubdivision (Voice& v, juce::MidiBuffer& out, int samplePos,
                                          int gateSamples, int intervalSamples)
{
    const auto vel = (juce::uint8) velocityForStep (v.baseVelocity, v.step);
    out.addEvent (juce::MidiMessage::noteOn (v.channel, v.note, vel), samplePos);
    v.isOn   = true;
    v.offIn  = gateSamples;
    v.nextIn = intervalSamples;
}

// ----------------------------------------------------------------------------
void SubdivFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    // --- Tempo y transporte ---
    double bpm = 120.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            playing = pos->getIsPlaying();
        }

    juce::MidiBuffer out;

    // Al detener el transporte, cortamos todo para no dejar notas colgadas
    if (wasPlaying && ! playing)
        killAllVoices (out, 0);
    wasPlaying = playing;

    const int    rateIdx  = juce::jlimit (0, rateNames.size() - 1, (int) pRate->load());
    const double interval = currentSampleRate * 60.0 / bpm * rateBeats[rateIdx];
    const int intervalSamples = juce::jmax (2, juce::roundToInt (interval));
    const int gateSamples     = juce::jlimit (1, intervalSamples,
                                              juce::roundToInt (intervalSamples * pGate->load() / 100.f));
    const int maxRepeats      = (int) pRepeats->load();

    auto handleEvent = [&] (const juce::MidiMessage& m, int s)
    {
        if (m.isNoteOn())
        {
            if (! matchesTarget (m)) { out.addEvent (m, s); return; }

            Voice* v = findVoice (m.getChannel(), m.getNoteNumber());
            if (v != nullptr && v->isOn)
                out.addEvent (juce::MidiMessage::noteOff (v->channel, v->note), s);
            if (v == nullptr) v = freeVoice();
            if (v == nullptr) { out.addEvent (m, s); return; }   // sin voces libres: pasa tal cual

            *v = Voice{};
            v->active       = true;
            v->channel      = m.getChannel();
            v->note         = m.getNoteNumber();
            v->baseVelocity = m.getVelocity();
            startSubdivision (*v, out, s, gateSamples, intervalSamples);
        }
        else if (m.isNoteOff())
        {
            if (Voice* v = findVoice (m.getChannel(), m.getNoteNumber()))
            {
                if (v->isOn) out.addEvent (m, s);
                *v = Voice{};
            }
            else
            {
                out.addEvent (m, s);
            }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            killAllVoices (out, s);
            out.addEvent (m, s);
        }
        else
        {
            out.addEvent (m, s);   // CC, pitch bend, etc. pasan sin cambios
        }
    };

    auto it = midi.begin();
    for (int s = 0; s < numSamples; ++s)
    {
        // 1) Eventos entrantes en esta muestra
        while (it != midi.end() && (*it).samplePosition <= s)
        {
            handleEvent ((*it).getMessage(), s);
            ++it;
        }

        // 2) Avanzar las voces activas
        for (auto& v : voices)
        {
            if (! v.active) continue;

            if (v.isOn && v.offIn == 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (v.channel, v.note), s);
                v.isOn  = false;
                v.offIn = -1;
            }

            if (v.nextIn == 0)
            {
                if (v.isOn)
                {
                    out.addEvent (juce::MidiMessage::noteOff (v.channel, v.note), s);
                    v.isOn = false;
                }

                ++v.step;
                if (maxRepeats > 0 && v.step >= maxRepeats)
                    v.nextIn = -1;   // llegó al máximo: queda en silencio hasta soltar la nota
                else
                    startSubdivision (v, out, s, gateSamples, intervalSamples);
            }

            if (v.offIn  > 0) --v.offIn;
            if (v.nextIn > 0) --v.nextIn;
        }
    }

    // Eventos rezagados (posición fuera del bloque)
    for (; it != midi.end(); ++it)
        handleEvent ((*it).getMessage(), juce::jmax (0, numSamples - 1));

    midi.swapWith (out);
}

// ----------------------------------------------------------------------------
void SubdivFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SubdivFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SubdivFXProcessor::createEditor()
{
    return new SubdivFXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubdivFXProcessor();
}
