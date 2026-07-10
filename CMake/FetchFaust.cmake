# Fetches Faust 2.85.5 prebuilt static binaries + source tarball.
# Exposes:
#   faust::withllvm    IMPORTED static library (libfaustwithllvm)
#   FAUST_INCLUDE_DIR  path to Faust headers (from the source tarball)
#   FAUST_STDLIB_DIR   path to share/faust/*.lib (from the source tarball)
#   FAUST_VERSION      version string ("2.85.5")
#
# Pin policy: bump only via a Design decision in design-vst.md.
#
# Per the plan, SHA256 hashes were computed against the Faust 2.85.5 release
# assets on 2026-05-10. If FetchContent reports a hash mismatch on first run,
# escalate before changing any hash blindly — Faust's release URLs are
# normally immutable so a mismatch indicates upstream changed something.

set(FAUST_VERSION "2.85.5")
set(FAUST_RELEASE_BASE "https://github.com/grame-cncm/faust/releases/download/${FAUST_VERSION}")

set(FAUST_SHA_SOURCE        "fc18bc2b1b31044d0bd2c35ee92d80d4428c9008ac6a03acf4163109803941d7")
set(FAUST_SHA_WIN64         "559fa6099ea9a2cde811677ecc5a1736df13dd31344f145a9df07ff75183330b")
set(FAUST_SHA_MACOS_ARM64   "78eed5667beb1f451a2d13fa89bc6575068cd40d8f814a3396088b725bd7194d")
set(FAUST_SHA_MACOS_X64     "9c7e214a4982e5eeaf00ea3108b8c2c28f496cc4eea8ea8202e8e7f36ccc6ab3")
set(FAUST_SHA_LINUX_X64     "8ad9e3c6c55f451a82da12c543c376276460c260413f216b8baed44bf07e32d5")
set(FAUST_SHA_LINUX_AARCH64 "4e2cfb57f9e9acdb03eb1e26d928250d228a202e074cf912c0eca45b15b1f081")

include(FetchContent)

# Source tarball — platform-neutral; ships headers + stdlib.
FetchContent_Declare(faust_source
  URL ${FAUST_RELEASE_BASE}/faust-${FAUST_VERSION}.tar.gz
  URL_HASH SHA256=${FAUST_SHA_SOURCE}
)
FetchContent_MakeAvailable(faust_source)

# Per-platform prebuilt static libraries.
if(WIN32)
  # Windows: NSIS installer .exe; needs 7z to unpack.
  FetchContent_Declare(faust_prebuilt
    URL ${FAUST_RELEASE_BASE}/Faust-${FAUST_VERSION}-win64.exe
    URL_HASH SHA256=${FAUST_SHA_WIN64}
    DOWNLOAD_NO_EXTRACT TRUE
  )
elseif(APPLE)
  # macOS: dmg; needs hdiutil (macOS-native) to attach + copy.
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(FAUST_DMG "Faust-${FAUST_VERSION}-arm64.dmg")
    set(_faust_dmg_sha ${FAUST_SHA_MACOS_ARM64})
  else()
    set(FAUST_DMG "Faust-${FAUST_VERSION}-x64.dmg")
    set(_faust_dmg_sha ${FAUST_SHA_MACOS_X64})
  endif()
  FetchContent_Declare(faust_prebuilt
    URL ${FAUST_RELEASE_BASE}/${FAUST_DMG}
    URL_HASH SHA256=${_faust_dmg_sha}
    DOWNLOAD_NO_EXTRACT TRUE
  )
elseif(UNIX)
  # Linux: zip; ships static libs only (no headers — those come from source tarball).
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(FAUST_ZIP "libfaust-ubuntu-aarch64.zip")
    set(_faust_zip_sha ${FAUST_SHA_LINUX_AARCH64})
  else()
    set(FAUST_ZIP "libfaust-ubuntu-x86_64.zip")
    set(_faust_zip_sha ${FAUST_SHA_LINUX_X64})
  endif()
  FetchContent_Declare(faust_prebuilt
    URL ${FAUST_RELEASE_BASE}/${FAUST_ZIP}
    URL_HASH SHA256=${_faust_zip_sha}
  )
else()
  message(FATAL_ERROR "Crucible: unsupported platform for Faust prebuilts")
endif()

FetchContent_MakeAvailable(faust_prebuilt)

# Per-platform extraction of libfaustwithllvm into a known path.
set(FAUST_LIB_DIR ${faust_prebuilt_SOURCE_DIR}/extracted)
file(MAKE_DIRECTORY ${FAUST_LIB_DIR})

