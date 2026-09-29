; GNARL - Windows installer (Inno Setup 6)
;
; WHY AN INSTALLER AND NOT A ZIP. The zip works and is honest about what it
; does, but it puts the burden of knowing where a VST3 goes onto the person
; downloading it - and that is exactly what went wrong the first time this
; was tested: the folder was copied to C:\Program Files\VSTPlugins, which is
; the VST2 directory, and FL Studio correctly never found it. A scanner only
; looks where the standard says to look, so the install path is not a
; preference, it is the contract.
;
; It also solves the SmartScreen problem halfway. The build is unsigned, so
; Windows still warns - but it warns ONCE, about one file, instead of
; Defender silently quarantining a loose .exe out of an extracted folder with
; no dialog at all, which is the failure mode that looks like a broken
; download.

#define AppName    "GNARL"
#define AppVersion GetEnv("GNARL_VERSION")
#define AppPublisher "GNARL"

[Setup]
; A fixed GUID, and it must never change: Windows identifies the product by
; this, so a new one would make every future installer look like a DIFFERENT
; application and leave the old one behind in Add/Remove Programs. Same
; reasoning as the frozen parameter IDs in section 4.
AppId={{7E2C1A44-9B3D-4F61-8A57-2D9F4C6B1E03}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=GNARL-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; The VST3 goes into Common Files, which is not writable by a normal user.
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
DisableProgramGroupPage=yes
LicenseFile=
UninstallDisplayIcon={app}\GNARL.exe

[Types]
Name: "full";   Description: "Everything"
Name: "custom"; Description: "Choose what to install"; Flags: iscustom

[Components]
; The VST3 is not optional in practice, but naming it makes the installer
; say out loud where it is going - which is the thing people get wrong.
Name: "vst3";       Description: "VST3 plugin (Common Files\VST3)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone app (no DAW needed)";  Types: full custom
Name: "presets";    Description: "150 factory presets";             Types: full custom

[Files]
; A VST3 is a FOLDER, not a file. recursesubdirs plus createallsubdirs, or
; it arrives as a bundle missing its contents and the scanner rejects it
; without saying why.
Source: "stage\GNARL.vst3\*"; DestDir: "{commoncf}\VST3\GNARL.vst3"; \
    Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

Source: "stage\GNARL.exe"; DestDir: "{app}"; \
    Components: standalone; Flags: ignoreversion skipifsourcedoesntexist

Source: "stage\Presets\*"; DestDir: "{commondocs}\GNARL\Presets"; \
    Components: presets; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

Source: "stage\INSTALL.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}";                Filename: "{app}\GNARL.exe"; Components: standalone
Name: "{autodesktop}\{#AppName}";          Filename: "{app}\GNARL.exe"; Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; \
    GroupDescription: "Shortcuts:"; Components: standalone; Flags: unchecked

[Run]
Filename: "{app}\GNARL.exe"; Description: "Open GNARL now"; \
    Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
; The VST3 bundle's own folder, which Inno will not remove on its own
; because it created subdirectories inside it.
Type: filesandordirs; Name: "{commoncf}\VST3\GNARL.vst3"
