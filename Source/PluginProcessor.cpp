#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::StringArray rateNames  { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
    const double            rateBeats[] { 1.0,   0.5,   1.0/3,  0.25,   1.0/6,   0.125,  1.0/12,  0.0625 };

    const juce::StringArray lengthNames  { "1/16", "1/8", "1/4", "1/2", "1 compas", "2 compases", "4 compases" };
    const double            lengthBeats[] { 0.25,   0.5,   1.0,   2.0,   4.0,        8.0,          16.0 };

    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }

    const char* paramIDs[] { "rootNote", "length", "velocity", "pitchDir", "pitchStep",
                             "mode", "rate", "parts", "rhythm", "rhythmAmt", "gate",
                             "velMode", "velDepth", "velSteps", "humanize" };
}

// ----------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout SubdivFXProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pctText);

    // --- Clip ---
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "rootNote", 1 }, "Nota", 0, 127, 60,
                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                        { return MidiMessage::getMidiNoteName (v, true, true, 3); })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "length", 1 }, "Largo", lengthNames, 3));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "velocity", 1 }, "Velocity", 1, 127, 110));

    // --- Pitch ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitchDir", 1 }, "Pitch",
                    StringArray { "Fijo", "Sube", "Baja" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "pitchStep", 1 }, "Paso", 1, 12, 1,
                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                        { return String (v) + " st"; })));

    // --- Subdivisión ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modo",
                    StringArray { "Division", "Partes iguales" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Division", rateNames, 5));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { "parts", 1 }, "Partes", 2, 32, 6));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rhythm", 1 }, "Ritmo",
                    StringArray { "Parejo", "Acelerando", "Frenando" }, 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "rhythmAmt", 1 }, "Curva ritmo",
                    NormalisableRange<float> (0.f, 100.f, 1.f), 50.f, pct));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "gate", 1 }, "Gate",
                    NormalisableRange<float> (5.f, 100.f, 1.f), 90.f, pct));

    // --- Velocity ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "velMode", 1 }, "Curva vel.",
                    StringArray { "Fija", "Rampa sube", "Rampa baja", "Acento", "Aleatorio" }, 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "velDepth", 1 }, "Profundidad",
                    NormalisableRange<float> (0.f, 100.f, 1.f), 60.f, pct));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { "velSteps", 1 }, "Largo curva", 0, 16, 0,
                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                        { return v == 0 ? String ("Todo el clip") : String (v) + " golpes"; })));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "humanize", 1 }, "Humanizar",
                    NormalisableRange<float> (0.f, 50.f, 1.f), 0.f, pct));

    return layout;
}

// ----------------------------------------------------------------------------
SubdivFXProcessor::SubdivFXProcessor()
    : AudioProcessor (BusesProperties()),   // efecto MIDI: sin buses de audio
      apvts (*this, nullptr, "SubdivFX", createLayout())
{
    for (auto* id : paramIDs)
        apvts.addParameterListener (id, this);
}

SubdivFXProcessor::~SubdivFXProcessor()
{
    for (auto* id : paramIDs)
        apvts.removeParameterListener (id, this);
}

void SubdivFXProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
}

// ----------------------------------------------------------------------------
//  Clip
// ----------------------------------------------------------------------------
double SubdivFXProcessor::patternLength() const
{
    const int idx = (int) apvts.getRawParameterValue ("length")->load();
    return lengthBeats[juce::jlimit (0, lengthNames.size() - 1, idx)];
}

NoteList SubdivFXProcessor::buildPattern() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    Note base;
    base.start    = 0.0;
    base.length   = patternLength();
    base.pitch    = (int) get ("rootNote");
    base.velocity = (int) get ("velocity");

    const int dir  = (int) get ("pitchDir");
    const int step = (int) get ("pitchStep");

    GenSettings g;
    g.mode      = (int) get ("mode");
    g.division  = rateBeats[juce::jlimit (0, rateNames.size() - 1, (int) get ("rate"))];
    g.parts     = (int) get ("parts");
    g.rhythm    = (int) get ("rhythm");
    g.rhythmAmt = get ("rhythmAmt") / 100.0f;
    g.gate      = get ("gate") / 100.0f;
    g.pitchStep = dir == 1 ? step : (dir == 2 ? -step : 0);
    g.curve     = (int) get ("velMode");
    g.depth     = get ("velDepth") / 100.0f;
    g.curveLen  = (int) get ("velSteps");
    g.humanize  = get ("humanize") / 100.0f;

    juce::Random rnd (seed.load());
    return NoteGen::subdivide (base, g, rnd);
}

