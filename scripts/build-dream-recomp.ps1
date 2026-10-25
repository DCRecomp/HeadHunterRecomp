# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$repo = 'C:\Source\recomp-head-hunter\dream-recomp'
$build = Join-Path $repo 'build-full'
$toolchain = 'C:\Tools\vcpkg\scripts\buildsystems\vcpkg.cmake'
$vulkan = [Environment]::GetEnvironmentVariable('VULKAN_SDK', 'Machine')

if ($vulkan) {
    $env:VULKAN_SDK = $vulkan.TrimEnd('\')
    $env:Path = "$env:VULKAN_SDK\Bin;$env:Path"
}

if (-not (Test-Path -LiteralPath $toolchain)) {
    throw "Toolchain vcpkg nao encontrado: $toolchain"
}

cmake -S $repo -B $build -G 'Visual Studio 18 2026' -A x64 `
    -DDREAM_DEV_INTERPRETER=ON `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
if ($LASTEXITCODE -ne 0) { throw 'Configuracao CMake falhou.' }

cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build falhou.' }

ctest --test-dir $build -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Testes falharam.' }

