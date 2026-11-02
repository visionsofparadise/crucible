# Crucible — Faust runtime authoring context

This document tells an author, human or LLM, how to write Faust programs
that run well inside Crucible. Crucible copies it to the clipboard from the
COPY CONTEXT button.

## What Crucible is

Crucible is a VST3 plugin that hosts **runtime-compiled Faust DSP**. It ships
in two variants:

- **Crucible FX** — audio in, audio out. Accepts Faust programs with at least
  one input channel.
- **Crucible Instrument** — MIDI in, audio out. Accepts generator-shaped Faust
  programs (zero inputs) and plays them polyphonically from MIDI notes.

Each variant rejects the other's programs with a "variant mismatch" error that
names the program's input and output counts.

The plugin compiles your `.dsp` file via libfaust + LLVM JIT each time the file
changes on disk, swaps the live DSP instance gaplessly, and renders controls
+ a block diagram from the program's JSON UI / SVG output.

## Single-file watcher

Open one `.dsp` file at a time. Crucible polls it every 200 ms and recompiles
on each change. External edits (your editor, an LLM agent, `echo` to the file)
win over the in-VST editor's unsaved draft. The in-VST SAVE button is always
Save-As; it confirms the path before writing.

A compile error shows libfaust's message, with line and column when libfaust
reports them, while the last program that compiled keeps playing. Fix the
error and save again.

## Required imports

Crucible passes the editor buffer to libfaust verbatim. Your source **must**
declare:

```faust
import("stdfaust.lib");
```

Crucible bundles the Faust 2.85.5 stdlib and exposes it on libfaust's include
path. `stdfaust.lib` binds the usual prefixes:

| Prefix | Library | Prefix | Library |
|---|---|---|---|
| `ba` | basics | `no` | noises |
| `si` | signals | `os` | oscillators |
| `ma` | maths | `en` | envelopes |
| `fi` | filters | `de` | delays |
| `re` | reverbs | `co` | compressors |
| `ef` | misceffects | `ve` | vaeffects |
| `pf` | phaflangers | `an` | analyzers |
| `sp` | spats | `pm` | physmodels |
| `sy` | synths | `ro` | routes |
| `aa` | aanl (antialiased nonlinearities) | `dx` | dx7 |

Imports relative to the watched `.dsp` file's directory also resolve, so a
helper library beside your program works:

```faust
import("stdfaust.lib");
tools = library("my-tools.lib"); // my-tools.lib sits next to this .dsp
process = tools.saturate(2);
```

## IO channel constraints

**Crucible FX** runs on a track whose input and output layouts match, which is
mono or stereo in practice.

- Faust input `i` reads track channel `i`. Inputs beyond the track's channels
  read silence, and track channels beyond the program's inputs are ignored.
- Faust output `i` writes track channel `i`. Processing is in place, so a track
  channel beyond the program's outputs keeps its dry input: a 1-in/1-out
  program on a stereo track processes the left channel and leaves the right
  dry. Write stereo explicitly: `process = fx, fx;` or `par(i, 2, fx)`.
- Keep outputs at or below the track's channel count (two on a stereo track).

**Crucible Instrument** has no audio input and a mono or stereo output.

- A one-output program plays on both channels.
- A two-output program plays left and right.
- Keep outputs at one or two.

## Parameter model

Crucible exposes a fixed APVTS slot pool to the host so DAW automation
survives hot reloads:

- 128 float slots `slot.f.000`–`slot.f.127`, normalised `[0, 1]`, for
  `hslider`, `vslider` and `nentry`.
- 32 bool slots `slot.b.000`–`slot.b.031`, for `button` and `checkbox`.
  `slot.b.000` is reserved for bypass, which leaves 31.
- 16 bargraph readback slots `slot.m.000`–`slot.m.015`, for `hbargraph` and
  `vbargraph`.

`ParamBridge` walks the JSON UI tree on every compile and binds each Faust
address (`/myprog/cutoff`) to the next free slot of the matching type. Bindings
persist by address across reloads, so a knob holding `cutoff = 0.5` stays at
0.5 after an edit that doesn't touch that control. Controls past a pool's
budget get no slot, so neither the panel nor the host can move them.

- **Addresses come from labels and groups.** Renaming a control or moving it to
  another group gives it a new address, a new slot and fresh automation.
