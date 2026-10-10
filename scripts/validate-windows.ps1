param(
    [Parameter(Mandatory = $true)][string]$PluginvalPath,
    [string]$BuildDirectory = 'build',
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = if ([IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $projectRoot $BuildDirectory }
$validator = (Resolve-Path -LiteralPath $PluginvalPath).Path
$plugin = Join-Path $buildRoot "QuickPitchShifter_artefacts/$Configuration/VST3/Quick Pitch Shifter.vst3"
if (-not (Test-Path -LiteralPath $plugin -PathType Container)) { throw "Missing VST3 bundle: $plugin" }
$logs = Join-Path $buildRoot 'validation'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$stdout = Join-Path $logs 'pluginval-stdout.log'
$stderr = Join-Path $logs 'pluginval-stderr.log'
$validationArguments = @('--strictness-level', '5', '--skip-gui-tests', '--output-dir', "`"$logs`"", '--validate', "`"$plugin`"")
# pluginval is a GUI-subsystem executable: wait explicitly for its exit status.
$validationProcess = Start-Process -FilePath $validator -ArgumentList $validationArguments -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $stdout -RedirectStandardError $stderr
Get-Content -LiteralPath $stdout
Get-Content -LiteralPath $stderr
if ($validationProcess.ExitCode -ne 0) { throw "VST3 validation failed (exit $($validationProcess.ExitCode)). See $logs" }
Write-Output 'VST3 validation passed at strictness level 5.'
