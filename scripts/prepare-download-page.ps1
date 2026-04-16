param(
    [string]$SourceReleaseDir = ".\release",
    [string]$WebsiteDir = ".\website",
    [string]$OutputDir = ".\website-dist",
    [string]$InstallerName = "HangoutSetup.exe",
    [int64]$MinimumInstallerBytes = 50000000
)

$ErrorActionPreference = "Stop"

function Format-FileSize($Bytes) {
    if ($Bytes -ge 1GB) {
        return "{0:N1} GB" -f ($Bytes / 1GB)
    }
    return "{0:N1} MB" -f ($Bytes / 1MB)
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

$releasePath = Resolve-Path $SourceReleaseDir
$websitePath = Resolve-Path $WebsiteDir

$installer = Get-ChildItem $releasePath -Filter "Hangout Setup *.exe" |
    Where-Object {
        $_.Name -notlike "*__uninstaller*" -and
        $_.Length -ge $MinimumInstallerBytes
    } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $installer) {
    throw "No valid installer found in $releasePath. Run npm run dist first and make sure the installer is larger than $MinimumInstallerBytes bytes."
}

$version = if ($installer.Name -match "Hangout Setup (.+)\.exe") { $Matches[1] } else { "latest" }
$outputPath = Join-Path $repoRoot $OutputDir
$downloadsPath = Join-Path $outputPath "downloads"

if (Test-Path $outputPath) {
    Remove-Item -LiteralPath $outputPath -Recurse -Force
}

New-Item -ItemType Directory -Force $downloadsPath | Out-Null
Copy-Item -Path (Join-Path $websitePath "*") -Destination $outputPath -Recurse -Force
Copy-Item -Path $installer.FullName -Destination (Join-Path $downloadsPath $InstallerName) -Force

$fileSize = Format-FileSize $installer.Length
$config = @"
window.HANGOUT_DOWNLOAD = {
  installerUrl: "./downloads/$InstallerName",
  version: "$version",
  fileSize: "$fileSize",
  updateFeedUrl: ""
};
"@

Set-Content -Path (Join-Path $outputPath "site-config.js") -Value $config -Encoding UTF8

Write-Host ""
Write-Host "Prepared Hostinger upload folder:" -ForegroundColor Green
Write-Host "  $outputPath"
Write-Host ""
Write-Host "Installer:" -ForegroundColor Green
Write-Host "  $($installer.Name) -> downloads/$InstallerName"
Write-Host "  Version $version, $fileSize"
Write-Host ""
Write-Host "Upload the contents of website-dist to Hostinger public_html or a subfolder like public_html/download."
