// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_saves_native.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH11: saves nativos (native/platform/saves.h): formato VMS e CRC, envelope, armazenamento atomico,
// persistencia, erros, falhas de escrita e recuperacao. So amostras sinteticas (nenhum byte do jogo).
#include "test.h"

#include "platform/saves.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace sv = hh::native::saves;
using sv::SaveError;

namespace {

using Bytes = std::vector<std::byte>;

// VMS com o layout do save do jogo (cabecalho, 1 icone, payload de 1 200, zeros ate 2 048) e
// conteudo sintetico
Bytes makeVms(uint32_t seed, uint32_t payload = 1200) {
    std::mt19937 r(seed);
    Bytes        v(0x80 + 0x200 + payload);
    for (auto& b : v) b = std::byte(r());
    std::memcpy(v.data(), "SINTETICO       ", 16);
    auto put16 = [&](size_t o, uint16_t x) { v[o] = std::byte(x); v[o + 1] = std::byte(x >> 8); };
    put16(0x40, 1), put16(0x42, 0), put16(0x44, 0), put16(0x46, 0);
    for (int i = 0; i < 4; ++i) v[0x48 + i] = std::byte(payload >> (8 * i));
    put16(0x46, sv::vms_crc16(v));
    v.resize((v.size() + 511) / 512 * 512);
    return v;
}

Bytes text(const char* s) {
    Bytes v(std::strlen(s));
    std::memcpy(v.data(), s, v.size());
    return v;
}

struct TempDir {
    fs::path p;
    explicit TempDir(const char* tag) {
        p = fs::temp_directory_path() / (std::string("hh-saves-") + tag + "-" + std::to_string(std::random_device{}()));
        fs::remove_all(p);
        fs::create_directories(p);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(p, ec); }
};

Bytes readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::vector<char> c((std::istreambuf_iterator<char>(f)), {});
    Bytes b(c.size());
    if (!c.empty()) std::memcpy(b.data(), c.data(), c.size());
    return b;
}
void writeFile(const fs::path& p, const Bytes& b) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}

sv::FileStoreOptions fixedClock() {
    sv::FileStoreOptions o;
    o.now = [] { return int64_t(1700000000); };
    return o;
}

} // namespace

// CRC-16/XMODEM (o da rotina 8C160B30) e o layout do VMS do jogo: 2 048 bytes, payload em 0x280.
HH_TEST(saves_vms_crc_and_layout) {
    const Bytes nine = text("123456789");
    HH_CHECK(sv::vms_crc16(nine) == 0x31C3);
    const Bytes v = makeVms(7);
    std::string why;
    HH_CHECK(v.size() == 2048 && sv::vms_check(v, &why));
    for (size_t i = 0x730; i < v.size(); ++i) HH_CHECK(v[i] == std::byte{0});
    Bytes w = v;
    w[0x300] ^= std::byte{1};
    HH_CHECK(!sv::vms_check(w));  // payload alterado: CRC acusa
    w = v, w[0x7FF] = std::byte{1};
    HH_CHECK(!sv::vms_check(w));  // preenchimento nao zero
    w.assign(v.begin(), v.begin() + 1536);
    HH_CHECK(!sv::vms_check(w));  // truncado
    w = v, w[0x44] = std::byte{4};
    HH_CHECK(!sv::vms_check(w));  // eyecatch invalido
    w = v, w[0x40] = std::byte{0};
    HH_CHECK(!sv::vms_check(w));  // sem icone
}

HH_TEST(saves_names) {
    for (const char* ok : {"H_HUNTER.SYS", "HHTESTE.005", "A", "SAVE_1.BIN"}) HH_CHECK(sv::valid_save_name(ok));
    for (const char* bad : {"", "h_hunter.sys", "..\\X", "../X", "A..B", ".HHSV", "H_HUNTER.SYS0X", "A/B", "A B", "C:X"})
        HH_CHECK(!sv::valid_save_name(bad));
}

