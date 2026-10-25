# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [string]$Workspace = 'C:\Source\recomp-head-hunter'
)

$ErrorActionPreference = 'Stop'
if (-not (Get-Command opencode -ErrorAction SilentlyContinue)) {
    throw 'OpenCode nao foi encontrado no PATH.'
}

& opencode $Workspace
exit $LASTEXITCODE


