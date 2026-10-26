// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_bu_machine.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// N4: protocolo da bu sobre o ISaveStore (transitional/bu_machine.h). A referencia e a sequencia do
// oraculo (jogo original no Flycast com VMU, docs/planos/HH11-PLANO.md): eventos, codigos, progresso e
// geometria do LOAD. So amostras sinteticas.
#include "test.h"

#include "bu_machine.h"
#include "platform/saves.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace sv = hh::native::saves;
namespace bu = hh::transitional::bu;
using bu::Event;

namespace {

using Bytes = std::vector<std::byte>;

Bytes makeVms(uint32_t seed) {  // VMS do jogo: 4 blocos, CRC valido
    std::mt19937 r(seed);
    Bytes        v(0x80 + 0x200 + 1200);
    for (auto& b : v) b = std::byte(r());
    auto put16 = [&](size_t o, uint16_t x) { v[o] = std::byte(x), v[o + 1] = std::byte(x >> 8); };
    put16(0x40, 1), put16(0x42, 8), put16(0x44, 0), put16(0x46, 0);
    for (int i = 0; i < 4; ++i) v[0x48 + i] = std::byte(1200 >> (8 * i));
    put16(0x46, sv::vms_crc16(v));
    v.resize(2048);
    return v;
}

struct TempDir {
    fs::path p;
    explicit TempDir(const char* tag) {
        p = fs::temp_directory_path() / (std::string("hh-bu-") + tag + "-" + std::to_string(std::random_device{}()));
        fs::remove_all(p);
        fs::create_directories(p);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(p, ec); }
};

sv::FileStoreOptions opts() {
    sv::FileStoreOptions o;
    o.now = [] { return int64_t(1700000000); };
    return o;
}

// Roda passadas ate a unidade ficar livre; devolve os eventos e quantas consultas deram ocupada.
std::vector<Event> runUntilFree(bu::Machine& m, int drive, int* busyPolls = nullptr) {
    std::vector<Event> all;
    int                busy = 0;
    for (int i = 0; i < 100 && m.busy(drive); ++i) {
        if (m.status(drive) == bu::kBusy) ++busy;
        for (auto& e : m.tick()) all.push_back(std::move(e));
    }
    if (busyPolls) *busyPolls = busy;
    return all;
}

bool isProgressRun(const std::vector<Event>& ev, uint32_t op, uint32_t first, uint32_t last, uint32_t total) {
    size_t n = 0;
    for (const auto& e : ev)
        if (e.kind == Event::Kind::Progress) {
            if (e.op != op || e.a != first + n || e.b != total) return false;
            ++n;
        }
    return n == last - first + 1;
}

const Event* completion(const std::vector<Event>& ev) {
    return !ev.empty() && ev.back().kind == Event::Kind::Complete ? &ev.back() : nullptr;
}

} // namespace

// Boot do oraculo c1: insercao (op 0x12, 15 blocos de trabalho), montagem (op 1), seis exists FF05,
// status 0 e ultimo erro 0.
HH_TEST(bu_boot_sequence_matches_oracle) {
    TempDir     d("boot");
    auto        store = sv::make_file_store(d.p, opts());
    bu::Machine m(*store);
    m.init(3);
    HH_CHECK(m.busy(0) && !m.busy(1));  // so a unidade 0 tem cartao
    auto ins = runUntilFree(m, 0);
    HH_CHECK(isProgressRun(ins, bu::kOpInsert, 1, 2, 0));
    const Event* c = completion(ins);
    HH_CHECK(c && c->op == bu::kOpInsert && c->a == bu::kWorkBlocks && c->drive == 0);
    HH_CHECK(m.mount(0) == bu::kOk);
    auto mnt = runUntilFree(m, 0);
    HH_CHECK(isProgressRun(mnt, bu::kOpMount, 1, 0x10, 0x11));
    c = completion(mnt);
    HH_CHECK(c && c->op == bu::kOpMount && c->a == 0);
    for (int i = 0; i < 6; ++i) HH_CHECK(m.exists(0, "HHTESTE.00" + std::to_string(i)) == bu::kNotFound);
    HH_CHECK(m.status(0) == bu::kOk && m.last_error(0) == bu::kOk);
    HH_CHECK(m.mount(1) == bu::kNoCard);  // unidade 1 habilitada pela mascara, sem cartao
}

