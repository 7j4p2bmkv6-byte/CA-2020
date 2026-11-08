; CA-2020 Chord Arpeggiator - Windows installer (Inno Setup 6)
#define AppName "CA-2020"
#define AppVersion "1.0.0"
#define Art "..\build\CA2020_artefacts\Release"

[Setup]
AppId={{8F2C7A61-2020-4CA0-9E1B-0CA2020C0DE5}
AppName={#AppName} Chord Arpeggiator
AppVersion={#AppVersion}
AppPublisher=Landgren Audio
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=CA2020-{#AppVersion}-Windows-Setup
SetupIconFile=..\Resources\icon.ico
UninstallDisplayIcon={app}\CA-2020.exe
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes

[Types]
Name: "full"; Description: "App + VST3 plug-in"
Name: "custom"; Description: "Choose"; Flags: iscustom

[Components]
Name: "app";  Description: "CA-2020 standalone app (with preview synth)"; Types: full custom
Name: "vst3"; Description: "CA-2020 VST3 plug-in (for L-DAW, Reaper, Cubase, Ableton, Bitwig...)"; Types: full custom

[Files]
Source: "{#Art}\Standalone\CA-2020.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#Art}\VST3\CA-2020.vst3\*"; DestDir: "{commoncf64}\VST3\CA-2020.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: "desktopicon"; Description: "Create a desktop icon"; Components: app

[Icons]
Name: "{group}\CA-2020"; Filename: "{app}\CA-2020.exe"; Components: app
Name: "{autodesktop}\CA-2020"; Filename: "{app}\CA-2020.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\CA-2020.exe"; Description: "Start CA-2020 now"; Flags: nowait postinstall skipifsilent; Components: app
