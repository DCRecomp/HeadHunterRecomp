// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger builder/project_export.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "project_export.h"

#include "child_process.h"

#include <ctime>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace hh::builder {

// O que o CMake raiz do HH precisa para o HeadHunter.exe com HH_BUILD_TOOLS=OFF e BUILD_TESTS=OFF (o Builder e a
// documentacao ficam de fora): app, runtime, camadas nativas e transitorias, o tradutor (alvos do CMake raiz), as
// ferramentas que o runtime liga (hh-extract, dcmap) e o hh-diff (alvo do app), a configuracao e os modulos do CMake.
const wchar_t* const kKitEntries[20] = {L"CMakeLists.txt", L"LICENSE", L"VERSION", L"app", L"runtime", L"native",
                                        L"legacy", L"transitional", L"translator", L"config", L"cmake",
                                        L"tools/CMakeLists.txt", L"tools/hh-extract", L"tools/dcmap", L"tools/hh-diff",
                                        L"tools/dist/LEIA-ME.txt", L"third_party/licenses", L"third_party/README.md",
                                        L"third_party/vulkan",  // cabecalhos Vulkan (Apache-2.0) do renderizador
                                        L"icone-local"};  // icone da tela de titulo do usuario (so local; opcional)

namespace {

// Copia `from` para `to` so com o que e mais novo na origem (edicoes mais novas no destino ficam).
void syncTree(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    if (fs::is_directory(from, ec)) {
        fs::create_directories(to);
        fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::update_existing);
    } else if (fs::is_regular_file(from, ec)) {
        fs::create_directories(to.parent_path());
        fs::copy_file(from, to, fs::copy_options::update_existing);
    }
}

void writeText(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
    f.close();
    if (!f) throw std::runtime_error("nao foi possivel gravar " + path_text(p));
}

const char* const kReadme = R"MD(# Head Hunter recompilado: projeto C++20 editável

Gerado pelo **HeadHunter-Developer** (ou `hh-builder --somente-exportar`) a partir da **sua** cópia do jogo
(os dois discos).

> **Aviso legal.** `generated/` e os dados em `<recursos>\.hh-build\data` são derivados dos discos do jogo.
> Use-os só localmente: **não publique** esta pasta, os dados nem o executável compilado a partir deles.
> O resto (runtime, camadas nativas, app, ferramentas) é do projeto Head Hunter Recompiled (licença MIT;
> SDL3 zlib, toml++ MIT).

## O que é este código (e o que não é)

É uma **recompilação estática**: cada função do executável original (`1ST_READ.BIN`, código de máquina SH-4
do Dreamcast) foi traduzida **instrução por instrução** para C++20. O resultado roda nativamente no Windows.
Não há emulação de CPU: o código do jogo só executa como C++ gerado.

**Não é o código-fonte original** do jogo, nem uma "descompilação" para código legível:
- as funções se chamam pelo endereço (`f_8C09E904`), não pelo nome que tinham;
- operam sobre o contexto da CPU (`c->r[4]`, flags, memória do Dreamcast), não sobre variáveis e tipos;
- estruturas, nomes e a divisão em módulos do original **não** foram recuperados.

O que dá para fazer com segurança é trocar funções inteiras por mods (pelo endereço) ou editar o C++ gerado
com cuidado.

## Estrutura

| Pasta/arquivo | Conteúdo |
|---|---|
| `generated/` | As funções do jogo traduzidas do SH-4 para C++ (uma função C++ por função SH-4) e a tabela de despacho. |
| `mapa_funcoes.tsv` | Endereço SH-4 → função C++ (`f_XXXXXXXX`) → arquivo → como foi descoberta → tamanho. |
| `runtime/` | O hardware do Dreamcast que o código traduzido ainda usa na fase híbrida (memória, PowerVR2, AICA, BIOS em HLE). Encolhe até sumir (etapa HH15). |
| `native/` | O que já é nativo e portável, sem nada do Dreamcast: arquivos, tempo, entrada, saves. |
| `transitional/` | As fronteiras entre o jogo e o nativo: adaptadores que trocam funções do SDK do jogo pelos provedores nativos. |
| `legacy/` | Os provedores antigos, alternativa (`--controles legado`, `--saves legado`...) até a remoção prevista. |
| `app/` | O executável Windows: janela SDL3, opções de linha de comando, ritmo de 59,94 Hz. |
| `config/` | A configuração da recompilação (`headhunter.toml`: fronteiras, slots nativos, esperas). |
| `translator/`, `tools/` | O tradutor e as bibliotecas que o runtime liga (leitura dos dados, mapa de regiões). |
| `mods/` | **Seus mods**: C++ (`*.cpp`) e recursos (`assets/`). O Builder nunca sobrescreve esta pasta. |
| `third_party/` | SDL3 (zlib) e toml++ (MIT), para compilar sem internet; licenças. |
| `CMakePresets.json` | Configuração do CMake para o MSVC (Release e Debug), igual à do `compilar.cmd`. |
| `hh-projeto.toml` | De onde veio o projeto: versão e a **pasta dos recursos** (dados já conferidos com os discos). |

