#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================================
//  Modelo de notas y generador de subdivisiones (sin dependencias de UI).
//  Todas las posiciones y duraciones están en beats (negras).
// ============================================================================
struct Note
{
    double start    = 0.0;
    double length   = 1.0;
    int    pitch    = 60;
    int    velocity = 100;
    int    channel  = 1;
    bool   selected = false;

    double end() const { return start + length; }
};

using NoteList = std::vector<Note>;

struct GenSettings
{
    enum Mode   { ByDivision = 0, EqualParts };
    enum Rhythm { Even = 0, Accelerate, Decelerate };
    enum Curve  { Scale = 0, RampUp, RampDown, Accent, RandomCurve };

    int    mode       = ByDivision;
    double division   = 0.25;   // en beats (0.25 = 1/16)
    int    parts      = 4;      // para EqualParts
    int    rhythm     = Even;   // reparto de los golpes dentro de la nota
    float  rhythmAmt  = 0.5f;   // 0..1, cuánto acelera / frena
    float  gate       = 0.9f;   // 0..1, largo de cada golpe respecto de su espacio
    int    pitchStep  = 0;      // semitonos que sube (+) o baja (-) cada golpe
    int    curve      = Scale;
    float  depth      = 0.5f;   // 0..1
    int    curveLen   = 0;      // 0 = la curva abarca toda la nota
    float  velScale   = 1.0f;   // multiplicador general
    float  humanize   = 0.0f;   // 0..0.5
};

namespace NoteGen
{
    // Reparte los golpes en el tiempo: x (0..1) -> posición (0..1) dentro de la nota.
    // Frenando: golpes cortos al principio que se van alargando. Acelerando: al revés.
    inline double warp (double x, int rhythm, float amount)
    {
        const double a = 1.0 + 1.2 * juce::jlimit (0.0f, 1.0f, amount);
        switch (rhythm)
        {
            case GenSettings::Decelerate: return std::pow (x, a);
            case GenSettings::Accelerate: return 1.0 - std::pow (1.0 - x, a);
            default:                      return x;
        }
    }

    inline int velocityFor (const GenSettings& g, int baseVel, int k, int count, juce::Random& rnd)
    {
        const int   len = g.curveLen > 0 ? juce::jmax (2, g.curveLen) : juce::jmax (2, count);
        const int   pos = k % len;
        const float t   = (float) pos / (float) (len - 1);

        float f = 1.0f;
        switch (g.curve)
        {
            case GenSettings::RampUp:      f = 1.0f - g.depth * (1.0f - t);       break;
            case GenSettings::RampDown:    f = 1.0f - g.depth * t;                break;
            case GenSettings::Accent:      f = pos == 0 ? 1.0f : 1.0f - g.depth;  break;
            case GenSettings::RandomCurve: f = 1.0f - g.depth * rnd.nextFloat();  break;
            default:                       f = 1.0f;                              break;
        }

        float v = (float) baseVel * f * g.velScale;
        if (g.humanize > 0.0f)
            v *= 1.0f + g.humanize * (rnd.nextFloat() * 2.0f - 1.0f);

        return juce::jlimit (1, 127, juce::roundToInt (v));
    }

    // Cantidad de golpes en que se parte una nota
    inline int countFor (const Note& n, const GenSettings& g)
    {
        if (g.mode == GenSettings::EqualParts)
            return juce::jlimit (1, 128, g.parts);

        const double div = juce::jmax (1.0 / 64.0, g.division);
        return juce::jlimit (1, 128, (int) std::round (n.length / div));
    }

    // Subdivide las notas seleccionadas; las no seleccionadas quedan igual.
    // Las piezas nuevas quedan seleccionadas, así se puede volver a aplicar.
    inline NoteList generate (const NoteList& in, const GenSettings& g, juce::Random& rnd)
    {
        NoteList out;
        out.reserve (in.size() * 4);

        for (const auto& n : in)
        {
            if (! n.selected) { out.push_back (n); continue; }

            const int count = countFor (n, g);

            for (int k = 0; k < count; ++k)
            {
                const double x0 = warp ((double) k       / count, g.rhythm, g.rhythmAmt);
                const double x1 = warp ((double) (k + 1) / count, g.rhythm, g.rhythmAmt);
                const double space = (x1 - x0) * n.length;

                Note p     = n;
                p.start    = n.start + x0 * n.length;
                p.length   = juce::jmax (1.0 / 128.0, space * juce::jlimit (0.05f, 1.0f, g.gate));
                p.pitch    = juce::jlimit (0, 127, n.pitch + k * g.pitchStep);
                p.velocity = velocityFor (g, n.velocity, k, count, rnd);
                p.selected = true;
                out.push_back (p);
            }
        }

        std::sort (out.begin(), out.end(), [] (const Note& a, const Note& b)
                   { return a.start < b.start || (! (b.start < a.start) && a.pitch < b.pitch); });
        return out;
    }

    // ---- Archivos MIDI -----------------------------------------------------

    // Lee todas las pistas de un .mid y devuelve las notas en beats.
    inline NoteList readMidiFile (const juce::File& file, double bpmFallback)
    {
        NoteList notes;
        juce::FileInputStream is (file);
        if (! is.openedOk()) return notes;

        juce::MidiFile mf;
        if (! mf.readFrom (is)) return notes;

        const short format = mf.getTimeFormat();
        if (format <= 0)                     // SMPTE: pasamos por segundos
            mf.convertTimestampTicksToSeconds();

        auto toBeats = [&] (double t)
        {
            return format > 0 ? t / (double) format
                              : t * bpmFallback / 60.0;
        };

        for (int tr = 0; tr < mf.getNumTracks(); ++tr)
        {
            juce::MidiMessageSequence seq (*mf.getTrack (tr));
            seq.updateMatchedPairs();

            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* ev = seq.getEventPointer (i);
                const auto& m = ev->message;
                if (! m.isNoteOn()) continue;

                const double on  = toBeats (m.getTimeStamp());
                const double off = ev->noteOffObject != nullptr
                                     ? toBeats (ev->noteOffObject->message.getTimeStamp())
                                     : on + 0.25;
                Note n;
                n.start    = on;
                n.length   = juce::jmax (1.0 / 128.0, off - on);
                n.pitch    = m.getNoteNumber();
                n.velocity = juce::jmax (1, (int) m.getVelocity());
                n.channel  = m.getChannel();
                notes.push_back (n);
            }
        }

        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
                   { return a.start < b.start || (! (b.start < a.start) && a.pitch < b.pitch); });
        return notes;
    }

    // Exporta a .mid (960 PPQ). Empieza en el compás (4/4) de la primera nota para
    // que, al soltarlo en el DAW, las notas conserven su lugar en la grilla.
    inline bool writeMidiFile (const NoteList& notes, const juce::File& file, double bpm)
    {
        if (notes.empty()) return false;

        double first = notes.front().start;
        for (const auto& n : notes) first = juce::jmin (first, n.start);
        const double offset = std::floor (first / 4.0) * 4.0;

        constexpr int ppq = 960;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm)), 0.0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

        for (const auto& n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn  (n.channel, n.pitch, (juce::uint8) n.velocity),
                          (n.start - offset) * ppq);
            seq.addEvent (juce::MidiMessage::noteOff (n.channel, n.pitch),
                          (n.end() - offset) * ppq);
        }
        seq.sort();
        seq.updateMatchedPairs();

        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (seq);

        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream os (file);
        if (! os.openedOk()) return false;
        return mf.writeTo (os);
    }
}
