param(
    [string]$SourceDirectory = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'
$appVersion = '1.5.3'

if (Test-Path -LiteralPath 'D:\') {
    $installRoot = 'D:\MyQt\PersonalDataManager'
} else {
    $installRoot = Join-Path $env:LOCALAPPDATA 'PersonalDataManager'
}

$appDirectory = Join-Path $installRoot 'app'
$payloadPath = Join-Path $SourceDirectory 'payload.zip'
$executablePath = Join-Path $appDirectory 'Nothing.exe'
$iconPath = Join-Path $appDirectory 'Nothing.ico'
$legacyExecutablePath = Join-Path $appDirectory 'PersonalDataManager.exe'
$uninstallerSource = Join-Path $SourceDirectory 'uninstall.ps1'
$uninstallerPath = Join-Path $installRoot 'uninstall.ps1'
$uninstallRegistryPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PersonalDataManager'

if (-not (Test-Path -LiteralPath $payloadPath)) {
    throw "Installation payload is missing: $payloadPath"
}
if (-not (Test-Path -LiteralPath $uninstallerSource)) {
    throw "Uninstaller is missing: $uninstallerSource"
}

Get-Process -Name 'Nothing', 'PersonalDataManager' -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -in @($executablePath, $legacyExecutablePath) } |
    Stop-Process -Force

if (Test-Path -LiteralPath $legacyExecutablePath) {
    Remove-Item -LiteralPath $legacyExecutablePath -Force
}

New-Item -ItemType Directory -Path $appDirectory -Force | Out-Null
Expand-Archive -LiteralPath $payloadPath -DestinationPath $appDirectory -Force
Copy-Item -LiteralPath $uninstallerSource -Destination $uninstallerPath -Force

$runtimeInstaller = Join-Path $appDirectory 'vc_redist.x64.exe'
$runtimeState = Get-ItemProperty `
    -LiteralPath 'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64' `
    -ErrorAction SilentlyContinue
if (($null -eq $runtimeState -or $runtimeState.Installed -ne 1) -and
    (Test-Path -LiteralPath $runtimeInstaller)) {
    try {
        $runtime = Start-Process -FilePath $runtimeInstaller `
            -ArgumentList '/install', '/quiet', '/norestart' `
            -Verb RunAs -Wait -PassThru
        if ($runtime.ExitCode -notin @(0, 1638, 3010)) {
            throw "VC++ runtime installer returned $($runtime.ExitCode)."
        }
    } catch {
        Add-Type -AssemblyName PresentationFramework
        [System.Windows.MessageBox]::Show(
            "The VC++ runtime installation did not complete. The application may not start.`n$($_.Exception.Message)",
            'Nothing', 'OK', 'Warning') | Out-Null
    }
}

$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutName = 'Nothing.lnk'
$shortcutPath = Join-Path $desktop $shortcutName
$legacyShortcutName = ((0x4E2A, 0x4EBA, 0x6570, 0x636E, 0x7BA1, 0x7406 |
    ForEach-Object { [char]$_ }) -join '') + '.lnk'
$legacyShortcutPath = Join-Path $desktop $legacyShortcutName
if (Test-Path -LiteralPath $legacyShortcutPath) {
    Remove-Item -LiteralPath $legacyShortcutPath -Force
}
if (Test-Path -LiteralPath $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
}
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $executablePath
$shortcut.WorkingDirectory = $appDirectory
$shortcut.IconLocation = if (Test-Path -LiteralPath $iconPath) { "$iconPath,0" } else { "$executablePath,0" }
$shortcut.Description = 'Nothing'
$shortcut.Save()

$iconRefresh = Join-Path $env:SystemRoot 'System32\ie4uinit.exe'
if (Test-Path -LiteralPath $iconRefresh) {
    & $iconRefresh -show
}

$powerShellPath = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$uninstallCommand = ('"{0}" -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "{1}" -InstallRoot "{2}"' -f `
    $powerShellPath, $uninstallerPath, $installRoot)
$quietUninstallCommand = $uninstallCommand + ' -Silent'
$estimatedSize = [Math]::Max(1, [int]((Get-ChildItem -LiteralPath $appDirectory -File -Recurse |
    Measure-Object -Property Length -Sum).Sum / 1KB))

New-Item -Path $uninstallRegistryPath -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'DisplayName' -Value 'Nothing' -PropertyType String -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'DisplayVersion' -Value $appVersion -PropertyType String -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'Publisher' -Value 'Nothing' -PropertyType String -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'DisplayIcon' -Value $iconPath -PropertyType String -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'InstallLocation' -Value $installRoot -PropertyType String -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'InstallDate' -Value (Get-Date -Format 'yyyyMMdd') -PropertyType String -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'UninstallString' -Value $uninstallCommand -PropertyType String -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'QuietUninstallString' -Value $quietUninstallCommand -PropertyType String -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'EstimatedSize' -Value $estimatedSize -PropertyType DWord -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'NoModify' -Value 1 -PropertyType DWord -Force | Out-Null
New-ItemProperty -Path $uninstallRegistryPath -Name 'NoRepair' -Value 1 -PropertyType DWord -Force | Out-Null

try {
    Start-Process -FilePath $executablePath
} catch {
    explorer.exe $executablePath
}
