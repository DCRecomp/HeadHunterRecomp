// SPDX-License-Identifier: MIT
// HH15.10: a infraestrutura da elevacao (transitional/elevacao) sobre "geradas" de teste nos slots dos enderecos das
// elevadas: a instalacao recusa elevada sem slot; o modo ativo chama a elevada (resultado e custo do caminho original);
// as premissas do literal pool sao conferidas; o modo conferir aceita uma gerada equivalente e acusa, pelo motivo
// certo, a gerada que escreve a mais na RAM, que escreve outro valor, que custa outros ciclos ou outros back-edges, que
// devolve outro r0, que muda macl, e a elevada que chegaria sozinha a um atendimento; uma chamada em que a gerada e
// atendida so conta; um erro no meio do ensaio o encerra (diario desfeito, ensaio desligado).
#include "test.h"

#include "cpu/sh4_mem.h"
#include "elevacao.h"
#include "game/angulo.h"
#include "mods/mod_api.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ev = hh::transitional::elevacao;
namespace ng = hh::native::game;

namespace {

constexpr uint32_t kAngulo = 0x8C06F62Cu, kGlobal = 0x8C073980u, kPalavra = 0x8C14B61Cu;
constexpr uint32_t kDado = 0x8C200000u;  // o angulo

struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// mutacao da "gerada" de teste
enum class Mut { Nenhuma, EscritaExtra, OutroValor, SoNaRam, Ciclos, Voltas, Retorno, Macl, SemVolta, SoNoBackEdge, RamNoBackEdge };
Mut g_mut = Mut::Nenhuma;

float lerf(hh::Sh4Context* c, uint32_t a) {
    const uint32_t u = hh::sh4_read32(c, a);
    float          f;
    std::memcpy(&f, &u, 4);
    return f;
}
void escf(hh::Sh4Context* c, uint32_t a, float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    hh::sh4_write32(c, a, u);
}

// back-edge da gerada de teste: o HH_POLL (a retomada e o caminho lento quando o contador pede)
void backEdge(hh::Sh4Context* c, uint32_t retomada) {
    if (g_mut == Mut::SemVolta) return;
    ++c->polls;
    // revisao Codex HH15.11b5: o angulo diferente na RAM so durante o back-edge (registradores e valor final iguais)
    const uint32_t p = c->r[3], salvo = g_mut == Mut::RamNoBackEdge ? hh::sh4_read32(c, p) : 0;
    if (g_mut == Mut::RamNoBackEdge) hh::sh4_write32(c, p, salvo ^ 0x00800000u);
    if (g_mut == Mut::SoNoBackEdge) c->r[3] ^= 1;  // um temporario diferente so no back-edge (restaurado depois)
    if (c->cycles >= c->next_event || c->polls >= c->next_poll) {
        c->pc = retomada;
        hh::sh4_poll_slow(c);
    }
    if (g_mut == Mut::RamNoBackEdge) hh::sh4_write32(c, p, salvo);
    if (g_mut == Mut::SoNoBackEdge) c->r[3] ^= 1;
}

// o que a gerada de 8C06F62C faz (custo, back-edges, o quadro de 4 bytes, escrita a cada volta, e os registradores de
// cada teste: r3 = p, r0 = a constante (mova), fr3 = ela, fr2 = o angulo lido), com as mutacoes
void gerada_angulo(hh::Sh4Context* c) {
    c->cycles += 4;
    c->r[15] -= 4;
    hh::sh4_write32(c, c->r[15], c->r[4]);
    const uint32_t p = c->r[4];
    c->r[3]          = p;
    auto teste       = [&](uint32_t k) {
        c->r[0] = k, c->fr[3] = hh::sh4_read32(c, k), c->fr[2] = hh::sh4_read32(c, p);
        return lerf(c, p);
    };
    auto corpo = [&](float sinal) {
        c->cycles += 6;
        c->r[0] = 0x8C06F670u, c->fr[3] = hh::sh4_read32(c, 0x8C06F670u);
        escf(c, p, lerf(c, p) + sinal * ng::kVolta);
        c->fr[2] = hh::sh4_read32(c, p);
        c->cycles += 6;
    };
    c->cycles += 6;
    while (teste(0x8C06F674u) < -ng::kPi) c->t = 1, backEdge(c, 0x8C06F634u), corpo(1);
    c->cycles += 8;
    while (teste(0x8C06F678u) > ng::kPi) c->t = 1, backEdge(c, 0x8C06F650u), corpo(-1);
    if (g_mut == Mut::OutroValor) escf(c, p, lerf(c, p) * 0.5f), c->fr[2] = hh::sh4_read32(c, p);  // mesmas voltas
    c->cycles += 3 + (g_mut == Mut::Ciclos ? 1 : 0);
    c->polls += g_mut == Mut::Voltas ? 1 : 0;
    if (g_mut == Mut::EscritaExtra) hh::sh4_write8(c, 0x8C300000u, 0x5A);
    if (g_mut == Mut::Macl) c->macl = 1;
    c->r[15] += 4;
    c->t = 0;
    if (g_mut == Mut::SoNaRam) escf(c, p, lerf(c, p) * 0.5f);  // outro valor so na RAM (fr2 ja fixado)
}

void gerada_global(hh::Sh4Context* c) {
    c->cycles += 4;
    c->r[3] = hh::sh4_read32(c, 0x8C07398Cu);
    c->r[0] = hh::sh4_read32(c, c->r[3]) + (g_mut == Mut::Retorno ? 1 : 0);
}

hh::Sh4Func g_slotAngulo = &gerada_angulo, g_slotGlobal = &gerada_global;

void outra(hh::Sh4Context*) {}  // a "gerada" das outras elevadas (nao chamadas aqui)

// slots para todas as elevadas (as duas primeiras com as geradas de teste); sem o da 8C073980, se pedido
void slots(bool comGlobal) {
    static std::vector<hh::Sh4Func>    fns(ev::lista().size(), &outra);
    static std::vector<hh::mods::Slot> s;
    s.clear();
    s.push_back({kAngulo, &g_slotAngulo, &gerada_angulo});
    if (comGlobal) s.push_back({kGlobal, &g_slotGlobal, &gerada_global});
    for (size_t i = 0; i < ev::lista().size(); ++i)
        if (ev::lista()[i].endereco != kAngulo && ev::lista()[i].endereco != kGlobal)
            s.push_back({ev::lista()[i].endereco, &fns[i], &outra});
    hh::mods::set_slots(s.data(), s.size());
}

struct Maquina {
    std::vector<uint8_t> ram = std::vector<uint8_t>(16u << 20);
    hh::Sh4Context       c{};
    Maquina() {
        c.ram        = ram.data();
        c.r[15]      = 0x8C800000u;
        c.next_event = UINT64_MAX, c.next_poll = UINT64_MAX;
        c.fpscr      = 0x00040001u;  // DN, RM para zero (o do jogo), PR = SZ = 0
        c.pr         = 0x8C010000u;
        // o literal pool que as elevadas supoem (as constantes do angulo conferidas a parte: 2pi, -pi, pi)
        for (const ev::Elevada& e : ev::lista())
            for (const ev::Premissa& p : e.premissas) hh::sh4_write32(&c, p.endereco, p.valor);
        HH_CHECK(hh::sh4_read32(&c, 0x8C06F670u) == 0x40C90FDBu && hh::sh4_read32(&c, 0x8C07398Cu) == kPalavra);
    }
    void angulo(float a) {
        escf(&c, kDado, a);
        c.r[4] = kDado;
        g_slotAngulo(&c);
    }
};

// true se `f` parou com o erro que contem `what`
bool fatal(const std::function<void()>& f, const char* what) {
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    bool hit = false;
    try {
        f();
        std::printf("    sem erro (esperado: %s)\n", what);
    } catch (const Fatal& e) {
        hit = std::string(e.what()).find(what) != std::string::npos;
        if (!hit) std::printf("    erro: %s (esperado: %s)\n", e.what(), what);
    }
    hh::sh4_set_fatal_handler(nullptr);
    return hit;
}

bool throws(const std::function<void()>& f, const char* what) {
    try {
        f();
    } catch (const std::runtime_error& e) {
        return std::string(e.what()).find(what) != std::string::npos;
    }
    return false;
}

} // namespace

