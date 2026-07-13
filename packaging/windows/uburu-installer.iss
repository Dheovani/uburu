#ifndef PackageDir
  #error PackageDir must point to the prepared portable bundle directory.
#endif

#ifndef OutputDir
  #error OutputDir must point to the installer output directory.
#endif

#ifndef AppVersion
  #define AppVersion "0.1.0-dev"
#endif

#define AppName "Uburu"
#define AppPublisher "Dheovani Xavier da Cruz"
#define AppExecutable "uburu_desktop.exe"

[Setup]
AppId={{E9BA56F5-3B94-4DD8-B0F6-8D62AFB0030E}
AppName={#AppName}
AppPublisher={#AppPublisher}
AppVersion={#AppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={localappdata}\Programs\Uburu
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputBaseFilename=uburu-setup-{#AppVersion}-windows-x64
OutputDir={#OutputDir}
PrivilegesRequired=lowest
SetupIconFile=..\..\apps\desktop\windows\uburu-icon.ico
SolidCompression=yes
UninstallDisplayIcon={app}\{#AppExecutable}
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExecutable}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExecutable}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExecutable}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
