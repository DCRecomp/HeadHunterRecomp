// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_n1.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH08.3: adaptador gdFs 1.09 sobre o sistema de arquivos nativo (contratos do HH08-PLANO).
#include "test.h"

#include "boundary_registry.h"
#include "cpu/sh4_runtime.h"
#include "debug/file_log.h"
#include "gdfs_adapter.h"
#include "mods/mod_api.h"
#include "platform/data_files.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace hh;
namespace fs = std::filesystem;
namespace nf = hh::native::files;
namespace gd = hh::transitional::gdfs;
using transitional::BoundaryRegistry;

namespace {

struct TempTree {
    fs::path root;
    TempTree() {
        root = fs::temp_directory_path() / ("hh_hh08g_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root / "disc");
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

// Guest sintetico: funcoes "legadas" nos enderecos do gdFs, com slots (como o codigo gerado com
// [native] slots), para o adaptador trocar o provedor.
int  g_legacyCalls = 0;
void legacyFn(Sh4Context* c) {
    ++g_legacyCalls;
    c->r[0] = 0x1234;
}

constexpr gd::Addresses kA = gd::kHeadHunter;
const uint32_t kAll[] = {kA.open,  kA.changeDir, kA.close,   kA.getFileSize, kA.getFileSctSize, kA.getFileFad,
                         kA.seek,  kA.tell,      kA.eof,     kA.setTransMode, kA.read,          kA.reqPio,
                         kA.reqRd32, kA.wait,    kA.getStat, kA.getTransStat, kA.stop,          kA.getErr,
                         kA.initGdfs, kA.driveStat};

struct Guest {
    std::vector<Sh4FuncEntry> table;
    std::vector<Sh4Func>      slots;
    std::vector<mods::Slot>   slotList;
    std::vector<uint8_t>      ram = std::vector<uint8_t>(16u << 20);
    Sh4Context                c{};
    BoundaryRegistry          registry;

    Guest() {
        std::vector<uint32_t> sorted(std::begin(kAll), std::end(kAll));
        std::sort(sorted.begin(), sorted.end());
        slots.resize(sorted.size(), &legacyFn);
        for (size_t i = 0; i < sorted.size(); ++i) {
            table.push_back({sorted[i] & 0x1FFFFFFF, &legacyFn});
            slotList.push_back({sorted[i], &slots[i], &legacyFn});
        }
        sh4_set_function_table(table.data(), table.size());
        mods::set_slots(slotList.data(), slotList.size());
        c.ram        = ram.data();
        c.next_event = ~0ull;
    }
    ~Guest() {
        gd::uninstall(registry);
        mods::reset();
        mods::set_slots(nullptr, 0);
        sh4_set_function_table(nullptr, 0);
    }
    uint32_t call(uint32_t addr, uint32_t r4 = 0, uint32_t r5 = 0, uint32_t r6 = 0) {
        c.r[4] = r4, c.r[5] = r5, c.r[6] = r6;
        sh4_lookup(addr)(&c);  // pelo slot/override, como o codigo gerado
        return c.r[0];
    }
    void str(uint32_t a, const std::string& s) { std::memcpy(ram.data() + (a & 0xFFFFFF), s.c_str(), s.size() + 1); }
    uint32_t u32(uint32_t a) const {
        uint32_t v;
        std::memcpy(&v, ram.data() + (a & 0xFFFFFF), 4);
        return v;
    }
};

[[noreturn]] void throwingFatal(Sh4Context*, const char* what, uint32_t, uint32_t) { throw std::runtime_error(what); }

} // namespace

// Contratos da API gdFs 1.09 (HH08-PLANO, "HH08.1 — resultados") sobre o sistema nativo.
HH_TEST(hh08_gdfs_adapter_contracts) {
    TempTree          t;
    const std::string a = pattern(5000, 'a');  // 3 setores da API (2048 + 2048 + 904)
    t.write("disc/AHFDATA/A.AHF", a);
    nf::DataFileSystem fsys(t.root / "disc");
    Guest              g;
    gd::install(g.registry, fsys);
    HH_CHECK(g.registry.count(transitional::Provider::Native) == std::size(kAll));
    file_log_init("hh_hh08_log_teste.tsv");

    g.str(0x8C100000, "\\");  // raiz (o jogo chama ChangeDir("\") antes de cada pasta)
    HH_CHECK(g.call(kA.changeDir, 0x8C100000) == 0);
    g.str(0x8C100000, "AHFDATA");
    HH_CHECK(g.call(kA.changeDir, 0x8C100000) == 0);
    g.str(0x8C100000, "\\NADA");
    HH_CHECK(g.call(kA.changeDir, 0x8C100000) == uint32_t(-1));
    g.str(0x8C100010, "a.ahf;1");  // relativo ao diretorio corrente, minusculas e versao
    const uint32_t h = g.call(kA.open, 0x8C100010, 0);
    HH_CHECK(gd::is_native_handle(h));
    HH_CHECK(g.call(kA.getStat, h) == 0);  // ocioso
    HH_CHECK(g.call(kA.getFileSize, h, 0x8C100100) == 1 && g.u32(0x8C100100) == 5000);
    HH_CHECK(g.call(kA.getFileSctSize, h, 0x8C100100) == 1 && g.u32(0x8C100100) == 3);

    // leitura sincrona de 2 unidades; posicao avanca
    HH_CHECK(g.call(kA.read, h, 2, 0x8C200000) == 0);
    HH_CHECK(std::memcmp(g.ram.data() + 0x200000, a.data(), 4096) == 0);
    HH_CHECK(g.call(kA.tell, h) == 2 && g.call(kA.getStat, h) == 1 && g.call(kA.getTransStat, h) == 4096);
    // pedido assincrono alem do fim: recortado a 1 unidade; os dados ja estao no buffer sem
    // nenhuma consulta (ultimo setor completado com zeros), e o estado fica ocupado (2) com 0
    // bytes ate a observacao
    std::memset(g.ram.data() + 0x300000, 0xEE, 4096);
    HH_CHECK(g.call(kA.reqRd32, h, 5, 0x8C300000) == 1);
    HH_CHECK(std::memcmp(g.ram.data() + 0x300000, a.data() + 4096, 904) == 0);
    HH_CHECK(g.ram[0x300000 + 904] == 0 && g.ram[0x300000 + 2047] == 0 && g.ram[0x300000 + 2048] == 0xEE);
    HH_CHECK(g.call(kA.tell, h) == 3);
    HH_CHECK(g.call(kA.getTransStat, h) == 2048 && g.call(kA.getStat, h) == 1);  // observado: concluido
    // no fim: 0 unidades, concluido
    HH_CHECK(g.call(kA.reqRd32, h, 1, 0x8C300000) == 0 && g.call(kA.getStat, h) == 1);
    HH_CHECK(g.call(kA.eof, h, 0x8C100100) == 0 && g.u32(0x8C100100) == 1);
    // erros: alinhamento, origem, faixa
    HH_CHECK(g.call(kA.reqRd32, h, 1, 0x8C300004) == uint32_t(-14));
    HH_CHECK(g.call(kA.seek, h, 0, 3) == uint32_t(-16));
    HH_CHECK(g.call(kA.seek, h, 1, 2) == uint32_t(-17));
    HH_CHECK(g.call(kA.seek, h, uint32_t(-1), 2) == 0 && g.call(kA.tell, h) == 2);
    HH_CHECK(g.call(kA.seek, h, 0, 0) == 0 && g.call(kA.tell, h) == 0);
    HH_CHECK(g.call(kA.stop, h) == 0 && g.call(kA.getStat, h) == 0);
    // Stop com pedido em andamento: os setores ja foram entregues (como no GD-ROM HLE do legado)
    // e o arquivo volta a ocioso; nada fica pendente depois
    std::memset(g.ram.data() + 0x380000, 0xEE, 2048);
    HH_CHECK(g.call(kA.reqRd32, h, 1, 0x8C380000) == 1);
    HH_CHECK(g.call(kA.stop, h) == 0 && g.call(kA.getStat, h) == 0 && g.call(kA.tell, h) == 1);
    HH_CHECK(std::memcmp(g.ram.data() + 0x380000, a.data(), 2048) == 0);
    // um pedido seguido de outro sem observacao: o anterior e publicado como concluido
    HH_CHECK(g.call(kA.reqRd32, h, 1, 0x8C380000) == 1);
    HH_CHECK(g.call(kA.read, h, 1, 0x8C200000) == 0);
    HH_CHECK(std::memcmp(g.ram.data() + 0x380000, a.data() + 2048, 2048) == 0 && g.call(kA.tell, h) == 3);
    HH_CHECK(g.call(kA.setTransMode, h, 1) == 0 && g.call(kA.getErr, h) == 0);
    // log de leituras: blocos de 2 048 com o nome logico
    const std::string log = file_log_dump();
    file_log_init("");
    HH_CHECK(log.find("nativo\tdado\tAHFDATA/A.AHF\t0\t2048\t") != std::string::npos);
    HH_CHECK(log.find("nativo\tdado\tAHFDATA/A.AHF\t4096\t904\t") != std::string::npos);
    // fechar invalida o handle
    g.call(kA.close, h);
    HH_CHECK(g.call(kA.getStat, h) == uint32_t(-10) && g.call(kA.getFileSize, h, 0x8C100100) == 0);
    HH_CHECK(g.call(kA.read, h, 1, 0x8C200000) == uint32_t(-10));
    // estado logico do drive: pausa (1); com a tampa aberta, 6 (o jogo sai para o BIOS)
    HH_CHECK(g.call(kA.driveStat) == 1 && !gd::lid_open());
    gd::open_lid();
    HH_CHECK(g.call(kA.driveStat) == 6 && gd::lid_open());
    // inexistente: 0
    g.str(0x8C100010, "NAO.AHF");
    HH_CHECK(g.call(kA.open, 0x8C100010, 0) == 0 && gd::stats().notFound == 1);
    HH_CHECK(gd::stats().opens == 1 && gd::stats().reads == 5);
}

// GDFS legado (ponteiro na RAM) vai para a funcao original; handle nativo numa funcao interna
// sem equivalente nativo e erro fatal com mensagem.
HH_TEST(hh08_gdfs_adapter_legacy_passthrough_and_guards) {
    TempTree t;
    t.write("disc/DISC/DISK1.ID", "abc");
    nf::DataFileSystem fsys(t.root / "disc");
    Guest              g;
    gd::install(g.registry, fsys);
    g_legacyCalls = 0;
    // 0x8C28AB80: GDFS interno do driver (o jogo consulta o estado dele em 0x8C04E754)
    HH_CHECK(g.call(kA.getStat, 0x8C28AB80) == 0x1234 && g_legacyCalls == 1);
    HH_CHECK(g.call(kA.wait, 0x8C28AB80) == 0x1234 && g_legacyCalls == 2);
    g.str(0x8C100010, "\\DISC\\disk1.id");
    const uint32_t h = g.call(kA.open, 0x8C100010, 0);
    HH_CHECK(gd::is_native_handle(h));
    sh4_set_fatal_handler(&throwingFatal);
    HH_CHECK(throwsWith([&] { g.call(kA.getFileFad, h, 0x8C100100); }, "gdFsGetFileFad"));
    HH_CHECK(throwsWith([&] { g.call(kA.wait, h); }, "espera de pedido"));
    HH_CHECK(throwsWith([&] { g.call(kA.reqPio, h, 1, 0x8C200000); }, "leitura por PIO"));
    HH_CHECK(throwsWith([&] { g.call(kA.initGdfs, h); }, "inicializacao do GDFS"));
    sh4_set_fatal_handler(nullptr);
    HH_CHECK(g_legacyCalls == 2);  // nenhuma guarda caiu no legado com o handle nativo
    // registro de diretorio carregado pelo legado (dirrec): a funcao original abre (GDFS legado)
    HH_CHECK(g.call(kA.open, 0x8C100010, 0x8C100200) == 0x1234 && g_legacyCalls == 3);
}

// Sem slot (codigo gerado sem [native] slots), instalar e erro: chamadas diretas iriam ao legado
// com handles nativos.
HH_TEST(hh08_gdfs_adapter_requires_slots) {
    TempTree t;
    t.write("disc/X.BIN", "x");
    nf::DataFileSystem fsys(t.root / "disc");
    const Sh4FuncEntry table[] = {{kA.open & 0x1FFFFFFF, &legacyFn}};
    sh4_set_function_table(table, 1);
    BoundaryRegistry r;
    HH_CHECK(throwsWith([&] { gd::install(r, fsys); }, "nao tem slot"));
    HH_CHECK(r.count(transitional::Provider::Native) == 0);
    sh4_set_function_table(nullptr, 0);
}
