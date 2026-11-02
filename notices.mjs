#!/usr/bin/env node
// Regenerates THIRD-PARTY-NOTICES.txt from the pinned upstream licence texts
// and the web interface's runtime packages in ui/node_modules. Run it from ui/
// with `npm run notices` after a dependency or JUCE/Faust/LLVM bump, and update
// the pins below to match CMakeLists.txt and CMake/FetchFaust.cmake.
import { execSync } from "node:child_process";
import { readdirSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.join(root, "ui");

const JUCE_TAG = "8.0.12";
const FAUST_VERSION = "2.85.5";
const LLVM_TAG = "llvmorg-17.0.6";
const UIW_TAG = "v4.25.9";

const juce = (file) =>
  `https://raw.githubusercontent.com/juce-framework/JUCE/${JUCE_TAG}/modules/${file}`;

const normalise = (text) => text.replace(/\r\n/g, "\n").trimEnd();

async function fetchText(url) {
  const response = await fetch(url, {
    headers: { "User-Agent": "curl/8 (crucible notices)" },
  });
  if (!response.ok) throw new Error(`${response.status} fetching ${url}`);
  return normalise(await response.text());
}

// Returns the lines from the first line matching `start` up to, not including,
// the first later line matching `end`.
function between(text, start, end) {
  const lines = text.split("\n");
  const from = lines.findIndex((line) => start.test(line));
  const to = lines.findIndex((line, index) => index > from && end.test(line));
  if (from < 0 || to < 0) throw new Error(`section ${start} not found`);
  return lines.slice(from, to).join("\n").trimEnd();
}

const RULE = "=".repeat(78);
const section = (title, meta, text) =>
  [RULE, title, RULE, ...meta, "", text, ""].join("\n");

const [
  faustCopying,
  llvm,
  vst3,
  zlibReadme,
  png,
  jpegReadme,
  harfbuzz,
  sheenbidi,
  flac,
  ogg,
  uiw,
] = await Promise.all([
  fetchText(
    `https://raw.githubusercontent.com/grame-cncm/faust/${FAUST_VERSION}/COPYING.txt`,
  ),
  fetchText(
    `https://raw.githubusercontent.com/llvm/llvm-project/${LLVM_TAG}/llvm/LICENSE.TXT`,
  ),
  fetchText(
    juce("juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt"),
  ),
  fetchText(juce("juce_core/zip/zlib/README")),
  fetchText(juce("juce_graphics/image_formats/pnglib/LICENSE")),
  fetchText(juce("juce_graphics/image_formats/jpglib/README")),
  fetchText(juce("juce_graphics/fonts/harfbuzz/COPYING")),
  fetchText(juce("juce_graphics/unicode/sheenbidi/LICENSE")),
  fetchText(juce("juce_audio_formats/codecs/flac/Flac%20Licence.txt")),
  fetchText(
    juce("juce_audio_formats/codecs/oggvorbis/Ogg%20Vorbis%20Licence.txt"),
  ),
  fetchText(
    `https://raw.githubusercontent.com/uiwjs/react-codemirror/${UIW_TAG}/LICENSE`,
  ),
]);

const sections = [
  section(
    `libfaust ${FAUST_VERSION}`,
    [
      "Copyright (C) 2003-2024 GRAME, Centre National de Creation Musicale",
      "Copyright (C) 2023-2024 INRIA",
      "Licence: GNU Lesser General Public License 2.1 or later",
      `Source: https://github.com/grame-cncm/faust/releases/download/${FAUST_VERSION}/faust-${FAUST_VERSION}.tar.gz`,
      "Crucible links libfaust statically and bundles the Faust standard libraries",
      "(*.lib) verbatim; each library file states its own licence in its header.",
    ],
    // COPYING.txt opens with the Faust notice; the LGPL text follows it.
    faustCopying.slice(
      faustCopying.search(/^ *GNU LESSER GENERAL PUBLIC LICENSE/m),
    ),
  ),
  section(
    `LLVM (bundled inside the libfaust ${FAUST_VERSION} prebuilt libraries)`,
    [
      "Licence: Apache License 2.0 with LLVM Exceptions",
      "Source: https://github.com/llvm/llvm-project",
    ],
    llvm,
  ),
  section(
    `JUCE ${JUCE_TAG}`,
    [
      "Copyright (c) Raw Material Software Limited",
      "Licence: GNU Affero General Public License 3.0 (the same text as Crucible's LICENSE)",
      `Source: https://github.com/juce-framework/JUCE/tree/${JUCE_TAG}`,
      "The ui/vendor/juce-framework-frontend files are part of JUCE under the same licence.",
    ],
    "See LICENSE.",
  ),
  section("VST 3 SDK (bundled in JUCE)", ["Licence: MIT"], vst3),
  section(
    "zlib (bundled in JUCE)",
    ["Licence: zlib"],
    between(zlibReadme, /^Copyright notice:/, /^If you use the zlib library/),
  ),
  section(
    "libpng (bundled in JUCE)",
    ["Licence: PNG Reference Library License version 2"],
    png,
  ),
  section(
    "libjpeg (bundled in JUCE)",
    [
      "Licence: Independent JPEG Group License",
      "This software is based in part on the work of the Independent JPEG Group.",
    ],
    between(jpegReadme, /^LEGAL ISSUES$/, /^The Unix configuration script/),
  ),
  section("HarfBuzz (bundled in JUCE)", ["Licence: Old MIT"], harfbuzz),
  section(
    "SheenBidi (bundled in JUCE)",
    ["Licence: Apache License 2.0"],
    sheenbidi,
  ),
  section("FLAC (bundled in JUCE)", ["Licence: BSD-3-Clause"], flac),
  section("Ogg Vorbis (bundled in JUCE)", ["Licence: BSD-3-Clause"], ogg),
];

// `npm ls` exits non-zero over the vendored juce-framework-frontend file
// dependency, which has no version; its output is still complete.
let listing;
try {
  listing = execSync("npm ls --omit=dev --all --parseable", {
    cwd: uiDir,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "ignore"],
  });
} catch (error) {
  listing = error.stdout;
}

