#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::StringArray rateNames  { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
    const double            rateBeats[] { 1.0,   0.5,   1.0/3,  0.25,   1.0/6,   0.125,  1.0/12,  0.0625 };

    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
}

// ----------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout SubdivFXProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pctText);

    // --- Subdivisión ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modo",
                    StringArray { "Division", "Partes iguales" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Division", rateNames, 3));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { "parts", 1 }, "Partes", 2, 32, 4));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rhythm", 1 }, "Ritmo",
                    StringArray { "Parejo", "Acelerando", "Frenando" }, 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "rhythmAmt", 1 }, "Curva ritmo",
                    NormalisableRange<float> (0.f, 100.f, 1.f), 50.f, pct));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "gate", 1 }, "Gate",
                    NormalisableRange<float> (5.f, 100.f, 1.f), 90.f, pct));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { "pitchStep", 1 }, "Escalera", -12, 12, 0,
                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                        { return v == 0 ? String ("Sin escalera") : (v > 0 ? "+" : "") + String (v) + " st"; })));

    // --- Velocity ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "velMode", 1 }, "Curva vel.",
                    StringArray { "Fija", "Rampa sube", "Rampa baja", "Acento", "Aleatorio" }, 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "velDepth", 1 }, "Profundidad",
                    NormalisableRange<float> (0.f, 100.f, 1.f), 50.f, pct));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { "velSteps", 1 }, "Largo curva", 0, 16, 0,
                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                        { return v == 0 ? String ("Toda la nota") : String (v) + " golpes"; })));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "velScale", 1 }, "Escala vel.",
                    NormalisableRange<float> (10.f, 200.f, 1.f), 100.f, pct));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "humanize", 1 }, "Humanizar",
                    NormalisableRange<float> (0.f, 50.f, 1.f), 0.f, pct));

    return layout;
}

// ----------------------------------------------------------------------------
SubdivFXProcessor::SubdivFXProcessor()
    : AudioProcessor (BusesProperties()),   // efecto MIDI: sin buses de audio
      apvts (*this, nullptr, "SubdivFX", createLayout())
{
    openStart.fill (-1.0);
    openVel.fill (0);
    startTimerHz (30);
}

SubdivFXProcessor::~SubdivFXProcessor()
{
    stopTimer();
}

void SubdivFXProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
}

// ----------------------------------------------------------------------------
//  Audio thread: el MIDI pasa sin cambios; si estamos capturando, anotamos
//  cada note on/off con su posición en beats.
// ----------------------------------------------------------------------------
void SubdivFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    double ppq = 0.0;
    bool playing = false, hasPpq = false;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm.store (juce::jlimit (20.0, 999.0, *b));
            if (auto p = pos->getPpqPosition()) { ppq = *p; hasPpq = true; }
            playing = pos->getIsPlaying();
        }

    if (! capturing.load() || ! playing || ! hasPpq)
        return;

    const double samplesPerBeat = sampleRateHz * 60.0 / bpm.load();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! (m.isNoteOn() || m.isNoteOff()))
            continue;

        CapEvent e { ppq + meta.samplePosition / samplesPerBeat,
                     m.getChannel(), m.getNoteNumber(), (int) m.getVelocity(), m.isNoteOn() };

        auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)      fifoData[(size_t) scope.startIndex1] = e;
        else if (scope.blockSize2 > 0) fifoData[(size_t) scope.startIndex2] = e;
    }
}

// ----------------------------------------------------------------------------
//  Message thread
// ----------------------------------------------------------------------------
void SubdivFXProcessor::timerCallback()
{
    const int ready = fifo.getNumReady();
    if (ready == 0) return;

    bool changed = false;
    auto scope = fifo.read (ready);

    scope.forEach ([&] (int idx)
    {
        const auto& e = fifoData[(size_t) idx];
        if (! capturing.load()) return;

        const int key = juce::jlimit (0, 15, e.channel - 1) * 128 + juce::jlimit (0, 127, e.pitch);

        if (e.isOn)
        {
            openStart[(size_t) key] = e.beat;
            openVel[(size_t) key]   = e.velocity;
            return;
        }

        const double st = openStart[(size_t) key];
        openStart[(size_t) key] = -1.0;
        if (st < 0.0 || e.beat <= st)        // nota sin inicio, o el loop volvió atrás
            return;

        // Al loopear la sección la misma nota vuelve a pasar: no la duplicamos
        for (const auto& n : notes)
            if (n.pitch == e.pitch && std::abs (n.start - st) < 1.0 / 64.0)
                return;

        Note n;
        n.start    = st;
        n.length   = e.beat - st;
        n.pitch    = e.pitch;
        n.velocity = juce::jmax (1, openVel[(size_t) key]);
        n.channel  = e.channel;
        notes.push_back (n);
        changed = true;
    });

    if (changed)
    {
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
                   { return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; });
        notesChanged();
    }
}