HH_TEST(hh15_elevacao_slot_e_modo_ativo) {
    HH_CHECK(ng::normalizar_angulo(4.0f) == 4.0f - ng::kVolta);
    HH_CHECK(ng::normalizar_angulo(-13.0f) == (-13.0f + ng::kVolta) + ng::kVolta);  // uma volta de cada vez
    hh::mods::reset();
    slots(false);
    HH_CHECK(throws([] { ev::install(ev::Modo::Ativo); }, "ler_global_8C14B61C nao e funcao gerada"));
    slots(true);
    ev::install(ev::Modo::Ativo);
    HH_CHECK(g_slotAngulo != &gerada_angulo && g_slotGlobal != &gerada_global);
    Maquina m;
    ev::verificar_premissas(&m.c);
    m.angulo(4.0f);  // uma volta subtraida
    HH_CHECK(lerf(&m.c, kDado) == 4.0f - ng::kVolta && m.c.cycles == 21 + 12 && m.c.polls == 1);
    m.angulo(1.0f);  // ja no intervalo: nada escrito, custo minimo
    HH_CHECK(lerf(&m.c, kDado) == 1.0f && m.c.cycles == 2 * 21 + 12 && m.c.polls == 1);
    hh::sh4_write32(&m.c, kPalavra, 0x12345678u);
    g_slotGlobal(&m.c);
    HH_CHECK(m.c.r[0] == 0x12345678u);
    m.c.fpscr |= hh::kFpscrPR;
    HH_CHECK(fatal([&] { m.angulo(4.0f); }, "FPU fora do contrato"));
    m.c.fpscr &= ~hh::kFpscrPR;
    hh::sh4_write32(&m.c, 0x8C06F674u, 0xC0490FDAu);  // o pool de outro executavel
    HH_CHECK(fatal([&] { ev::verificar_premissas(&m.c); }, "premissa de normalizar_angulo 8C06F62C diferente em 8C06F674"));
    ev::install(ev::Modo::Gerado);
    HH_CHECK(g_slotAngulo == &gerada_angulo && g_slotGlobal == &gerada_global);
    hh::mods::reset();
}

