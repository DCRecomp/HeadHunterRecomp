// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger builder/builder_core.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Builder: dos dois CDIs do usuario ao HeadHunter.exe nativo, na maquina do usuario (HH18, com a arquitetura do
// Builder do Blue Stinger: o mesmo nucleo para o HeadHunter-Recompiler.exe, o HeadHunter-Developer.exe e o
// hh-builder de linha de comando).
//
// Etapas: valida as entradas -> localiza o compilador -> prepara os dados de execucao de cada disco (conferidos
// setor a setor contra o proprio CDI e a versao: disco 1 hedhunter_v1006_a, disco 2 hedhunter_v1006_b) e junta os
// dois numa instalacao unica (HH08, sem troca de disco) -> analisa e gera o C++ -> configura (CMake) -> compila e
// liga com o runtime (kit de fontes) -> monta o pacote numa pasta temporaria -> verifica o executavel montado
// (--version e boot headless) -> troca a versao instalada.
//
// Saida em <out>:
//   HeadHunter\    jogo instalado (so trocado depois de montado e verificado)
//   .hh-build\      trabalho: game-data, generated, analysis, builder.log
// Build incremental do CMake: %LOCALAPPDATA%\HeadHunterRecomp\build\<hash da saida>
// (caminho curto: o MSVC nao passa de 260 caracteres).
// Uma falha em qualquer etapa deixa a versao instalada anterior intacta.
#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace hh::builder {

enum class Step { Validate, Toolchain, Extract, Recompile, Configure, Compile, Package, Verify, Export, Install, Done };
const char* step_name(Step s);

struct Request {
    std::filesystem::path disc;    // imagem CDI do disco 1
    std::filesystem::path disc2;   // imagem CDI do disco 2 (os dois sao obrigatorios para preparar os dados)
    std::filesystem::path outDir;  // pasta de saida (criada se nao existir)
    std::wstring          installName = L"HeadHunter";  // subpasta do jogo em outDir
    std::string           versionLabel;  // vazio = arquivo VERSION do kit (ou "dev")
    // Projeto C++ editavel (project_export.h): exportar depois de compilar e verificar;
    // ou compilar A PARTIR de um projeto (com os mods; o CDI so e preciso se os dados ainda
    // nao foram preparados nesta saida).
    std::filesystem::path exportProject;
    std::filesystem::path fromProject;
    // So exportar (HeadHunter-Developer, hh-builder --somente-exportar): valida o CDI, prepara e confere os
    // dados em <outDir>\.hh-build\data (a pasta dos recursos), gera o C++ e exporta o projeto. Sem compilador,
    // sem compilacao e sem instalacao. Compilar o projeto depois com a mesma outDir dispensa o CDI.
    bool                  exportOnly = false;
    // Tipo de build do CMake: "Release" (padrao, o do pacote) ou "Debug" (depuracao nesta maquina: usa o CRT de
    // depuracao do Visual Studio, que nao e redistribuivel). Cada tipo tem a sua pasta de build no cache.
    std::string           buildType = "Release";
    std::filesystem::path kitDir;  // fontes do runtime + jogo (MIT/zlib), config e textos
    std::filesystem::path sdl3Dir; // fontes do SDL3; vazio = <kit>\third_party\SDL3
    std::filesystem::path vswhere, vcvars;  // localizacao do compilador (vazio = automatica)
    unsigned              verifyVblanks = 120;  // boot headless na verificacao
    bool                  clean         = false;  // apaga o build incremental antes
    // Local dos saves gravado em <instalacao>\instalacao.toml (native/platform/install_config.h).
    // Vazio: o arquivo da instalacao anterior fica como esta (arquivo do usuario, preservado).
    std::optional<bool>   portableSaves;
    // HH12: FFmpeg que decodifica os filmes 4X (A11: processo externo). Vazio = "ffmpeg" no PATH. Sem FFmpeg os filmes
    // ficam "nao convertidos" (o jogo os pula) e o resto segue.
    std::filesystem::path ffmpeg;
};

struct Callbacks {
    // fracao: progresso total de 0 a 1. Pode ser chamada de outra thread que nao a da GUI.
    std::function<void(Step, double fraction, const std::string& message)> progress;
    std::function<void(const std::string& line)>                           log;
    const std::atomic<bool>*                                               cancel = nullptr;
};

// Tipo do erro, para a interface simples mostrar uma mensagem traduzida (a mensagem tecnica fica em
// Result::error e no builder.log).
enum class ErrorKind { None, Cancelled, MissingInput, DiscNotFound, InvalidDisc, NoSpace, NoPermission, PathTooLong,
                       Busy, Toolchain, Data, Build, Verify, Install, Other };
