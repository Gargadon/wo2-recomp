# Builds the Warriors Orochi 2 recompilation from scratch:
#   1. applies the local SDK patches listed in patches\sdk-patches.txt
#   2. prepares Title Update #1 (extracts default.xexp from the STFS container)
#   3. runs the code generator
#   4. configures and builds with CMake/Ninja
#
# Usage:
#   .\build.ps1 -UpdateFile "D:\...\TU_15KU1V3_0000008000000.0000000000082"
#   .\build.ps1 -UpdateFile "D:\Warriors Orochi 2 Title Update #1"   (a folder also works)

param(
    [Parameter(Mandatory = $false)]
    [Alias('TitleUpdatePackage')]
    [string]$UpdateFile,

    # Use the Title Update that is already prepared in the game directory instead
    # of extracting it again from the STFS container.
    [switch]$Prepared,

    # Extra arguments forwarded to the final cmake --build invocation.
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$BuildArgs
)

$ErrorActionPreference = 'Stop'

function Resolve-UpdateFile([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'No Title Update package given. Pass -UpdateFile <path to TU container or its folder>.'
    }
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
    if (!$resolved) { throw "Title Update package not found: $Path" }
    $full = $resolved.ProviderPath
    if (Test-Path -LiteralPath $full -PathType Leaf) { return $full }
    # A folder was given: look for the STFS container inside it.
    $candidates = @(Get-ChildItem -LiteralPath $full -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'TU_*' } | Sort-Object Length -Descending)
    if ($candidates.Count -eq 0) {
        throw "No TU_* container found inside '$full'. Pass the container file itself."
    }
    Write-Host "Update package folder given, using: $($candidates[0].FullName)"
    return $candidates[0].FullName
}

function Apply-SdkPatches([string] $sdkRoot, [string] $projectRoot) {
    $listPath = Join-Path $projectRoot 'patches\sdk-patches.txt'
    if (!(Test-Path -LiteralPath $listPath)) {
        Write-Warning "No patch list at $listPath, skipping SDK patches."
        return
    }
    $patches = @(Get-Content -LiteralPath $listPath |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and !$_.StartsWith('#') })
    if ($patches.Count -eq 0) {
        Write-Warning 'Patch list is empty, skipping SDK patches.'
        return
    }
    $git = Get-Command git -ErrorAction SilentlyContinue
    if (!$git) { throw 'git was not found; it is needed to apply the SDK patches.' }

    # git writes "patch does not apply" to stderr, which PowerShell would turn
    # into a terminating error because of $ErrorActionPreference, so the calls are
    # wrapped and judged by their exit code instead.
    function Invoke-Git([string[]] $GitArgs) {
        $previous = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $output = & git @GitArgs 2>&1
            return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = ($output -join [Environment]::NewLine) }
        } finally {
            $ErrorActionPreference = $previous
        }
    }

    foreach ($relative in $patches) {
        $patch = Join-Path $projectRoot $relative
        if (!(Test-Path -LiteralPath $patch)) { throw "Patch not found: $patch" }

        $reverseCheck = Invoke-Git @('-C', $sdkRoot, 'apply', '--check', '--reverse', $patch)
        if ($reverseCheck.ExitCode -eq 0) {
            Write-Host "Already applied: $relative"
            continue
        }
        $forwardCheck = Invoke-Git @('-C', $sdkRoot, 'apply', '--check', $patch)
        if ($forwardCheck.ExitCode -ne 0) {
            throw "Patch does not apply cleanly (wrong SDK revision?): $patch`n$($forwardCheck.Output)"
        }
        $apply = Invoke-Git @('-C', $sdkRoot, 'apply', $patch)
        if ($apply.ExitCode -ne 0) { throw "Failed to apply patch: $patch`n$($apply.Output)" }
        Write-Host "Applied: $relative"
    }
}

$projectRoot = $PSScriptRoot
$sdkRoot = Split-Path -Parent $projectRoot
if ($Prepared) {
    $package = ''
    $prepareGame = 'OFF'
} else {
    $package = Resolve-UpdateFile $UpdateFile
    $prepareGame = 'ON'
}

Write-Host "SDK:     $sdkRoot"
Write-Host "Project: $projectRoot"
if ($Prepared) {
    Write-Host 'TU:      using the already prepared Title Update'
} else {
    Write-Host "TU:      $package"
}

Apply-SdkPatches $sdkRoot $projectRoot

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe not found at $vswhere. Install Visual Studio with the C++ workload and Clang/LLVM."
}
$vsDir = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsDir -or !(Test-Path -LiteralPath $vsDir)) { throw 'Visual Studio with C++ tools not found.' }

$vsDevShell = Join-Path $vsDir 'Common7\Tools\Launch-VsDevShell.ps1'
if (Test-Path -LiteralPath $vsDevShell) {
    & $vsDevShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
} else {
    $vsDevCmd = Join-Path $vsDir 'Common7\Tools\VsDevCmd.bat'
    & cmd.exe /c "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && set" | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') { Set-Item -Force -Path "env:\$($matches[1])" -Value $matches[2] }
    }
}

$env:PATH = "$vsDir\VC\Tools\Llvm\x64\bin;$vsDir\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$vsDir\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$vsDir\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"

Push-Location $projectRoot
try {
    # Native tools write progress (and harmless warnings) to stderr, which
    # PowerShell would turn into terminating errors; the exit code is checked
    # explicitly instead.
    function Invoke-Native([string] $FilePath, [string[]] $Arguments) {
        $previous = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & $FilePath @Arguments 2>&1 | Out-Host
            return $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previous
        }
    }

    if ((Invoke-Native 'cmake' @('--preset', 'win-amd64-release', "-DREXSDK_DIR=$sdkRoot", "-DWO2_PREPARE_GAME=$prepareGame", "-DWO2_TU_PACKAGE=$package", '-DCMAKE_CXX_FLAGS=-mssse3')) -ne 0) {
        throw 'CMake configuration failed.'
    }

    # Codegen also prepares Title Update #1 before generating code.
    if ((Invoke-Native 'cmake' @('--build', '--preset', 'win-amd64-release', '--target', 'warriors_orochi_2_codegen', '-j', '6')) -ne 0) {
        throw 'Code generation failed.'
    }

    if ((Invoke-Native 'cmake' @('--preset', 'win-amd64-release')) -ne 0) {
        throw 'CMake configuration after codegen failed.'
    }

    $buildInvocation = @('--build', '--preset', 'win-amd64-release', '-j', '6') + @($BuildArgs)
    if ((Invoke-Native 'cmake' $buildInvocation) -ne 0) {
        throw 'Build failed.'
    }
} finally {
    Pop-Location
}

$exe = Join-Path $projectRoot 'out\build\win-amd64-release\warriors_orochi_2.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "Build finished but $exe is missing." }
Write-Host ''
Write-Host "Build ready: $exe"
