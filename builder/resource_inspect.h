// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger builder/resource_inspect.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Inspecao dos recursos de uma instalacao ja gerada (HeadHunter-Developer.exe), sem o CDI: so o que tem
// metadados confiaveis. Filmes so com o catalogo nativo do HH12 e audio so com o manifesto do HH13; ate la, as duas
// familias aparecem como pendentes, nunca como lista inventada. Devolve codigos (a janela traduz pelo catalogo de textos), nada de texto de interface.
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace hh::builder {

enum class ResourceKind : uint8_t { Movie, Audio, Config, Functions, Generated, Mod, Project };
enum class ResourceState : uint8_t {
    Ok,           // o arquivo existe
    FileMissing,  // o manifesto aponta um arquivo que nao esta la
    Absent,       // nao existe no disco original e nenhum mod o fornece (o jogo pula)
    NotInstalled, // a familia toda nao foi instalada (p.ex. sem a pasta data\video: instalacao sem filmes)
    Pending,      // a familia ainda nao tem manifesto (filmes ate o HH12, audio ate o HH13)
    Unverified,   // marca do formato atual presente; o conteudo (hash de cada arquivo) so e conferido ao compilar
};
enum class ResourceOrigin : uint8_t { Builder, Mod, Installation, None };

struct ResourceRow {
    ResourceKind          kind = ResourceKind::Movie;
    std::string           id;        // ID logico estavel ("3"), vazio quando a familia toda
    std::string           name;      // nome original ("SCENE0.4XM") ou do arquivo de configuracao
    ResourceState         state = ResourceState::Ok;
    ResourceOrigin        origin = ResourceOrigin::None;
    std::filesystem::path file;      // o que o jogo abre (vazio se nada)
    std::filesystem::path manifest;  // de onde veio a entrada
    bool                  moddable = false;  // substituivel por mod sem recompilar
    std::string           detail;    // tecnico, igual em qualquer idioma (resolucao, quadros, legendas)
};

enum class InspectProblem : uint8_t { NoInstallation, NoExecutable, NoData, ManifestError };
struct InspectIssue {
    InspectProblem kind;
    std::string    detail;  // caminho ou mensagem do leitor do manifesto
};

struct ResourceReport {
    bool                      project = false;  // a pasta e um projeto exportado (nao uma instalacao)
    std::filesystem::path     installDir;  // a pasta com HeadHunter.exe (aceita tambem a pasta acima dela)
    std::vector<ResourceRow>  rows;
    std::vector<InspectIssue> issues;
};

// Uma instalacao (HeadHunter.exe + data) ou um projeto exportado (mapa_funcoes.tsv, generated/): no projeto,
// o mapa de funcoes (contagem por origem), o C++ gerado, os mods (C++, assets, video), o CMakePresets.json, a
// pasta dos recursos do hh-projeto.toml (pronta = compilar sem o CDI) e os filmes como o jogo os resolveria com
// os mods do projeto.
ResourceReport inspect_resources(const std::filesystem::path& dir, const std::wstring& installName = L"HeadHunter");

} // namespace hh::builder
