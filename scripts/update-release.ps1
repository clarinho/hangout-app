param(
    [string]$Droplet = "45.55.245.3",
    [string]$RemoteUser = "root",
    [string]$UpdatePath = "/var/www/hangout-updates",
    [string]$UpdateUrl = "http://45.55.245.3/updates/latest.yml",
    [switch]$SkipVersionBump
)

$ErrorActionPreference = "Stop"

function Step($Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Run($Command, $Arguments) {
    Write-Host "+ $Command $($Arguments -join ' ')" -ForegroundColor DarkGray
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $Command $($Arguments -join ' ')"
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

Step "Checking git state"
$pending = git status --short
if ($LASTEXITCODE -ne 0) {
    throw "git status failed."
}

if ($pending) {
    Write-Host "Publishing with these local changes included in the packaged app:" -ForegroundColor Yellow
    Write-Host $pending
}

if (-not $SkipVersionBump) {
    Step "Bumping patch version"
    Run "npm" @("version", "patch", "--no-git-tag-version")
}

$package = Get-Content ".\package.json" -Raw | ConvertFrom-Json
$version = $package.version
$installer = ".\release\Hangout Setup $version.exe"
$blockmap = "$installer.blockmap"

Step "Building installer $version"
Run "npm" @("run", "dist")

if (-not (Test-Path $installer)) {
    throw "Missing installer after build: $installer"
}

if (-not (Test-Path $blockmap)) {
    throw "Missing blockmap after build: $blockmap"
}

if (-not (Test-Path ".\release\latest.yml")) {
    throw "Missing update metadata after build: .\release\latest.yml"
}

$target = "$RemoteUser@$Droplet"

Step "Uploading update files to $target`:$UpdatePath"
Run "ssh" @($target, "mkdir -p '$UpdatePath'")
Run "scp" @(".\release\latest.yml", "$target`:$UpdatePath/")
Run "scp" @($installer, "$target`:$UpdatePath/")
Run "scp" @($blockmap, "$target`:$UpdatePath/")

Step "Verifying update feed"
$latest = Invoke-RestMethod -Uri $UpdateUrl -TimeoutSec 20
Write-Host $latest

if ($latest -notmatch "version:\s*$([regex]::Escape($version))") {
    throw "Update feed did not report version $version. Check $UpdateUrl"
}

Write-Host ""
Write-Host "Published Hangout $version successfully." -ForegroundColor Green
Write-Host "Users will receive it from: $UpdateUrl" -ForegroundColor Green