## Estado atual (o que é nativo)

- **Arquivos, tempo, controles e saves:** nativos por padrão, com o legado como alternativa.
- **Vídeo, áudio e renderização:** ainda pelo runtime da fase híbrida (etapas HH12 a HH14).
- Os dois discos são uma instalação só: o jogo não pede troca de disco.

## Requisitos

- Windows 10/11 x64 e o **Visual Studio 2022 ou Build Tools** com o componente "Desenvolvimento para desktop
  com C++" (MSVC x64, SDK do Windows). CMake e Ninja vêm junto com ele.
- Nada é baixado: SDL3 e toml++ estão em `third_party/`.
- O caminho desta pasta deve usar só caracteres da página de código do Windows (limite do Ninja).
- O Builder compila num cache de caminho curto (`%LOCALAPPDATA%\HeadHunterRecomp\build\<hash da pasta de saída>`).
  Ele guarda também uma cópia do C++ gerado: é derivado do jogo como `generated/`, então fica só nesta máquina.
  Para removê-lo, apague essa pasta (a próxima compilação a recria).

## Como modificar

1. **Mods em C++** (recomendado): um arquivo `mods/<nome>.cpp` troca uma função do jogo pelo endereço
   (veja `mapa_funcoes.tsv` e o exemplo em `mods/exemplos/`):

       #include "mods/mod_api.h"
       HH_MOD(meu_mod) { hh::mods::replace(0x8C09E904, minha_funcao); }

   `hh::mods::call_original(endereco, c)` chama a função original de dentro do mod.
2. **Recursos:** um arquivo em `mods/assets/<caminho do disco>` substitui o do disco (por exemplo
   `mods/assets/AHFDATA/HEADHUNTER.AHF`).
3. **C++ gerado:** editar `generated/` funciona, mas uma nova exportação guarda essa pasta em
   `generated.anterior-<data>` e escreve outra. Prefira mods.

## Como compilar e testar

- **Pelo HeadHunter-Developer** (área *Compilar projeto*), ou `hh-builder --de-projeto "<esta pasta>" --out "<recursos>"`:
  compila com os mods, **verifica** o executável (dados conferidos e boot sem os CDIs) e só então instala em
  `<recursos>\HeadHunter` (a versão anterior fica em `HeadHunter.anterior`). Com a pasta dos recursos de
  `hh-projeto.toml`, os CDIs **não são necessários**.
- **Numa IDE com CMake:** abra esta pasta no Visual Studio (Abrir Pasta), no VS Code (CMake Tools) ou no
  CLion, escolha o preset `release` ou `debug` (o compilador tem de ser o MSVC x64) e compile o alvo `HeadHunter`.
- **Direto:** `compilar.cmd [release|debug]` (o mesmo preset, sem IDE).

Compilado fora do Builder, o executável sai em `build\<preset>\app\HeadHunter.exe` e roda com os dados da
instalação: `HeadHunter.exe --data "<recursos>\HeadHunter\data"`. O preset `debug` usa o CRT de depuração
do Visual Studio: roda só numa máquina com ele instalado.
)MD";

const char* const kModsReadme = R"(Mods do Head Hunter recompilado
================================

*.cpp nesta pasta: compilados junto com o jogo quando o Builder compila a partir do projeto
(ou com compilar.cmd). Cada mod usa HH_MOD(nome) { ... } e hh::mods::replace(endereco, funcao).
Veja LEIA-ME-PROJETO.md e os exemplos em exemplos\ (nao compilados).

assets\<caminho do disco>: arquivos que substituem os do disco (qualquer tamanho).

Esta pasta e sua: o Builder nunca a sobrescreve.
)";

