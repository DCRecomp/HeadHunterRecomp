// SPDX-License-Identifier: MIT
#include "elevacao.h"

#include "cpu/sh4_context.h"
#include "cpu/sh4_mem.h"
#include "mods/mod_api.h"

#include <array>
#include <cstdlib>
#include <deque>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hh::transitional::elevacao {
namespace {

constexpr size_t   kMax          = 1024;       // thunks de conferencia (um por elevada)
constexpr uint32_t kQuadro       = 256;  // abaixo do r15 de entrada: o quadro da gerada (lixo depois do retorno)
constexpr uint64_t kCompletasAte = 256;        // RAM inteira nas primeiras chamadas de cada elevada...
constexpr uint64_t kCompletaCada = 1024;       // ... e depois numa a cada kCompletaCada

Modo               g_modo = Modo::Gerado;
Stats              g_stats;
std::vector<Stats> g_por;  // por elevada

// ensaio da conferencia: back-edges so contados, escritas no diario
bool g_ensaio      = false;
bool g_atendimento = false;  // o ensaio teria atendido
struct Escrita {
    uint32_t off;
    uint32_t n;
    uint8_t  antes[4];
};
std::vector<Escrita> g_diario;
// RAM antes da chamada (conferencia completa), uma por nivel: a gerada de uma elevada chama outra elevada pelo slot, e a
// conferencia dela roda dentro da da mae
std::deque<std::vector<uint8_t>> g_copias;  // deque: crescer no nivel filho nao invalida a referencia da mae
size_t                            g_nivel = 0;

uint32_t offsetRam(Sh4Context* c, uint32_t a, uint32_t n, const char* what) {
    if (!sh4_is_ram(a) || (a & (n - 1))) sh4_fatal(c, what, c->pc, a);
    return a & kRamMask;
}

void escrever(Sh4Context* c, uint32_t a, const void* v, uint32_t n) {
    const uint32_t off = offsetRam(c, a, n, "elevacao: escrita fora da RAM principal ou desalinhada");
    if (g_ensaio) {
        Escrita e{off, n, {}};
        std::memcpy(e.antes, c->ram + off, n);
        g_diario.push_back(e);
    }
    std::memcpy(c->ram + off, v, n);
}

[[noreturn]] void divergiu(Sh4Context* c, const Elevada& e, const char* what, uint64_t esperado, uint64_t obtido) {
    char m[256];
    std::snprintf(m, sizeof m, "elevacao: %s %08X diverge da gerada em %s (gerada %llX, elevada %llX)", e.nome,
                  e.endereco, what, (unsigned long long)esperado, (unsigned long long)obtido);
    static std::string msg;
    msg = m;
    sh4_fatal(c, msg.c_str(), e.endereco, 0);
}

void premissas(Sh4Context* c, const Elevada& e) {
    for (const Premissa& p : e.premissas)
        if (ler32(c, p.endereco) != p.valor) {
            char m[192];
            std::snprintf(m, sizeof m, "elevacao: premissa de %s %08X diferente em %08X (esperado %08X, RAM %08X)",
                          e.nome, e.endereco, p.endereco, p.valor, ler32(c, p.endereco));
            static std::string msg;
            msg = m;
            sh4_fatal(c, msg.c_str(), e.endereco, p.endereco);
        }
}

// O ensaio da elevada: termina (diario desfeito, contexto de volta, ensaio desligado) inclusive por excecao.
class Ensaio {
public:
    Ensaio(Sh4Context* c) : c_(c), c0_(*c) {
        g_diario.clear();
        g_ensaio      = true;
        g_atendimento = false;
    }
    ~Ensaio() {
        for (auto it = g_diario.rbegin(); it != g_diario.rend(); ++it) std::memcpy(c_->ram + it->off, it->antes, it->n);
        g_diario.clear();
        *c_      = c0_;
        g_ensaio = false;
    }

private:
    Sh4Context*      c_;
    const Sh4Context c0_;
};

struct Final {
    uint32_t off, n;
    uint8_t  depois[4];
};

void conferir(size_t i, Sh4Context* c) {
    const Elevada& e = lista()[i];
    Stats&         s = g_por[i];
    ++g_stats.chamadas, ++s.chamadas;
    premissas(c, e);
    const bool       full = s.chamadas <= kCompletasAte || s.chamadas % kCompletaCada == 0;
    const Sh4Context c0   = *c;
    struct Nivel {
        Nivel() { ++g_nivel; }
        ~Nivel() { --g_nivel; }
    } nivel;
    if (g_copias.size() < g_nivel) g_copias.resize(g_nivel);
    std::vector<uint8_t>& g_copia = g_copias[g_nivel - 1];
    if (full) g_copia.assign(c->ram, c->ram + (16u << 20));

    // ensaio da elevada
    Sh4Context         ce;
    std::vector<Final> finais;
    bool               atenderia = false;
    {
        Ensaio guarda(c);
        e.funcao(c);
        ce        = *c;
        atenderia = g_atendimento;
        finais.reserve(g_diario.size());
        for (const Escrita& w : g_diario) {
            Final f{w.off, w.n, {}};
            std::memcpy(f.depois, c->ram + w.off, w.n);
            finais.push_back(f);
        }
    }

    // a gerada, de verdade (a trajetoria de referencia)
    mods::original(e.endereco)(c);
    if (c->next_event != c0.next_event || c->next_poll != c0.next_poll) {  // a gerada foi atendida
        ++g_stats.comAtendimento, ++s.comAtendimento;
        const bool difere = (e.retorno == Retorno::R0 && c->r[0] != ce.r[0]) || (e.retorno == Retorno::Fr0 && c->fr[0] != ce.fr[0]);
        if (difere && s.atendidasComRetornoDiferente++ < 3)
            std::fprintf(stderr, "[elevacao] %s atendida com retorno diferente: gerada r0 %08X fr0 %08X, elevada r0 %08X fr0 %08X; "
                         "entrada fr4 %08X fr5 %08X r4 %08X; fpscr %08X -> %08X\n", e.nome, c->r[0], c->fr[0], ce.r[0], ce.fr[0],
                         c0.fr[4], c0.fr[5], c0.r[4], c0.fpscr, c->fpscr);
        g_stats.atendidasComRetornoDiferente += difere;
        return;
    }
    if (atenderia) divergiu(c, e, "atendimento (so a elevada chegaria a um atendimento)", 0, 1);
    ++g_stats.conferidas, ++s.conferidas;
    if (c->cycles - c0.cycles != ce.cycles - c0.cycles) divergiu(c, e, "ciclos", c->cycles - c0.cycles, ce.cycles - c0.cycles);
    if (c->polls - c0.polls != ce.polls - c0.polls) divergiu(c, e, "back-edges", c->polls - c0.polls, ce.polls - c0.polls);
    for (int r = 8; r < 16; ++r)
        if (c->r[r] != ce.r[r]) divergiu(c, e, r == 15 ? "r15" : "registrador preservado (r8-r14)", c->r[r], ce.r[r]);
    for (int r = 0; r < 8; ++r)
        if (c->rbank[r] != ce.rbank[r]) divergiu(c, e, "banco inativo (rbank)", c->rbank[r], ce.rbank[r]);
    if (c->pr != ce.pr) divergiu(c, e, "pr", c->pr, ce.pr);
    const uint32_t mascaraSr = e.contrato == Contrato::Auxiliar ? ~(kSrM | kSrQ) : ~0u;
    if ((c->sr & mascaraSr) != (ce.sr & mascaraSr)) divergiu(c, e, "sr", c->sr, ce.sr);
    if (c->gbr != ce.gbr || c->vbr != ce.vbr || c->ssr != ce.ssr || c->spc != ce.spc || c->sgr != ce.sgr || c->dbr != ce.dbr)
        divergiu(c, e, "gbr/vbr/ssr/spc/sgr/dbr", c->gbr, ce.gbr);
    if (c->mach != ce.mach || c->macl != ce.macl) divergiu(c, e, "mach/macl", c->macl, ce.macl);
    if (c->fpscr != ce.fpscr) divergiu(c, e, "fpscr", c->fpscr, ce.fpscr);
    for (int f = 12; f < 16; ++f)
        if (c->fr[f] != ce.fr[f]) divergiu(c, e, "fr12-fr15", c->fr[f], ce.fr[f]);
    for (int f = 0; f < 16; ++f)
        if (c->xf[f] != ce.xf[f]) divergiu(c, e, "banco inativo da FPU (xf)", c->xf[f], ce.xf[f]);
    if (e.retorno == Retorno::R0 && c->r[0] != ce.r[0]) divergiu(c, e, "retorno r0", c->r[0], ce.r[0]);
    if (e.contrato != Contrato::C) {
        for (int r = 0; r < 8; ++r)
            if (c->r[r] != ce.r[r]) divergiu(c, e, "registrador do contrato completo (r0-r7)", c->r[r], ce.r[r]);
        if ((e.contrato == Contrato::Completo && c->t != ce.t) || c->fpul != ce.fpul)
            divergiu(c, e, "T/FPUL do contrato completo", c->t, ce.t);
        for (int f = 0; f < 12; ++f)
            if (c->fr[f] != ce.fr[f]) divergiu(c, e, f == 0 ? "fr0 do contrato completo" : "fr1-fr11 do contrato completo", c->fr[f], ce.fr[f]);
    }
    if (e.retorno == Retorno::Fr0 && c->fr[0] != ce.fr[0]) divergiu(c, e, "retorno fr0", c->fr[0], ce.fr[0]);
    // o valor final de cada palavra escrita pela elevada (a ultima escrita vale)
    for (auto it = finais.rbegin(); it != finais.rend(); ++it) {
        bool depois = false;  // uma escrita posterior a mesma palavra ja foi conferida
        for (auto jt = finais.rbegin(); jt != it; ++jt) depois |= jt->off == it->off && jt->n == it->n;
        if (depois || std::memcmp(c->ram + it->off, it->depois, it->n) == 0) continue;
        uint32_t g = 0, v = 0;
        std::memcpy(&g, c->ram + it->off, it->n);
        std::memcpy(&v, it->depois, it->n);
        divergiu(c, e, "escrita da elevada", g, v);
    }
    if (!full) return;
    ++g_stats.completas, ++s.completas;
    for (const Final& f : finais) std::memcpy(g_copia.data() + f.off, f.depois, f.n);
    auto cmp = [&](uint32_t lo, uint32_t hi) {
        if (hi <= lo || std::memcmp(c->ram + lo, g_copia.data() + lo, hi - lo) == 0) return;
        uint32_t k = lo;
        while (c->ram[k] == g_copia[k]) ++k;
        divergiu(c, e, "RAM (a gerada escreveu o que a elevada nao)", 0x0C000000u | k, g_copia[k]);
    };
    if (sh4_is_ram(c0.r[15])) {
        const uint32_t topo = c0.r[15] & kRamMask, quadro = topo > kQuadro ? topo - kQuadro : 0;
        cmp(0, quadro);
        cmp(topo, 16u << 20);
    } else {
        cmp(0, 16u << 20);
    }
}

template <size_t I>
void thunk(Sh4Context* c) {
    conferir(I, c);
}

template <size_t... I>
constexpr std::array<Sh4Func, sizeof...(I)> thunks(std::index_sequence<I...>) {
    return {&thunk<I>...};
}

constexpr auto kThunks = thunks(std::make_index_sequence<kMax>{});

} // namespace