// SAVE do oraculo c2: aceito (0), consultas ocupadas, progresso 1..10/10, conclusao (0x0C, 0) com o
// arquivo ja no disco; depois status 0. Uma segunda operacao durante o ocupado e recusada.
HH_TEST(bu_save_then_load_matches_oracle) {
    TempDir     d("save");
    auto        store = sv::make_file_store(d.p, opts());
    bu::Machine m(*store);
    m.init(3);
    runUntilFree(m, 0);
    m.mount(0), runUntilFree(m, 0);
    const Bytes vms = makeVms(11);
    HH_CHECK(m.save(0, "H_HUNTER.SYS", vms) == bu::kOk);
    HH_CHECK(m.status(0) == bu::kBusy);
    HH_CHECK(m.load(0, "H_HUNTER.SYS", 1, 0) == bu::kBusy);  // pendente intacto
    HH_CHECK(m.save(0, "HHTESTE.001", vms) == bu::kBusy);
    int  busy = 0;
    auto ev   = runUntilFree(m, 0, &busy);
    HH_CHECK(busy >= 1 && busy < 450);
    HH_CHECK(isProgressRun(ev, bu::kOpSave, 1, 10, 10));
    const Event* c = completion(ev);
    HH_CHECK(c && c->op == bu::kOpSave && c->a == 0);
    HH_CHECK(m.status(0) == bu::kOk && m.last_error(0) == bu::kOk);
    Bytes back;
    HH_CHECK(store->load(0, "H_HUNTER.SYS", back) == sv::SaveError::None && back == vms);
    HH_CHECK(m.exists(0, "H_HUNTER.SYS") == bu::kOk && m.exists(0, "HHTESTE.001") == bu::kNotFound);
    // conferencia do jogo (bloco 1, 1 bloco) e LOAD completo (bloco 1, nblocos 0 = ate o fim)
    HH_CHECK(m.load(0, "H_HUNTER.SYS", 1, 1) == bu::kOk);
    ev = runUntilFree(m, 0);
    c  = completion(ev);
    HH_CHECK(isProgressRun(ev, bu::kOpLoad, 1, 1, 1) && c && c->a == 0 && c->data.size() == 512);
    HH_CHECK(c && std::memcmp(c->data.data(), vms.data() + 512, 512) == 0);
    HH_CHECK(m.load(0, "H_HUNTER.SYS", 1, 0) == bu::kOk);
    ev = runUntilFree(m, 0);
    c  = completion(ev);
    HH_CHECK(isProgressRun(ev, bu::kOpLoad, 1, 3, 3) && c && c->a == 0 && c->data.size() == 1536);
    HH_CHECK(c && std::memcmp(c->data.data(), vms.data() + 512, 1536) == 0);  // payload em +0x80
}

// disk_info: layout do oraculo e os livres que o jogo copia (200 -> 196 depois de um save de 4).
HH_TEST(bu_disk_info_tracks_free_blocks) {
    TempDir     d("info");
    auto        store = sv::make_file_store(d.p, opts());
    bu::Machine m(*store);
    m.init(1);
    runUntilFree(m, 0);
    std::array<std::byte, bu::kDiskInfoSize> di{};
    auto u16 = [&](size_t o) { return uint16_t(uint8_t(di[o]) | uint8_t(di[o + 1]) << 8); };
    HH_CHECK(m.disk_info(0, di) == bu::kOk);
    HH_CHECK(uint8_t(di[0]) == 1 && u16(32) == 256 && u16(34) == 200 && u16(36) == 200 && u16(38) == 200);
    HH_CHECK(u16(44) == 512 && u16(48) == 1998);
    m.save(0, "HHTESTE.003", makeVms(3)), runUntilFree(m, 0);
    HH_CHECK(m.disk_info(0, di) == bu::kOk && u16(34) == 200 && u16(36) == 196 && u16(38) == 196);
    m.save(0, "HHTESTE.003", makeVms(4)), runUntilFree(m, 0);  // sobrescrita nao consome blocos
    HH_CHECK(m.disk_info(0, di) == bu::kOk && u16(36) == 196);
}

