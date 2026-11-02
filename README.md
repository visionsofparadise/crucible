# Crucible

Crucible is a VST3 plugin that runs your own [Faust](https://faust.grame.fr) DSP code inside a DAW. Open a Faust source file, and Crucible compiles it on the fly and reloads it each time you save.

It comes in two variants, both installed together:

- **Crucible FX** is an effect plugin, audio in and audio out.
- **Crucible Instrument** is an instrument plugin, MIDI in and audio out.

Each variant also runs as a standalone application. The plugins appear in your DAW as **Crucible FX** and **Crucible Instrument** by **ZCROSS**, so rescan your plugins after installing.

## Install

Download the installer for your platform from the [releases page](https://github.com/visionsofparadise/crucible/releases). The releases are unsigned, so each system asks you to approve the installer once.

### Windows

Run `Crucible-vX.Y.Z-windows-x64.exe`. SmartScreen warns about an unknown publisher, so choose **More info**, then **Run anyway**. The plugin's interface needs the [Microsoft Edge WebView2 runtime](https://developer.microsoft.com/microsoft-edge/webview2/), which most Windows 11 systems already have. Uninstall from Add/Remove Programs.

### macOS

Open `Crucible-vX.Y.Z-macos-universal.dmg` with a right-click, then **Open**. Open the `Crucible.pkg` inside the same way and follow the installer. To uninstall, delete `Crucible FX.vst3` and `Crucible Instrument.vst3` from `/Library/Audio/Plug-Ins/VST3/`, both `.app` files from `/Applications/`, and the `/Library/Application Support/ZCROSS/Crucible/` folder.

If macOS still refuses to load the plugins, clear their quarantine attribute:

```sh
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/Crucible FX.vst3"
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/Crucible Instrument.vst3"
```

### Linux

Install the WebView runtime with `sudo apt-get install libwebkit2gtk-4.1-0`. Extract `Crucible-vX.Y.Z-linux-x64.tar.gz` and run `bash install.sh`. It installs the plugins to `~/.vst3/` and the standalone applications to `~/.local/bin/` as `crucible-fx` and `crucible-instrument`, and the license files to `~/.local/share/doc/crucible/`. To uninstall, delete those five paths.

## Demos

The [demos](demos) folder holds two Faust sources to open in Crucible: `demo-fx.dsp` for the effect and `demo-instrument.dsp` for the instrument.

## License

Copyright (C) 2026 ZCROSS Audio.

Crucible is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, version 3. It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE](LICENSE) for the full terms.

Crucible includes these third-party components, whose licenses and notices are in [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt). The installers place both files beside the plugins.

- **[JUCE](https://github.com/juce-framework/JUCE/tree/8.0.12) 8.0.12**, under the GNU Affero General Public License 3.0, with the libraries it bundles: the VST 3 SDK, zlib, libpng, libjpeg, HarfBuzz, SheenBidi, FLAC and Ogg Vorbis. This software is based in part on the work of the Independent JPEG Group.
- **[libfaust](https://github.com/grame-cncm/faust/releases/download/2.85.5/faust-2.85.5.tar.gz) 2.85.5**, Copyright (C) 2003-2024 GRAME and (C) 2023-2024 INRIA, under the GNU Lesser General Public License 2.1 or later.
- **[LLVM](https://github.com/llvm/llvm-project)**, bundled inside libfaust, under the Apache License 2.0 with LLVM exceptions.
- **The Faust standard libraries**, bundled verbatim under the licenses each file and function declares.
- **The web interface's packages**: React, CodeMirror, Lezer, lucide-react, clsx, tailwind-merge and their dependencies under the MIT and ISC licenses, and the Azeret Mono, Doto and Pirata One fonts under the SIL Open Font License 1.1.

The source for each release is the tagged commit of this repository, which fetches the pinned JUCE and Faust sources linked above when it builds.
