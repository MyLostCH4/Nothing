[Version]
Class=IEXPRESS
SEDVersion=3

[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=0
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=%InstallPrompt%
DisplayLicense=%DisplayLicense%
FinishMessage=%FinishMessage%
TargetName=%TargetName%
FriendlyName=%FriendlyName%
AppLaunched=%AppLaunched%
PostInstallCmd=%PostInstallCmd%
AdminQuietInstCmd=%AdminQuietInstCmd%
UserQuietInstCmd=%UserQuietInstCmd%
SourceFiles=SourceFiles

[Strings]
InstallPrompt="Install Nothing?"
DisplayLicense=""
FinishMessage="Installation completed. Nothing 1.5 is ready."
TargetName="D:\MyQt\Package\Nothing_Setup_1.5.exe"
FriendlyName="Nothing 1.5 Setup"
AppLaunched="powershell.exe -NoProfile -ExecutionPolicy Bypass -File install.ps1"
PostInstallCmd="<None>"
AdminQuietInstCmd="powershell.exe -NoProfile -ExecutionPolicy Bypass -File install.ps1"
UserQuietInstCmd="powershell.exe -NoProfile -ExecutionPolicy Bypass -File install.ps1"
FILE0="payload.zip"
FILE1="install.cmd"
FILE2="install.ps1"
FILE3="uninstall.ps1"

[SourceFiles]
SourceFiles0=D:\MyQt\Package\PersonalDataManager\staging\

[SourceFiles0]
%FILE0%=
%FILE1%=
%FILE2%=
%FILE3%=
