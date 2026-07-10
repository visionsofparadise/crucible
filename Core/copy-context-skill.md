# Crucible — Faust runtime authoring context

This document is a v1 placeholder. It will grow as Crucible's LLM-authoring
workflow matures.

## What Crucible is

Crucible is a VST3 plugin that hosts **runtime-compiled Faust DSP**. It ships
in two variants:

- **Crucible FX** — audio in, audio out. Accepts Faust programs with at least
  one input channel.
- **Crucible Instrument** — MIDI in, audio out. Accepts generator-shaped Faust
  programs (zero inputs) that respond to MIDI via `[midi:on]` metadata.

The plugin compiles your `.dsp` file via libfaust + LLVM JIT each time the file
changes on disk, swaps the live DSP instance gaplessly, and renders controls
+ a block diagram from the program's JSON UI / SVG output.

## Required imports

Crucible passes the editor buffer to libfaust verbatim. Your source **must**
declare:

```faust
import("stdfaust.lib");
```

Crucible bundles the Faust 2.85.5 stdlib (`oscillators.lib`, `filters.lib`,
`envelopes.lib`, etc.) and exposes it on libfaust's include path. Imports
relative to the watched `.dsp` file's parent directory also resolve.

## Parameter model

Crucible exposes a fixed APVTS slot pool to the host so DAW automation
survives hot reloads:

- 128 float slots `slot.f.000`–`slot.f.127`, normalised `[0, 1]`.
- 32 bool slots `slot.b.000`–`slot.b.031`. `slot.b.000` is reserved for
  bypass.
- 16 bargraph readback slots `slot.m.000`–`slot.m.015`.

`ParamBridge` walks the JSON UI tree on every compile and binds each Faust
address (`/myprog/cutoff`) to the next free slot of the matching type. Bindings
persist by address across reloads, so a knob holding `cutoff = 0.5` stays at
0.5 after an edit that doesn't touch that control.

DAW automation lanes show the slot id (`slot.f.012`), not your Faust label —
a documented quirk of the dynamic-parameter model.

## Single-file watcher

Open one `.dsp` file at a time. External edits (your editor, an LLM agent,
`echo` to the file) win over the in-VST editor's unsaved draft. The in-VST
SAVE button is always Save-As; it confirms the path before writing.

## Common idioms

```faust
import("stdfaust.lib");

// Per-control smoothing avoids zipper noise on knob moves.
cutoff = hslider("cutoff [unit:Hz][scale:log]", 1000, 20, 20000, 1) : si.smoo;
res    = hslider("res",  0.5, 0, 1, 0.01) : si.smoo;

// Stereo split-by-cloning before splitting into L/R.
process = fi.resonlp(cutoff, res*10+0.5, 1) <: _, _;
```

For bargraph metering, declare `hbargraph` / `vbargraph` controls — Crucible
will render them in the controls panel.

## This is a v1 placeholder

The full skill document will cover MIDI metadata routing, voice management
for the Instrument variant, IO channel constraints, latency reporting, and
example libraries. For now: write Faust, save the file, listen.
