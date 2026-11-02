param([switch]$NoVSync)
$ErrorActionPreference = 'Stop'
$traceArgs = @('--render_target_path_d3d12=rov', '--wo2_queue_trace', '--wo2_queue_latency_trace')
if ($NoVSync) { $traceArgs += '--no-vsync' }
Write-Host 'Prueba ROV: entra al mismo stage y juega durante 30 segundos.'
Write-Host 'Anota los FPS en una zona tranquila y durante el combate; luego cierra el juego.'
$traceStarted = Get-Date
& (Join-Path $PSScriptRoot 'run.ps1') @traceArgs
$binaryDir = Join-Path $PSScriptRoot 'out/build/win-amd64-release'
$trace = Get-ChildItem -LiteralPath $binaryDir -Filter 'queue-trace-*.csv' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($trace -and $trace.LastWriteTime -ge $traceStarted) {
    [ordered]@{
        started = $traceStarted.ToString('o')
        render_target_path_d3d12 = 'rov'
        vsync = !$NoVSync.IsPresent
        wo2_fps_limit = 60
        args = $traceArgs
    } | ConvertTo-Json | Set-Content -LiteralPath ($trace.FullName + '.meta.json')
    Write-Host "Traza: $($trace.FullName)"
}