const char* const kExampleMod = R"(// Exemplo (nao compilado: copie para mods\ para usar).
// Conta quantas vezes o jogo chama a funcao 0x8C09E904 (a que monta o nome "Scene%s.4xm" e toca uma cena) e
// mantem o comportamento original.
#include "mods/mod_api.h"

#include <cstdio>

namespace {
unsigned g_chamadas = 0;

void contar(hh::Sh4Context* c) {
    std::fprintf(stderr, "[mod exemplo] cena %u\n", ++g_chamadas);
    hh::mods::call_original(0x8C09E904, c);
}
} // namespace

HH_MOD(exemplo_contador) { hh::mods::replace(0x8C09E904, contar); }
)";

const char* const kBuildCmd = R"CMD(@echo off
rem Compila o jogo a partir deste projeto (com os mods), sem instalar, pelos presets do CMakePresets.json
rem (os mesmos da IDE). Requer o Visual Studio 2022 ou Build Tools (C++ x64). Para compilar, verificar e
rem instalar, prefira o HeadHunter-Developer.   compilar.cmd [release|debug]
setlocal
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=release"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio Build Tools nao encontrado & exit /b 1)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -prerelease -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (echo Compilador C++ x64 nao encontrado & exit /b 1)
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
echo Pronto: build\%PRESET%\app\HeadHunter.exe (use --data ^<instalacao^>\data)
)CMD";

// CMakePresets.json do projeto: as mesmas opcoes do Builder ao compilar a partir de um projeto (MSVC, Ninja,
// mods ligados, gerado e dependencias de third_party, sem internet). Versao 6 (CMake 3.25+, o do VS 2022).
const char* const kPresets = R"JSON({
  "version": 6,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" },
      "cacheVariables": {
        "CMAKE_C_COMPILER": "cl",
        "CMAKE_CXX_COMPILER": "cl",
        "BUILD_TESTS": "OFF",
        "HH_BUILD_TOOLS": "OFF",
        "HH_MODS": "ON",
        "HH_MODS_DIR": "${sourceDir}/mods",
        "HH_GENERATED_DIR": "${sourceDir}/generated",
        "FETCHCONTENT_SOURCE_DIR_SDL3": "${sourceDir}/third_party/SDL3",
        "FETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS": "${sourceDir}/third_party/tomlplusplus",
        "FETCHCONTENT_FULLY_DISCONNECTED": "ON",
        "HH_VERSION_OVERRIDE": "@VERSAO@"
      }
    },
    { "name": "release", "inherits": "base", "displayName": "Release (MSVC x64)", "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" } },
    { "name": "debug", "inherits": "base", "displayName": "Debug (MSVC x64)", "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" } }
  ],
  "buildPresets": [
    { "name": "release", "configurePreset": "release", "targets": [ "HeadHunter" ] },
    { "name": "debug", "configurePreset": "debug", "targets": [ "HeadHunter" ] }
  ]
}
)JSON";

} // namespace

std::string check_project(const fs::path& dir) {
    std::error_code ec;
    for (const wchar_t* f : {L"CMakeLists.txt", L"generated/dispatch.cpp", L"generated/funcs.h", L"runtime/CMakeLists.txt",
                             L"app/CMakeLists.txt", L"third_party/SDL3/CMakeLists.txt", L"third_party/tomlplusplus/CMakeLists.txt",
                             L"config/headhunter.toml"})
        if (!fs::is_regular_file(dir / f, ec)) return "projeto incompleto: falta " + path_text(dir / f);
    return {};
}