// Envelope v1: ida e volta byte a byte; recusa de magic, versao futura, tamanhos, checksum, truncado.
HH_TEST(saves_envelope) {
    const Bytes v   = makeVms(3);
    const Bytes env = sv::envelope_encode(v, 1234567890123);
    HH_CHECK(env.size() == 32 + 2048);
    Bytes   back;
    int64_t t = 0;
    HH_CHECK(sv::envelope_decode(env, back, &t) == SaveError::None && back == v && t == 1234567890123);
    Bytes e = env;
    e[0] = std::byte{'X'};
    HH_CHECK(sv::envelope_decode(e, back) == SaveError::Corrupt);
    e = env, e[4] = std::byte{2};
    HH_CHECK(sv::envelope_decode(e, back) == SaveError::FutureVersion);
    e = env, e[4] = std::byte{0};
    HH_CHECK(sv::envelope_decode(e, back) == SaveError::Corrupt);
    e = env, e[40] ^= std::byte{0x80};
    HH_CHECK(sv::envelope_decode(e, back) == SaveError::Corrupt);  // imagem alterada: CRC-32
    e.assign(env.begin(), env.end() - 1);
    HH_CHECK(sv::envelope_decode(e, back) == SaveError::Corrupt);  // truncado
    e = env, e[8] = std::byte{0x01};  // tamanho da imagem 0x801: fora do multiplo de 512
    HH_CHECK(sv::envelope_decode(e, back) == SaveError::Corrupt);
    HH_CHECK(sv::envelope_decode(Bytes(10), back) == SaveError::Corrupt);
}

// Seis slots: gravar, listar, carregar, sobrescrever (sem consumir blocos), persistir numa nova
// abertura do store; estado do cartao coerente em cada passo.
HH_TEST(saves_store_six_slots_and_persistence) {
    TempDir d("slots");
    {
        auto s = sv::make_file_store(d.p, fixedClock());
        sv::CardState c;
        HH_CHECK(s->card(0, c) == SaveError::None && c.present && c.formatted && !c.read_only);
        HH_CHECK(c.total_blocks == 200 && c.free_blocks == 200 && c.free_entries == c.total_entries);
        Bytes img;
        HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::NotFound);
        for (int i = 0; i < 6; ++i) HH_CHECK(s->save_atomic(0, "HHTESTE.00" + std::to_string(i), makeVms(i)) == SaveError::None);
        HH_CHECK(s->card(0, c) == SaveError::None && c.free_blocks == 200 - 24 && c.free_entries == c.total_entries - 6);
        std::vector<sv::SaveEntry> l;
        HH_CHECK(s->list(0, l) == SaveError::None && l.size() == 6);
        for (const auto& e : l) HH_CHECK(e.blocks == 4 && e.modified_unix == 1700000000);
        HH_CHECK(s->save_atomic(0, "HHTESTE.002", makeVms(99)) == SaveError::None);  // sobrescrita
        HH_CHECK(s->card(0, c) == SaveError::None && c.free_blocks == 176);
        HH_CHECK(fs::exists(d.p / "card0" / "HHTESTE.002.hhsv.bak"));
    }
    auto s = sv::make_file_store(d.p, fixedClock());  // outro processo, na pratica
    for (int i = 0; i < 6; ++i) {
        Bytes img;
        HH_CHECK(s->load(0, "HHTESTE.00" + std::to_string(i), img) == SaveError::None);
        HH_CHECK(img == makeVms(i == 2 ? 99 : uint32_t(i)));
    }
    for (const auto& e : fs::directory_iterator(d.p / "card0")) HH_CHECK(e.path().extension() != ".tmp");
}

HH_TEST(saves_store_errors) {
    TempDir d("erros");
    auto    s = sv::make_file_store(d.p, fixedClock());
    Bytes   img;
    sv::CardState c;
    HH_CHECK(s->card(1, c) == SaveError::NoCard && !c.present);  // um cartao so (unidade 0)
    HH_CHECK(s->save_atomic(1, "H_HUNTER.SYS", makeVms(1)) == SaveError::NoCard);
    HH_CHECK(s->save_atomic(0, "../H_HUNTER.SYS", makeVms(1)) == SaveError::BadName);
    HH_CHECK(s->load(0, "..\\..\\X", img) == SaveError::BadName);
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", Bytes(100)) == SaveError::BadImage);
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", Bytes()) == SaveError::BadImage);
    HH_CHECK(s->remove(0, "H_HUNTER.SYS") == SaveError::NotFound);
    // cheio: 197 blocos ocupados, um save de 4 nao cabe; sobrescrever o grande com o mesmo tamanho cabe
    HH_CHECK(s->save_atomic(0, "GRANDE", makeVms(20, 197 * 512 - 0x280)) == SaveError::None);
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(1)) == SaveError::NoSpace);
    HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::NotFound);
    HH_CHECK(s->save_atomic(0, "GRANDE", makeVms(21, 197 * 512 - 0x280)) == SaveError::None);
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", Bytes(2048)) == SaveError::BadImage);  // nao e VMS
    HH_CHECK(s->remove(0, "GRANDE") == SaveError::None && s->card(0, c) == SaveError::None && c.free_blocks == 200);
    // pasta do cartao apagada por fora: sem cartao (nada recriado no meio da sessao)
    fs::remove_all(d.p / "card0");
    HH_CHECK(s->card(0, c) == SaveError::NoCard);
}

