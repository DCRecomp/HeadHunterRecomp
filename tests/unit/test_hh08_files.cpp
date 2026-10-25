// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_n1.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH08.2: sistema de arquivos nativo (DataFileSystem) sobre arquivos comuns.
#include "test.h"

#include "platform/data_files.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace hh;
namespace fs = std::filesystem;
namespace nf = hh::native::files;

namespace {

struct TempTree {
    fs::path root;
    TempTree() {
        root = fs::temp_directory_path() / ("hh_hh08_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root / "disc" / "AHFDATA");
    }
    ~TempTree() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    void write(const fs::path& rel, const std::string& bytes) const {
        fs::create_directories((root / rel).parent_path());
        std::ofstream(root / rel, std::ios::binary) << bytes;
    }
};

std::string pattern(size_t n, char seed) {
    std::string s(n, '\0');
    for (size_t i = 0; i < n; ++i) s[i] = char(seed + i % 251);
    return s;
}

bool throwsWith(const std::function<void()>& fn, const std::string& needle) {
    try {
        fn();
    } catch (const std::exception& e) {
        return std::string(e.what()).find(needle) != std::string::npos;
    }
    return false;
}

} // namespace

HH_TEST(hh08_canonical_path) {
    HH_CHECK(nf::canonical_path("\\AHFDATA\\HeadHunter.AHF;1") == "AHFDATA/HEADHUNTER.AHF");
    HH_CHECK(nf::canonical_path("/ahfdata//./a.bin") == "AHFDATA/A.BIN");
    HH_CHECK(nf::canonical_path("disc\\disk1.id") == "DISC/DISK1.ID");
    HH_CHECK(nf::canonical_path("") == "");
}

HH_TEST(hh08_data_files_open_read_list) {
    TempTree          t;
    const std::string a = pattern(5000, 'a');
    t.write("disc/AHFDATA/A.AHF", a);
    t.write("disc/1ST_READ.BIN", "boot");
    nf::DataFileSystem fsys(t.root / "disc");
    HH_CHECK(fsys.file_count() == 2);
    auto h = fsys.open("\\ahfdata\\a.ahf;1");  // maiusculas, '\', versao
    HH_CHECK(h && fsys.size(h.value()).value() == 5000);
    HH_CHECK(fsys.logical_name(h.value()) == "AHFDATA/A.AHF");
    std::vector<std::byte> buf(2048);
    HH_CHECK(fsys.read(h.value(), 4096, buf).value() == 904);  // recortado no fim
    HH_CHECK(std::memcmp(buf.data(), a.data() + 4096, 904) == 0);
    HH_CHECK(fsys.read(h.value(), 5000, buf).value() == 0);
    HH_CHECK(fsys.read(h.value(), 5001, buf).error() == native::Error::OutOfRange);
    auto rq = fsys.read_async(h.value(), 0, buf);
    HH_CHECK(rq && fsys.poll(rq.value()).state == nf::RequestState::Done);
    HH_CHECK(fsys.poll(rq.value()).state == nf::RequestState::Failed);  // consumido
    const auto root = fsys.list("");
    HH_CHECK(root && root.value().size() == 2);
    const auto dir = fsys.list("\\AHFDATA");
    HH_CHECK(dir && dir.value().size() == 1 && dir.value()[0].name == "A.AHF" && dir.value()[0].size == 5000);
    HH_CHECK(fsys.list("NADA").error() == native::Error::NotFound);
    fsys.close(h.value());
    HH_CHECK(fsys.size(h.value()).error() == native::Error::InvalidHandle);
    HH_CHECK(fsys.open("AHFDATA/B.AHF").error() == native::Error::NotFound);
}

// Mods de recursos: qualquer tamanho; mod sem recurso correspondente e erro com o caminho.
HH_TEST(hh08_data_files_mods_verified) {
    TempTree t;
    t.write("disc/AHFDATA/A.AHF", pattern(100, 'a'));
    t.write("mods/AHFDATA/a.ahf", pattern(9000, 'm'));  // maior que o original
    {
        nf::DataFileSystem fsys(t.root / "disc", t.root / "mods");
        HH_CHECK(fsys.mods().size() == 1 && fsys.mods()[0].originalSize == 100 && fsys.mods()[0].modSize == 9000);
        auto h = fsys.open("AHFDATA/A.AHF");
        HH_CHECK(h && fsys.size(h.value()).value() == 9000);
        std::vector<std::byte> b(1);
        fsys.read(h.value(), 0, b);
        HH_CHECK(char(b[0]) == 'm');
    }
    t.write("mods/AHFDATA/NOVO.AHF", "x");
    HH_CHECK(throwsWith([&] { nf::DataFileSystem f(t.root / "disc", t.root / "mods"); },
                        "AHFDATA/NOVO.AHF: nao substitui nenhum recurso"));
    HH_CHECK(throwsWith([&] { nf::DataFileSystem f(t.root / "nada"); }, "pasta de recursos do jogo ausente"));
    nf::DataFileSystem ok(t.root / "disc", t.root / "sem_mods");  // sem pasta de mods: tudo bem
    HH_CHECK(ok.mods().empty());
}

// Mod que nao pode ser lido e erro claro com o caminho (arquivo aberto com acesso exclusivo).
HH_TEST(hh08_data_files_unreadable_mod) {
    TempTree t;
    t.write("disc/AHFDATA/A.AHF", "abc");
    t.write("mods/AHFDATA/A.AHF", "xyz");
    const HANDLE lock = CreateFileW((t.root / "mods" / "AHFDATA" / "A.AHF").c_str(), GENERIC_READ, 0, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HH_CHECK(lock != INVALID_HANDLE_VALUE);
    HH_CHECK(throwsWith([&] { nf::DataFileSystem f(t.root / "disc", t.root / "mods"); }, "AHFDATA/A.AHF: nao pode ser lido"));
    CloseHandle(lock);
    nf::DataFileSystem ok(t.root / "disc", t.root / "mods");  // liberado: aplica
    HH_CHECK(ok.mods().size() == 1);
}

// Documentacao na raiz da pasta de mods (LEIA-ME/README) nao e mod; o mesmo nome numa pasta e erro.
HH_TEST(hh08_data_files_mods_readme_ignored) {
    TempTree t;
    t.write("disc/AHFDATA/A.AHF", "abc");
    t.write("mods/LEIA-ME.txt", "instrucoes");
    t.write("mods/README.md", "instrucoes");
    nf::DataFileSystem ok(t.root / "disc", t.root / "mods");
    HH_CHECK(ok.mods().empty());
    t.write("mods/AHFDATA/LEIA-ME.txt", "x");
    HH_CHECK(throwsWith([&] { nf::DataFileSystem f(t.root / "disc", t.root / "mods"); }, "AHFDATA/LEIA-ME.TXT: nao substitui"));
}