- **Slots store the knob position.** Changing a control's min or max keeps the
  knob where it was, so its value rescales to the new range.
- **Group with prefixes or group functions.** `hslider("h:filter/cutoff", ...)`
  and `hgroup("filter", hslider("cutoff", ...))` both make a `filter` group;
  a bare `"filter/cutoff"` label stays one flat label.

DAW automation lanes show the slot id (`slot.f.012`), not your Faust label —
a documented quirk of the dynamic-parameter model.

### Widgets and metadata

| Faust | Crucible panel |
|---|---|
| `hslider`, `vslider`, `nentry` | knob |
| same, with `[style:menu{'A':0;'B':1}]` | dropdown |
| same, with `[style:radio{'A':0;'B':1}]` | radio buttons |
| `checkbox` | toggle |
| `button` | momentary button |
| `hbargraph`, `vbargraph` | meter |

- `[unit:Hz]` and other units print after the value.
- Knobs move linearly from min to max. `[scale:log]`, `[scale:exp]`,
  `[tooltip:...]` and `[hidden:...]` are ignored, so build a log taper in the
  DSP: `cutoff = 20 * pow(1000, hslider("cutoff", 0.5, 0, 1, 0.001));` spans
  20 Hz–20 kHz.
- `soundfile` is unsupported.
- Bargraph values are read back after every audio block into the readback
  slots, so the host can see them too.

Smooth any control that feeds audio directly (`: si.smoo`) to avoid zipper
noise when a knob moves.

## MIDI

**Crucible FX** takes no MIDI, so `[midi:...]` metadata does nothing there.

**Crucible Instrument** receives notes on all 16 channels; note-on with
velocity 0 counts as note-off. Each note drives a voice through Faust's
standard voice controls, matched by the end of the address so they may sit
inside groups:

| Control | Set to |
|---|---|
| `freq` | the note's frequency in Hz |
| `key` | the note number, in place of `freq` |
| `gain` | velocity / 127 |
| `vel` or `velocity` | velocity 0–127, in place of `gain` |
| `gate` | 1 at note-on, 0 at note-off |

Declare them as ordinary controls (`hslider("freq", ...)`, `button("gate")`).
They appear in the panel like any other control, but notes set them, so leave
them unautomated.

The Instrument also adds a **Panic** button (`/Polyphonic/Voices/Panic`), and
MIDI CC 120 (all sound off) and CC 123 (all notes off) stop every voice.

Crucible also forwards aftertouch, CC, program change, channel pressure, pitch
bend, clock and start/stop to libfaust's `[midi:...]` metadata mappings
(`[midi:ctrl 74]`, `[midi:pitchwheel]`, `[midi:keyon 60]` and the rest). In
the Instrument these mappings write the first voice's copy of the control
only, and the panel and host never see the value, so the result is uneven
across voices. Drive tone controls from the DAW instead: automate the slot, or
map a hardware controller to it with the DAW's MIDI learn.

## Voice management

The Instrument wraps every program in Faust's polyphonic voice allocator with
**16 voices**. `declare options "[nvoices:N]"` and `[midi:on]` are ignored;
the voice count is fixed.

- **Each voice runs the whole `process`.** A reverb or delay inside `process`
  runs once per sounding voice, at up to 16× the CPU, with a tail per voice.
  `effect = ...;` definitions are ignored, so put shared effects in a Crucible
  FX instance after the Instrument on the track.
- **A voice sounds from note-on until its output falls silent.** Note-off sets
  `gate` to 0 and the voice frees itself once its output stays below −90 dB.
  Multiply the output by an envelope that reaches 0 after the gate falls
  (`en.adsr`, `en.ar`, `en.are`); an ungated oscillator never frees its voice.
- **Without notes the Instrument is silent.** Every voice waits for a note-on,
  so a drone also needs a held note.
- **When all 16 voices are busy**, a new note steals the oldest releasing
  voice, or else the oldest playing one, with a short crossfade.
- **Voices sum.** Scale the output (`* gain * 0.3` in the examples below) so
  chords stay clear of clipping.

## Latency

Crucible reports zero latency to the host for every program. Delay a program
introduces on purpose — lookahead limiting, `@(n)` alignment, block-based or
linear-phase processing — reaches the output late, and the DAW's delay
compensation leaves it uncorrected. Keep programs zero-latency, or offset the
track by hand in the DAW.

## Examples