void SubdivFXProcessor::notesChanged()
{
    sendChangeMessage();
}

void SubdivFXProcessor::pushUndo()
{
    undoStack.push_back (notes);
    if (undoStack.size() > 30)
        undoStack.erase (undoStack.begin());
}

void SubdivFXProcessor::undo()
{
    if (undoStack.empty()) return;
    notes = std::move (undoStack.back());
    undoStack.pop_back();
    notesChanged();
}

void SubdivFXProcessor::clearNotes()
{
    if (notes.empty()) return;
    pushUndo();
    notes.clear();
    notesChanged();
}

void SubdivFXProcessor::selectAll (bool shouldSelect)
{
    for (auto& n : notes) n.selected = shouldSelect;
    notesChanged();
}

int SubdivFXProcessor::numSelected() const
{
    return (int) std::count_if (notes.begin(), notes.end(), [] (const Note& n) { return n.selected; });
}

void SubdivFXProcessor::loadMidiFile (const juce::File& file)
{
    auto loaded = NoteGen::readMidiFile (file, bpm.load());
    if (loaded.empty()) return;

    stopCapture();
    pushUndo();
    notes = std::move (loaded);
    notesChanged();
}

GenSettings SubdivFXProcessor::currentSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    GenSettings g;
    g.mode      = (int) get ("mode");
    g.division  = rateBeats[juce::jlimit (0, rateNames.size() - 1, (int) get ("rate"))];
    g.parts     = (int) get ("parts");
    g.rhythm    = (int) get ("rhythm");
    g.rhythmAmt = get ("rhythmAmt") / 100.0f;
    g.gate      = get ("gate") / 100.0f;
    g.pitchStep = (int) get ("pitchStep");
    g.curve     = (int) get ("velMode");
    g.depth     = get ("velDepth") / 100.0f;
    g.curveLen  = (int) get ("velSteps");
    g.velScale  = get ("velScale") / 100.0f;
    g.humanize  = get ("humanize") / 100.0f;
    return g;
}

void SubdivFXProcessor::generate()
{
    if (notes.empty()) return;

    NoteList source = notes;
    if (numSelected() == 0)                       // nada seleccionado: se aplica a todo
        for (auto& n : source) n.selected = true;

    pushUndo();
    notes = NoteGen::generate (source, currentSettings(), random);
    notesChanged();
}

bool SubdivFXProcessor::exportMidi (const juce::File& file, bool onlySelected)
{
    NoteList out;
    for (const auto& n : notes)
        if (! onlySelected || n.selected)
            out.push_back (n);

    return NoteGen::writeMidiFile (out, file, bpm.load());
}

void SubdivFXProcessor::startCapture()
{
    // descartar lo que haya quedado en la fifo
    auto scope = fifo.read (fifo.getNumReady());
    juce::ignoreUnused (scope);

    pushUndo();
    notes.clear();
    openStart.fill (-1.0);
    capturing.store (true);
    notesChanged();
}

void SubdivFXProcessor::stopCapture()
{
    if (! capturing.load()) return;
    timerCallback();                // procesar lo último que llegó
    capturing.store (false);
    openStart.fill (-1.0);
    notesChanged();
}

// ----------------------------------------------------------------------------
//  Estado: parámetros + notas (se guardan con el proyecto)
// ----------------------------------------------------------------------------
void SubdivFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::ValueTree nt ("NOTES");
    for (const auto& n : notes)
    {
        juce::ValueTree c ("N");
        c.setProperty ("s", n.start, nullptr);
        c.setProperty ("l", n.length, nullptr);
        c.setProperty ("p", n.pitch, nullptr);
        c.setProperty ("v", n.velocity, nullptr);
        c.setProperty ("c", n.channel, nullptr);
        nt.appendChild (c, nullptr);
    }
    state.appendChild (nt, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SubdivFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    auto nt = tree.getChildWithName ("NOTES");
    if (nt.isValid())
    {
        NoteList loaded;
        for (int i = 0; i < nt.getNumChildren(); ++i)
        {
            auto c = nt.getChild (i);
            Note n;
            n.start    = (double) c.getProperty ("s", 0.0);
            n.length   = (double) c.getProperty ("l", 0.25);
            n.pitch    = (int)    c.getProperty ("p", 60);
            n.velocity = (int)    c.getProperty ("v", 100);
            n.channel  = (int)    c.getProperty ("c", 1);
            loaded.push_back (n);
        }
        notes = std::move (loaded);
        tree.removeChild (nt, nullptr);
    }

    apvts.replaceState (tree);
    sendChangeMessage();
}

juce::AudioProcessorEditor* SubdivFXProcessor::createEditor()
{
    return new SubdivFXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SubdivFXProcessor();
}
