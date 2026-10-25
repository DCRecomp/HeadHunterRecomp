# SPDX-License-Identifier: MIT
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$ollama = 'C:\Users\rodri\AppData\Local\Programs\Ollama\ollama.exe'
$workspace = 'C:\Source\recomp-head-hunter'
$port = 45278

if (-not (Test-Path -LiteralPath $ollama)) {
    throw "Ollama nao encontrado: $ollama"
}
if (-not (Get-Command opencode -ErrorAction SilentlyContinue)) {
    throw 'OpenCode nao foi encontrado no PATH.'
}

# O servidor fica leve em segundo plano. O modelo sai da RAM/VRAM apos
# cinco minutos sem requisicoes e sera recarregado na proxima pergunta.
$env:OLLAMA_CONTEXT_LENGTH = '32768'
$env:OLLAMA_KEEP_ALIVE = '5m'

$ollamaReady = $false
try {
    $null = Invoke-RestMethod -Uri 'http://127.0.0.1:11434/api/tags' -TimeoutSec 2
    $ollamaReady = $true
} catch {}

if (-not $ollamaReady) {
    Start-Process -FilePath $ollama -ArgumentList 'serve' -WindowStyle Hidden
    foreach ($attempt in 1..30) {
        Start-Sleep -Seconds 1
        try {
            $null = Invoke-RestMethod -Uri 'http://127.0.0.1:11434/api/tags' -TimeoutSec 2
            $ollamaReady = $true
            break
        } catch {}
    }
}

if (-not $ollamaReady) {
    throw 'Ollama nao respondeu em http://127.0.0.1:11434.'
}

$tags = Invoke-RestMethod -Uri 'http://127.0.0.1:11434/api/tags' -TimeoutSec 5
if (@($tags.models.name) -notcontains 'devstral-small-2:24b') {
    throw 'O modelo devstral-small-2:24b nao esta instalado no Ollama.'
}

& opencode service set hostname '0.0.0.0' | Out-Null
& opencode service set port $port | Out-Null
& opencode service start | Out-Null

$route = Get-NetRoute -AddressFamily IPv4 -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue |
    Where-Object { $_.NextHop -ne '0.0.0.0' } |
    Sort-Object RouteMetric, InterfaceMetric |
    Select-Object -First 1
$lanIp = if ($route) {
    Get-NetIPAddress -AddressFamily IPv4 -InterfaceIndex $route.InterfaceIndex -ErrorAction SilentlyContinue |
        Where-Object { $_.IPAddress -notmatch '^(127|169\.254)\.' } |
        Select-Object -ExpandProperty IPAddress -First 1
} else {
    $null
}
if (-not $lanIp) {
    throw 'Nao foi possivel descobrir o endereco IPv4 da rede local.'
}

$serverUrl = "http://${lanIp}:$port"
Write-Host ''
Write-Host 'Ambiente pronto:' -ForegroundColor Green
Write-Host '  Principal: opencode/glm-5.3-flash-free'
Write-Host '  Auxiliares: opencode/deepseek-v4.1-flash-free'
Write-Host '  Fallback local: ollama/devstral-small-2:24b'
Write-Host "  OpenCode: $serverUrl"
Write-Host '  Descarga automatica da GPU: 5 minutos sem uso'
Write-Host ''
Write-Host 'Escaneie o QR abaixo em ate 5 minutos. Ele funciona uma vez:' -ForegroundColor Cyan
& opencode pair --url $serverUrl
if ($LASTEXITCODE -ne 0) {
    throw 'Nao foi possivel gerar o pareamento do OpenCode.'
}

