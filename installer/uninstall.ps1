[CmdletBinding()]
param(
    [string]$InstallRoot,
    [switch]$Silent,
    [switch]$RemoveData,
    [string]$RegistryPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PersonalDataManager',
    [string]$ShortcutPath
)

$ErrorActionPreference = 'Stop'

function Get-LocalizedText {
    param([Parameter(Mandatory)][string]$EscapedText)
    return [regex]::Unescape($EscapedText)
}

$windowTitle = (Get-LocalizedText '\u5378\u8f7d') + ' Nothing'
$promptText = Get-LocalizedText '\u662f\u5426\u540c\u65f6\u5220\u9664\u5168\u90e8\u4e2a\u4eba\u6570\u636e\uff1f\n\n\u9009\u62e9 \u662f\uff1a\u5220\u9664\u7a0b\u5e8f\u548c\u6240\u6709\u8bb0\u5f55\n\u9009\u62e9 \u5426\uff1a\u53ea\u5220\u9664\u7a0b\u5e8f\uff0c\u4fdd\u7559\u6570\u636e\u5e93\n\u9009\u62e9 \u53d6\u6d88\uff1a\u505c\u6b62\u5378\u8f7d'
$completedTitle = Get-LocalizedText '\u5378\u8f7d\u5b8c\u6210'
$keptDataText = Get-LocalizedText '\u7a0b\u5e8f\u5df2\u5378\u8f7d\uff0c\u4e2a\u4eba\u6570\u636e\u5df2\u4fdd\u7559\u3002'
$removedDataText = Get-LocalizedText '\u7a0b\u5e8f\u548c\u4e2a\u4eba\u6570\u636e\u5747\u5df2\u5220\u9664\u3002'
$failureTitle = Get-LocalizedText '\u5378\u8f7d\u5931\u8d25'

try {
    if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
        if (Test-Path -LiteralPath 'D:\MyQt\PersonalDataManager') {
            $InstallRoot = 'D:\MyQt\PersonalDataManager'
        } else {
            $InstallRoot = Join-Path $env:LOCALAPPDATA 'PersonalDataManager'
        }
    }

    $resolvedRoot = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
    if ([IO.Path]::GetFileName($resolvedRoot) -ne 'PersonalDataManager') {
        throw "Refusing to uninstall an unexpected directory: $resolvedRoot"
    }

    $removePersonalData = $RemoveData.IsPresent
    if (-not $Silent) {
        Add-Type -AssemblyName PresentationFramework
        $choice = [System.Windows.MessageBox]::Show(
            $promptText,
            $windowTitle,
            [System.Windows.MessageBoxButton]::YesNoCancel,
            [System.Windows.MessageBoxImage]::Question)
        if ($choice -eq [System.Windows.MessageBoxResult]::Cancel) {
            exit 2
        }
        $removePersonalData = $choice -eq [System.Windows.MessageBoxResult]::Yes
    }

    $appDirectory = Join-Path $resolvedRoot 'app'
    $dataDirectory = Join-Path $resolvedRoot 'data'
    $executablePath = Join-Path $appDirectory 'Nothing.exe'

    Get-Process -Name 'Nothing' -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -eq $executablePath } |
        Stop-Process -Force
    Start-Sleep -Milliseconds 300

    if (Test-Path -LiteralPath $appDirectory) {
        Remove-Item -LiteralPath $appDirectory -Recurse -Force
    }
    if ($removePersonalData -and (Test-Path -LiteralPath $dataDirectory)) {
        Remove-Item -LiteralPath $dataDirectory -Recurse -Force
    }

    if ([string]::IsNullOrWhiteSpace($ShortcutPath)) {
        $desktop = [Environment]::GetFolderPath('Desktop')
        $shortcutName = 'Nothing.lnk'
        $ShortcutPath = Join-Path $desktop $shortcutName
    }
    if (Test-Path -LiteralPath $ShortcutPath) {
        Remove-Item -LiteralPath $ShortcutPath -Force
    }
    if (Test-Path -LiteralPath $RegistryPath) {
        Remove-Item -LiteralPath $RegistryPath -Recurse -Force
    }

    if (-not $Silent) {
        Add-Type -AssemblyName PresentationFramework
        $completedText = if ($removePersonalData) { $removedDataText } else { $keptDataText }
        [System.Windows.MessageBox]::Show(
            $completedText,
            $completedTitle,
            [System.Windows.MessageBoxButton]::OK,
            [System.Windows.MessageBoxImage]::Information) | Out-Null
    }

    $selfPath = $PSCommandPath
    $cleanupCommand = 'timeout /t 2 /nobreak >nul & del /f /q "{0}" >nul 2>&1 & rmdir "{1}" >nul 2>&1' -f `
        $selfPath, $resolvedRoot
    Start-Process -FilePath $env:ComSpec -ArgumentList "/d /c `"$cleanupCommand`"" -WindowStyle Hidden
    exit 0
} catch {
    if (-not $Silent) {
        try {
            Add-Type -AssemblyName PresentationFramework
            [System.Windows.MessageBox]::Show(
                $_.Exception.Message,
                $failureTitle,
                [System.Windows.MessageBoxButton]::OK,
                [System.Windows.MessageBoxImage]::Error) | Out-Null
        } catch {
        }
    }
    exit 1
}
