#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================================
//  Notas y generador de subdivisiones (sin dependencias de UI).
//  Todas las posiciones y duraciones están en beats (negras).
// ============================================================================
struct Note
{
    double start    = 0.0;
    double length   = 1.0;
    int    pitch    = 60;
    int    velocity = 100;
    int    channel  = 1;

    double end() const { return start + length; }
};

using NoteList = std::vector<Note>;

struct GenSettings
{
    enum Mode   { ByDivision = 0, EqualParts };
    enum Rhythm { Even = 0, Accelerate, Decelerate };
    enum Curve  { Fixed = 0, RampUp, RampDown, Accent, RandomCurve };

    int    mode       = ByDivision;
    double division   = 0.25;   // en beats (0.25 = 1/16)
    int    parts      = 4;      // para EqualParts
    int    rhythm     = Even;
    float  rhythmAmt  = 0.5f;   // 0..1
    float  gate       = 0.9f;   // 0..1
    int    pitchStep  = 0;      // semitonos por golpe: + sube, - baja
    int    curve      = Fixed;
    float  depth      = 0.5f;   // 0..1
    int    curveLen   = 0;      // 0 = la curva abarca todos los golpes
    float  humanize   = 0.0f;   // 0..0.5
};

namespace NoteGen
{
    // x (0..1) -> posición (0..1). Frenando: golpes cortos que se alargan. Acelerando: al revés.
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

        float v = (float) baseVel * f;
        if (g.humanize > 0.0f)
            v *= 1.0f + g.humanize * (rnd.nextFloat() * 2.0f - 1.0f);

        return juce::jlimit (1, 127, juce::roundToInt (v));
    }

    inline int countFor (double length, const GenSettings& g)
    {
        if (g.mode == GenSettings::EqualParts)
            return juce::jlimit (1, 128, g.parts);

        const double div = juce::jmax (1.0 / 64.0, g.division);
        return juce::jlimit (1, 128, (int) std::round (length / div));
    }

    // Parte una nota en golpes según los ajustes.
    inline NoteList subdivide (const Note& n, const GenSettings& g, juce::Random& rnd)
    {
        NoteList out;
        const int count = countFor (n.length, g);

        for (int k = 0; k < count; ++k)
        {
            const double x0 = warp ((double) k       / count, g.rhythm, g.rhythmAmt);
            const double x1 = warp ((double) (k + 1) / count, g.rhythm, g.rhythmAmt);

            Note p     = n;
            p.start    = n.start + x0 * n.length;
            p.length   = juce::jmax (1.0 / 128.0, (x1 - x0) * n.length * juce::jlimit (0.05f, 1.0f, g.gate));
            p.pitch    = juce::jlimit (0, 127, n.pitch + k * g.pitchStep);
            p.velocity = velocityFor (g, n.velocity, k, count, rnd);
            out.push_back (p);
        }
        return out;
    }

    // Escribe un .mid (960 PPQ, sin tempo: al soltarlo en el DAW toma el tempo del proyecto).
    inline bool writeMidiFile (const NoteList& notes, const juce::File& file)
    {
        if (notes.empty()) return false;

        constexpr int ppq = 960;
        juce::MidiMessageSequence seq;
        for (const auto& n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn  (n.channel, n.pitch, (juce::uint8) n.velocity), n.start * ppq);
            seq.addEvent (juce::MidiMessage::noteOff (n.channel, n.pitch), n.end() * ppq);
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
