#ifndef AppRepoPath
	#define AppRepoPath "D:\Git\NexgenRedux"
#endif
#ifndef AppVersion
	#define AppVersion "1.0.0"
#endif
#define AppName "Nexgen Redux"
#define AppPublisher "Team Resurgent"
#define AppExeName "NexgenRedux.exe"

[Setup]
AppId={{B1135DFE-D42E-4EC5-BE93-147FD3247D23}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
OutputDir={#AppRepoPath}\build
OutputBaseFilename=NexgenReduxSetup
SetupIconFile={#AppRepoPath}\Artwork\Icon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
WizardImageFile="{#AppRepoPath}\Artwork\InnoSetupTeamResurgentBanner.bmp"
WizardSmallImageFile="{#AppRepoPath}\Artwork\InnoSetupTeamResurgentIcon.bmp"

[Code]
function VCRedistNeedsInstall: Boolean;
var 
  Version: String;
begin
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Version', Version) then
  begin
    Log('VC Redist Version check : found ' + Version);
    Result := (CompareStr(Version, 'v14.34.31931.00') <> 0);
  end
  else 
  begin
    Result := True;
  end;
  if (Result) then
  begin
    ExtractTemporaryFile('VC_redist.x64.exe');
  end;
end;

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#AppRepoPath}\build\bin\Release\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#AppRepoPath}\build\bin\Release\libEGL.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#AppRepoPath}\build\bin\Release\libGLESv2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#AppRepoPath}\Libraries\VisualC\VC_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]   
Filename: {tmp}\VC_redist.x64.exe; Parameters: "/quiet"; StatusMsg: "Installing VC++ Redistributable..."; Check: VCRedistNeedsInstall; Flags: waituntilterminated
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