// Persistencia: maquina e store novos (processo novo) acham o slot e o LOAD entrega o VMS gravado.
HH_TEST(bu_persistence_across_sessions) {
    TempDir     d("persist");
    const Bytes vms = makeVms(21);
    {
        auto        store = sv::make_file_store(d.p, opts());
        bu::Machine m(*store);
        m.init(1), runUntilFree(m, 0);
        m.save(0, "HHTESTE.005", vms), runUntilFree(m, 0);
    }
    auto        store = sv::make_file_store(d.p, opts());
    bu::Machine m(*store);
    m.init(1), runUntilFree(m, 0);
    HH_CHECK(m.exists(0, "HHTESTE.005") == bu::kOk && m.exists(0, "H_HUNTER.SYS") == bu::kNotFound);
    m.load(0, "HHTESTE.005", 1, 0);
    const auto   ev = runUntilFree(m, 0);
    const Event* c  = completion(ev);
    HH_CHECK(c && c->a == 0 && c->data.size() == 1536 && std::memcmp(c->data.data(), vms.data() + 512, 1536) == 0);
}

// Erros: LOAD de slot ausente conclui com FF05 sem dados; falha de escrita conclui com FF07 e o save
// anterior continua; sem cartao, o init nao insere e as chamadas devolvem FF01.
HH_TEST(bu_errors) {
    TempDir     d("erros");
    bool        failSwap = false;
    auto        o        = opts();
    o.fault              = [&](sv::FaultPoint p) { return failSwap && p == sv::FaultPoint::Swap; };
    auto        store    = sv::make_file_store(d.p, o);
    bu::Machine m(*store);
    m.init(1), runUntilFree(m, 0);
    m.load(0, "HHTESTE.002", 1, 0);
    auto         ev = runUntilFree(m, 0);
    const Event* c  = completion(ev);
    HH_CHECK(c && c->op == bu::kOpLoad && int32_t(c->a) == bu::kNotFound && c->data.empty());
    HH_CHECK(m.last_error(0) == bu::kNotFound);
    const Bytes first = makeVms(1);
    m.save(0, "H_HUNTER.SYS", first), runUntilFree(m, 0);
    failSwap = true;
    m.save(0, "H_HUNTER.SYS", makeVms(2));
    ev = runUntilFree(m, 0);
    c  = completion(ev);
    HH_CHECK(c && int32_t(c->a) == bu::kCannotOpen);
    for (const auto& e : ev) HH_CHECK(e.kind != Event::Kind::Progress);  // sem progresso de sucesso
    Bytes back;
    HH_CHECK(store->load(0, "H_HUNTER.SYS", back) == sv::SaveError::None && back == first);
    HH_CHECK(bu::result_code(sv::SaveError::NoCard) == bu::kNoCard &&
             bu::result_code(sv::SaveError::Unformatted) == bu::kUnformatted &&
             bu::result_code(sv::SaveError::NoSpace) == bu::kCannotOpen);
    // unidade sem cartao (o store tem 1 cartao)
    bu::Machine m2(*store);
    m2.init(2);
    HH_CHECK(!m2.busy(1) && m2.mount(1) == bu::kNoCard && m2.exists(1, "H_HUNTER.SYS") == bu::kNoCard);
    HH_CHECK(m2.status(9) == bu::kBusy);  // unidade invalida
}

