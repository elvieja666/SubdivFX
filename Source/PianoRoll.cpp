#include "PianoRoll.h"

namespace
{
    const juce::Colour bgCol     { 0xff1b1d22 };
    const juce::Colour rowDark   { 0xff17191d };
    const juce::Colour keyStrip  { 0xff23262d };
    const juce::Colour gridFaint { 0xff24272e };
    const juce::Colour gridBeat  { 0xff30343c };
    const juce::Colour gridBar   { 0xff4a4f59 };
    const juce::Colour textDim   { 0xff8a8f98 };
    const juce::Colour accent    { 0xff4fc3f7 };

    bool isBlackKey (int pitch)
    {
        const int pc = pitch % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }
}

PianoRoll::PianoRoll (SubdivFXProcessor& p) : proc (p)
{
    setOpaque (true);
}

juce::Colour PianoRoll::velocityColour (int velocity)
{
    // Como en Logic: azul = suave, rojo = fuerte
    const float t = juce::jlimit (0.0f, 1.0f, (float) (velocity - 1) / 126.0f);
    return juce::Colour::fromHSV (0.62f * (1.0f - t), 0.68f, 0.92f, 1.0f);
}

void PianoRoll::setDropHighlight (bool shouldHighlight)
{
    dropHighlight = shouldHighlight;
    repaint();
}

// ----------------------------------------------------------------------------
void PianoRoll::updateView()
{
    const auto& notes = proc.getNotes();
    if (notes.empty())
    {
        beatStart = 0.0; beatEnd = 8.0; lowPitch = 48; highPitch = 72;
        return;
    }

    double first = notes.front().start, last = notes.front().end();
    int lo = notes.front().pitch, hi = lo;
    for (const auto& n : notes)
    {
        first = juce::jmin (first, n.start);
        last  = juce::jmax (last, n.end());
        lo    = juce::jmin (lo, n.pitch);
        hi    = juce::jmax (hi, n.pitch);
    }

    beatStart = std::floor (first / 4.0) * 4.0;
    beatEnd   = juce::jmax (beatStart + 4.0, std::ceil (last / 4.0) * 4.0);

    lowPitch  = lo - 2;
    highPitch = hi + 2;
    while (highPitch - lowPitch < 12) { --lowPitch; ++highPitch; }
    lowPitch  = juce::jmax (0, lowPitch);
    highPitch = juce::jmin (127, highPitch);
}

juce::Rectangle<int> PianoRoll::rollArea() const
{
    return getLocalBounds().withTrimmedLeft (keyW).withTrimmedTop (rulerH).withTrimmedBottom (velLaneH + 2);
}

juce::Rectangle<int> PianoRoll::velArea() const
{
    return getLocalBounds().withTrimmedLeft (keyW).removeFromBottom (velLaneH);
}

float PianoRoll::beatToX (double beat) const
{
    const auto r = rollArea();
    return (float) r.getX() + (float) ((beat - beatStart) / (beatEnd - beatStart)) * (float) r.getWidth();
}

float PianoRoll::rowHeight() const
{
    return (float) rollArea().getHeight() / (float) (highPitch - lowPitch + 1);
}

float PianoRoll::pitchToY (int pitch) const
{
    return (float) rollArea().getY() + (float) (highPitch - pitch) * rowHeight();
}

juce::Rectangle<float> PianoRoll::noteRect (const Note& n) const
{
    const float x0 = beatToX (n.start);
    const float x1 = beatToX (n.end());
    return { x0, pitchToY (n.pitch) + 1.0f, juce::jmax (3.0f, x1 - x0 - 1.0f), rowHeight() - 2.0f };
}

int PianoRoll::noteAt (juce::Point<int> pt) const
{
    const auto& notes = proc.getNotes();
    for (int i = (int) notes.size() - 1; i >= 0; --i)
        if (noteRect (notes[(size_t) i]).expanded (1.0f).contains (pt.toFloat()))
            return i;
    return -1;
}

