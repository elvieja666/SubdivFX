# SubdivFX

Editor de subdivisiones MIDI. Metés una sección MIDI en el plugin, seleccionás notas y con un botón las partís en golpes. Podés cambiar el ritmo de los golpes (parejo, acelerando o frenando), armar escaleras cromáticas y aplicar curvas de velocity. Después arrastrás el resultado de vuelta a tu pista.

Formatos: **AU** (MIDI FX en Logic) y **VST3**, para macOS (Intel y Apple Silicon).

---

## Cómo se usa

1. **Cargá el plugin** en el slot **MIDI FX** de la pista de instrumento. El MIDI pasa sin cambios, así la pista suena normal.
2. **Meté las notas** de alguna de estas formas:
   - **Arrastrá** una región MIDI o un archivo `.mid` sobre el piano roll del plugin.
   - Usá **Abrir .mid**.
   - Usá **Capturar**: apretalo, dale play a la sección en Logic y volvé a apretarlo al terminar. Si la sección loopea, las notas no se duplican.
3. **Seleccioná** las notas que querés subdividir:
   - clic sobre una nota;
   - Shift o Cmd + clic para sumar o quitar notas;
   - arrastrar en un lugar vacío para seleccionar con un rectángulo;
   - Cmd+A para seleccionar todas.
4. Ajustá los controles y apretá **GENERAR** (o Enter). Si no seleccionaste nada, se aplica a todas las notas. Las notas generadas quedan seleccionadas, así podés volver a aplicar. Con **Deshacer** (Cmd+Z) volvés atrás.
5. **Arrastrá** el recuadro *"Arrastra esto a tu pista"* hasta la pista de Logic: se crea una región nueva con las notas reales. También podés usar **Guardar .mid**.
   Cada arrastre guarda además una copia en `~/Music/SubdivFX/`.

Las notas del plugin se guardan junto con el proyecto.

## Controles

| Sección | Control | Qué hace |
|---|---|---|
| Subdivisión | **Modo** | *Division*: la nota se parte según una figura (1/16, 1/32…). *Partes iguales*: la nota se parte en N golpes, sea cual sea su largo. |
| | **Division** | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T o 1/64 (para el modo *Division*). |
| | **Partes** | Cantidad de golpes, de 2 a 32 (para el modo *Partes iguales*). |
| | **Ritmo** | *Parejo*, *Acelerando* (golpes que se acortan) o *Frenando* (golpes cortos que se alargan). |
| | **Curva ritmo** | Cuánto acelera o frena. |
| | **Gate** | Largo de cada golpe respecto de su espacio. 100% = legato. |
| | **Escalera** | Semitonos que sube (+) o baja (−) cada golpe, de −12 a +12. 0 = misma nota. |
| Velocity | **Curva vel.** | *Fija*, *Rampa sube*, *Rampa baja*, *Acento* (primer golpe fuerte) o *Aleatorio*. |
| | **Profundidad** | Cuánto se aleja la curva de la velocity original. |
| | **Largo curva** | *Toda la nota* (la curva abarca todos los golpes) o cada cuántos golpes se repite. |
| | **Escala vel.** | Multiplicador general (10 a 200%). |
| | **Humanizar** | Variación aleatoria extra. |

Los colores de las notas siguen el criterio de Logic: azul = suave, rojo = fuerte.

**Ejemplos:**
- *Ratchet*: Modo Partes iguales, 6 partes, Ritmo Parejo.
- *Escalera descendente que se apaga*: 1/32, Escalera −1, Rampa baja.
- *Redoble que frena*: 1/32, Ritmo Frenando, Curva ritmo 70%.

## Compilar

El repo incluye un workflow de GitHub Actions que compila la versión de macOS en cada push. El resultado aparece como artifact `SubdivFX-macOS`.

Para compilar a mano en macOS (requiere las Command Line Tools y `brew install cmake`):

```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Estructura

```
Source/NoteModel.h        notas, generador de subdivisiones, lectura y escritura de .mid
Source/PluginProcessor.*  captura, deshacer, parámetros, estado guardado
Source/PianoRoll.*        mini piano roll con selección
Source/PluginEditor.*     interfaz, arrastrar y soltar
```
