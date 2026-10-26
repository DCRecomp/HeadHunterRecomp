# Monta a distribuicao de teste local dist/HeadHunter-Preview com o proprio Builder (como tools/dist/make_preview.ps1
# do Blue Stinger, MIT): hh-builder com kit = este repositorio extrai, confere e junta os dados dos dois CDIs, gera o
# C++, compila, verifica o executavel SEM os CDIs e troca a Preview anterior (que fica em
# dist/HeadHunter-Preview.anterior para rollback; saves, controles e mods passam para a nova).
#
#   powershell -ExecutionPolicy Bypass -File tools/dist/make_preview.ps1 [-Disc <disco 1.cdi>] [-Disc2 <disco 2.cdi>]
#              [-Out <pasta>] [-Clean]
#
# Sem -Disc/-Disc2: ISO\HeadHunter (Disc 1).cdi e ISO\HeadHunter (Disc 2).cdi. A Preview contem o executavel e a
# pasta data\ derivados da copia do usuario: e para uso local e nao deve ser publicada (dist/ esta no .gitignore).
param(
    [string]$Disc = "",
    [string]$Disc2 = "",
    [string]$Out = "",
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not $Out) { $Out = Join-Path $repo "dist\HeadHunter-Preview" }
$Out = [IO.Path]::GetFullPath($Out).TrimEnd('\')
# a pasta e trocada inteira: nunca uma raiz de volume, o repositorio ou um ancestral dele
if ([IO.Path]::GetPathRoot($Out).TrimEnd('\') -eq $Out -or
    ($repo.TrimEnd('\') + '\').StartsWith($Out + '\', [StringComparison]::OrdinalIgnoreCase)) {
    Write-Host "ERRO: pasta de saida recusada: $Out" -ForegroundColor Red; exit 1
}
if (-not $Disc) { $Disc = Join-Path $repo "ISO\HeadHunter (Disc 1).cdi" }
if (-not $Disc2) { $Disc2 = Join-Path $repo "ISO\HeadHunter (Disc 2).cdi" }
$Disc = (Resolve-Path -LiteralPath $Disc).Path
$Disc2 = (Resolve-Path -LiteralPath $Disc2).Path
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if ($env:HH_VCVARS_PATH) { $vcvars = $env:HH_VCVARS_PATH }

function Fail($msg) { Write-Host "ERRO: $msg" -ForegroundColor Red; exit 1 }

if (-not (Test-Path $vcvars)) { Fail "Visual Studio Build Tools nao encontrado ($vcvars). Defina HH_VCVARS_PATH." }
$envLines = cmd /c "`"$vcvars`" >nul 2>nul && set"
foreach ($l in $envLines) { if ($l -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] } }

Write-Host "== compilando o hh-builder (Release)"
Push-Location $repo
try {
    cmake --build build --config Release --target hh-builder
    if ($LASTEXITCODE -ne 0) { Fail "compilacao do hh-builder falhou" }
} finally { Pop-Location }
$sdl = Join-Path $repo "build\_deps\sdl3-src"
if (-not (Test-Path (Join-Path $sdl "CMakeLists.txt"))) { Fail "fontes do SDL3 nao encontradas em $sdl" }

Write-Host "== Builder: $Disc + $Disc2 -> $Out"
$builderArgs = @("--disc", $Disc, "--disc2", $Disc2, "--out", (Split-Path $Out), "--name", (Split-Path $Out -Leaf),
                 "--kit", $repo, "--sdl3", $sdl)
$label = (git -C $repo describe --always --dirty --abbrev=7 2>$null)
if ($label) { $builderArgs += @("--version-label", "$label") }
if ($Clean) { $builderArgs += "--clean" }
& (Join-Path $repo "build\builder\Release\hh-builder.exe") @builderArgs
if ($LASTEXITCODE -ne 0) { Fail "o Builder falhou (codigo $LASTEXITCODE); a Preview anterior foi mantida" }

# nenhum caminho do ambiente de desenvolvimento nos binarios (os dados nao entram na busca)
foreach ($f in Get-ChildItem -LiteralPath $Out -File -Include *.exe, *.dll -Recurse | Where-Object { $_.FullName -notlike "*\data\*" }) {
    $bytes = [IO.File]::ReadAllBytes($f.FullName)
    foreach ($t in [Text.Encoding]::ASCII.GetString($bytes), [Text.Encoding]::Unicode.GetString($bytes)) {
        foreach ($p in @($repo, $env:USERPROFILE)) {
            if ($p -and ($t.IndexOf($p, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or $t.IndexOf($p.Replace('\', '/'), [StringComparison]::OrdinalIgnoreCase) -ge 0)) {
                Fail "$($f.Name) contem o caminho de desenvolvimento '$p'"
            }
        }
    }
}
Write-Host "OK: $Out"
& (Join-Path $Out "HeadHunter.exe") --version
