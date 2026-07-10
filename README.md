# Crucible

A VST3 plugin host for runtime-compiled Faust DSP, by ZCROSS. Built
on JUCE 8. Ships in two variants that share a single Core library:

- **Crucible FX** — VST3 effect (audio in -> audio out).
- **Crucible Instrument** — VST3 instrument (MIDI in -> audio out).

Both variants embed `libfaust` with an LLVM JIT and host whichever
Faust source file the user opens, compiling it on the fly and hot-
reloading on save.

## Install

Download the installer for your platform from the GitHub Releases
page:

**https://github.com/visionsofparadise/crucible/releases**

A single installer per platform drops both variants:

- `Crucible-vX.Y.Z-windows-x64.exe` — Inno Setup installer covering
  both VST3 bundles and both Standalone executables.
- `Crucible-vX.Y.Z-macos-universal.dmg` — disk image containing a
  `Crucible.pkg` that installs both VST3 bundles and both `.app`
  Standalones (universal: arm64 + x86_64).
- `Crucible-vX.Y.Z-linux-x64.tar.gz` — tarball with both `.vst3`
  bundles, both Standalone executables, and a user-local `install.sh`.

The v1 release artefacts are **unsigned**. Each OS has a one-time
unblock step on first run; the per-OS sections below tell the OS to
trust them.

### Windows

1. Download `Crucible-vX.Y.Z-windows-x64.exe` from the Releases page.
2. Double-click the installer. SmartScreen will warn about an unknown
   publisher — click **More info** -> **Run anyway**. JUCE plugins also
   require the **Microsoft Edge WebView2 runtime** for the embedded UI
   to render; it is preinstalled on most Windows 11 hosts. If missing,
   install the evergreen bootstrapper from
   https://developer.microsoft.com/microsoft-edge/webview2/.
3. UAC will prompt for elevation, then the installer drops
   `Crucible FX.vst3` and `Crucible Instrument.vst3` at
   `C:\Program Files\Common Files\VST3\` and both Standalones at
   `C:\Program Files\ZCROSS\Crucible\`. Start Menu shortcuts are added.
   To uninstall, use Add/Remove Programs.

Then rescan plugins in your DAW. The plugins appear as **Crucible FX**
and **Crucible Instrument** by **ZCROSS** under Fx and Instrument
respectively.

### macOS

1. Download `Crucible-vX.Y.Z-macos-universal.dmg` from the Releases
   page.
2. **Right-click** the `.dmg` -> **Open** to clear Gatekeeper, then
   double-click the mounted volume to view the contained
   `Crucible.pkg`.
3. **Right-click** `Crucible.pkg` -> **Open** to clear Gatekeeper on
   the installer itself, then follow the system installer. Files land
   at `/Library/Audio/Plug-Ins/VST3/Crucible FX.vst3/`,
   `/Library/Audio/Plug-Ins/VST3/Crucible Instrument.vst3/`,
   `/Applications/Crucible FX.app`, and
   `/Applications/Crucible Instrument.app`. To uninstall, delete those
   four paths manually.

If macOS still refuses to load the bundles after install (older
Gatekeeper policies sometimes quarantine paths under
`/Library/Audio/Plug-Ins/`), clear the quarantine attribute from a
terminal:

```
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/Crucible FX.vst3"
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/Crucible Instrument.vst3"
sudo spctl --add "/Library/Audio/Plug-Ins/VST3/Crucible FX.vst3"
sudo spctl --add "/Library/Audio/Plug-Ins/VST3/Crucible Instrument.vst3"
```

Then rescan plugins in your DAW. The plugins appear as **Crucible FX**
and **Crucible Instrument** by **ZCROSS** under Fx and Instrument
respectively.

### Linux

1. Download `Crucible-vX.Y.Z-linux-x64.tar.gz` from the Releases page.
2. Install the WebView runtime dependency so JUCE's
   `WebBrowserComponent` has a backend:
   ```
   sudo apt-get install libwebkit2gtk-4.1-0
   ```
   No signing concerns on Linux.
3. Extract the tarball into a fresh directory and run `bash
   install.sh`. VST3 bundles land at `~/.vst3/Crucible FX.vst3/` and
   `~/.vst3/Crucible Instrument.vst3/`; Standalones land at
   `~/.local/bin/crucible-fx` and `~/.local/bin/crucible-instrument`.
   To uninstall, delete those four paths.

Then rescan plugins in your DAW. The plugins appear as **Crucible FX**
and **Crucible Instrument** by **ZCROSS** under Fx and Instrument
respectively.

## Repository layout

```
code/
├── CMakeLists.txt        Top-level: JUCE FetchContent, options, UI build orchestration.
├── CMakePresets.json     Windows / macOS / Linux Debug + Release matrix.
├── Core/                 CrucibleCore static library (shared by both plugin variants).
├── FX/                   Crucible FX (Fx category, audio in -> audio out).
├── Instrument/           Crucible Instrument (Instrument category, MIDI in -> audio out).
├── ui/                   Shared Vite/React UI bundle, consumed by both plugins.
└── tests/                Catch2 v3 test target.
```

The two plugin folders are deliberately thin — bus configuration,
category metadata, and JUCE plugin entry points only. All hosted-Faust
runtime, UI plumbing, and parameter routing lives in `Core/`.

## Build from source

Only required if you want to build a development cut yourself. Releases
above already cover the supported platforms.

### Prerequisites

- **CMake 3.22 or newer**
- **Ninja** — single generator across every OS. On Windows install with
  `winget install Ninja-build.Ninja`. macOS: `brew install ninja`. Linux:
  `apt install ninja-build` (or distro equivalent).
- **C++17 toolchain**:
  - **Windows** — MSVC 2022 or later (Visual Studio 2026 Community is
    fine). The configure step must be run from a **Developer Command
    Prompt for VS 2026** (or any shell where `vcvarsall.bat x64` has
    been invoked) so that `cl.exe` and the Windows SDK are on `PATH`.
  - **macOS** — Xcode 14 or newer (Clang).
  - **Linux** — GCC 11 or newer, or Clang. On Debian/Ubuntu also install
    `libwebkit2gtk-4.1-dev` (or the 4.0 fallback) for the JUCE
    `WebBrowserComponent`.
- **Microsoft Edge WebView2 runtime** on Windows (preinstalled on most
  Windows 11 hosts; evergreen installer:
  https://developer.microsoft.com/microsoft-edge/webview2/).
- **Microsoft.Web.WebView2 NuGet package** (Windows builds only) — JUCE
  links statically against the WebView2 SDK headers and loader. Install
  the NuGet provider once, register the public NuGet feed, and pull the
  package from a PowerShell:
  ```powershell
  Install-PackageProvider -Name NuGet -Scope CurrentUser -Force
  Register-PackageSource -Provider NuGet -Name nugetRepository `
      -Location https://www.nuget.org/api/v2 -Force
  Install-Package Microsoft.Web.WebView2 -Scope CurrentUser `
      -RequiredVersion 1.0.3485.44 -Source nugetRepository -Force
  ```
  CMake's `FindWebView2.cmake` looks for the package under
  `%LOCALAPPDATA%\PackageManagement\NuGet\Packages\Microsoft.Web.WebView2*`.
  Set `JUCE_WEBVIEW2_PACKAGE_LOCATION` if you've installed it elsewhere.
- **Node.js 20+** with `npm` (Windows / macOS / Linux) — the release
  build runs `npm install && npm run build` inside `ui/` to bundle the
  React UI into the plugins via `juce_add_binary_data`. Debug builds
  with `CRUCIBLE_UI_DEV_SERVER=ON` skip the npm step and expect the
  Vite dev server at `http://localhost:5173`.