std::span<const Elevada> lista() {
    static const std::vector<Elevada> todas = [] {
        std::vector<Elevada> v;
        for (const auto& fonte : {elevadas_jogo(), elevadas_runtime_c(), elevadas_libm()}) v.insert(v.end(), fonte.begin(), fonte.end());
        return v;
    }();
    return todas;
}

const char* nome_modo(Modo m) {
    switch (m) {
    case Modo::Gerado: return "gerado";
    case Modo::Ativo: return "ativo";
    case Modo::Conferir: return "conferir";
    }
    return "?";
}

void install(Modo m) {
    const auto l = lista();
    if (l.size() > kMax) throw std::runtime_error("elevacao: mais elevadas que thunks de conferencia");
    for (const Elevada& e : l) {
        if (!mods::original(e.endereco))
            throw std::runtime_error("elevacao: " + std::string(e.nome) + " nao e funcao gerada");
        if (!mods::has_slot(e.endereco))
            throw std::runtime_error("elevacao: " + std::string(e.nome) + " sem slot (falta em [native] slots do config)");
    }
    g_modo  = m;
    g_stats = {};
    g_por.assign(l.size(), Stats{});
    // HH_ELEVACAO_SO=<hexa,...> (so na verificacao, para isolar uma divergencia): so as elevadas listadas; as outras geradas
    std::string so;
    if (const char* v = std::getenv("HH_ELEVACAO_SO")) so = std::string(",") + v + ",";
    for (size_t i = 0; i < l.size(); ++i) {
        char h[16];
        std::snprintf(h, sizeof h, ",%08X,", l[i].endereco);
        const bool fora = !so.empty() && so.find(h) == std::string::npos;
        mods::replace(l[i].endereco, m == Modo::Gerado || fora ? nullptr : m == Modo::Ativo ? l[i].funcao : kThunks[i]);
    }
}

