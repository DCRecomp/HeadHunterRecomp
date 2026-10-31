// SPDX-License-Identifier: MIT
// HH15.11b: a biblioteca matematica do SDK (faixa 8C125600-8C127000) elevada: a logica em native/sdk/matematica.h, aqui
// o adaptador (argumentos em fr4/fr5 e r4, retorno em fr0) com o custo de cada caminho da gerada. Identificacao: o
// desmonte (comportamento e casos especiais) e a conferencia contra a gerada em cada chamada da rota.
#include "elevacao.h"

#include "sdk/matematica.h"

namespace hh::transitional::elevacao {
namespace {

namespace ns = hh::native::sdk;

// 8C1263C0: resto de fr4 por fr5 com o quociente truncado em *r4 (a reducao de argumento das funcoes do SDK). O NaN
// devolvido nos casos invalidos e a palavra apontada pelo pool (8C145430), lida da RAM como na gerada.
constexpr Premissa kResto[] = {
    {0x8C126494u, 0x7F800000u}, {0x8C126498u, 0x007FFFFFu}, {0x8C12649Cu, 0x8C145430u}, {0x8C1264A0u, 0x80000000u},
    {0x8C1264A4u, 0x7FFFFFFFu}, {0x8C1264A8u, 0x4B000000u}, {0x8C1264ACu, 0x4F000000u}, {0x8C1264B0u, 0xCF800000u},
};
void resto_com_quociente(Sh4Context* c) {
    using Caso = ns::RestoComQuociente::Caso;
    exigir_fpu_simples(c, 0x8C1263C0u);
    const uint32_t pq = c->r[4];
    const auto     r  = ns::resto_com_quociente(ns::de_bits(c->fr[4]), ns::de_bits(c->fr[5]), ler_f(c, 0x8C145430u));
    const uint32_t sp = c->r[15], yb = c->fr[5];
    // os registradores como a rotina do SDK os deixa em cada caminho (r6/r7: os enderecos dos locais no quadro)
    c->r[5] = 0x7F800000u, c->r[6] = sp - 8, c->r[7] = sp - 16;
    switch (r.caso) {
    case Caso::XEspecial: cobrar(c, 15 + 6 + 4), c->t = 1; break;
    case Caso::YNaN: cobrar(c, 15 + 4 + 4 + 6 + 4), c->t = 0; break;
    case Caso::YInfinito:
        cobrar(c, 15 + 4 + 4 + 5 + 4 + 4 + 4);
        c->r[0] = 20, c->r[1] = 0x7F800000u, c->r[2] = 0, c->r[3] = 0x007FFFFFu, c->fr[2] = yb, c->fr[3] = 0, c->t = 1;
        break;
    case Caso::YZero: cobrar(c, 15 + 4 + 5 + 6 + 4), c->r[0] = 20, c->fr[2] = yb, c->t = 1; break;
    case Caso::Normal:
        cobrar(c, 15 + 4 + 5 + 4 + 25 + (r.jaInteiro ? 0 : 6) + 10 + (r.acimaDe2a31 ? 5 : 1) + 13 + 4);
        c->r[0] = r.acimaDe2a31 ? 0x8C1264B0u : 0x8C1264ACu;
        c->r[1] = ns::bits(r.divisao), c->r[2] = ns::bits(r.resto), c->r[3] = ns::bits(ns::de_bits(c->fr[4])) & ns::kSinal;
        c->r[5] = ns::bits(r.divisao) & ns::kExpoente;
        c->fpul = uint32_t(r.quociente);
        if (r.acimaDe2a31) c->fr[1] = 0xCF800000u;  // -2^32 do pool
        c->fr[2] = ns::bits(r.produto), c->fr[3] = ns::bits(r.restoSemSinal), c->fr[4] = ns::bits(r.divisao);
        c->t = r.acimaDe2a31;
        break;
    }
    if (r.caso != Caso::Normal && r.caso != Caso::YInfinito) {  // o NaN do SDK
        c->r[2] = 0, c->r[3] = 0x8C145430u, c->fr[3] = ns::bits(r.resto);
    }
    escrever32(c, pq, uint32_t(r.quociente));
    c->fr[0] = ns::bits(r.resto);
}

// Os 7 coeficientes de um nucleo, da tabela apontada pelo pool (lidos da RAM, como na gerada).
void coeficientes(Sh4Context* c, uint32_t tabela, float (&k)[7]) {
    for (uint32_t i = 0; i < 7; ++i) k[i] = ler_f(c, tabela + 4 * i);
}

// 8C128990: nucleo do cosseno, P(fr4^2) (Horner com fmac, 6 voltas); 8C1289B0: nucleo do seno, fr4 * Q(fr4^2)
constexpr Premissa kCosNucleo[] = {{0x8C1289ACu, 0x8C145394u}};
constexpr Premissa kSenNucleo[] = {{0x8C1289D0u, 0x8C1453B0u}};
void nucleo(Sh4Context* c, bool seno) {
    exigir_fpu_simples(c, seno ? 0x8C1289B0u : 0x8C128990u);
    float k[7];
    coeficientes(c, seno ? 0x8C1453B0u : 0x8C145394u, k);
    // o estado vivo entre as voltas fica nos registradores do contexto, como na gerada: um atendimento no back-edge (que
    // pode trocar de thread) ve e pode mudar o mesmo estado (x^2 em fr5/fr6, a soma em fr4/fr5)
    // (e os inteiros: r4 conta as voltas de 6 a 0, r5 anda pela tabela, T = r4 > 0)
    const uint32_t fx2 = seno ? 6 : 5, fp = seno ? 5 : 4, tabela = seno ? 0x8C1453B0u : 0x8C145394u;
    c->fr[fx2]         = ns::bits(ns::de_bits(c->fr[4]) * ns::de_bits(c->fr[4]));
    c->fr[fp]          = ns::bits(k[0]);
    cobrar(c, 5);
    for (uint32_t i = 1; i < 7; ++i) {
        if (i > 1) volta(c, seno ? 0x8C1289BAu : 0x8C12899Au);
        cobrar(c, 7);
        c->r[5]       = tabela + 4 * (i + 1);
        c->r[4]       = 6 - i;
        c->fr[0]      = c->fr[fx2];
        c->t          = c->r[4] > 0;
        const float m = ns::de_bits(c->fr[0]) * ns::de_bits(c->fr[fp]);  // fmac: produto e soma arredondados
        c->fr[3]      = ns::bits(m + k[i]);
        c->fr[fp]     = c->fr[3];
    }
    cobrar(c, seno ? 4 : 2);
    if (seno) c->fr[6] = c->fr[5];
    c->fr[0] = seno ? ns::bits(ns::de_bits(c->fr[4]) * ns::de_bits(c->fr[6])) : c->fr[4];
}
void cos_nucleo(Sh4Context* c) { nucleo(c, false); }
void sen_nucleo(Sh4Context* c) { nucleo(c, true); }

// 8C1264D0: cosseno (sem o errno): x infinito ou NaN da o NaN do SDK; |x| reduzido por pi/4 (resto_com_quociente, o
// octante no quadro), nos octantes impares mais -pi/4, e o nucleo do octante (0 e 7 cos, 1 e 2 -sen, 3 e 4 -cos, 5 e 6
// sen). Um resto acima de pi/4 (so com quociente saturado) da a constante 1,0 do SDK.
constexpr Premissa kCosseno[] = {
    {0x8C1265B4u, 0x7FFFFFFFu}, {0x8C1265B8u, 0x7F800000u}, {0x8C1265BCu, 0x8C145430u},
    {0x8C1265C0u, 0x8C14545Cu}, {0x8C1265C4u, 0x8C1263C0u}, {0x8C1265C8u, 0x8C14543Cu},
    {0x8C1265CCu, 0x8C14548Cu}, {0x8C1265D0u, 0x8C128990u}, {0x8C1265D4u, 0x8C1289B0u},
};
void cosseno_interno(Sh4Context* c) {
    exigir_fpu_simples(c, 0x8C1264D0u);
    const uint32_t pr0 = c->pr, sp0 = c->r[15], xb = c->fr[4];
    const float    x   = ns::de_bits(xb);
    c->r[15]           = sp0 - 12;  // o PR salvo e o quadro de 8 bytes (o octante em [r15])
    const bool neg     = int32_t(xb) < 0;
    cobrar(c, 10 + (neg ? 1 : 0) + 3);
    uint32_t classe = 0;
    if (x == 0.0f) cobrar(c, 3);
    else if (cobrar(c, 6), (xb & 0x7FFFFFFFu) == 0x7F800000u) cobrar(c, 3), classe = 2;
    else if (cobrar(c, 2), x != x) cobrar(c, 2), classe = 1;
    cobrar(c, 4);
    // os registradores como a rotina do SDK os deixa: a classe em r4 (8 negativo, 4 zero, 2 infinito, 1 NaN), r5 = 8,
    // fr5 = x; depois o que as chamadas deixam
    const uint32_t classeBits = (neg ? 8 : 0) | (x == 0.0f ? 4 : 0) | classe;
    c->r[4] = classeBits, c->r[5] = 8, c->fr[5] = xb, c->fr[3] = 0;
    if (x != 0.0f) c->r[1] = xb & 0x7FFFFFFFu, c->r[3] = 0x7FFFFFFFu, c->r[2] = 0x7F800000u;
    if (x == 0.0f) c->r[2] = 4;
    float r;
    if (classe) {
        cobrar(c, 6);
        r       = ler_f(c, 0x8C145430u);
        c->r[0] = classeBits, c->r[1] = 0x8C145430u, c->r[2] = 3, c->t = 0;
    } else {
        cobrar(c, 2 + (neg ? 1 : 0) + 5);
        c->fr[4] = neg ? xb ^ ns::kSinal : xb;
        c->fr[5]            = ns::bits(ler_f(c, 0x8C14545Cu));
        c->r[4]             = c->r[15];
        c->r[2] = 0x8C14545Cu, c->r[3] = 0x8C1263C0u;
        chamar(c, 0x8C1263C0u, &resto_com_quociente, 0x8C126530u);
        r = ns::de_bits(c->fr[0]);
        cobrar(c, 5);
        c->r[3] = 0x8C14545Cu, c->fr[4] = c->fr[0], c->fr[3] = ns::bits(ler_f(c, 0x8C14545Cu));
        if (r > ler_f(c, 0x8C14545Cu)) {
            cobrar(c, 6);
            r       = ler_f(c, 0x8C14543Cu);
            c->r[1] = 0x8C14543Cu, c->t = 1;
        } else {
            const uint32_t k = ler32(c, c->r[15]) & 7;
            escrever32(c, c->r[15], k);
            cobrar(c, 5);
            if (k & 1) cobrar(c, 4), c->r[3] = 0x8C14548Cu, c->fr[3] = c->fr[4], r = ler_f(c, 0x8C14548Cu) + r;
            static constexpr uint32_t kPosicao[8] = {0, 2, 3, 4, 5, 6, 7, 1};  // a ordem dos testes: 0, 7, 1, ..., 6
            cobrar(c, 3 + 2 * kPosicao[k]);
            c->fr[4]         = ns::bits(r);
            c->r[0]          = k;
            const bool seno  = k == 1 || k == 2 || k == 5 || k == 6;
            const bool menos = k >= 1 && k <= 4;
            static constexpr uint32_t kRetorno[8] = {0x8C126584u, 0x8C12658Eu, 0x8C12658Eu, 0x8C12659Au,
                                                     0x8C12659Au, 0x8C1265A8u, 0x8C1265A8u, 0x8C126584u};
            cobrar(c, 3);
            c->r[3] = seno ? 0x8C1289B0u : 0x8C128990u;
            chamar(c, seno ? 0x8C1289B0u : 0x8C128990u, seno ? &sen_nucleo : &cos_nucleo, kRetorno[k]);
            cobrar(c, k == 0 || k == 7 ? 2 + 1 : k <= 2 ? 3 + 2 : k <= 4 ? 2 + 2 : 1);
            if (menos) c->fr[4] = c->fr[0];  // 1 a 4: fr4 = o resultado do nucleo antes da troca de sinal
            r        = ns::de_bits(menos ? c->fr[0] ^ ns::kSinal : c->fr[0]);
            c->fr[5] = ns::bits(r);
            cobrar(c, 5);
        }
    }
    c->fr[0] = ns::bits(r);
    c->r[15] = sp0, c->pr = pr0;
}

// 8C126744: seno (sem o errno): como o cosseno, com os nucleos trocados (0 e 7 sen, 1 e 2 cos, 3 e 4 -sen, 5 e 6
// -cos) e o sinal de x no fim; um resto acima de pi/4 da a constante 0,0 do SDK (8C145434). Salva r13 e r14.
constexpr Premissa kSeno[] = {
    {0x8C126834u, 0x7FFFFFFFu}, {0x8C126838u, 0x7F800000u}, {0x8C12683Cu, 0x8C145430u},
    {0x8C126840u, 0x8C14545Cu}, {0x8C126844u, 0x8C1263C0u}, {0x8C126848u, 0x8C145434u},
    {0x8C12684Cu, 0x8C14548Cu}, {0x8C126850u, 0x8C1289B0u}, {0x8C126854u, 0x8C128990u},
};
void seno_interno(Sh4Context* c) {
    exigir_fpu_simples(c, 0x8C126744u);
    const uint32_t pr0 = c->pr, sp0 = c->r[15], xb = c->fr[4];
    const float    x   = ns::de_bits(xb);
    c->r[15]           = sp0 - 20;  // r14, r13 e PR salvos e o quadro de 8 bytes (o octante em [r15])
    const bool neg     = int32_t(xb) < 0;
    cobrar(c, 13 + (neg ? 1 : 0) + 4);
    uint32_t classe = 0;
    if (x == 0.0f) cobrar(c, 3);
    else if (cobrar(c, 6), (xb & 0x7FFFFFFFu) == 0x7F800000u) cobrar(c, 3), classe = 2;
    else if (cobrar(c, 2), x != x) cobrar(c, 1), classe = 1;
    cobrar(c, 4);
    // os registradores como a rotina do SDK os deixa (r13/r14 sao restaurados): r0 = 4 (2 no infinito), r4 = a classe,
    // r5 = 8, r6 = 0, fr5 = x; depois o que as chamadas deixam
    const uint32_t classeBits = (neg ? 8 : 0) | (x == 0.0f ? 4 : 0) | classe;
    c->r[0] = classe == 2 ? 2 : 4, c->r[4] = classeBits, c->r[5] = 8, c->r[6] = 0, c->fr[5] = xb, c->fr[3] = 0;
    if (x != 0.0f) c->r[1] = xb & 0x7FFFFFFFu, c->r[3] = 0x7FFFFFFFu, c->r[2] = 0x7F800000u;
    if (x == 0.0f) c->r[2] = 4;
    float r;
    if (classe) {
        cobrar(c, 4);
        r       = ler_f(c, 0x8C145430u);
        c->r[1] = 0x8C145430u, c->r[2] = 3, c->t = 0;
    } else {
        cobrar(c, 2 + (neg ? 3 : 1) + 5);
        c->fr[4] = neg ? xb ^ ns::kSinal : xb;
        c->fr[5] = ns::bits(ler_f(c, 0x8C14545Cu));
        c->r[4]  = c->r[15];
        c->r[2] = 0x8C1263C0u, c->r[3] = 0x8C14545Cu;
        chamar(c, 0x8C1263C0u, &resto_com_quociente, 0x8C1267ACu);
        r = ns::de_bits(c->fr[0]);
        cobrar(c, 5);
        c->r[3] = 0x8C14545Cu, c->fr[4] = c->fr[0], c->fr[3] = ns::bits(ler_f(c, 0x8C14545Cu));
        if (r > ler_f(c, 0x8C14545Cu)) {
            cobrar(c, 4);
            r       = ler_f(c, 0x8C145434u);
            c->r[1] = 0x8C145434u, c->t = 1;
        } else {
            const uint32_t k = ler32(c, c->r[15]) & 7;
            escrever32(c, c->r[15], k);
            cobrar(c, 5);
            if (k & 1) cobrar(c, 4), c->r[3] = 0x8C14548Cu, c->fr[3] = c->fr[4], r = ler_f(c, 0x8C14548Cu) + r;
            static constexpr uint32_t kPosicao[8] = {0, 2, 3, 4, 5, 6, 7, 1};  // a ordem dos testes: 0, 7, 1, ..., 6
            cobrar(c, 3 + 2 * kPosicao[k]);
            c->fr[4]         = ns::bits(r);
            c->r[0]          = k;
            const bool cosk  = k == 1 || k == 2 || k == 5 || k == 6;
            const bool menos = k >= 3 && k <= 6;
            static constexpr uint32_t kRetorno[8] = {0x8C1267FCu, 0x8C126806u, 0x8C126806u, 0x8C126810u,
                                                     0x8C126810u, 0x8C12681Cu, 0x8C12681Cu, 0x8C1267FCu};
            cobrar(c, 3);
            c->r[3] = cosk ? 0x8C128990u : 0x8C1289B0u;
            chamar(c, cosk ? 0x8C128990u : 0x8C1289B0u, cosk ? &cos_nucleo : &sen_nucleo, kRetorno[k]);
            cobrar(c, k == 0 || k == 7 ? 2 + 2 : k <= 2 ? 2 : k <= 4 ? 3 + 1 : 2 + 1);
            if (menos) c->fr[4] = c->fr[0];  // 3 a 6: fr4 = o resultado do nucleo antes da troca de sinal
            uint32_t v = menos ? c->fr[0] ^ ns::kSinal : c->fr[0];
            cobrar(c, 2 + (neg ? 1 : 0) + 1);
            if (neg) v ^= ns::kSinal;
            c->t     = !neg;  // tst r14,r14
            r        = ns::de_bits(v);
            c->fr[5] = v;
        }
    }
    cobrar(c, 5);
    c->fr[0] = ns::bits(r);
    c->r[15] = sp0, c->pr = pr0;
}

// Os envoltorios publicos (cosf 8C1243B8, sinf 8C1246F8): a funcao interna e o errno (resultado NaN: 0x44D; infinito:
// 0x44C) na variavel de erro do SDK
void com_errno(Sh4Context* c, uint32_t endereco, Sh4Func interna, uint32_t retorno) {
    const uint32_t pr0 = c->pr, sp0 = c->r[15];
    c->r[15]           = sp0 - 8;  // o PR salvo e o quadro de 4 bytes
    cobrar(c, 5);
    chamar(c, endereco, interna, retorno);
    const uint32_t rb = c->fr[0];
    cobrar(c, 7);
    // r2 = expoente do resultado, r3 = o quadro, r4 = a mascara do expoente; no erro, o errno e a mantissa
    c->r[2] = rb & 0x7F800000u, c->r[3] = c->r[15], c->r[4] = 0x7F800000u, c->t = c->r[2] == 0x7F800000u;
    if (c->t) {
        cobrar(c, 5);
        c->r[3] = 0x007FFFFFu;
        if (rb & 0x007FFFFFu) {
            cobrar(c, 4), escrever32(c, 0x8C30FA50u, 0x44Du);
            c->r[0] = 0x44D, c->r[1] = rb, c->r[2] = 0x8C30FA50u, c->t = 0;
        } else {
            cobrar(c, 3), escrever32(c, 0x8C30FA50u, 0x44Cu);
            c->r[1] = 0x8C30FA50u, c->r[2] = 0x44C, c->t = 1;
        }
    }
    cobrar(c, 5);
    c->r[15] = sp0, c->pr = pr0;
}
constexpr Premissa kCosf[] = {{0x8C1243F0u, 0x044D0009u}, {0x8C1243F4u, 0x0000044Cu}, {0x8C1243F8u, 0x8C1264D0u},
                              {0x8C1243FCu, 0x7F800000u}, {0x8C124400u, 0x007FFFFFu}, {0x8C124404u, 0x8C30FA50u}};
constexpr Premissa kSinf[] = {{0x8C124730u, 0x044D0009u}, {0x8C124734u, 0x0000044Cu}, {0x8C124738u, 0x8C126744u},
                              {0x8C12473Cu, 0x7F800000u}, {0x8C124740u, 0x007FFFFFu}, {0x8C124744u, 0x8C30FA50u}};
void cosf_(Sh4Context* c) { com_errno(c, 0x8C1264D0u, &cosseno_interno, 0x8C1243C2u); }
void sinf_(Sh4Context* c) { com_errno(c, 0x8C126744u, &seno_interno, 0x8C124702u); }

constexpr Elevada kLista[] = {
    {0x8C1263C0u, "resto_com_quociente", &resto_com_quociente, Retorno::Fr0, kResto, Contrato::Completo},
    {0x8C128990u, "cos_nucleo", &cos_nucleo, Retorno::Fr0, kCosNucleo, Contrato::Completo},
    {0x8C1289B0u, "sen_nucleo", &sen_nucleo, Retorno::Fr0, kSenNucleo, Contrato::Completo},
    {0x8C1264D0u, "cosseno_interno", &cosseno_interno, Retorno::Fr0, kCosseno, Contrato::Completo},
    {0x8C1243B8u, "cosf", &cosf_, Retorno::Fr0, kCosf, Contrato::Completo},
    {0x8C126744u, "seno_interno", &seno_interno, Retorno::Fr0, kSeno, Contrato::Completo},
    {0x8C1246F8u, "sinf", &sinf_, Retorno::Fr0, kSinf, Contrato::Completo},
};

} // namespace

std::span<const Elevada> elevadas_libm() { return kLista; }

} // namespace hh::transitional::elevacao