Each example compiles as written. Save one as a `.dsp` file and open it in the
named variant.

### FX: stereo filter

```faust
import("stdfaust.lib");

// Knobs move linearly between min and max, so a log-feel cutoff maps a
// 0-1 position onto 20 Hz-20 kHz itself.
position = hslider("h:filter/cutoff", 0.5, 0, 1, 0.001) : si.smoo;
cutoff   = 20 * pow(1000, position);
res      = hslider("h:filter/resonance", 1, 0.5, 10, 0.01) : si.smoo;
mode     = hslider("h:filter/type [style:menu{'LP':0;'HP':1}]", 0, 0, 1, 1);

filter = _ <: fi.resonlp(cutoff, res, 1), fi.resonhp(cutoff, res, 1) : select2(mode);

// Two inputs, two outputs: one filter per channel.
process = filter, filter;
```

### FX: feedback delay

```faust
import("stdfaust.lib");

maxDelay = 524288; // samples; covers 2 s at 192 kHz

time = hslider("h:delay/time [unit:ms]", 350, 1, 2000, 1) : si.smoo;
fb   = hslider("h:delay/feedback", 0.4, 0, 0.95, 0.01) : si.smoo;
mix  = hslider("h:delay/mix", 0.3, 0, 1, 0.01) : si.smoo;

d = time * ma.SR / 1000;

// Wet path: the delay line fed by input plus scaled feedback.
echo = (+ : de.fdelay(maxDelay, d)) ~ *(fb);

channel = _ <: *(1 - mix), (echo : *(mix)) :> _;

process = channel, channel;
```

### FX: compressor with output meters

```faust
import("stdfaust.lib");

ratio  = hslider("h:comp/ratio", 4, 1, 20, 0.1);
thresh = hslider("h:comp/threshold [unit:dB]", -18, -60, 0, 0.1);
att    = hslider("h:comp/attack [unit:ms]", 10, 0.1, 100, 0.1) / 1000;
rel    = hslider("h:comp/release [unit:ms]", 150, 10, 1000, 1) / 1000;
makeup = hslider("h:comp/makeup [unit:dB]", 0, 0, 24, 0.1) : ba.db2linear : si.smoo;

// Pass the signal through while a bargraph shows its level in dB.
meter(bar) = _ <: attach(_, an.amp_follower_ar(0.001, 0.2) : ba.linear2db : max(-60) : bar);

process = co.compressor_stereo(ratio, thresh, att, rel)
        : *(makeup), *(makeup)
        : meter(hbargraph("h:out/L [unit:dB]", -60, 0)), meter(hbargraph("h:out/R [unit:dB]", -60, 0));
```

### Instrument: subtractive synth

```faust
import("stdfaust.lib");

// Voice controls, set by each MIDI note.
freq = hslider("freq [unit:Hz]", 440, 20, 20000, 0.01);
gain = hslider("gain", 0.5, 0, 1, 0.01);
gate = button("gate");

// Tone controls, the same on every voice.
cutoff  = hslider("h:tone/cutoff", 0.4, 0, 1, 0.001) : si.smoo;
release = hslider("h:tone/release [unit:s]", 0.4, 0.01, 4, 0.01);

env = en.adsr(0.005, 0.2, 0.6, release, gate);
osc = (os.sawtooth(freq) + os.sawtooth(freq * 1.005)) * 0.5;
fc  = min(18000, freq * (1 + 15 * cutoff * env));

// The envelope returns to 0 after the gate falls, which lets the voice free itself.
process = osc : fi.lowpass(2, fc) : *(env * gain * 0.3) <: _, _;
```

### Instrument: FM bell

```faust
import("stdfaust.lib");

freq = hslider("freq [unit:Hz]", 440, 20, 20000, 0.01);
gain = hslider("gain", 0.5, 0, 1, 0.01);
gate = button("gate");

ratio   = hslider("h:fm/ratio", 3.5, 0.5, 8, 0.01);
index   = hslider("h:fm/index", 2, 0, 10, 0.01) : si.smoo;
release = hslider("h:fm/release [unit:s]", 2.5, 0.1, 8, 0.01);

env = en.are(0.002, release, gate);
mod = os.osc(freq * ratio) * index * env * freq;

// Mono out: Crucible Instrument copies a single output to both channels.
process = os.osc(freq + mod) * env * gain * 0.3;
```
