# Contributing

## Prerequisites

- CMake 3.22 or newer
- Ninja
- Node 24
- npm 12
- A C++17 toolchain: MSVC 2022 or newer on Windows, run from a shell where `vcvarsall.bat x64` has been invoked, Xcode 14 or newer on macOS, GCC 11 or newer or Clang on Linux
- On Windows, 7-Zip and the `Microsoft.Web.WebView2` NuGet package, found under `%LOCALAPPDATA%\PackageManagement\NuGet\Packages` or at `JUCE_WEBVIEW2_PACKAGE_LOCATION`
- On Linux, `libwebkit2gtk-4.1-dev`
- About 1 GB of free disk for the Faust prebuilt that the first configure downloads

## Setup

Run `npm ci` in `ui/`. Its `prepare` script installs the git hooks and the pinned Trivy and actionlint binaries.

## Pull requests

These checks and tests must pass before a pull request is opened. Replace `windows` with `macos` or `linux` on those hosts, and use the matching `default-test-macos` or `default-test-linux` test preset.

```sh
cd ui && npm run check
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset default-test
```

## Commits

Commits follow [Conventional Commits](https://www.conventionalcommits.org/), and the commit-msg hook enforces it. A commit body states the reasoning for the change and links the code, changes and commits it references with [Inspectory](https://github.com/visionsofparadise/inspectory) links.
