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
int      g_legacyCalls = 0;
uint32_t g_legacyRet   = 0x1234;
struct LegacyCall {
    uint32_t fn, r4, pr, retExpect, retSp;
};
std::vector<LegacyCall> g_legacyLog;  // o contexto de cada chamada
void legacyFn(Sh4Context* c) {
    ++g_legacyCalls;
    g_legacyLog.push_back({c->cur_fn, c->r[4], c->pr, c->ret_expect, c->ret_sp});
    c->r[0] = g_legacyRet;
}

constexpr gd::Addresses kA = gd::kHeadHunter;
const uint32_t kAll[] = {kA.open,  kA.changeDir, kA.close,   kA.getFileSize, kA.getFileSctSize, kA.getFileFad,
                         kA.seek,  kA.tell,      kA.eof,     kA.setTransMode, kA.read,          kA.reqPio,
                         kA.reqRd32, kA.wait,    kA.getStat, kA.getTransStat, kA.stop,          kA.getErr,
                         kA.initGdfs, kA.driveStat, kA.loadDir, kA.discGetter, kA.saveCall, kA.applySave};

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
    // todos os enderecos de kAll viram provedor nativo, menos a funcao do LOAD que o fim de disco so chama (HH16.P1)
    HH_CHECK(g.registry.count(transitional::Provider::Native) == std::size(kAll) - 1);
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
    // sem troca de disco: o drive esta sempre pronto (1); a arvore so tem o disco 1
    HH_CHECK(g.call(kA.driveStat) == 1 && gd::current_disc() == 1);
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

// Discos juntos (sem troca de disco): o jogo ve o disco em que esta, pela propria variavel
// 0x8C15174C. Disco 1: DISK1.ID e o arquivo comum; disco 2: DISK2.ID e a variante _DISCO2. A
// pasta das variantes nunca aparece para o jogo.
HH_TEST(hh08_gdfs_merged_discs_follow_game_variable) {
    TempTree t;
    t.write("disc/DISC/DISK1.ID", "1");
    t.write("disc/DISC/DISK2.ID", "2");
    t.write("disc/MOVIES/SCENE0.4XM", "um");
    t.write("disc/_DISCO2/MOVIES/SCENE0.4XM", "dois!");
    t.write("disc/AHFDATA/SO_DO_2.AHF", "x");
    nf::DataFileSystem fsys(t.root / "disc");
    Guest              g;
    gd::install(g.registry, fsys);
    auto opens = [&](const char* name) {
        g.str(0x8C100010, name);
        const uint32_t h = g.call(kA.open, 0x8C100010, 0);
        if (h) g.call(kA.close, h);
        return h != 0;
    };
    auto size = [&](const char* name) {
        g.str(0x8C100010, name);
        const uint32_t h = g.call(kA.open, 0x8C100010, 0);
        if (!h) return uint32_t(0);
        g.call(kA.getFileSize, h, 0x8C100100);
        g.call(kA.close, h);
        return g.u32(0x8C100100);
    };
    // variavel 0 (inicio) = disco 1
    HH_CHECK(opens("/DISC/disk1.id;1") && !opens("/DISC/disk2.id;1") && size("/MOVIES/Scene0.4xm") == 2);
    HH_CHECK(gd::current_disc() == 1 && opens("/AHFDATA/SO_DO_2.AHF") && !opens("/_DISCO2/MOVIES/SCENE0.4XM"));
    // o jogo passa para o disco 2 (0x8C02A974 grava 2): sem troca, a checagem acha o disco 2
    const uint32_t two = 2;
    std::memcpy(g.ram.data() + 0x15174C, &two, 4);
    HH_CHECK(!opens("/DISC/disk1.id;1") && opens("/DISC/disk2.id;1") && size("/MOVIES/Scene0.4xm") == 5);
    HH_CHECK(gd::current_disc() == 2 && !opens("/_DISCO2/MOVIES/SCENE0.4XM"));
    HH_CHECK(g.call(kA.driveStat) == 1);  // o drive nunca abre
}

