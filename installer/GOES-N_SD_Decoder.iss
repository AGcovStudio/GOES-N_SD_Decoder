; Inno Setup 6 script - GOES-N SD Decoder
; Build with scripts\build_release.bat (it creates the "dist" folder first), or open this file in Inno Setup.
#define MyAppName "GOES-N SD Decoder"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "AGcov（BI2QYN）"
#define MyAppExe "GOES-N_SD_Decoder.exe"
#define DistDir "..\dist"

[Setup]
; The AppId identifies this program for upgrades and uninstall. Do not change it between versions.
AppId={{6F1C2D84-93A7-4E0B-B5D2-7A18C3E94F51}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany=AGcov (BI2QYN)
VersionInfoDescription={#MyAppName} Setup
DefaultDirName={autopf}\GOES-N_SD_Decoder
DefaultGroupName=GOES-N SD Decoder
DisableProgramGroupPage=yes
LicenseFile=EULA.txt
OutputDir=output
OutputBaseFilename=GOES-N_SD_Decoder_Setup_{#MyAppVersion}
SetupIconFile=..\resources\icon\app.ico
UninstallDisplayIcon={app}\{#MyAppExe}
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern
ShowLanguageDialog=auto
MinVersion=10.0

[Languages]
#if FileExists(CompilerPath + "Languages\ChineseSimplified.isl")
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
#elif FileExists(CompilerPath + "Languages\Unofficial\ChineseSimplified.isl")
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\Unofficial\ChineseSimplified.isl"
#elif FileExists(SourcePath + "ChineseSimplified.isl")
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"
#endif
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\GOES-N SD Decoder"; Filename: "{app}\{#MyAppExe}"
Name: "{autodesktop}\GOES-N SD Decoder"; Filename: "{app}\{#MyAppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