- **~1 GB of free disk** for the Faust 2.85.5 prebuilt artefact pulled
  by `FetchFaust.cmake` at configure time (313 MB on Linux, 424 MB on
  Windows, plus headers, plus the LLVM-bearing static library that
  `libfaustwithllvm` links into the plugins).
- **7-Zip** on Windows (`choco install 7zip` or `winget install
  7zip.7zip`) — `FetchFaust.cmake` shells out to `7z` to crack the
  upstream NSIS installer for `libfaustwithllvm.lib`. Not needed on
  macOS (uses `hdiutil`) or Linux (the prebuilt is a zip).

### Configure and build

From the `code/` directory (this folder):

```
cmake --preset windows-release
cmake --build --preset windows-release
```

Substitute `macos-release` / `linux-release` on the corresponding host.
Add `--target CrucibleFX_VST3` or `--target CrucibleInstrument_VST3` to
build a single artifact.

The first configure pulls JUCE 8.0.12 via `FetchContent` and can take
several minutes on a cold network.

### Debug build with Vite dev server

The `*-debug` presets set `CRUCIBLE_UI_DEV_SERVER=ON`, pointing both
plugins' `WebBrowserComponent` at `http://localhost:5173`. Start the
Vite dev server separately (`cd code/ui && npm run start`) before
launching the plugin in debug mode.

### Tests

```
cmake --build --preset windows-release --target crucible_tests
ctest --preset default-test --output-on-failure
```

On macOS / Linux, use `default-test-macos` / `default-test-linux`
respectively (the test presets are host-conditioned).

## Releases

Releases are produced by a GitHub Actions matrix workflow
(`.github/workflows/release.yml`) on every `v*` tag push. The workflow
builds Windows / macOS / Linux in parallel, runs the Catch2 tests, and
publishes the resulting installers to GitHub Releases as unsigned
artefacts. To cut a release:

```
git tag v0.1.0
git push origin v0.1.0
```

## License

Proprietary. All rights reserved. See [LICENSE](LICENSE).

Third-party components used at build and/or runtime:

- **JUCE 8** — used under its Personal license terms (free distribution
  while annual revenue is below the JUCE-published threshold). Re-check
  JUCE's current threshold before each paid release.
- **libfaust** (Faust 2.85.5) — released under GPLv2 **with a runtime
  linking exception** that explicitly permits linking `libfaust` into
  proprietary applications without imposing GPL on the application.
  Crucible relies on that exception.
- **LLVM** (bundled inside `libfaustwithllvm`) — used under the Apache
  License 2.0 with LLVM exceptions.