// Ocupada e timeout: com operacao pendente, outra operacao na mesma unidade e recusada (-1) sem perder a
// primeira; SAVE e LOAD concluem bem antes do limite do jogo (no BS, 8C01CE00 desiste depois de 450 consultas
// ocupadas) e a conclusao vem antes do primeiro status 0, com o callback antes da liberacao.
HH_TEST(bu_busy_and_timeout_bound) {
    TempDir     d("ocupada");
    auto        store = sv::make_file_store(d.p, opts());
    bu::Machine m(*store);
    m.init(1), runUntilFree(m, 0);
    HH_CHECK(m.save(0, "H_HUNTER.SYS", makeVms(5)) == bu::kOk && m.busy(0));
    HH_CHECK(m.save(0, "HHTESTE.001", makeVms(6)) == bu::kBusy);
    HH_CHECK(m.load(0, "H_HUNTER.SYS", 1, 0) == bu::kBusy && m.mount(0) == bu::kBusy);
    int polls = 0;
    auto ev = runUntilFree(m, 0, &polls);
    const Event* c = completion(ev);
    HH_CHECK(c && c->op == bu::kOpSave && c->a == 0 && polls >= 1 && polls < 450);
    HH_CHECK(m.status(0) == 0);
    Bytes img;
    HH_CHECK(store->load(0, "HHTESTE.001", img) == sv::SaveError::NotFound);  // a recusada nao gravou
    HH_CHECK(m.load(0, "H_HUNTER.SYS", 1, 0) == bu::kOk);
    ev = runUntilFree(m, 0, &polls);
    c  = completion(ev);
    HH_CHECK(c && c->op == bu::kOpLoad && c->a == 0 && polls < 450 && c->data.size() == 1536);
}

// Ocupada durante os progressos (oraculo c2: status -1 intercalado com o progresso, 0 so depois da
// conclusao): com hold, a unidade continua -1 ate release(); o adaptador libera logo antes do callback
// de conclusao.
HH_TEST(bu_busy_until_release_with_hold) {
    TempDir     d("hold");
    auto        store = sv::make_file_store(d.p, opts());
    bu::Machine m(*store);
    m.init(1);
    for (int i = 0; i < 10 && m.busy(0); ++i) m.tick(true), m.release(0);
    HH_CHECK(m.save(0, "H_HUNTER.SYS", makeVms(7)) == bu::kOk);
    std::vector<Event> ev;
    for (int i = 0; i < 10 && ev.empty(); ++i) ev = m.tick(true);
    HH_CHECK(!ev.empty() && ev.front().kind == Event::Kind::Progress && completion(ev));
    HH_CHECK(m.status(0) == bu::kBusy && m.save(0, "HHTESTE.001", makeVms(8)) == bu::kBusy);  // durante os progressos
    m.release(0);
    HH_CHECK(m.status(0) == bu::kOk);
    // sem release, a proxima passada libera (nao prende a unidade)
    HH_CHECK(m.load(0, "H_HUNTER.SYS", 1, 0) == bu::kOk);
    ev.clear();
    for (int i = 0; i < 10 && ev.empty(); ++i) ev = m.tick(true);
    m.tick(true);
    HH_CHECK(m.status(0) == bu::kOk);
}

// Cartao presente sem formatacao: as operacoes devolvem FF03 (nao FF01).
HH_TEST(bu_unformatted_card) {
    struct Unformatted final : sv::ISaveStore {
        sv::SaveError card(int, sv::CardState& c) override { c = {}, c.present = true; return sv::SaveError::None; }
        sv::SaveError list(int, std::vector<sv::SaveEntry>&) override { return sv::SaveError::Unformatted; }
        sv::SaveError load(int, std::string_view, std::vector<std::byte>&) override { return sv::SaveError::Unformatted; }
        sv::SaveError save_atomic(int, std::string_view, std::span<const std::byte>, std::optional<int64_t>) override {
            return sv::SaveError::Unformatted;
        }
        sv::SaveError remove(int, std::string_view) override { return sv::SaveError::Unformatted; }
    } store;
    bu::Machine m(store);
    m.init(1);
    const auto ins = runUntilFree(m, 0);  // presente: a insercao acontece (o jogo aloca o trabalho nela)
    const Event* c = completion(ins);
    HH_CHECK(c && c->op == bu::kOpInsert);
    std::array<std::byte, bu::kDiskInfoSize> info{};
    HH_CHECK(!m.busy(0) && m.mount(0) == bu::kUnformatted && m.exists(0, "H_HUNTER.SYS") == bu::kUnformatted);
    HH_CHECK(m.disk_info(0, info) == bu::kUnformatted && m.save(0, "H_HUNTER.SYS", makeVms(1)) == bu::kUnformatted);
    HH_CHECK(m.load(0, "H_HUNTER.SYS", 1, 0) == bu::kUnformatted);
}

