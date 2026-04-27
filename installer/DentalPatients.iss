; Inno Setup script for Dental Patients.
; Build with:  iscc /DAppVersion=1.0.0 installer\DentalPatients.iss
; Output:      installer\Output\DentalPatients-Setup-<version>.exe
;
; Patient data lives in {userappdata}\DentalPatients and is NEVER touched
; by install/uninstall, so re-running this installer with a higher version
; safely upgrades the binaries while preserving every patient record.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#ifndef SourceDir
  #define SourceDir "..\build\release\dist"
#endif

#define AppName        "Dental Patients"
#define AppNameFa      "مدیریت بیماران دندانپزشکی"
#define AppPublisher   "Dental Patients"
#define AppExeName     "DentalPatients.exe"
; Stable AppId - never change this, it is what makes the installer recognise
; an existing install and upgrade it in place.
#define AppId          "{{B5E0D3F1-7D2A-4FA5-9F44-DE4F8DAFB371}"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\DentalPatients
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=DentalPatients-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\assets\icons\app.ico
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
ShowLanguageDialog=no
UninstallDisplayName={#AppName} {#AppVersion}
UninstallDisplayIcon={app}\{#AppExeName}
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} - {#AppNameFa}
VersionInfoProductName={#AppName}
CloseApplications=force
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";   Description: "{cm:CreateDesktopIcon}";   GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Take the entire dist/ folder produced by `windeployqt` (Qt DLLs, plugins,
; vc_redist runtime, app exe + seed CSV).
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

; ---- Important: do NOT include any [UninstallDelete] entry that touches
; ---- {userappdata}\DentalPatients. Patient records must survive uninstalls.