// HH16.P1: LOAD de um save do outro disco. No console o menu de LOAD (0x8C0EEA20) so aceita um save do disco 2 com o
// console ligado pelo disco 2; com os discos juntos o boot e sempre pelo disco 1. O getter do disco (0x8C09EC68), so na
// chamada do menu de LOAD (retorno 0x8C0EEC34) e com o disco do save em r8, passa o jogo para o disco do save; as
// outras chamadas, um disco invalido, o mesmo disco e a instalacao de um disco so ficam como o original.
HH_TEST(hh16_load_save_other_disc) {
    TempTree t;
    t.write("disc/DISC/DISK1.ID", "1");
    t.write("disc/DISC/DISK2.ID", "2");
    t.write("disc/MOVIES/SCENE0.4XM", "um");
    t.write("disc/_DISCO2/MOVIES/SCENE0.4XM", "dois!");
    nf::DataFileSystem fsys(t.root / "disc");
    Guest              g;
    gd::install(g.registry, fsys);
    auto var  = [&] { return g.u32(kA.discVar); };
    auto setv = [&](uint32_t v) { std::memcpy(g.ram.data() + (kA.discVar & 0xFFFFFF), &v, 4); };
    auto get  = [&](uint32_t pr, uint32_t r8) {
        g.c.pr = pr, g.c.r[8] = r8, g.c.r[2] = 0;
        const uint64_t ciclos = g.c.cycles;
        const uint32_t r0     = g.call(kA.discGetter);
        HH_CHECK(g.c.r[2] == kA.discVar && g.c.r[8] == r8);  // r2 como o original; r8 do chamador intacto
        HH_CHECK(g.c.cycles - ciclos == 4);                    // o custo da gerada
        return r0;
    };
    auto size = [&] {
        g.str(0x8C100010, "/MOVIES/Scene0.4xm");
        const uint32_t h = g.call(kA.open, 0x8C100010, 0);
        if (!h) return uint32_t(0);
        g.call(kA.getFileSize, h, 0x8C100100);
        g.call(kA.close, h);
        return g.u32(0x8C100100);
    };
    g_legacyCalls = 0;
    setv(1);  // boot pelo disco 1 (0x8C09EBC4)
    // outras chamadas do getter: so devolvem a variavel
    HH_CHECK(get(0x8C0EEC48, 2) == 1 && var() == 1);
    HH_CHECK(get(0x8C09EC00, 2) == 1 && var() == 1);
    // disco do save invalido ou igual ao atual: o menu compara como o original
    HH_CHECK(get(kA.loadMenuDiscRet, 0) == 1 && get(kA.loadMenuDiscRet, 3) == 1 && var() == 1);
    HH_CHECK(get(kA.loadMenuDiscRet, 1) == 1 && var() == 1 && gd::stats().loadDiscSwitches == 0);
    HH_CHECK(size() == 2);
    // save do disco 2 no menu de LOAD: o jogo passa para o disco 2 e ve os arquivos dele
    HH_CHECK(get(kA.loadMenuDiscRet, 2) == 2 && var() == 2 && gd::stats().loadDiscSwitches == 1);
    HH_CHECK(size() == 5 && gd::current_disc() == 2);
    HH_CHECK(get(kA.loadMenuDiscRet, 2) == 2 && gd::stats().loadDiscSwitches == 1);  // ja no disco 2
    // e de volta: save do disco 1 carregado no disco 2
    HH_CHECK(get(kA.loadMenuDiscRet, 1) == 1 && var() == 1 && gd::stats().loadDiscSwitches == 2);
    HH_CHECK(size() == 2 && gd::current_disc() == 1);
    // variavel 0 (antes da checagem do boot) = disco 1: um save do disco 1 nao muda nada
    setv(0);
    HH_CHECK(get(kA.loadMenuDiscRet, 1) == 0 && var() == 0 && gd::stats().loadDiscSwitches == 2);
    HH_CHECK(g_legacyCalls == 0);  // o getter nunca foi ao legado
}

// HH16.P1: fim do disco 1 sem troca. O save do menu "End of Disc" (retorno 0x8C086C18) com o modo fim de disco ligado,
// a variavel em 2 e o save gravado desliga o modo e aplica a imagem salva pela funcao do LOAD (0x8C09C6C2, r4 = buffer);
// qualquer outra combinacao e so o save original.
HH_TEST(hh16_end_of_disc_continues_on_disc_2) {
    TempTree t;
    t.write("disc/DISC/DISK1.ID", "1");
    t.write("disc/DISC/DISK2.ID", "2");
    nf::DataFileSystem fsys(t.root / "disc");
    Guest              g;
    gd::install(g.registry, fsys);
    auto put  = [&](uint32_t a, uint32_t v) { std::memcpy(g.ram.data() + (a & 0xFFFFFF), &v, 4); };
    auto save = [&](uint32_t pr, uint32_t flag, uint32_t disc, uint32_t ret) {
        put(kA.endOfDiscFlag, flag), put(kA.discVar, disc);
        g_legacyRet = ret, g_legacyCalls = 0, g_legacyLog.clear();
        g.c.pr = pr, g.c.r[8] = 0xCAFE;
        const uint32_t r0 = g.call(kA.saveCall, 7);
        HH_CHECK(g.c.pr == pr && g.c.r[8] == 0xCAFE);
        return r0;
    };
    auto applied = [&] {
        return std::count_if(g_legacyLog.begin(), g_legacyLog.end(),
                             [](const auto& e) { return e.fn == kA.applySave && e.r4 == kA.saveImage; });
    };
    g.str(kA.saveImage + 76, "SXX_AQ_MAIN");
    // negativos: outra chamada do save, modo desligado, disco 1, save que falhou
    HH_CHECK(save(0x8C0EEBDA, 1, 2, 0x1234) == 0x1234 && g_legacyCalls == 1 && g.u32(kA.endOfDiscFlag) == 1);
    HH_CHECK(save(kA.saveEndRet, 0, 2, 0x1234) == 0x1234 && g_legacyCalls == 1);
    HH_CHECK(save(kA.saveEndRet, 1, 1, 0x1234) == 0x1234 && g_legacyCalls == 1 && g.u32(kA.endOfDiscFlag) == 1);
    HH_CHECK(save(kA.saveEndRet, 1, 2, 0) == 0 && g_legacyCalls == 1 && g.u32(kA.endOfDiscFlag) == 1);
    HH_CHECK(gd::stats().endOfDiscContinues == 0);
    // o caso: o save e a aplicacao da imagem, o modo desligado, o resultado do save devolvido ao menu; a chamada como o
    // HH_CALL da gerada (revisao Codex HH16.P1): pr e ret_expect = o retorno do save, ret_sp = r15, um poll no retorno,
    // e o contexto do chamador (ret_expect, ret_sp, cur_fn) restaurado
    g.c.ret_expect = 0xAAAA0001u, g.c.ret_sp = 0xBBBB0002u, g.c.cur_fn = 0xCCCC0003u, g.c.r[15] = 0x8C0FFF00u;
    g.c.next_poll = ~0ull;
    const uint64_t polls = g.c.polls;
    HH_CHECK(save(kA.saveEndRet, 1, 2, 0x1234) == 0x1234 && g_legacyCalls == 2 && applied() == 1);
    HH_CHECK(g.u32(kA.endOfDiscFlag) == 0 && g.u32(kA.discVar) == 2 && gd::stats().endOfDiscContinues == 1);
    HH_CHECK(g_legacyLog.size() == 2 && g_legacyLog[1].pr == kA.saveEndRet && g_legacyLog[1].retExpect == kA.saveEndRet &&
             g_legacyLog[1].retSp == 0x8C0FFF00u);
    HH_CHECK(g.c.ret_expect == 0xAAAA0001u && g.c.ret_sp == 0xBBBB0002u && g.c.cur_fn == 0xCCCC0003u);
    HH_CHECK(g.c.polls == polls + 1);
    g_legacyRet = 0x1234;
}