// HH11 (oraculo c2 do Flycast): op 0x0B le o arquivo inteiro no destino, progresso 1..n de n blocos e conclusao 0;
// sem o arquivo, conclusao "nao encontrado" sem dados.
HH_TEST(bu_load_whole_file_op0b_matches_hh_oracle) {
    TempDir     d("op0b");
    auto        store = sv::make_file_store(d.p, opts());
    bu::Machine m(*store);
    m.init(0xFF);
    runUntilFree(m, 0);
    m.mount(0), runUntilFree(m, 0);
    const Bytes vms = makeVms(5);
    HH_CHECK(m.save(0, "H_HUNTER.SYS", vms) == bu::kOk);
    runUntilFree(m, 0);
    HH_CHECK(m.load(0, "H_HUNTER.SYS", 0, 0, bu::kOpLoadFile) == bu::kOk);
    auto         ev = runUntilFree(m, 0);
    const Event* c  = completion(ev);
    HH_CHECK(isProgressRun(ev, bu::kOpLoadFile, 1, 4, 4) && c && c->op == bu::kOpLoadFile && c->a == 0 && c->data == vms);
    HH_CHECK(m.load(0, "H_HUNTER.001", 0, 0, bu::kOpLoadFile) == bu::kOk);
    ev = runUntilFree(m, 0);
    c  = completion(ev);
    HH_CHECK(c && c->op == bu::kOpLoadFile && int32_t(c->a) == bu::kNotFound && c->data.empty());
}

// HH11: data do bu (BuTime). As duas amostras medidas: o argumento de tempo do SAVE com --rtc 1700000000
// (2023-11-14 22:13:43, terca = 2) e a data do file_info no oraculo c3 (2026-10-25 19:16:42, domingo = 0).
HH_TEST(bu_time_conversions_match_samples) {
    const uint8_t a[8] = {0xE7, 0x07, 11, 14, 22, 13, 43, 2}, b[8] = {0xEA, 0x07, 10, 25, 19, 16, 42, 0};
    for (const uint8_t* t : {a, b}) {
        uint8_t back[8];
        bu::unix_to_bu_time(bu::bu_time_to_unix(t), back);
        HH_CHECK(std::memcmp(back, t, 8) == 0);  // inclusive o dia da semana
    }
    HH_CHECK(bu::bu_time_to_unix(a) == 1700000023);  // 1700000000 = 2023-11-14 22:13:20
    uint8_t epoch[8];
    bu::unix_to_bu_time(0, epoch);
    HH_CHECK(epoch[0] == 0xB2 && epoch[1] == 0x07 && epoch[2] == 1 && epoch[3] == 1 && epoch[7] == 4);  // quinta
}

// HH11 (oraculo c3): file_info do arquivo gravado: tamanho em bytes, blocos, campos de dados zerados e a data que
// o jogo passou ao SAVE; sem o arquivo, nao encontrado.
HH_TEST(bu_file_info_matches_hh_oracle) {
    TempDir     d("fileinfo");
    auto        store = sv::make_file_store(d.p, opts());
    bu::Machine m(*store);
    m.init(0xFF);
    runUntilFree(m, 0);
    m.mount(0), runUntilFree(m, 0);
    const uint8_t when[8] = {0xEA, 0x07, 10, 25, 19, 16, 42, 0};
    HH_CHECK(m.save(0, "H_HUNTER.SYS", makeVms(3), bu::bu_time_to_unix(when)) == bu::kOk);
    runUntilFree(m, 0);
    std::array<std::byte, bu::kFileInfoSize> info{};
    HH_CHECK(m.file_info(0, "H_HUNTER.SYS", info) == bu::kOk);
    const uint8_t want[18] = {0x00, 0x08, 0, 0, 4, 0, 0, 0, 0, 0, 0xEA, 0x07, 10, 25, 19, 16, 42, 0};  // 2048 bytes, 4 blocos
    HH_CHECK(std::memcmp(info.data(), want, 18) == 0);
    HH_CHECK(m.file_info(0, "H_HUNTER.001", info) == bu::kNotFound);
}
