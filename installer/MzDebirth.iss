;
; MzDebirth - Created by Simon Moscrop
; Free software under the GNU AGPL v3; see LICENSE.
;
; The Windows installer: the standalone app, and the sample disks the Mz950 opens with.
;
;     .\build.ps1 -Release         builds the app, then this, into dist\
;
; build.ps1 passes the version read from CMakeLists.txt, so it is written down once, and
; where Mz950's disks are. Compiled by hand without them, the installer says 0.0.0 and
; looks for the disks beside this repository.
;
; Setup asks whether to install for everyone or just this user. Just me needs no
; administrator rights.
;
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#ifndef Disks
  #define Disks "..\..\Mz950\disks"
#endif

#define Artefacts "..\build\MzDebirth_artefacts\Release"

[Setup]
; The installer's identity: never change it, or a new version installs beside the old one
; rather than upgrading it.
AppId={{6C2F8E5A-3B1D-4F7A-9E0C-8D4B2A1F5E73}
AppName=MzDebirth
AppVersion={#AppVersion}
AppVerName=MzDebirth {#AppVersion}
AppPublisher=Simon Moscrop
AppPublisherURL=https://github.com/simozzer/MzDebirth
AppSupportURL=https://github.com/simozzer/MzDebirth/issues
AppUpdatesURL=https://github.com/simozzer/MzDebirth/releases
AppCopyright=Created by Simon Moscrop
VersionInfoVersion={#AppVersion}
VersionInfoCopyright=Created by Simon Moscrop
VersionInfoCompany=Simon Moscrop

DefaultDirName={autopf}\MzDebirth
DefaultGroupName=MzDebirth
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE

PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

OutputDir=..\dist
OutputBaseFilename=MzDebirth-{#AppVersion}-win64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\MzDebirth.exe
UninstallDisplayName=MzDebirth {#AppVersion}

CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#Artefacts}\Standalone\MzDebirth.exe"; DestDir: "{app}"; Flags: ignoreversion
; The app opens the drums from here, and LOAD DISK starts here.
Source: "{#Disks}\*.hfe"; DestDir: "{app}\disks"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\MzDebirth"; Filename: "{app}\MzDebirth.exe"
Name: "{autodesktop}\MzDebirth"; Filename: "{app}\MzDebirth.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MzDebirth.exe"; Description: "{cm:LaunchProgram,MzDebirth}"; Flags: nowait postinstall skipifsilent unchecked