// ----------------------------------------------------------------------------
void PianoRoll::paint (juce::Graphics& g)
{
    updateView();
    const auto& notes = proc.getNotes();
    const auto roll = rollArea();
    const auto vel  = velArea();
    const float rh  = rowHeight();

    g.fillAll (bgCol);

    // Filas (teclas negras más oscuras) y franja de teclado
    for (int p = lowPitch; p <= highPitch; ++p)
    {
        const float y = pitchToY (p);
        if (isBlackKey (p))
        {
            g.setColour (rowDark);
            g.fillRect ((float) roll.getX(), y, (float) roll.getWidth(), rh);
        }

        g.setColour (isBlackKey (p) ? keyStrip.darker (0.4f) : keyStrip);
        g.fillRect (0.0f, y, (float) keyW - 1.0f, rh - 0.5f);

        if (p % 12 == 0 && rh >= 9.0f)
        {
            g.setColour (textDim);
            g.setFont (juce::FontOptions (juce::jmin (11.0f, rh - 1.0f)));
            g.drawText (juce::MidiMessage::getMidiNoteName (p, true, true, 3),
                        juce::Rectangle<float> (3.0f, y, (float) keyW - 6.0f, rh),
                        juce::Justification::centredLeft);
        }
    }

    // Grilla vertical y regla de compases
    const double pxPerBeat = roll.getWidth() / (beatEnd - beatStart);
    const double step = pxPerBeat >= 48.0 ? 0.25 : (pxPerBeat >= 20.0 ? 0.5 : 1.0);
    for (double b = beatStart; b <= beatEnd + 1e-9; b += step)
    {
        const float x = beatToX (b);
        const bool isBar  = std::abs (std::fmod (b, 4.0)) < 1e-6;
        const bool isBeat = std::abs (b - std::round (b)) < 1e-6;

        g.setColour (isBar ? gridBar : (isBeat ? gridBeat : gridFaint));
        g.drawVerticalLine ((int) x, (float) roll.getY(), (float) vel.getBottom());

        if (isBar)
        {
            g.setColour (textDim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (juce::String ((int) std::round (b / 4.0) + 1), (int) x + 3, 0, 40, rulerH,
                        juce::Justification::centredLeft);
        }
    }

    // Separador del carril de velocity
    g.setColour (keyStrip);
    g.fillRect (0, vel.getY() - 2, getWidth(), 2);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("VEL", 0, vel.getY(), keyW - 4, vel.getHeight(), juce::Justification::centred);

    // Notas
    for (const auto& n : notes)
    {
        const auto r   = noteRect (n);
        const auto col = velocityColour (n.velocity);

        g.setColour (n.selected ? col : col.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.7f));
        g.fillRoundedRectangle (r, 2.0f);

        if (n.selected)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 2.0f, 1.4f);
        }
        else
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (r, 2.0f, 1.0f);
        }

        // Barra de velocity
        const float h  = (float) (vel.getHeight() - 6) * (float) n.velocity / 127.0f;
        const float vx = r.getX();
        g.setColour (n.selected ? col : col.withAlpha (0.45f));
        g.fillRect (vx, (float) vel.getBottom() - 3.0f - h, juce::jmax (2.0f, juce::jmin (5.0f, r.getWidth())), h);
    }

    // Rectángulo de selección
    if (marqueeActive)
    {
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRect (marquee);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawRect (marquee, 1);
    }

    // Estado vacío
    if (notes.empty())
    {
        g.setColour (textDim);
        g.setFont (juce::FontOptions (15.0f));
        g.drawFittedText (proc.isCapturing()
                              ? "Capturando... dale play a la seccion en tu DAW"
                              : "Arrastra aca una region o un archivo MIDI\n"
                                "o toca  Capturar  y dale play a la seccion",
                          roll.reduced (20), juce::Justification::centred, 3);
    }

    if (dropHighlight)
    {
        g.setColour (accent.withAlpha (0.12f));
        g.fillRect (getLocalBounds());
        g.setColour (accent);
        g.drawRect (getLocalBounds(), 2);
    }
}

// ----------------------------------------------------------------------------
void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    auto& notes = proc.getNotes();
    const bool shift = e.mods.isShiftDown() || e.mods.isCommandDown();
    const int idx = noteAt (e.getPosition());

    if (idx >= 0)
    {
        auto& n = notes[(size_t) idx];
        if (shift)
            n.selected = ! n.selected;
        else if (! n.selected)
            for (size_t i = 0; i < notes.size(); ++i)
                notes[i].selected = ((int) i == idx);

        proc.notesChanged();
        return;
    }

    // Lugar vacío: arranca el rectángulo de selección
    if (! shift)
        for (auto& n : notes) n.selected = false;

    selectionAtDragStart.clear();
    for (const auto& n : notes) selectionAtDragStart.push_back (n.selected);

    marqueeActive = true;
    marquee = { e.getPosition(), e.getPosition() };
    proc.notesChanged();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (! marqueeActive) return;

    marquee = juce::Rectangle<int> (e.getMouseDownPosition(), e.getPosition());
    const auto mq = marquee.toFloat();

    auto& notes = proc.getNotes();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const bool before = i < selectionAtDragStart.size() && selectionAtDragStart[i];
        notes[i].selected = before || noteRect (notes[i]).intersects (mq);
    }

    proc.notesChanged();
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (marqueeActive)
    {
        marqueeActive = false;
        repaint();
    }
}