const char* error_key(ErrorKind k);  // chave do catalogo da interface ("err.no_space"...)
// Falta de espaco e de permissao pelo codigo do sistema; senao `otherwise`.
ErrorKind error_kind_of(const std::error_code& ec, ErrorKind otherwise);

struct Result {
    bool                  ok = false;
    ErrorKind             kind = ErrorKind::None;
    bool                  cancelled = false;
    Step                  failedStep = Step::Validate;
    std::string           error;       // mensagem para o usuario
    std::string           warning;     // sucesso com ressalva (versao anterior nao arquivada: ArchiveError)
    std::filesystem::path installDir;  // <out>\HeadHunter
    std::filesystem::path exe;         // <out>\HeadHunter\HeadHunter.exe
    std::filesystem::path logFile;     // <out>\.hh-build\builder.log
    std::filesystem::path projectDir;  // projeto exportado (vazio se nao exportou)
};

Result build(const Request& req, const Callbacks& cb);

// Instalacao que um build em <outDir>\<installName> substituiria (para os avisos da interface).
struct InstallInfo {
    bool                  exists = false;    // a pasta existe e nao esta vazia
    bool                  ours = false;      // instalada por nos (marca .hh-install)
    bool                  portable = false;  // instalacao.toml pede saves portateis
    std::filesystem::path dir, previous;     // a pasta e onde a versao atual ficara guardada
};
InstallInfo inspect_install(const std::filesystem::path& outDir, const std::wstring& installName = L"HeadHunter");

// Marcas gravadas no pacote instalado: lista dos arquivos do pacote (o resto da pasta e do
// usuario) e o instante da instalacao (identifica backups nossos e o mais recente).
inline constexpr wchar_t kPackageManifest[] = L".hh-package";
inline constexpr wchar_t kInstallMarker[]   = L".hh-install";

struct SwapCancelled : std::runtime_error {
    using std::runtime_error::runtime_error;
};
// A troca falhou ANTES de a nova versao entrar: a anterior continua (codigo do sistema em `code`).
struct SwapError : std::runtime_error {
    std::error_code code;
    SwapError(const std::string& m, std::error_code c) : std::runtime_error(m), code(c) {}
};
// A nova versao JA esta instalada, mas a anterior nao pode ser guardada como <dest>.anterior: a
// mensagem diz onde ela ficou (o build termina com sucesso e um aviso).
struct ArchiveError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Troca segura de pasta (exposta para testes): `stage` passa a ser `dest`.
// - Sem `dest` e com backups `<dest>.old-*` marcados (troca interrompida), o mais recente
//   volta primeiro; com `dest`, backups marcados que sobraram sao apagados.
// - Grava em `stage` a lista do pacote e a marca de instalacao.
// - Arquivos do usuario na versao anterior (tudo fora da lista do pacote anterior; sem
//   lista, fora de isPackageFile) sao copiados para `stage`.
// - `cancel` vale ate o primeiro rename (lanca SwapCancelled); depois, a troca termina.
// - Se a troca falhar, `dest` anterior e restaurado e a funcao lanca (a mensagem diz onde
//   esta a versao anterior se nem a restauracao der certo).
// - Com sucesso, a versao anterior fica em `<dest>.anterior` (rollback: renomear de
//   volta); uma `.anterior` mais antiga nunca e apagada: vai para `<dest>.anterior-<data>-<x>`.
void install_swap(const std::filesystem::path& stage, const std::filesystem::path& dest,
                  const std::function<bool(const std::filesystem::path& relative)>& isPackageFile,
                  const std::atomic<bool>* cancel = nullptr);

// Criterio do pacote para instalacoes sem lista (.hh-package): HeadHunter.exe, DLLs, textos, licencas e, do
// Builder antigo (HH06-HH11), a marca .hh-builder e data\. O resto (saves, controles.toml, mods) e do usuario.
bool is_package_file(const std::filesystem::path& relative);

// Kit de fontes valido? Vazio = sim; senao, o que falta.
std::string check_kit(const std::filesystem::path& kitDir, const std::filesystem::path& sdl3Dir);

// Fontes do toml++ (N3-1: o produto le o arquivo de controles): <kit>\third_party\tomlplusplus
// (distribuicao e projeto editavel) ou, no repositorio, ao lado das fontes do SDL3
// (build\...\_deps\tomlplusplus-src).
std::filesystem::path toml_dir(const std::filesystem::path& kitDir, const std::filesystem::path& sdl3Dir);

} // namespace hh::builder
