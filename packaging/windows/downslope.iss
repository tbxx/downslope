; Inno Setup script for the Downslope VST3 plugin (Windows).
; Plugin only — no Standalone, no Host app. Unsigned.
;
; Version is injected by CI via /DMyAppVersion=x.y.z; falls back to the value
; below for local runs. Build with:
;   ISCC.exe /DMyAppVersion=0.1.0 packaging\windows\downslope.iss

#define MyAppName "Downslope"
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#define MyAppPublisher "x09Music"

; Path to the built VST3 bundle, relative to this script. Override with
; /DVST3Source=... if your build tree differs.
#ifndef VST3Source
  #define VST3Source "..\..\build\Downslope_artefacts\Release\VST3\Downslope.vst3"
#endif

[Setup]
AppId={{8B2D5F3A-1C4E-4A7B-9E2D-3F6A8C1B5D7E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName={#MyAppName} (VST3)
OutputDir=.
OutputBaseFilename=Downslope-{#MyAppVersion}-Windows
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern

[Files]
; VST3 on Windows is a folder bundle; copy it recursively.
Source: "{#VST3Source}\*"; DestDir: "{commoncf64}\VST3\Downslope.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Downslope.vst3"