// Falha em cada ponto da escrita atomica: o save anterior sobrevive, nenhum temporario fica, e uma
// nova abertura do store ve o anterior.
HH_TEST(saves_store_write_faults_keep_previous) {
    for (sv::FaultPoint p : {sv::FaultPoint::CreateTemp, sv::FaultPoint::Write, sv::FaultPoint::Flush,
                             sv::FaultPoint::Reread, sv::FaultPoint::Swap}) {
        TempDir d("falha");
        bool    armed = false;
        sv::FileStoreOptions o = fixedClock();
        o.fault = [&](sv::FaultPoint q) { return armed && q == p; };
        auto s = sv::make_file_store(d.p, o);
        HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(1)) == SaveError::None);
        armed = true;
        HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(2)) == SaveError::Io);
        HH_CHECK(s->save_atomic(0, "HHTESTE.001", makeVms(3)) == SaveError::Io);  // novo: nao aparece
        Bytes img;
        HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::None && img == makeVms(1));
        HH_CHECK(s->load(0, "HHTESTE.001", img) == SaveError::NotFound);
        for (const auto& e : fs::directory_iterator(d.p / "card0")) HH_CHECK(e.path().extension() != ".tmp");
        armed = false;
        auto s2 = sv::make_file_store(d.p, fixedClock());
        HH_CHECK(s2->load(0, "H_HUNTER.SYS", img) == SaveError::None && img == makeVms(1));
    }
}

// Abertura depois de queda: .tmp descartado; destino corrompido volta pelo .bak; os dois invalidos
// viram .corrompido (preservados) e o slot fica vazio; versao futura e recusada e preservada.
HH_TEST(saves_store_recovery_on_open) {
    TempDir d("recupera");
    {
        auto s = sv::make_file_store(d.p, fixedClock());
        HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(1)) == SaveError::None);
        HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(2)) == SaveError::None);  // .bak = 1
        HH_CHECK(s->save_atomic(0, "HHTESTE.001", makeVms(3)) == SaveError::None);
        HH_CHECK(s->save_atomic(0, "HHTESTE.003", makeVms(4)) == SaveError::None);
    }
    const fs::path c0 = d.p / "card0";
    writeFile(c0 / "H_HUNTER.SYS.hhsv", text("lixo"));                              // destino corrompido
    writeFile(c0 / "HHTESTE.004.hhsv.tmp", sv::envelope_encode(makeVms(5), 1));   // queda antes da troca
    writeFile(c0 / "HHTESTE.001.hhsv", text("lixo"));                              // sem .bak
    Bytes fut = sv::envelope_encode(makeVms(6), 1);
    fut[4] = std::byte{9};
    writeFile(c0 / "HHTESTE.003.hhsv", fut);                                       // versao futura
    std::string log;
    sv::FileStoreOptions o = fixedClock();
    o.log = [&](const std::string& m) { log += m + "\n"; };
    auto  s = sv::make_file_store(d.p, o);
    Bytes img;
    HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::None && img == makeVms(1));
    HH_CHECK(log.find("recuperado") != std::string::npos);
    HH_CHECK(s->load(0, "HHTESTE.004", img) == SaveError::NotFound && !fs::exists(c0 / "HHTESTE.004.hhsv.tmp"));
    HH_CHECK(s->load(0, "HHTESTE.001", img) == SaveError::NotFound && fs::exists(c0 / "HHTESTE.001.hhsv.corrompido"));
    HH_CHECK(fs::exists(c0 / "H_HUNTER.SYS.hhsv.corrompido"));
    // versao futura: intocada, recusada no LOAD e nunca sobrescrita
    HH_CHECK(s->load(0, "HHTESTE.003", img) == SaveError::FutureVersion);
    HH_CHECK(s->save_atomic(0, "HHTESTE.003", makeVms(7)) == SaveError::FutureVersion);
    HH_CHECK(readFile(c0 / "HHTESTE.003.hhsv") == fut && !fs::exists(c0 / "HHTESTE.003.hhsv.corrompido"));
}

