# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [string]$SessionId,
    [int]$MaxInfrastructureRetries = 5
)

$ErrorActionPreference = 'Stop'
$workspace = 'C:\Source\recomp-head-hunter'
$primaryModel = 'opencode/glm-5.3-flash-free'
$stopFile = Join-Path $workspace '.opencode-autonomo.stop'
$sessionFile = Join-Path $workspace '.opencode-autonomo.session'
$initialPrompt = @'
Trabalhe de forma autonoma no projeto Head Hunter Recompiled. Releia integralmente
AGENTS.md, ARCHITECTURE.md, ROADMAP.md, LEGAL.md e STATUS.md. Retome a primeira
etapa nao concluida e execute acoes reais: investigue profundamente, implemente,
compile, teste e documente. Continue automaticamente entre etapas. Nao encerre
um turno narrando a proxima acao e nao trate entregaveis ainda inexistentes,
como hh-extract, como ferramentas que o usuario precisa instalar. Construa-os na
etapa prevista. So pare com a primeira linha DECISAO_NECESSARIA quando existir
uma escolha material nao resolvida pelo plano e depois de esgotar toda evidencia
independente da decisao. Nao faca commits, pushes ou publicacoes.
Neste estado atual, a primeira acao obrigatoria e executar diretamente, sem
procurar nem recompilar, os dois comandos abaixo; o executavel existe:
& 'C:\Source\recomp-BStinger\build\msvc-release\tools\extractor\bs-extract.exe' --info 'ISO\HeadHunter (Disc 1).cdi'
& 'C:\Source\recomp-BStinger\build\msvc-release\tools\extractor\bs-extract.exe' --info 'ISO\HeadHunter (Disc 2).cdi'
Depois continue os itens pendentes escritos em docs/relatorios/HH01-RELATORIO.md.
'@
$continuePrompt = @'
Continue trabalhando agora a partir do estado em disco. Nao resuma nem apenas
descreva a proxima acao: execute-a. Releia AGENTS.md e STATUS.md se necessario.
Resolva falhas tecnicas, implemente entregaveis ausentes, rode investigacoes e
testes profundos e avance automaticamente pelas etapas. So encerre por uma
decisao material usando DECISAO_NECESSARIA como primeira linha, por PAUSA
registrada pelo mantenedor, ou pela conclusao comprovada de HH18. O alvo e Head
Hunter para Dreamcast; nao existe pasta C64. Nao despeje binarios inteiros no
terminal: use leitura limitada, parsers e hashes em streaming. Um erro de
ferramenta ou de stream nao e decisao nem bloqueio; corrija e continue.
Nao procure nem compile bs-extract: use diretamente o executavel existente em
C:\Source\recomp-BStinger\build\msvc-release\tools\extractor\bs-extract.exe.
Nao use ferramentas chamadas search ou exe e nao aninhe powershell -Command.
'@
$recoveryPrompt = @'
Retome autonomamente o projeto Head Hunter Recompiled usando o estado persistido
em AGENTS.md, STATUS.md e nos relatorios. A sessao anterior saturou o contexto e
foi descartada; nao repita seus despejos ou tentativas. Trabalhe em um bloco
substancial e verificavel da primeira etapa nao concluida, salve codigo, testes,
evidencias e status em disco e continue executando acoes reais. O alvo e Sega
Dreamcast. Para CDI, examine primeiro o extrator MIT de
C:\Source\recomp-BStinger\tools\extractor e seu bs-extract --info; dcdisc nao
suporta CDI. Exclua arvores dream-recomp/build* das buscas. Nunca leia um CDI
inteiro no terminal/contexto. So pare por DECISAO_NECESSARIA material conforme
AGENTS.md. Nao faca commits ou publicacoes.
'@

Set-Location -LiteralPath $workspace
if (Test-Path -LiteralPath $stopFile) {
    Remove-Item -LiteralPath $stopFile -Force
}
if (-not (Get-Command opencode -ErrorAction SilentlyContinue)) {
    throw 'OpenCode nao foi encontrado no PATH.'
}

function Get-NewestSessionId {
    $line = & opencode session list 2>$null | Select-Object -First 1
    if (-not $line) { return $null }
    return ($line -split "`t")[0].Trim()
}

