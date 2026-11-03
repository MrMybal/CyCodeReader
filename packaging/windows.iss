#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef SourceDir
  #error SourceDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif

[Setup]
AppId={{0BFB1184-2553-4C04-B6B0-57D0BB0D7124}
AppName=CyCodeReader
AppVersion={#AppVersion}
AppPublisher=Cyberalien
AppPublisherURL=https://github.com/MrMybal/CyCodeReader
AppSupportURL=https://github.com/MrMybal/CyCodeReader/issues
AppUpdatesURL=https://github.com/MrMybal/CyCodeReader/releases
DefaultDirName={localappdata}\Programs\CyCodeReader
DefaultGroupName=CyCodeReader
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=CyCodeReader-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile={#SourceDir}\LICENSE
SetupIconFile={#SourceDir}\icons\CyCodeReader.ico
WizardSmallImageFile={#SourceDir}\icons\CyCodeReader.png
UninstallDisplayIcon={app}\CyCodeReader.exe
AppMutex=CyCodeReader.Installed
CloseApplications=no
RestartApplications=no
SetupLogging=yes
DisableProgramGroupPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "portable.flag"

[Icons]
Name: "{autoprograms}\CyCodeReader"; Filename: "{app}\CyCodeReader.exe"; IconFilename: "{app}\CyCodeReader.exe"; IconIndex: 0
Name: "{autodesktop}\CyCodeReader"; Filename: "{app}\CyCodeReader.exe"; IconFilename: "{app}\CyCodeReader.exe"; IconIndex: 0; Tasks: desktopicon

; No automatic application launch, forced process closure, association takeover,
; credential handling, runtime installation or deletion of user preferences.

[CustomMessages]
english.PortableDirectory=Choose a separate installation folder. This folder contains a portable edition and its settings.
french.PortableDirectory=Choisissez un autre dossier d'installation. Ce dossier contient une version portable et ses réglages.

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if FileExists(ExpandConstant('{app}\portable.flag')) then
    Result := CustomMessage('PortableDirectory');
end;
