#ifndef PluginVersion
 #error PluginVersion must be supplied by scripts/build.ps1
#endif
#ifndef StageDir
 #error StageDir must be supplied by scripts/build.ps1
#endif
[Setup]
AppId={{87415D1A-4C9C-472C-95AF-BC507376CE27}
AppName=OBS Recording Actions
AppVersion={#PluginVersion}
AppPublisher=Diddlik and contributors
AppPublisherURL=https://github.com/Diddlik/obs-recording-actions
DefaultDirName={autopf}\obs-studio
DisableProgramGroupPage=yes
DisableDirPage=no
DirExistsWarning=no
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
AppMutex=Global\ObsRecordingActions
CloseApplications=no
RestartApplications=no
UninstallDisplayName=OBS Recording Actions
UninstallFilesDir={app}\data\obs-plugins\obs-recording-actions\uninstall
OutputBaseFilename=obs-recording-actions-{#PluginVersion}-windows-x64-setup
OutputDir=..\dist
LicenseFile=..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
[Files]
Source: "{#StageDir}\obs-recording-actions\bin\64bit\obs-recording-actions.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "{#StageDir}\obs-recording-actions\data\*"; DestDir: "{app}\data\obs-plugins\obs-recording-actions"; Flags: ignoreversion recursesubdirs createallsubdirs
[CustomMessages]
english.CloseOBS=Close OBS Studio before installing or uninstalling. Recording Actions never stops recordings or closes OBS automatically.
german.CloseOBS=Schließe OBS Studio vor dem Installieren oder Deinstallieren. Recording Actions beendet weder Aufnahmen noch OBS automatisch.
english.SelectOBS=Select the OBS Studio installation folder (containing bin\64bit\obs64.exe).
german.SelectOBS=Wähle den OBS-Studio-Installationsordner (mit bin\64bit\obs64.exe).
english.Duplicate=Remove the existing ProgramData copy of Recording Actions first to avoid loading two copies. Your OBS settings can stay.
german.Duplicate=Entferne zuerst die vorhandene ProgramData-Kopie von Recording Actions, um doppeltes Laden zu vermeiden. Deine OBS-Einstellungen können bleiben.
[Code]
function OBSClosed: Boolean;
var Services, Processes: Variant;
begin
  Result := False;
  try
    Services := GetActiveOleObject('winmgmts:');
  except
    try
      Services := CreateOleObject('WbemScripting.SWbemLocator');
      Services := Services.ConnectServer('.', 'root\CIMV2');
    except
      Exit;
    end;
  end;
  try
    Processes := Services.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name = ''obs64.exe'' OR Name = ''obs32.exe''');
    Result := Processes.Count = 0;
  except
    Result := False;
  end;
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not OBSClosed then
    Result := CustomMessage('CloseOBS')
  else if not FileExists(ExpandConstant('{app}\bin\64bit\obs64.exe')) then
    Result := CustomMessage('SelectOBS')
  else if FileExists(ExpandConstant('{commonappdata}\obs-studio\plugins\obs-recording-actions\bin\64bit\obs-recording-actions.dll')) then
    Result := CustomMessage('Duplicate');
end;
function InitializeUninstall: Boolean;
begin
  Result := OBSClosed;
  if not Result then MsgBox(CustomMessage('CloseOBS'), mbError, MB_OK);
end;