void export_project(const ExportInputs& in, const fs::path& dest, const std::function<void(const std::string&)>& log,
                    const std::atomic<bool>* cancel) {
    auto check = [&] {
        if (cancel && cancel->load()) throw ExportCancelled{};
    };
    std::error_code ec;
    fs::create_directories(dest, ec);
    if (ec) throw std::runtime_error("nao foi possivel criar o projeto em " + path_text(dest) + " (" + error_text(ec) + ")");

    // fontes do runtime/app (edicoes mais novas no projeto ficam), SDL3 e toml++
    for (const wchar_t* e : kKitEntries) {
        check();
        syncTree(in.kit / e, dest / e);
    }
    check();
    syncTree(in.sdl3, dest / L"third_party" / L"SDL3");
    check();
    syncTree(in.toml, dest / L"third_party" / L"tomlplusplus");
    check();

    // C++ gerado: o anterior (talvez editado) e guardado, nunca perdido
    const fs::path gen = dest / L"generated";
    if (fs::exists(gen, ec)) {
        char ts[32];
        const std::time_t t = std::time(nullptr);
        std::strftime(ts, sizeof ts, "%Y%m%d-%H%M%S", std::localtime(&t));
        const fs::path old = dest / (L"generated.anterior-" + widen(ts));
        fs::rename(gen, old);
        log("generated/ anterior guardado em " + path_text(old));
    }
    fs::copy(in.generated, gen, fs::copy_options::recursive);
    check();

    // mods/: so na primeira exportacao (e do usuario)
    const fs::path mods = dest / L"mods";
    if (!fs::exists(mods, ec)) {
        writeText(mods / L"LEIA-ME-MODS.txt", kModsReadme);
        writeText(mods / L"exemplos" / L"exemplo_contador.cpp", kExampleMod);
        writeText(mods / L"assets" / L"LEIA-ME.txt",
                  "Arquivos que substituem os do disco, no mesmo caminho (ex.: AHFDATA\\HEADHUNTER.AHF).\n"
                  "Qualquer tamanho. Um arquivo que nao corresponde a nenhum do disco e recusado.\n");
    }

    // mapa endereco -> funcao C++ -> arquivo -> origem -> instrucoes
    std::map<std::string, std::string> fileOf;
    const std::regex                   fn(R"(^void (f_([0-9A-F]{8}))\(hh::Sh4Context\* c\) \{)");
    for (const auto& e : fs::directory_iterator(gen)) {
        if (e.path().extension() != L".cpp") continue;
        std::ifstream f(e.path());
        for (std::string l; std::getline(f, l);) {
            std::smatch m;
            if (l.rfind("void f_", 0) == 0 && std::regex_search(l, m, fn)) fileOf[m[2]] = path_text(e.path().filename());
        }
    }
    std::ostringstream map;
    map << "endereco\tfuncao_cpp\tarquivo\torigem\tinstrucoes\n";
    std::ifstream tsv(in.functionsTsv);
    std::string   line;
    std::getline(tsv, line);  // cabecalho
    size_t n = 0;
    while (std::getline(tsv, line)) {
        std::istringstream s(line);
        std::string        addr, origin, insns;
        std::getline(s, addr, '\t');
        std::getline(s, origin, '\t');
        std::getline(s, insns, '\t');
        const std::string hex = addr.size() > 2 ? addr.substr(2) : addr;
        map << addr << "\tf_" << hex << "\t" << (fileOf.count(hex) ? fileOf[hex] : "?") << "\t" << origin << "\t"
            << insns << "\n";
        ++n;
    }
    writeText(dest / L"mapa_funcoes.tsv", map.str());
    writeText(dest / L"LEIA-ME-PROJETO.md", kReadme);
    writeText(dest / L"compilar.cmd", kBuildCmd);
    {  // a versao do Builder (como no configure do Builder ao compilar de projeto), com escape JSON
        std::string v;
        for (unsigned char ch : in.versionLabel.empty() ? std::string("dev") : in.versionLabel) {
            if (ch == '"' || ch == '\\') v += '\\', v += char(ch);
            else if (ch >= 0x20) v += char(ch);
        }
        std::string presets = kPresets;
        presets.replace(presets.find("@VERSAO@"), 8, v);
        writeText(dest / L"CMakePresets.json", presets);
    }
    {  // de onde veio o projeto: a pasta dos recursos (compilar com ela dispensa o CDI)
        auto esc = [](std::string v) {
            std::string o;
            for (char ch : v) o += (ch == '\\' || ch == '"') ? std::string("\\") + ch : std::string(1, ch);
            return o;
        };
        char              ts[32];
        const std::time_t t = std::time(nullptr);
        std::strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        std::string meta = "# Projeto exportado pelo Head Hunter Builder (HeadHunter-Developer)\n";
        meta += "versao = \"" + esc(in.versionLabel) + "\"\n";
        meta += "exportado = \"" + std::string(ts) + "\"\n";
        if (!in.resources.empty())
            meta += "# pasta de saida com os dados ja conferidos com o disco (.hh-build\\data): compilar com ela dispensa o CDI\n"
                    "recursos = \"" + esc(path_text(in.resources)) + "\"\n";
        writeText(dest / L"hh-projeto.toml", meta);
    }
    if (!in.versionLabel.empty()) writeText(dest / L"VERSION", in.versionLabel + "\n");
    log("projeto C++ exportado em " + path_text(dest) + " (" + std::to_string(n) + " funcoes no mapa)");
}

} // namespace hh::builder