HH_TEST(hh16_end_of_disc_single_disc_keeps_original) {
    TempTree t;
    t.write("disc/DISC/DISK1.ID", "1");
    nf::DataFileSystem fsys(t.root / "disc");
    Guest              g;
    gd::install(g.registry, fsys);
    const uint32_t one = 1, two = 2;
    std::memcpy(g.ram.data() + (kA.endOfDiscFlag & 0xFFFFFF), &one, 4);
    std::memcpy(g.ram.data() + (kA.discVar & 0xFFFFFF), &two, 4);
    g_legacyCalls = 0, g_legacyLog.clear();
    g.c.pr = kA.saveEndRet;
    HH_CHECK(g.call(kA.saveCall) == 0x1234 && g_legacyCalls == 1 && g.u32(kA.endOfDiscFlag) == 1);
    HH_CHECK(gd::stats().endOfDiscContinues == 0);
}

// Instalacao com um disco so: nao ha outro disco para onde ir; o menu recusa como no console.
HH_TEST(hh16_load_save_other_disc_single_disc) {
    TempTree t;
    t.write("disc/DISC/DISK1.ID", "1");
    nf::DataFileSystem fsys(t.root / "disc");
    Guest              g;
    gd::install(g.registry, fsys);
    const uint32_t one = 1;
    std::memcpy(g.ram.data() + (kA.discVar & 0xFFFFFF), &one, 4);
    g.c.pr = kA.loadMenuDiscRet, g.c.r[8] = 2;
    HH_CHECK(g.call(kA.discGetter) == 1 && g.u32(kA.discVar) == 1 && gd::stats().loadDiscSwitches == 0);
}

// gdFsLoadDir com dirrec (revisao Codex HH08): a funcao original preenche o registro e o
// adaptador guarda o diretorio; o gdFsOpen(nome, dirrec) seguinte abre pelo provedor nativo,
// inclusive arquivo que so existe no disco 2. Dirrec desconhecido continua no legado.
HH_TEST(hh08_gdfs_dirrec_opens_native) {
    TempTree t;
    t.write("disc/STREAMS/SO_DO_2.ADX", "abcd");
    nf::DataFileSystem fsys(t.root / "disc");
    Guest              g;
    gd::install(g.registry, fsys);
    g_legacyCalls = 0;
    g.str(0x8C100000, "/Streams");
    g.call(kA.loadDir, 0x8C100000, 0x8C100400);
    HH_CHECK(g_legacyCalls == 1);  // o registro do guest foi preenchido pela funcao original
    g.str(0x8C100010, "so_do_2.adx");
    const uint32_t h = g.call(kA.open, 0x8C100010, 0x8C100400);
    HH_CHECK(gd::is_native_handle(h) && g.call(kA.getFileSize, h, 0x8C100100) == 1 && g.u32(0x8C100100) == 4);
    HH_CHECK(g.call(kA.open, 0x8C100010, 0x8C100800) == 0x1234 && g_legacyCalls == 2);  // dirrec desconhecido
    // loadDir sem dirrec = ChangeDir nativo
    g.str(0x8C100000, "/NADA");
    HH_CHECK(g.call(kA.loadDir, 0x8C100000, 0) == uint32_t(-1) && g_legacyCalls == 2);
}