Modo modo() { return g_modo; }

void verificar_premissas(Sh4Context* c) {
    if (g_modo == Modo::Gerado) return;
    for (const Elevada& e : lista()) premissas(c, e);
}

const Stats& stats() { return g_stats; }

const Stats& stats(size_t indice) { return g_por.at(indice); }

bool em_ensaio() { return g_ensaio; }

std::string summary() {
    char b[256];
    std::snprintf(b, sizeof b,
                  "[elevacao] %s: %zu funcoes; chamadas %llu, conferidas %llu (completas %llu), com atendimento %llu (retorno "
                  "diferente %llu)\n",
                  nome_modo(g_modo), lista().size(), (unsigned long long)g_stats.chamadas,
                  (unsigned long long)g_stats.conferidas, (unsigned long long)g_stats.completas,
                  (unsigned long long)g_stats.comAtendimento, (unsigned long long)g_stats.atendidasComRetornoDiferente);
    std::string out = b;
    for (size_t i = 0; i < lista().size() && i < g_por.size(); ++i) {
        std::snprintf(b, sizeof b, "[elevacao]   %08X %s: chamadas %llu, conferidas %llu (completas %llu)\n",
                      lista()[i].endereco, lista()[i].nome, (unsigned long long)g_por[i].chamadas,
                      (unsigned long long)g_por[i].conferidas, (unsigned long long)g_por[i].completas);
        out += b;
    }
    return out;
}

