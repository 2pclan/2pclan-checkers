; Inno Setup Script for LAN Checkers
; This creates a standalone installer that includes all Qt dependencies

#define MyAppName "LAN Checkers"
#define MyAppVersion "1.0"
#define MyAppPublisher "2PClan"
#define MyAppExeName "2pclan-checkers.exe"
#define MyAppSourceDir "2pclan-checkers-win64"

[Setup]
; Application info
AppId={{8F3B5A2D-4C1E-4D8F-9A3B-7C2E1F5D6A8B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; Output settings
OutputDir=.
OutputBaseFilename=LAN-Checkers-Setup
; Compression
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
; Installer appearance
WizardStyle=modern
SetupIconFile=
; Privileges
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; Other
AllowNoIcons=yes
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main executable
Source: "{#MyAppSourceDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt Core DLLs
Source: "{#MyAppSourceDir}\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyAppSourceDir}\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyAppSourceDir}\Qt6Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyAppSourceDir}\Qt6Svg.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyAppSourceDir}\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion

; MinGW runtime
Source: "{#MyAppSourceDir}\libgcc_s_seh-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyAppSourceDir}\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyAppSourceDir}\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion

; DirectX
Source: "{#MyAppSourceDir}\D3Dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion

; Qt plugins - platforms
Source: "{#MyAppSourceDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs

; Qt plugins - styles
Source: "{#MyAppSourceDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs

; Qt plugins - imageformats
Source: "{#MyAppSourceDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs

; Qt plugins - iconengines
Source: "{#MyAppSourceDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs

; Qt plugins - generic
Source: "{#MyAppSourceDir}\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs

; Qt plugins - networkinformation
Source: "{#MyAppSourceDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs

; Qt plugins - tls
Source: "{#MyAppSourceDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
