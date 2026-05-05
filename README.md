# Crucible

A VST3 plugin that hosts user-supplied Faust source code, by ZCROSS. Built
on JUCE 8. Ships in two variants that share a single Core library:

- **Crucible FX** — VST3 effect (audio in -> audio out).
- **Crucible Instrument** — VST3 instrument (MIDI in -> audio out).

## Install

Download the latest release for your platform from the GitHub Releases
page:

**https://github.com/visionsofparadise/crucible/releases**

Each release ships archives for both variants per platform:

- `Crucible-vX.Y.Z-windows-x64.zip` — `Crucible FX.vst3` and
  `Crucible Instrument.vst3` (directory bundles) plus the matching
  Standalone executables.
- `Crucible-vX.Y.Z-macos-universal.zip` — `.vst3` bundles and `.app`
  Standalones (universal: arm64 + x86_64).
- `Crucible-vX.Y.Z-linux-x64.zip` — `.vst3` directory bundles plus the
  Standalone executables.

### Install location

After unzipping, move each `.vst3` to your DAW's VST3 search path:

| OS      | Path                                          |
|---------|-----------------------------------------------|
| Windows | `C:\Program Files\Common Files\VST3\`         |
| macOS   | `~/Library/Audio/Plug-Ins/VST3/` (or `/Library/Audio/Plug-Ins/VST3/` system-wide) |
| Linux   | `~/.vst3/` (or `/usr/lib/vst3/` system-wide)  |

Then rescan plugins in your DAW. The plugins appear as **Crucible FX**
and **Crucible Instrument** by **ZCROSS** under Fx and Instrument
respectively.

### Unblocking the unsigned binaries

The v1 release artifacts are **unsigned**. Each OS will refuse to load
them on first run; the steps below tell the OS to trust them.

- **Windows** — SmartScreen will block the Standalone executables on
  first run. Click **More info** -> **Run anyway**. The `.vst3`s do not
  trigger SmartScreen since the DAW loads them as plain DLLs inside
  directory bundles. Note: JUCE-backed plugins require the
  **Microsoft Edge WebView2 runtime** for the embedded UI to render. It
  is preinstalled on most Windows 11 hosts; if missing, install the
  evergreen bootstrapper from
  https://developer.microsoft.com/microsoft-edge/webview2/.
- **macOS** — Gatekeeper will quarantine the `.vst3`s and `.app`s.
  After unzipping, from a terminal in the unzip directory:
  ```
  xattr -cr "Crucible FX.vst3"
  xattr -cr "Crucible Instrument.vst3"
  xattr -cr "Crucible FX.app"
  xattr -cr "Crucible Instrument.app"
  spctl --add "Crucible FX.vst3"
  spctl --add "Crucible Instrument.vst3"
  spctl --add "Crucible FX.app"
  spctl --add "Crucible Instrument.app"
  ```
  Then move the `.vst3`s to `~/Library/Audio/Plug-Ins/VST3/` and the
  `.app`s wherever convenient. (`spctl --add` requires `sudo` if the
  bundle lives under `/Library/`; user-local installs do not.)
- **Linux** — no signing concerns. Make sure `libwebkit2gtk-4.1-0` (or
  the 4.0 fallback) is installed at runtime so JUCE's
  `WebBrowserComponent` has a webview backend:
  ```
  sudo apt-get install libwebkit2gtk-4.1-0
  ```

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
cmake --build --preset windows-release --target CrucibleTests
ctest --preset default-test --output-on-failure
```

On macOS / Linux, use `default-test-macos` / `default-test-linux`
respectively (the test presets are host-conditioned).

## Releases

Releases are produced by a GitHub Actions matrix workflow on every `v*`
tag push. The workflow builds Windows / macOS / Linux in parallel, runs
the Catch2 tests, and publishes the resulting ZIPs to GitHub Releases as
unsigned artefacts. To cut a release:

```
git tag v0.1.0
git push origin v0.1.0
```

## License

Proprietary. See [LICENSE](LICENSE). JUCE 8 used under its Personal
license terms.
