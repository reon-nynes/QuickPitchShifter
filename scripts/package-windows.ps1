param(
    [string]$BuildDirectory = 'build',
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = if ([IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $projectRoot $BuildDirectory }
$artefacts = Join-Path $buildRoot "QuickPitchShifter_artefacts/$Configuration"
$stage = Join-Path $projectRoot 'dist/windows-x64'

$vstBinary = Join-Path $artefacts 'VST3/Quick Pitch Shifter.vst3/Contents/x86_64-win/Quick Pitch Shifter.vst3'
$clapBinary = Join-Path $artefacts 'CLAP/Quick Pitch Shifter.clap'
foreach ($binary in @($vstBinary, $clapBinary)) {
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Missing plugin binary: $binary" }
}

foreach ($format in @('VST3', 'CLAP')) {
    $destination = Join-Path $stage $format
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    $plugin = Join-Path $artefacts $(if ($format -eq 'VST3') { 'VST3/Quick Pitch Shifter.vst3' } else { 'CLAP/Quick Pitch Shifter.clap' })
    Copy-Item -LiteralPath $plugin -Destination $destination -Recurse -Force
}

foreach ($file in @('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'licenses')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $file) -Destination $stage -Recurse -Force
}

$source = Join-Path $stage 'Source'
New-Item -ItemType Directory -Force -Path $source | Out-Null
foreach ($file in @('CMakeLists.txt', '.gitignore', '.gitattributes', 'GLOSSARY.md', 'README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'src', 'tests', 'docs', 'scripts', 'licenses', '.github')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $file) -Destination $source -Recurse -Force
}

$archive = Join-Path $projectRoot 'dist/QuickPitchShifter-windows-x64.zip'
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
Write-Output "Packaged $archive"