if(WIN32)
  # Use 7z (assumed available on the developer machine and on CI runners) to
  # crack the NSIS installer and pull out the static lib + headers.
  find_program(SEVENZIP_EXE NAMES 7z 7za 7zz REQUIRED)
  if(NOT EXISTS ${FAUST_LIB_DIR}/libfaustwithllvm.lib)
    execute_process(
      COMMAND ${SEVENZIP_EXE} x -y "${faust_prebuilt_SOURCE_DIR}/Faust-${FAUST_VERSION}-win64.exe"
              -o${FAUST_LIB_DIR} "lib/libfaustwithllvm.lib"
      RESULT_VARIABLE _rc
      OUTPUT_QUIET
    )
    if(NOT _rc EQUAL 0)
      message(FATAL_ERROR "Crucible: failed to extract libfaustwithllvm.lib from Faust Windows installer (rc=${_rc})")
    endif()
    if(NOT EXISTS ${FAUST_LIB_DIR}/lib/libfaustwithllvm.lib)
      message(FATAL_ERROR
        "Crucible: 7-Zip extraction returned 0 but produced no "
        "lib/libfaustwithllvm.lib in ${FAUST_LIB_DIR}. The upstream "
        "installer layout may have changed — re-check the asset path.")
    endif()
    file(RENAME ${FAUST_LIB_DIR}/lib/libfaustwithllvm.lib ${FAUST_LIB_DIR}/libfaustwithllvm.lib)
  endif()
  set(FAUST_LIB_PATH ${FAUST_LIB_DIR}/libfaustwithllvm.lib)
elseif(APPLE)
  find_program(HDIUTIL_EXE hdiutil REQUIRED)
  if(NOT EXISTS ${FAUST_LIB_DIR}/libfaustwithllvm.a)
    set(_mount /tmp/crucible-faust-mount)
    execute_process(COMMAND ${HDIUTIL_EXE} attach -nobrowse -mountpoint ${_mount} "${faust_prebuilt_SOURCE_DIR}/${FAUST_DMG}")
    # The dmg's internal folder nesting is not contractual; locate the lib wherever it sits.
    file(GLOB_RECURSE _faust_dmg_lib "${_mount}/*libfaustwithllvm.a")
    if(_faust_dmg_lib)
      list(GET _faust_dmg_lib 0 _faust_dmg_lib_src)
      file(COPY ${_faust_dmg_lib_src} DESTINATION ${FAUST_LIB_DIR})
    endif()
    execute_process(COMMAND ${HDIUTIL_EXE} detach ${_mount})
  endif()
  set(FAUST_LIB_PATH ${FAUST_LIB_DIR}/libfaustwithllvm.a)
else()
  # Linux: zip auto-extracted by FetchContent; the lib's nesting inside the
  # archive is not contractual, so locate it wherever it landed.
  file(GLOB_RECURSE _faust_linux_lib "${faust_prebuilt_SOURCE_DIR}/*libfaustwithllvm.a")
  if(_faust_linux_lib)
    list(GET _faust_linux_lib 0 FAUST_LIB_PATH)
  else()
    set(FAUST_LIB_PATH ${faust_prebuilt_SOURCE_DIR}/lib/libfaustwithllvm.a)
  endif()
endif()

if(NOT EXISTS ${FAUST_LIB_PATH})
  message(FATAL_ERROR "Crucible: libfaustwithllvm not found at ${FAUST_LIB_PATH}")
endif()

# Source-tarball layout: top-level architecture/, compiler/, libraries/. Headers
# live at architecture/faust/. Stdlib lives at libraries/*.lib.
set(FAUST_INCLUDE_DIR ${faust_source_SOURCE_DIR}/architecture)
set(FAUST_HEADERS_DIR ${faust_source_SOURCE_DIR}/compiler)
set(FAUST_STDLIB_DIR  ${faust_source_SOURCE_DIR}/libraries)

# Verify headers we depend on exist.
#
# Note: the plan's verification list originally included faust/libfaust.h,
# but in the 2.85.5 source tarball libfaust.h actually lives at
# compiler/generator/libfaust.h, not under architecture/faust/. Crucible's
# Phase 1 smoke test only needs <faust/dsp/llvm-dsp.h>, so the check is
# scoped to headers that genuinely live under FAUST_INCLUDE_DIR.
foreach(_h dsp/llvm-dsp.h dsp/poly-llvm-dsp.h)
  if(NOT EXISTS ${FAUST_INCLUDE_DIR}/faust/${_h})
    message(FATAL_ERROR "Crucible: missing Faust header faust/${_h} (looked at ${FAUST_INCLUDE_DIR}/faust/${_h})")
  endif()
endforeach()

add_library(faust_withllvm STATIC IMPORTED GLOBAL)
set_target_properties(faust_withllvm PROPERTIES
  IMPORTED_LOCATION ${FAUST_LIB_PATH}
  INTERFACE_INCLUDE_DIRECTORIES ${FAUST_INCLUDE_DIR}
)
add_library(faust::withllvm ALIAS faust_withllvm)