function Get-LatestAssistantText([string]$Id) {
    try {
        $raw = (& opencode session export $Id 2>$null | Out-String)
        $data = $raw | ConvertFrom-Json
        $message = @($data.messages | Where-Object { $_.type -eq 'assistant' })[-1]
        return (@($message.content | Where-Object { $_.type -eq 'text' } | ForEach-Object { $_.text }) -join "`n")
    } catch {
        return ''
    }
}

function Start-RecoverySession {
    param([string]$PreviousId)
    Write-Warning "Descartando a sessao saturada $PreviousId e criando uma continuacao limpa."
    & opencode run --model $primaryModel $recoveryPrompt
    $newId = Get-NewestSessionId
    if (-not $newId -or $newId -eq $PreviousId) {
        return $null
    }
    Set-Content -LiteralPath $sessionFile -Value $newId -Encoding ascii
    Write-Host "Nova sessao autonoma: $newId" -ForegroundColor Green
    return $newId
}

if (-not $SessionId) {
    Write-Host 'Criando uma sessao autonoma nova para carregar as regras atuais...' -ForegroundColor Cyan
    & opencode run --model $primaryModel $initialPrompt
    $initialExitCode = $LASTEXITCODE
    $SessionId = Get-NewestSessionId
    if (-not $SessionId) {
        throw "Nao foi possivel identificar a nova sessao do OpenCode (codigo $initialExitCode)."
    }
    if ($initialExitCode -ne 0) {
        Write-Warning "O primeiro stream terminou com codigo $initialExitCode, mas a sessao foi preservada e sera retomada."
    }
} else {
    Write-Host "Retomando a sessao informada: $SessionId" -ForegroundColor Cyan
}

Set-Content -LiteralPath $sessionFile -Value $SessionId -Encoding ascii
Write-Host "Sessao autonoma: $SessionId" -ForegroundColor Green
Write-Host 'Use PARAR-TRABALHO-AUTONOMO.bat para parar depois do turno atual.'

$failures = 0
while ($true) {
    if (Test-Path -LiteralPath $stopFile) {
        Write-Host 'Pausa solicitada pelo mantenedor.' -ForegroundColor Yellow
        Remove-Item -LiteralPath $stopFile -Force
        break
    }

    $statusText = Get-Content -Raw -LiteralPath (Join-Path $workspace 'STATUS.md')
    if ($statusText -match '\| HH18 \| CONCLUIDA \|') {
        Write-Host 'HH18 concluida. Executor encerrado.' -ForegroundColor Green
        break
    }
    if ($statusText -match '\| HH\d{2} \| BLOQUEADA_DECISAO \|') {
        Write-Host 'O projeto aguarda uma decisao material do mantenedor.' -ForegroundColor Yellow
        break
    }

    Write-Host ''
    Write-Host "Continuando $SessionId..." -ForegroundColor Cyan
    & opencode run --model $primaryModel --session $SessionId $continuePrompt
    if ($LASTEXITCODE -ne 0) {
        $failures++
        Write-Warning "Turno/stream terminou com erro (sequencia $failures). A sessao sera retomada automaticamente."
        if ($failures -ge 2) {
            $newSessionId = Start-RecoverySession -PreviousId $SessionId
            if ($newSessionId) {
                $SessionId = $newSessionId
                $failures = 0
                continue
            }
            Write-Warning 'Nao foi possivel criar uma sessao limpa; reiniciando o servico e tentando novamente.'
            & opencode service start 2>$null | Out-Null
            Start-Sleep -Seconds 30
            if ($failures -ge $MaxInfrastructureRetries) {
                $failures = 0
            }
        } else {
            Start-Sleep -Seconds ([Math]::Min(30, 5 * $failures))
        }
        continue
    }
    $failures = 0

    $latest = Get-LatestAssistantText $SessionId
    if ($latest -match '(?m)^DECISAO_NECESSARIA\b') {
        Write-Host 'O agente chegou a uma decisao material. Revise a sessao no OpenCode.' -ForegroundColor Yellow
        break
    }

    Start-Sleep -Seconds 2
}

Write-Host "Sessao preservada em: $sessionFile"

