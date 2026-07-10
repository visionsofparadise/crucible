; Inno Setup script for Crucible (Windows x64).
;
; Builds a system-install installer that drops two VST3 bundles at
;   C:\Program Files\Common Files\VST3\Crucible FX.vst3\
;   C:\Program Files\Common Files\VST3\Crucible Instrument.vst3\
; and both Standalone executables at
;   C:\Program Files\ZCROSS\Crucible\Crucible FX.exe
;   C:\Program Files\ZCROSS\Crucible\Crucible Instrument.exe
; with a Start Menu group and an Add/Remove Programs uninstaller.
;
; Version is supplied by the CI workflow via the preprocessor:
;   iscc /DAppVersion=v0.1.0 installers\windows\Crucible.iss
; The script is intentionally text-only (no banner images, no custom
; wizard pages) per design-vst.md "Installer chrome is text-only
; ZCROSS branding for v1".

#ifndef AppVersion
  #error AppVersion must be supplied via /DAppVersion=...
#endif

[Setup]
; AppId is a stable GUID that identifies this product across versions
; for upgrade and uninstall. Do not regenerate.
AppId={{4B2C0D9A-2F8E-4C4B-9F0A-7E5C9B2A1D34}
AppName=Crucible
AppVersion={#AppVersion}
AppPublisher=ZCROSS
AppPublisherURL=https://zcross.media
DefaultDirName={commonpf}\ZCROSS\Crucible
DefaultGroupName=Crucible
OutputDir=..\..\dist
OutputBaseFilename=Crucible-{#AppVersion}-windows-x64
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
WizardStyle=modern
UninstallDisplayName=Crucible
Compression=lzma2/ultra64
SolidCompression=yes

[Files]
; Crucible FX VST3 bundle (a directory) goes into the system-wide VST3 folder.
Source: "..\..\build\windows-release\FX\CrucibleFX_artefacts\Release\VST3\Crucible FX.vst3\*"; DestDir: "{commoncf64}\VST3\Crucible FX.vst3"; Flags: recursesubdirs createallsubdirs
; Crucible Instrument VST3 bundle.
Source: "..\..\build\windows-release\Instrument\CrucibleInstrument_artefacts\Release\VST3\Crucible Instrument.vst3\*"; DestDir: "{commoncf64}\VST3\Crucible Instrument.vst3"; Flags: recursesubdirs createallsubdirs
; Standalone executables go into the per-product Program Files folder.
Source: "..\..\build\windows-release\FX\CrucibleFX_artefacts\Release\Standalone\Crucible FX.exe"; DestDir: "{app}"
Source: "..\..\build\windows-release\Instrument\CrucibleInstrument_artefacts\Release\Standalone\Crucible Instrument.exe"; DestDir: "{app}"

[Icons]
Name: "{group}\Crucible FX"; Filename: "{app}\Crucible FX.exe"
Name: "{group}\Crucible Instrument"; Filename: "{app}\Crucible Instrument.exe"
Name: "{group}\Uninstall Crucible"; Filename: "{uninstallexe}"
