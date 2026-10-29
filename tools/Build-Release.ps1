param(
    [Parameter(Mandatory = $true)][string]$ResourceMedia,
    [string]$OutputDirectory,
    [string]$Devenv = "${env:ProgramFiles(x86)}\Microsoft Visual Studio 9.0\Common7\IDE\devenv.com"
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$version = (Get-Content -LiteralPath (Join-Path $repo 'VERSION') -Raw).Trim()
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo ('dist\build-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw "Choose a new output directory: $output already exists." }
$font = Join-Path $ResourceMedia 'xarialuni.ttf'
foreach ($path in @($Devenv, "$env:XEDK\bin\win32\xuipkg.exe", $font)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing build input: $path" }
}
New-Item -ItemType Directory -Path $output | Out-Null
Push-Location $repo
try {
    & $Devenv (Join-Path $repo 'fceux.sln') /Rebuild 'Release_LTCG|Xbox 360' /Out (Join-Path $output 'build.log')
    if ($LASTEXITCODE -ne 0) { throw 'Xbox 360 build failed. See build.log.' }
} finally { Pop-Location }
$package = Join-Path $output 'fce360-enhanced'
$media = Join-Path $package 'media'
New-Item -ItemType Directory -Path $media -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'fceux\Release_LTCG\fceux.xex') -Destination $package
Copy-Item -LiteralPath $font -Destination $media
foreach ($name in @('background.png', 'effect.fx', 'screen.x')) {
    Copy-Item -LiteralPath (Join-Path $repo "fceux\media\$name") -Destination $media
}
# Preserve media\xui paths used by the scene loader. Prefer XUI source over old XUR copies.
$source = Join-Path $repo 'fceux'
$files = @(Get-ChildItem -LiteralPath (Join-Path $source 'media\xui') -File | Where-Object {
    $_.Extension -ne '.xur' -or -not (Test-Path -LiteralPath (Join-Path $_.DirectoryName ($_.BaseName + '.xui')))
} | ForEach-Object { 'media\xui\' + $_.Name })
$files += @(Get-ChildItem -LiteralPath (Join-Path $source 'media\xui\Graphics') -File -Recurse | ForEach-Object {
    $_.FullName.Substring($source.Length + 1)
})
Push-Location $source
try {
    & "$env:XEDK\bin\win32\xuipkg.exe" /nologo /d /o (Join-Path $media 'ui.xzp') @files
    if ($LASTEXITCODE -ne 0) { throw 'XUI packaging failed.' }
} finally { Pop-Location }
foreach ($directory in @('roms', 'snaps', 'states', 'sav', 'lua')) {
    New-Item -ItemType Directory -Path (Join-Path $package $directory) | Out-Null
}
Copy-Item -LiteralPath (Join-Path $repo 'README.md'), (Join-Path $repo 'LICENSE'), (Join-Path $repo "docs\release-$version.md") -Destination $package
Write-Output "Release package: $package"
