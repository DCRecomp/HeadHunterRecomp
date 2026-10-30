// SPDX-License-Identifier: MIT
// HH15.10: as funcoes do jogo elevadas (fase intermediaria: adaptadores sobre a RAM guest, logica em native/game). Cada
// uma reproduz o caminho original na ordem observavel: os custos e os back-edges da gerada (cobrar/volta: HH_CYCLES e
// HH_POLL, na mesma ordem) e as escritas na RAM antes de cada back-edge, para que um atendimento no meio veja o mesmo
// estado. As palavras do literal pool de que a elevada depende sao premissas declaradas (conferidas antes da entrada do
// jogo e em cada chamada da conferencia). O endereco de cada uma esta em [native] slots de config/headhunter.toml.
#include "elevacao.h"

#include "game/angulo.h"

namespace hh::transitional::elevacao {
namespace {

namespace ng = hh::native::game;

// 8C073980: devolve a palavra do global apontado pelo pool 8C07398C (72 chamadores, sem outro efeito)
constexpr Premissa kGlobal[] = {{0x8C07398Cu, 0x8C14B61Cu}};
void ler_global_8C14B61C(Sh4Context* c) {
    cobrar(c, 4);
    c->r[0] = ler32(c, 0x8C14B61Cu);
}

// 8C06F62C: normaliza para [-pi, pi] o angulo (float) apontado por r4, relendo-o e escrevendo-o a cada volta como a
// gerada; o pool tem 2pi, -pi e pi (iguais nos dois discos), as constantes de native/game/angulo.h
constexpr Premissa kAngulo[] = {{0x8C06F670u, 0x40C90FDBu}, {0x8C06F674u, 0xC0490FDBu}, {0x8C06F678u, 0x40490FDBu}};
void normalizar_angulo(Sh4Context* c) {
    exigir_fpu_simples(c, 0x8C06F62Cu);
    const uint32_t p = c->r[4];
    cobrar(c, 4 + 6);  // entrada e o teste do primeiro laco
    while (ng::abaixo_do_intervalo(ler_f(c, p))) {
        volta(c, 0x8C06F634u);
        cobrar(c, 6);
        escrever_f(c, p, ler_f(c, p) + ng::kVolta);
        cobrar(c, 6);  // teste
    }
    cobrar(c, 2 + 6);  // saida do primeiro laco e o teste do segundo
    while (ng::acima_do_intervalo(ler_f(c, p))) {
        volta(c, 0x8C06F650u);
        cobrar(c, 6);
        escrever_f(c, p, ler_f(c, p) - ng::kVolta);
        cobrar(c, 6);  // teste
    }
    cobrar(c, 3);
}

constexpr Elevada kLista[] = {
    {0x8C06F62Cu, "normalizar_angulo", &normalizar_angulo, Retorno::Nenhum, kAngulo},
    {0x8C073980u, "ler_global_8C14B61C", &ler_global_8C14B61C, Retorno::R0, kGlobal},
};

} // namespace

std::span<const Elevada> elevadas_jogo() { return kLista; }

} // namespace hh::transitional::elevacao