// VMS puro: exportar da o arquivo de dados exato; importar so aceita VMS integro.
HH_TEST(saves_vms_import_export) {
    TempDir d("vms");
    auto    s = sv::make_file_store(d.p / "saves", fixedClock());
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(8)) == SaveError::None);
    HH_CHECK(sv::export_vms(*s, 0, "H_HUNTER.SYS", d.p / "x.vms") == SaveError::None);
    HH_CHECK(readFile(d.p / "x.vms") == makeVms(8));
    HH_CHECK(sv::import_vms(*s, 0, "HHTESTE.005", d.p / "x.vms") == SaveError::None);
    Bytes img;
    HH_CHECK(s->load(0, "HHTESTE.005", img) == SaveError::None && img == makeVms(8));
    Bytes bad = makeVms(8);
    bad[0x300] ^= std::byte{1};
    writeFile(d.p / "ruim.vms", bad);
    HH_CHECK(sv::import_vms(*s, 0, "HHTESTE.004", d.p / "ruim.vms") == SaveError::BadImage);
    HH_CHECK(sv::import_vms(*s, 0, "HHTESTE.004", d.p / "nao_existe.vms") == SaveError::NotFound);
    HH_CHECK(s->load(0, "HHTESTE.004", img) == SaveError::NotFound);
}

// Permissao: destino somente leitura (atributo do Windows) recusa a gravacao sem perder o save, sem
// temporario sobrando; com a permissao de volta, grava normalmente.
HH_TEST(saves_store_read_only_destination) {
    TempDir d("somente_leitura");
    auto    s = sv::make_file_store(d.p, fixedClock());
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(1)) == SaveError::None);
    const fs::path dst = d.p / "card0" / "H_HUNTER.SYS.hhsv";
    fs::permissions(dst, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove);
    const SaveError e = s->save_atomic(0, "H_HUNTER.SYS", makeVms(2));
    HH_CHECK(e == SaveError::ReadOnly);
    Bytes img;
    HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::None && img == makeVms(1));
    for (const auto& f : fs::directory_iterator(d.p / "card0")) HH_CHECK(f.path().extension() != ".tmp");
    fs::permissions(dst, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::add);
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(2)) == SaveError::None);
    HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::None && img == makeVms(2));
}

// Gravacao so de VMS integro: CRC-16 do jogo adulterado (campo 0x46 ou o payload) e recusado sem tocar
// no save anterior.
HH_TEST(saves_store_rejects_bad_vms_crc) {
    TempDir d("crc");
    auto    s = sv::make_file_store(d.p, fixedClock());
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(1)) == SaveError::None);
    Bytes bad = makeVms(2);
    bad[0x300] ^= std::byte{0x40};
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", bad) == SaveError::BadImage);
    bad = makeVms(2);
    bad[0x46] ^= std::byte{1};
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", bad) == SaveError::BadImage);
    Bytes img;
    HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::None && img == makeVms(1));
}

// Versao futura reconhecida antes do tamanho de cabecalho desta versao: um arquivo de formato mais novo
// com cabecalho curto continua intocado (nao vira .corrompido) e nao e sobrescrito.
HH_TEST(saves_future_version_short_header) {
    Bytes f = text("HHSV");
    f.push_back(std::byte{9}), f.push_back(std::byte{0}), f.push_back(std::byte{7});
    Bytes img;
    HH_CHECK(sv::envelope_decode(f, img) == SaveError::FutureVersion);
    HH_CHECK(sv::envelope_decode(text("HHSV"), img) == SaveError::Corrupt);
    TempDir d("futura_curta");
    fs::create_directories(d.p / "card0");
    writeFile(d.p / "card0" / "H_HUNTER.SYS.hhsv", f);
    auto s = sv::make_file_store(d.p, fixedClock());
    HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::FutureVersion);
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", makeVms(3)) == SaveError::FutureVersion);
    HH_CHECK(readFile(d.p / "card0" / "H_HUNTER.SYS.hhsv") == f && !fs::exists(d.p / "card0" / "H_HUNTER.SYS.hhsv.corrompido"));
}

