# SPDX-License-Identifier: MIT
[CmdletBinding()]
param()

$ErrorActionPreference = 'Continue'
$expectedGhidra = 'C:\Tools\ghidra_12.1.3_PUBLIC'
$expectedPrimaryModel = 'opencode/glm-5.3-flash-free'
$expectedSecondaryModel = 'opencode/deepseek-v4.1-flash-free'
$env:GHIDRA_INSTALL_DIR = [Environment]::GetEnvironmentVariable('GHIDRA_INSTALL_DIR', 'User')
$env:JAVA_HOME = [Environment]::GetEnvironmentVariable('JAVA_HOME', 'Machine')

function Show-Check([string]$Name, [bool]$Ok, [string]$Detail) {
    $mark = if ($Ok) { '[OK]' } else { '[FALHA]' }
    Write-Host "$mark $Name - $Detail"
}

$ghidraUser = [Environment]::GetEnvironmentVariable('GHIDRA_INSTALL_DIR', 'User')
Show-Check 'GHIDRA_INSTALL_DIR' ($ghidraUser -eq $expectedGhidra) $ghidraUser
Show-Check 'Ghidra' (Test-Path -LiteralPath "$expectedGhidra\ghidraRun.bat") $expectedGhidra
Show-Check 'JDK 21' (Test-Path -LiteralPath 'C:\Program Files\Microsoft\jdk-21.0.12.101-hotspot\bin\java.exe') 'Microsoft OpenJDK 21'
Show-Check 'dc-re-ghidra' (Test-Path -LiteralPath "$env:USERPROFILE\ghidra_scripts\DreamcastRE.java") "$env:USERPROFILE\ghidra_scripts"
Show-Check 'dream-recomp' (Test-Path -LiteralPath 'C:\Source\recomp-head-hunter\dream-recomp\CMakeLists.txt') 'repositorio presente'
Show-Check 'dream-recomp build' (Test-Path -LiteralPath 'C:\Source\recomp-head-hunter\dream-recomp\build-full\translator\Debug\dream-translate.exe') 'dream-translate.exe'

foreach ($command in 'node', 'npm', 'opencode', 'cmake', 'ninja', 'dcdisc', 'clang-format') {
    $found = Get-Command $command -ErrorAction SilentlyContinue
    Show-Check $command ($null -ne $found) $(if ($found) { $found.Source } else { 'nao encontrado no PATH' })
}

$ollamaPath = 'C:\Users\rodri\AppData\Local\Programs\Ollama\ollama.exe'
Show-Check 'Ollama' (Test-Path -LiteralPath $ollamaPath) $ollamaPath
$ollamaApiReady = $false
$ollamaModelNames = @()
try {
    $ollamaTags = Invoke-RestMethod -Uri 'http://127.0.0.1:11434/api/tags' -TimeoutSec 5
    $ollamaModelNames = @($ollamaTags.models.name)
    $ollamaApiReady = $ollamaModelNames -contains 'devstral-small-2:24b'
} catch {}
Show-Check 'devstral-small-2:24b' ($ollamaModelNames -contains 'devstral-small-2:24b') 'fallback Ollama local'
Show-Check 'GPT-OSS removido' (-not (($ollamaModelNames -join "`n") -match 'gpt-oss')) 'nenhum modelo GPT-OSS local exposto'
Show-Check 'Runtime Devstral ativo' $ollamaApiReady $(if ($ollamaApiReady) { 'devstral-small-2:24b' } else { 'Ollama indisponivel' })

$openCodeModels = & opencode models 2>$null
Show-Check 'OpenCode Devstral' ($openCodeModels -contains 'ollama/devstral-small-2:24b') 'seletor /models'
Show-Check 'OpenCode GLM gratuito' ($openCodeModels -contains $expectedPrimaryModel) $expectedPrimaryModel
Show-Check 'OpenCode DeepSeek gratuito' ($openCodeModels -contains $expectedSecondaryModel) $expectedSecondaryModel
Show-Check 'OpenCode sem GPT-OSS' (-not (($openCodeModels -join "`n") -match 'gpt-oss')) 'nenhum modelo GPT-OSS exposto'
Show-Check 'OpenCode sem Qwen' (-not (($openCodeModels -join "`n") -match 'qwen')) 'nenhum modelo Qwen exposto'
$openCodeConfigPath = Join-Path $env:USERPROFILE '.config\opencode\opencode.json'
$openCodeConfig = Get-Content -Raw -LiteralPath $openCodeConfigPath | ConvertFrom-Json
Show-Check 'Modelo padrao do OpenCode' ($openCodeConfig.model -eq $expectedPrimaryModel) $openCodeConfig.model
Show-Check 'DeepSeek nos agentes auxiliares' (
    $openCodeConfig.agents.plan.model -eq $expectedSecondaryModel -and
    $openCodeConfig.agents.general.model -eq $expectedSecondaryModel -and
    $openCodeConfig.agents.explore.model -eq $expectedSecondaryModel
) $expectedSecondaryModel
Show-Check 'Provedores OpenCode e Ollama' (
    @($openCodeConfig.enabled_providers).Count -eq 2 -and
    $openCodeConfig.enabled_providers -contains 'opencode' -and
    $openCodeConfig.enabled_providers -contains 'ollama'
) ($openCodeConfig.enabled_providers -join ', ')

Write-Host "`nOpenCode MCP:"
& opencode mcp list

Write-Host "`nTestes dream-recomp:"
ctest --test-dir 'C:\Source\recomp-head-hunter\dream-recomp\build-full' -C Debug --output-on-failure