bool SubdivFXProcessor::exportMidi (const juce::File& file) const
{
    return NoteGen::writeMidiFile (buildPattern(), file);
}

void SubdivFXProcessor::newVariation()
{
    seed.store (juce::Random::getSystemRandom().nextInt64());
    ++version;
}

// ----------------------------------------------------------------------------
//  Probar
// ----------------------------------------------------------------------------
void SubdivFXProcessor::startPreview()
{
    std::vector<PrevEv> ev;
    for (const auto& n : buildPattern())
    {
        ev.push_back ({ n.start, n.pitch, n.velocity, true });
        ev.push_back ({ n.end(), n.pitch, 0, false });
    }
    // a igual tiempo, primero los note off (notas repetidas en legato)
    std::sort (ev.begin(), ev.end(), [] (const PrevEv& a, const PrevEv& b)
               { return a.beat < b.beat || (! (b.beat < a.beat) && ! a.isOn && b.isOn); });

    const juce::SpinLock::ScopedLockType lock (previewLock);
    pending = std::move (ev);
    hasPending = true;
    stopRequested = false;
}

void SubdivFXProcessor::stopPreview()
{
    const juce::SpinLock::ScopedLockType lock (previewLock);
    stopRequested = true;
    hasPending = false;
}

void SubdivFXProcessor::releaseActive (juce::MidiBuffer& midi)
{
    for (int p = 0; p < 128; ++p)
        if (active[(size_t) p])
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, p), 0);
            active[(size_t) p] = false;
        }
}

void SubdivFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm.store (juce::jlimit (20.0, 999.0, *b));

    {
        const juce::SpinLock::ScopedTryLockType lock (previewLock);
        if (lock.isLocked())
        {
            if (stopRequested)
            {
                releaseActive (midi);
                playing.clear();             // no libera memoria (clear mantiene la capacidad)
                previewActive.store (false);
                stopRequested = false;
            }
            if (hasPending)
            {
                releaseActive (midi);
                std::swap (playing, pending); // sin reservar memoria en el audio thread
                hasPending = false;
                playIdx  = 0;
                playBeat = 0.0;
                previewActive.store (true);
            }
        }
    }

    if (! previewActive.load() || numSamples <= 0)
        return;

    const double beatsPerSample = bpm.load() / 60.0 / sampleRateHz;
    const double blockEnd = playBeat + numSamples * beatsPerSample;

    while (playIdx < playing.size() && playing[playIdx].beat < blockEnd)
    {
        const auto& e = playing[playIdx];
        const int s = juce::jlimit (0, numSamples - 1, (int) ((e.beat - playBeat) / beatsPerSample));

        if (e.isOn)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, e.pitch, (juce::uint8) e.velocity), s);
            active[(size_t) e.pitch] = true;
        }
        else
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, e.pitch), s);
            active[(size_t) e.pitch] = false;
        }
        ++playIdx;
    }

    playBeat = blockEnd;
    if (playIdx >= playing.size())
        previewActive.store (false);
}

// ----------------------------------------------------------------------------
//  Estado
// ----------------------------------------------------------------------------
void SubdivFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("seed", juce::String (seed.load()), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SubdivFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (tree.hasProperty ("seed"))
        seed.store (tree.getProperty ("seed").toString().getLargeIntValue());

    apvts.replaceState (tree);
    ++version;
}

juce::AudioProcessorEditor* SubdivFXProcessor::createEditor()
{
    return new SubdivFXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubdivFXProcessor();
}
