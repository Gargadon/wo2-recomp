# Applies the SDK patches, prepares Title Update #1, regenerates, builds and
# optionally runs the game. With no arguments it builds only if the executable is
# missing.
#
# Usage:
#   .\run.ps1 -UpdateFile "D:\...\TU_15KU1V3_0000008000000.0000000000082"
#   .\run.ps1 -UpdateFile "D:\Warriors Orochi 2 Title Update #1" -Build
#   .\run.ps1                      # only launches an already built binary
#
# Any remaining arguments are passed to the game executable.

param(
    [Alias('TitleUpdatePackage')]
    [string]$UpdateFile,

    [switch]$Build,

    [switch]$NoRun,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$GameArgs
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$sdkRoot = Split-Path -Parent $projectRoot
$exe = Join-Path $projectRoot 'out\build\win-amd64-release\warriors_orochi_2.exe'
$needsBuild = $Build -or !(Test-Path -LiteralPath $exe)

if ($needsBuild) {
    if ([string]::IsNullOrWhiteSpace($UpdateFile)) {
        $preparedXex = Join-Path $sdkRoot 'Warriors Orochi 2 (USA) (En,Fr,De)\Warriors Orochi 2 Title Update #1\default.xex'
        if (Test-Path -LiteralPath $preparedXex) {
            Write-Host 'No -UpdateFile given; using the Title Update already prepared in the game directory.'
            & (Join-Path $projectRoot 'build.ps1') -Prepared
        } else {
            throw 'Nothing built yet. Pass -UpdateFile <path to the Title Update container or its folder>.'
        }
    } else {
        & (Join-Path $projectRoot 'build.ps1') -UpdateFile $UpdateFile
    }
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
}

if ($NoRun) {
    Write-Host "Built: $exe"
    return
}

if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }

$gameDir = Join-Path $sdkRoot 'Warriors Orochi 2 (USA) (En,Fr,De)'
if (!(Test-Path -LiteralPath $gameDir)) { throw "Game data not found: $gameDir" }

$env:PATH = "$sdkRoot\out\win-amd64;" + $env:PATH
Write-Host 'F3: toggle the guest FPS overlay.'

# Defaults used for this title (the same ones documented in the bring-up notes).
# They are passed on the command line, which overrides the runtime TOML, so the
# build works on a fresh machine without editing any configuration file.
$defaultArgs = @(
    "--game_data_root=`"$gameDir`"",
    '--gpu_plugin=xenos',
    '--render_target_path_d3d12=rov',
    '--no-fullscreen',
    '--resolution=720p',
    '--video_mode_refresh_rate=60',
    '--no-vsync',
    '--mnk_mode',
    '--input_backend=xinput'
)

$allArgs = @($defaultArgs) + @($GameArgs)
$quotedArgs = ($allArgs | ForEach-Object { $_ }) -join ' '
$startOptions = @{
    FilePath = $exe
    WorkingDirectory = Split-Path -Parent $exe
    PassThru = $true
    Wait = $true
}
if ($quotedArgs) { $startOptions.ArgumentList = $quotedArgs }

$gameProcess = Start-Process @startOptions
$gameExitCode = $gameProcess.ExitCode
if ($gameExitCode -ne 0) {
    $latestLog = Get-ChildItem (Join-Path (Split-Path -Parent $exe) 'logs') -Filter '*.log' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($latestLog) { Get-Content -LiteralPath $latestLog.FullName -Tail 20 }
    Write-Warning "The game exited with code $gameExitCode."
}
Write-Host "Game exited with code $gameExitCode"
