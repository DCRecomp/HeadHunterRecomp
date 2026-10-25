# SPDX-License-Identifier: MIT
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$ollama = 'C:\Users\rodri\AppData\Local\Programs\Ollama\ollama.exe'

if (-not (Test-Path -LiteralPath $ollama)) {
    throw "Ollama nao encontrado: $ollama"
}

$ollamaReady = $false
try {
    $null = Invoke-RestMethod -Uri 'http://127.0.0.1:11434/api/tags' -TimeoutSec 2
    $ollamaReady = $true
} catch {}

if (-not $ollamaReady) {
    $env:OLLAMA_CONTEXT_LENGTH = '32768'
    $env:OLLAMA_KEEP_ALIVE = '5m'
    Start-Process -FilePath $ollama -ArgumentList 'serve' -WindowStyle Hidden
    Start-Sleep -Seconds 3
}

$models = & $ollama list
if (($models -join "`n") -notmatch 'devstral-small-2:24b') {
    throw 'devstral-small-2:24b nao esta instalado no Ollama.'
}

Write-Host 'Devstral pronto. Execute .\scripts\start-opencode.ps1.'
