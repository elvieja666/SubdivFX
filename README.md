# SubdivFX

Generador de clips MIDI de subdivisión. Ajustás la nota, el largo, la subdivisión, la rampa de pitch y la curva de velocity, y **arrastrás el clip** directo a tu pista.

Formatos: **AU** (MIDI FX en Logic) y **VST3**, para macOS (Intel y Apple Silicon).

---

## Cómo se usa

1. Cargá SubdivFX en el slot **MIDI FX** de una pista de instrumento.
2. Ajustá los controles. La franja de arriba muestra el clip en tiempo real.
3. **Probar** toca el clip con el instrumento de la pista, para escucharlo antes de arrastrarlo. La pista tiene que estar seleccionada.
4. **Arrastrá la franja** a la pista: se crea una región con las notas reales, que podés mover y editar. También podés usar **Guardar .mid**.

El .mid no lleva tempo, así que toma el tempo del proyecto.

## Controles

| Sección | Control | Qué hace |
|---|---|---|
| Clip | **Nota** | Nota de inicio (C3 = 60, como en Logic). |
| | **Largo** | Duración total del clip: de 1/16 a 4 compases. |
| | **Velocity** | Velocity de partida. |
| Pitch | **Pitch** | *Fijo*, *Sube* o *Baja*: rampa cromática de un golpe al siguiente. |
| | **Paso** | Semitonos por golpe (1 a 12). |
| Subdivisión | **Modo** | *Division*: golpes de una figura fija (1/16, 1/32…). *Partes iguales*: el clip se parte en N golpes. |
| | **Division** | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T o 1/64. |
| | **Partes** | Cantidad de golpes (2 a 32) para *Partes iguales*. |
| | **Ritmo** | *Parejo*, *Acelerando* (golpes que se acortan) o *Frenando* (golpes cortos que se alargan). |
| | **Curva ritmo** | Cuánto acelera o frena. |
| | **Gate** | Largo de cada golpe respecto de su espacio. 100% = legato. |
| Velocity | **Curva vel.** | *Fija*, *Rampa sube*, *Rampa baja*, *Acento* o *Aleatorio*. |
| | **Profundidad** | Cuánto cambia la velocity a lo largo de la curva. |
| | **Largo curva** | *Todo el clip* o cada cuántos golpes se repite la curva. |
| | **Humanizar** | Variación aleatoria. **Variar** genera otra versión al azar. |

Los colores siguen el criterio de Logic: azul = suave, rojo = fuerte.

**Recetas:**
- *Escalera que baja y se apaga*: Largo 1/4 · Partes iguales 5 · Pitch Baja 1 st · Rampa baja.
- *Escalera que sube*: lo mismo con Pitch Sube.
- *Ratchet*: Largo 1/2 · Partes iguales 6 · Pitch Fijo.
- *Redoble que frena*: Largo 1 compás · 1/32 · Ritmo Frenando 60%.

## Compilar

Cada push a `main` compila la versión de macOS con GitHub Actions. El resultado queda como artifact `SubdivFX-macOS`.

A mano en macOS (Command Line Tools + `brew install cmake`):

```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
