// SPDX-License-Identifier: MIT
// HH18: troca da instalacao pelo Builder (builder_core.install_swap) a partir de uma instalacao do Builder antigo
// (HH06-HH11: marca .hh-builder, data\, sem lista .hh-package). Arquivos sinteticos numa pasta temporaria.
#include "test.h"

#include "builder_core.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace hh::builder;

namespace {

void put(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

std::string get(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

fs::path freshDir(const char* name) {
    const fs::path d = fs::temp_directory_path() / (std::string("hh18_") + name);
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d);
    return d;
}

} // namespace

HH_TEST(hh18_package_criterion_covers_old_builder_install) {
    HH_CHECK(is_package_file(L"HeadHunter.exe"));
    HH_CHECK(is_package_file(L"msvcp140.dll"));
    HH_CHECK(is_package_file(L".hh-builder"));
    HH_CHECK(is_package_file(fs::path(L"data") / L"disc" / L"X.AHF"));
    HH_CHECK(!is_package_file(L"controles.toml"));
    HH_CHECK(!is_package_file(fs::path(L"saves") / L"card0" / L"A.hhsv"));
    HH_CHECK(!is_package_file(fs::path(L"mods") / L"x.dll"));  // DLL so conta na raiz
}

HH_TEST(hh18_swap_from_old_builder_keeps_user_files_only) {
    const fs::path root = freshDir("swap_antigo");
    const fs::path dest = root / L"HeadHunter", stage = root / L"stage";
    // instalacao do Builder antigo
    put(dest / L".hh-builder", "1\n");
    put(dest / L"HeadHunter.exe", "exe antigo");
    put(dest / L"data" / L"velho.bin", "dado que o pacote novo nao tem");
    put(dest / L"controles.toml", "# do usuario\n");
    put(dest / L"saves" / L"card0" / L"A.hhsv", "save");
    // pacote novo
    put(stage / L"HeadHunter.exe", "exe novo");
    put(stage / L"data" / L"novo.bin", "dado novo");
    install_swap(stage, dest, is_package_file);

    HH_CHECK(get(dest / L"HeadHunter.exe") == "exe novo");
    HH_CHECK(get(dest / L"controles.toml") == "# do usuario\n");
    HH_CHECK(get(dest / L"saves" / L"card0" / L"A.hhsv") == "save");
    HH_CHECK(!fs::exists(dest / L".hh-builder"));          // a marca antiga nao passa como arquivo do usuario
    HH_CHECK(!fs::exists(dest / L"data" / L"velho.bin"));  // nem os dados antigos
    HH_CHECK(fs::exists(dest / L".hh-install") && fs::exists(dest / L".hh-package"));
    HH_CHECK(get(root / L"HeadHunter.anterior" / L"HeadHunter.exe") == "exe antigo");  // rollback intacto

    // segunda troca: agora pela lista do pacote; a .anterior antiga e arquivada, nunca apagada
    put(stage / L"HeadHunter.exe", "exe 3");
    install_swap(stage, dest, is_package_file);
    HH_CHECK(get(dest / L"HeadHunter.exe") == "exe 3");
    HH_CHECK(get(dest / L"controles.toml") == "# do usuario\n");
    HH_CHECK(fs::exists(root / L"HeadHunter.anterior" / L".hh-install"));
    int archived = 0;
    for (const auto& e : fs::directory_iterator(root))
        if (e.path().filename().wstring().rfind(L"HeadHunter.anterior-", 0) == 0) ++archived;
    HH_CHECK(archived == 1);
    std::error_code ec;
    fs::remove_all(root, ec);
}
