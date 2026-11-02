# Ejecutar en PowerShell como administrador, con el juego dentro del stage.
param([ValidateRange(5,120)][int]$Seconds = 20)
$ErrorActionPreference = 'Stop'
if (!(Get-Process warriors_orochi_2 -ErrorAction SilentlyContinue)) {
    throw 'Abre el juego y entra al stage antes de iniciar la captura.'
}
$traceDir = Join-Path $PSScriptRoot 'out\profiles'
New-Item -ItemType Directory -Path $traceDir -Force | Out-Null
$tracePath = Join-Path $traceDir ('stage-' + (Get-Date -Format yyyyMMdd-HHmmss) + '.etl')
& wpr.exe -start CPU -filemode
if ($LASTEXITCODE -ne 0) { throw 'No se pudo iniciar WPR. Comprueba privilegios de administrador y que no haya otra captura activa.' }
try {
    Write-Host "Capturando CPU durante $Seconds segundos. Mantente en el stage."
    Start-Sleep -Seconds $Seconds
} finally {
    & wpr.exe -stop $tracePath
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo guardar la captura WPR.' }
}
$binaryDir = Join-Path $PSScriptRoot 'out\build\win-amd64-release'
$binarySnapshot = $tracePath + '.binaries'
New-Item -ItemType Directory -Path $binarySnapshot -Force | Out-Null
Get-ChildItem -LiteralPath $binaryDir -File | Where-Object { $_.Extension -in '.exe', '.dll', '.pdb', '.map' } | Copy-Item -Destination $binarySnapshot
Write-Host "Captura guardada: $tracePath"