const packages = new Map();
for (const dir of listing.trim().split(/\r?\n/).slice(1)) {
  if (dir.endsWith("juce-framework-frontend")) continue;
  const manifest = JSON.parse(
    readFileSync(path.join(dir, "package.json"), "utf8"),
  );
  const key = `${manifest.name}@${manifest.version}`;
  if (packages.has(key)) continue;
  const licenceFile = readdirSync(dir).find((file) =>
    /^(licen[cs]e|copying|ofl)/i.test(file),
  );
  // The @uiw packages publish without a licence file; their repository carries it.
  const text = licenceFile
    ? normalise(readFileSync(path.join(dir, licenceFile), "utf8"))
    : manifest.name.startsWith("@uiw/")
      ? uiw
      : null;
  if (text === null) throw new Error(`no licence file for ${key}`);
  packages.set(key, { licence: manifest.license, text });
}
for (const [key, { licence, text }] of [...packages].sort(([a], [b]) =>
  a.localeCompare(b),
)) {
  sections.push(
    section(`${key} (web interface)`, [`Licence: ${licence}`], text),
  );
}

const header = [
  "Crucible third-party notices",
  "",
  "Crucible is licensed under the GNU Affero General Public License 3.0 (see",
  "LICENSE). Its plugins and standalone applications include the third-party",
  "components below, each under the licence that follows its heading.",
  "",
].join("\n");

writeFileSync(
  path.join(root, "THIRD-PARTY-NOTICES.txt"),
  `${header}\n${sections.join("\n")}`,
);
console.log(
  `THIRD-PARTY-NOTICES.txt: ${sections.length} components, ${packages.size} web interface packages`,
);