// HH11: leiaute do save do Head Hunter (medido no oraculo do Flycast, H_HUNTER.SYS de 6 blocos): campo 0x48 =
// bytes cobertos pelo CRC (cabecalho + 1 icone + eyecatch tipo 3 + dados) e o resto da imagem com o que estava
// no buffer. Imagem sintetica com o mesmo leiaute (nenhum byte do jogo).
HH_TEST(saves_vms_headhunter_layout) {
    const uint32_t covered = 0x80 + 0x200 + (32 + 72 * 56 / 2) + 60;  // 2748
    Bytes          v(6 * 512);
    std::mt19937   r(7);
    for (auto& b : v) b = std::byte(r());  // inclusive depois do trecho coberto (nao zerado)
    auto put16 = [&](size_t o, uint16_t x) { v[o] = std::byte(x); v[o + 1] = std::byte(x >> 8); };
    put16(0x40, 1), put16(0x42, 1), put16(0x44, 3), put16(0x46, 0);
    for (int i = 0; i < 4; ++i) v[0x48 + i] = std::byte(covered >> (8 * i));
    put16(0x46, sv::vms_crc16(std::span(v).first(covered)));
    std::string why;
    HH_CHECK(sv::vms_check(v, &why));
    Bytes w = v;
    w[100] ^= std::byte{1};  // byte coberto pelo CRC
    HH_CHECK(!sv::vms_check(w));
    w = v;
    w[covered + 10] ^= std::byte{1};  // fora do trecho coberto: continua valido
    HH_CHECK(sv::vms_check(w));
    w = v;
    w.resize(w.size() + 512);  // bloco a mais
    HH_CHECK(!sv::vms_check(w));
    TempDir d("hh_leiaute");
    auto    s = sv::make_file_store(d.p);
    HH_CHECK(s->save_atomic(0, "H_HUNTER.SYS", v) == SaveError::None);
    Bytes img;
    HH_CHECK(s->load(0, "H_HUNTER.SYS", img) == SaveError::None && img == v);
}

// HH11 (revisao Codex): na importacao, uma imagem que so passa pelo leiaute do Head Hunter precisa da descricao longa
// "HEADHUNTER"; o leiaute padrao continua aceito como antes.
HH_TEST(saves_import_requires_headhunter_identity) {
    const uint32_t covered = 0x80 + 0x200 + (32 + 72 * 56 / 2) + 60;
    auto make = [&](const char* longDesc) {
        Bytes        v(6 * 512);
        std::mt19937 r(9);
        for (auto& b : v) b = std::byte(r());
        std::memset(v.data() + 0x10, ' ', 32);
        std::memcpy(v.data() + 0x10, longDesc, std::strlen(longDesc));
        auto put16 = [&](size_t o, uint16_t x) { v[o] = std::byte(x); v[o + 1] = std::byte(x >> 8); };
        put16(0x40, 1), put16(0x42, 1), put16(0x44, 3), put16(0x46, 0);
        for (int i = 0; i < 4; ++i) v[0x48 + i] = std::byte(covered >> (8 * i));
        put16(0x46, sv::vms_crc16(std::span(v).first(covered)));
        return v;
    };
    TempDir d("importa_id");
    auto    s = sv::make_file_store(d.p);
    for (const auto& [desc, want] : {std::pair{"HEADHUNTER", SaveError::None}, std::pair{"OUTRO JOGO", SaveError::BadImage}}) {
        const Bytes v = make(desc);
        HH_CHECK(sv::vms_layout(v) == sv::VmsLayout::HeadHunter);
        writeFile(d.p / "x.vms", v);
        HH_CHECK(sv::import_vms(*s, 0, "H_HUNTER.SYS", d.p / "x.vms") == want);
    }
    writeFile(d.p / "p.vms", makeVms(4));  // leiaute padrao, descricao qualquer
    HH_CHECK(sv::vms_layout(makeVms(4)) == sv::VmsLayout::Standard);
    HH_CHECK(sv::import_vms(*s, 0, "HHTESTE.001", d.p / "p.vms") == SaveError::None);
}
