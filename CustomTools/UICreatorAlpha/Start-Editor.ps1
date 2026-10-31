param([ValidateRange(1024,65535)][int]$Port = 5190, [switch]$OpenBrowser)
$ErrorActionPreference = 'Stop'
$alphaRoot = $PSScriptRoot
$alphaUrl = "http://127.0.0.1:$Port"
$running = $false
try {
    $health = Invoke-RestMethod -Uri "$alphaUrl/api/health" -TimeoutSec 1
    if ($health.app -eq 'UICreator Alpha' -and $health.toolRoot -eq $alphaRoot) { $running = $true }
    else { throw "Port $Port is already used by another application." }
} catch {
    if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) {
        throw "Port $Port is already in use. Choose a different -Port."
    }
}
if (-not $running) {
    $alphaNode = (Get-Command node -ErrorAction Stop).Source
    $alphaWork = Join-Path $alphaRoot 'work'
    New-Item -ItemType Directory -Path $alphaWork -Force | Out-Null
    $previousPort = $env:UI_ALPHA_PORT
    try {
        $env:UI_ALPHA_PORT = "$Port"
        $alphaProcess = Start-Process -FilePath $alphaNode -ArgumentList @('server.mjs') -WorkingDirectory $alphaRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $alphaWork 'server.log') -RedirectStandardError (Join-Path $alphaWork 'server.error.log') -PassThru
        Set-Content -LiteralPath (Join-Path $alphaWork 'server.pid') -Value $alphaProcess.Id
    } finally { $env:UI_ALPHA_PORT = $previousPort }
    for ($attempt = 0; $attempt -lt 25; $attempt++) {
        Start-Sleep -Milliseconds 200
        try { $health = Invoke-RestMethod -Uri "$alphaUrl/api/health" -TimeoutSec 1; if ($health.ok) { $running = $true; break } } catch {}
        if ($alphaProcess.HasExited) { break }
    }
    if (-not $running) { throw "Editor failed to start. Check $alphaWork/server.error.log" }
}
Write-Host "UICreator Alpha: $alphaUrl"
if ($OpenBrowser) { Start-Process $alphaUrl }
