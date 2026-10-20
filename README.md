# SubdivFX

Efecto MIDI que **subdivide notas** (las repite sincronizadas al tempo mientras se mantienen) y les aplica **curvas de velocity**. Sólo procesa las notas que pasan el filtro **Destino**; todo lo demás (otras notas, CC, pitch bend) pasa sin cambios.

Formatos: **AU** (MIDI FX en Logic) y **VST3**, para macOS (Intel y Apple Silicon).

---

## Compilar en macOS

Requisitos: Xcode (o las Command Line Tools) y CMake (`brew install cmake`).

```bash
cd SubdivFX
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -G Xcode
cmake --build build --config Release
```

Al terminar, los plugins se copian solos a:
- `~/Library/Audio/Plug-Ins/Components/SubdivFX.component` (AU)
- `~/Library/Audio/Plug-Ins/VST3/SubdivFX.vst3`


## Usarlo en Logic

1. Si Logic estaba abierto, cerralo y volvé a abrirlo. Va a validar el plugin nuevo.
2. En un canal de instrumento, abrí el slot **MIDI FX** y elegí **Audio Units → 0600musica → SubdivFX**.
3. Para que funcione al tocar en vivo, el canal tiene que estar seleccionado o armado.

### Cómo procesar solo algunas notas ("notas seleccionadas")

Un plugin no puede leer la selección del piano roll, así que el filtro **Destino** cumple esa función. Hay dos formas fáciles de marcar notas en Logic:

- **Por canal MIDI**: seleccioná las notas en el Piano Roll y, en el Inspector, cambiales el **canal** (por ejemplo, a 2). En SubdivFX poné **Canal = 2**. Solo esas notas se subdividen.
- **Por velocity**: dales a las notas elegidas una velocity alta (por ejemplo, 120 a 127) y poné **Vel min = 120**. Ojo: la curva de velocity parte de esa velocity original, así que podés compensarla con **Escala vel.**

También sirve **Nota min / Nota max** para procesar, por ejemplo, solo los hi-hats de un kit.

## Parámetros

| Sección | Parámetro | Qué hace |
|---|---|---|
| Subdivisión | **Division** | Valor de cada repetición: 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T o 1/64 (sigue el tempo del DAW). |
| | **Gate** | Duración de cada repetición, en % del intervalo. 100% = legato. |
| | **Repeticiones** | Cantidad máxima de golpes por nota. "Mientras suene" = repite hasta que soltás la nota. |
| Velocity | **Curva vel.** | Escalar · Rampa sube · Rampa baja · Acento (primer golpe fuerte) · Aleatorio. |
| | **Profundidad** | Cuánto se aleja la curva de la velocity original. |
| | **Largo curva** | Cada cuántos golpes se repite el patrón de la curva (2 a 16). |
| | **Escala vel.** | Multiplicador general (10 a 200%). |
| | **Humanizar** | Variación aleatoria extra de ± %. |
| Destino | **Canal** | Todos o un canal de 1 a 16. |
| | **Nota min / max** | Rango de notas a procesar. |
| | **Vel min / max** | Rango de velocity de entrada a procesar. |

## Notas por DAW

- **Logic**: AU MIDI FX nativo.
- **Reaper, Bitwig, Cubase, Studio One**: el VST3 funciona como efecto MIDI insertado antes del instrumento. En algunos DAW hay que rutear la salida MIDI del plugin hacia la pista del instrumento.
- **Ableton Live**: no carga efectos MIDI VST3 en la cadena MIDI. Hay que ponerlo en una pista MIDI propia y mandar su salida MIDI a la pista del instrumento. La alternativa es una versión Max for Live.

## Estructura

```
CMakeLists.txt            configuración de build (JUCE / CMake)
Source/PluginProcessor.*  lógica MIDI: subdivisión, curvas, filtro
Source/PluginEditor.*     interfaz
```