HH_TEST(hh15_elevacao_conferir_acusa_cada_divergencia) {
    hh::mods::reset();
    slots(true);
    ev::install(ev::Modo::Conferir);
    Maquina m;
    g_mut = Mut::Nenhuma;
    m.angulo(-13.0f);  // duas voltas somadas
    m.angulo(9.0f);
    m.angulo(0.5f);
    g_slotGlobal(&m.c);
    HH_CHECK(ev::stats().chamadas == 4 && ev::stats().conferidas == 4 && ev::stats().completas == 4);
    HH_CHECK(ev::stats(0).conferidas == 3 && ev::stats(1).conferidas == 1);
    HH_CHECK(ev::stats().ramNasVoltas == 2 && ev::stats().semRamNasVoltas == 0);  // -13 e 9 tem back-edges; 0,5 nao
    HH_CHECK(lerf(&m.c, kDado) == 0.5f);  // a trajetoria e a da gerada
    const struct {
        Mut         mut;
        const char* motivo;
    } casos[] = {
        {Mut::EscritaExtra, "RAM (a gerada escreveu o que a elevada nao)"},
        {Mut::OutroValor, "fr2"},  // o valor final fica em fr2 (o contrato completo o acusa antes da escrita)
        {Mut::SoNaRam, "escrita da elevada"},
        {Mut::SoNoBackEdge, "r3 (back-edge 0)"},
        {Mut::RamNoBackEdge, "no back-edge 0"},
        {Mut::Ciclos, "ciclos"},
        {Mut::Voltas, "back-edges"},
        {Mut::Retorno, "retorno r0"},
        {Mut::Macl, "mach/macl"},
    };
    for (const auto& k : casos) {
        g_mut = k.mut;
        HH_CHECK(fatal([&] { k.mut == Mut::Retorno ? g_slotGlobal(&m.c) : m.angulo(9.0f); }, k.motivo));
    }
    // o atendimento: da gerada (o tick no back-edge) so conta; so da elevada e divergencia
    g_mut                = Mut::Nenhuma;
    const uint64_t antes = ev::stats().comAtendimento;
    m.c.next_poll        = m.c.polls + 1;
    m.angulo(9.0f);
    HH_CHECK(ev::stats().comAtendimento == antes + 1);
    g_mut         = Mut::SemVolta;
    m.c.next_poll = m.c.polls + 1;
    HH_CHECK(fatal([&] { m.angulo(9.0f); }, "atendimento (so a elevada"));
    m.c.next_poll = UINT64_MAX;
    // premissa alterada no meio da sessao (o jogo gravou no pool)
    g_mut = Mut::Nenhuma;
    hh::sh4_write32(&m.c, 0x8C07398Cu, 0x8C14B620u);
    HH_CHECK(fatal([&] { g_slotGlobal(&m.c); }, "premissa de ler_global_8C14B61C"));
    hh::sh4_write32(&m.c, 0x8C07398Cu, kPalavra);
    // erro no meio do ensaio: o ensaio termina, o angulo nao muda e a proxima chamada confere
    m.c.fpscr |= hh::kFpscrPR;
    HH_CHECK(fatal([&] { m.angulo(9.0f); }, "FPU fora do contrato"));
    HH_CHECK(!ev::em_ensaio() && lerf(&m.c, kDado) == 9.0f);
    m.c.fpscr &= ~hh::kFpscrPR;
    const uint64_t conferidas = ev::stats().conferidas;
    m.angulo(9.0f);
    HH_CHECK(ev::stats().conferidas == conferidas + 1);
    HH_CHECK(ev::summary().find("[elevacao] conferir: " + std::to_string(ev::lista().size()) + " funcoes") == 0 &&
             ev::summary().find("[elevacao]   8C073980 ler_global_8C14B61C: chamadas") != std::string::npos);
    ev::install(ev::Modo::Gerado);
    hh::mods::reset();
}