// Um longjmp pendente (c->redirect) desenrolaria os quadros ate o do setjmp; uma elevada nao sabe desenrolar, e nenhuma
// das elevadas fica no caminho de um longjmp do jogo: erro explicito em vez de seguir com o contexto errado.
void sem_longjmp(Sh4Context* c, uint32_t onde) {
    if (c->redirect) sh4_fatal(c, "elevacao: longjmp atravessando uma elevada (fora do contrato)", onde, c->redirect);
}

void chamar(Sh4Context* c, uint32_t endereco, Sh4Func f, uint32_t retorno) {
    const uint32_t expect = c->ret_expect, sp = c->ret_sp, fn = c->cur_fn;
    c->pr = c->ret_expect = retorno;
    c->ret_sp             = c->r[15];
    c->cur_fn             = endereco;  // o HH_FN_ENTER da chamada
    f(c);
    c->ret_expect = expect, c->ret_sp = sp, c->cur_fn = fn;
    sem_longjmp(c, retorno);
    volta(c, retorno);  // o poll no retorno da chamada (HH_POLL_RET)
}

void volta(Sh4Context* c, uint32_t retomada) {
    ++c->polls;
    if (c->cycles >= c->next_event || c->polls >= c->next_poll) {
        if (g_ensaio) {
            g_atendimento = true;
            return;
        }
        c->pc = retomada;
        sh4_poll_slow(c);
        sem_longjmp(c, retomada);
    }
}

uint32_t ler32(Sh4Context* c, uint32_t a) {
    uint32_t v;
    std::memcpy(&v, c->ram + offsetRam(c, a, 4, "elevacao: leitura fora da RAM principal ou desalinhada"), 4);
    return v;
}

uint8_t ler8(Sh4Context* c, uint32_t a) {
    return c->ram[offsetRam(c, a, 1, "elevacao: leitura fora da RAM principal")];
}

void escrever8(Sh4Context* c, uint32_t a, uint8_t v) { escrever(c, a, &v, 1); }

float ler_f(Sh4Context* c, uint32_t a) {
    const uint32_t u = ler32(c, a);
    float          f;
    std::memcpy(&f, &u, 4);
    return f;
}

void escrever32(Sh4Context* c, uint32_t a, uint32_t v) { escrever(c, a, &v, 4); }

void escrever_f(Sh4Context* c, uint32_t a, float v) { escrever(c, a, &v, 4); }

void exigir_fpu_simples(Sh4Context* c, uint32_t funcao) {
    if (c->fpscr & (kFpscrPR | kFpscrSZ)) sh4_fatal(c, "elevacao: FPU fora do contrato (PR ou SZ ligado)", funcao, c->fpscr);
}

} // namespace hh::transitional::elevacao
