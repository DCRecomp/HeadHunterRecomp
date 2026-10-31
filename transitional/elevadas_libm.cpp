// SPDX-License-Identifier: MIT
// HH15.11b: a biblioteca matematica do SDK (faixa 8C125600-8C127000) elevada: a logica em native/sdk/matematica.h, aqui
// o adaptador (argumentos em fr4/fr5 e r4, retorno em fr0) com o custo de cada caminho da gerada. Identificacao: o
// desmonte (comportamento e casos especiais) e a conferencia contra a gerada em cada chamada da rota.
#include "elevacao.h"

#include "sdk/matematica.h"

#include <cmath>

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

// 8C1289D4: ldexpf(fr4, r4); o infinito, o -infinito, o NaN e os zeros que devolve vem das palavras apontadas pelo pool
// (lidas da RAM como na gerada; os valores sao os de native/sdk/matematica.h)
constexpr Premissa kLdexp[] = {{0x8C128A70u, 0x00FF6EF6u}, {0x8C128A74u, 0x7F800000u}, {0x8C128A78u, 0x80000000u},
                               {0x8C128A7Cu, 0x8C145438u}, {0x8C128A80u, 0x8C145468u}, {0x8C128A84u, 0x007FFFFFu},
                               {0x8C128A88u, 0x8C145430u}, {0x8C128A8Cu, 0x8C145434u}, {0x8C128A90u, 0x8C145464u}};
void ldexpf_(Sh4Context* c) {
    using Caso      = ns::Ldexp::Caso;
    const uint32_t xb = c->fr[4], sp = c->r[15];
    const int32_t  n  = int32_t(c->r[4]);
    const auto     r  = ns::ldexp_sdk(ns::de_bits(xb), n, ler_f(c, 0x8C145430u));
    const bool     neg = r.negativo;
    // os registradores como a rotina do SDK os deixa (r14 e restaurado): r1 = -infinito do pool, r6 = o local, fr5/fr7 =
    // -infinito/+infinito; no resto, por caminho
    c->r[1] = 0x8C145468u, c->r[6] = sp - 8, c->r[7] = xb & 0x7F800000u, c->r[5] = 0x80000000u;
    c->fr[5] = 0xFF800000u, c->fr[7] = 0x7F800000u;
    c->r[2] = 0x8C145438u, c->r[3] = 0x7F800000u;
    const int32_t e2 = int32_t((xb & 0x7F800000u) >> 23) + n;
    switch (r.caso) {
    case Caso::Nan: cobrar(c, 15 + 3 + 5), c->r[0] = 0x007FFFFFu, c->r[3] = 0x8C145430u, c->t = 0; break;
    case Caso::MenosInfinito: cobrar(c, 15 + 3 + 2 + 4), c->r[0] = 0x007FFFFFu, c->t = 0; break;
    case Caso::MaisInfinito: cobrar(c, 15 + 3 + 2 + 2 + 4), c->r[0] = 0x007FFFFFu, c->t = 1; break;
    default:
        c->r[2] = 0x8C145434u, c->r[3] = 0x8C145464u, c->fr[4] = 0, c->fr[6] = 0x80000000u;  // +0 e -0 do pool
        switch (r.caso) {
        case Caso::Zero: cobrar(c, 15 + 6 + 2 + (neg ? 0 : 2) + 4), c->t = !neg; break;
        case Caso::Normal:
            cobrar(c, 15 + 6 + 5 + 3 + 12);
            c->r[2] = xb & 0x007FFFFFu, c->r[3] = 23, c->r[5] = ns::bits(r.resultado), c->r[7] = uint32_t(e2) << 23, c->t = 0;
            break;
        case Caso::Estouro: cobrar(c, 15 + 6 + 5 + 3 + 2 + 2 + 4), c->r[3] = 0xFF, c->r[7] = uint32_t(e2), c->t = !neg; break;
        default: cobrar(c, 15 + 6 + 5 + 2 + 2 + 4), c->r[3] = uint32_t(-23), c->r[7] = uint32_t(e2), c->t = !neg; break;
        }
    }
    c->fr[0] = ns::bits(r.resultado);
}

// Os 7 coeficientes de um nucleo, da tabela apontada pelo pool (lidos da RAM, como na gerada).
// Um nucleo polinomial do SDK (Horner em x^2 com fmac): P(x^2) (cosseno) ou x * P(x^2) (seno, arco-tangente), com os
// n coeficientes da tabela apontada pelo pool (lidos da RAM, como na gerada). O estado vivo entre as voltas fica nos
// registradores do contexto, como na gerada (um atendimento no back-edge, que pode trocar de thread, ve e pode mudar o
// mesmo estado): x^2 em fr5 (P) ou fr6 (x * P), a soma em fr4 ou fr5; r4 conta as voltas ate 0, r5 anda pela tabela,
// T = r4 > 0.
struct Nucleo {
    uint32_t funcao, tabela, volta, n;
    bool     vezesX;
};
void nucleo(Sh4Context* c, const Nucleo& d) {
    exigir_fpu_simples(c, d.funcao);
    const uint32_t fx2 = d.vezesX ? 6 : 5, fp = d.vezesX ? 5 : 4;
    c->fr[fx2]         = ns::bits(ns::de_bits(c->fr[4]) * ns::de_bits(c->fr[4]));
    c->fr[fp]          = ns::bits(ler_f(c, d.tabela));
    cobrar(c, 5);
    for (uint32_t i = 1; i < d.n; ++i) {
        if (i > 1) volta(c, d.volta);
        cobrar(c, 7);
        const float k = ler_f(c, d.tabela + 4 * i);
        c->r[5]       = d.tabela + 4 * (i + 1);
        c->r[4]       = d.n - 1 - i;
        c->fr[0]      = c->fr[fx2];
        c->t          = c->r[4] > 0;
        const float m = ns::de_bits(c->fr[0]) * ns::de_bits(c->fr[fp]);  // fmac: produto e soma arredondados
        c->fr[3]      = ns::bits(m + k);
        c->fr[fp]     = c->fr[3];
    }
    cobrar(c, d.vezesX ? 4 : 2);
    if (d.vezesX) c->fr[6] = c->fr[5];
    c->fr[0] = d.vezesX ? ns::bits(ns::de_bits(c->fr[4]) * ns::de_bits(c->fr[6])) : c->fr[4];
}

// 8C128990: nucleo do cosseno, P(fr4^2), 7 coeficientes; 8C1289B0: nucleo do seno, fr4 * Q(fr4^2), 7; 8C12896C: nucleo
// do arco-tangente, fr4 * A(fr4^2), 6 (-1/11, 1/9, -1/7, 1/5, -1/3, 1)
constexpr Premissa kCosNucleo[]  = {{0x8C1289ACu, 0x8C145394u}};
constexpr Premissa kSenNucleo[]  = {{0x8C1289D0u, 0x8C1453B0u}};
constexpr Premissa kAtanNucleo[] = {{0x8C12898Cu, 0x8C14537Cu}};
void cos_nucleo(Sh4Context* c) { nucleo(c, {0x8C128990u, 0x8C145394u, 0x8C12899Au, 7, false}); }
void sen_nucleo(Sh4Context* c) { nucleo(c, {0x8C1289B0u, 0x8C1453B0u, 0x8C1289BAu, 7, true}); }
void atan_nucleo(Sh4Context* c) { nucleo(c, {0x8C12896Cu, 0x8C14537Cu, 0x8C128976u, 6, true}); }

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

// 8C126124: arco-tangente (sem o errno). NaN da o NaN do SDK; |x| >= 2^32, +-pi/2; zero, x; |x| = 1, +-pi/4. Senao t = |x|
// (|x| < 1, base 0) ou (|x| - 1)/(|x| + 1) (base pi/4); q = o quociente de t por 1/8 (resto_com_quociente) escolhe o ponto
// de quarto k/4: a base soma atan(k/4) e t vira (t - k/4)/(1 + t * k/4) (t/4 e t/2 pelo ldexpf, 3t/4 por fmac); o
// resultado e base + nucleo(t), com o sinal de x. Salva r13, r14 e fr12-fr15; os registradores na ordem da gerada.
constexpr Premissa kAtan[] = {
    {0x8C1262B8u, 0x7FFFFFFFu}, {0x8C1262BCu, 0x7F800000u}, {0x8C1262C0u, 0x80000000u}, {0x8C1262C4u, 0x8C145430u},
    {0x8C1262C8u, 0x8C145458u}, {0x8C1262CCu, 0x8C14543Cu}, {0x8C1262D0u, 0x8C14545Cu}, {0x8C1262D4u, 0x8C145434u},
    {0x8C1262D8u, 0x8C14546Cu}, {0x8C1262DCu, 0x8C14522Cu}, {0x8C1262E0u, 0x8C1263C0u}, {0x8C1262E4u, 0x8C12896Cu},
    {0x8C1262E8u, 0x8C145218u}, {0x8C1262ECu, 0x8C145474u}, {0x8C1262F0u, 0x8C1289D4u}, {0x8C1262F4u, 0x8C14521Cu},
    {0x8C1262F8u, 0x8C145470u}, {0x8C1262FCu, 0x8C145220u}, {0x8C126300u, 0x8C145228u}, {0x8C126304u, 0x8C145224u},
};
float fr(Sh4Context* c, int i) { return ns::de_bits(c->fr[i]); }
void  fr(Sh4Context* c, int i, float v) { c->fr[i] = ns::bits(v); }
void  atan_interno(Sh4Context* c) {
    exigir_fpu_simples(c, 0x8C126124u);
    const uint32_t pr0 = c->pr, sp0 = c->r[15], r13 = c->r[13], r14 = c->r[14];
    const uint32_t f12 = c->fr[12], f13 = c->fr[13], f14 = c->fr[14], f15 = c->fr[15];
    const uint32_t sp = sp0 - 40, xb = c->fr[4];  // r14, r13, fr15-fr12 e PR salvos e o quadro de 12 bytes
    const float    x  = ns::de_bits(xb);
    const bool     neg = int32_t(xb) < 0;
    c->r[15]           = sp;
    c->r[0] = 8, c->fr[5] = xb, c->r[3] = xb;
    cobrar(c, 15 + (neg ? 1 : 0) + 5);
    c->r[4] = neg ? 8 : 0, c->fr[3] = 0, c->r[7] = 4, c->r[6] = 1;
    if (x == 0.0f) cobrar(c, 2), c->r[4] |= 4;
    else {
        cobrar(c, 6);
        c->r[2] = xb & 0x7FFFFFFFu, c->r[3] = 0x7FFFFFFFu, c->r[1] = 0x7F800000u;
        if (c->r[2] == 0x7F800000u) cobrar(c, 3), c->r[0] = 2, c->r[4] |= 2;
        else if (cobrar(c, 2), x != x) cobrar(c, 1), c->r[4] |= 1;
    }
    const uint32_t classe = c->r[4], axb = xb & 0x7FFFFFFFu;
    const float    ax     = ns::de_bits(axb);
    cobrar(c, 7);
    c->r[3] = sp, c->r[2] = 0x80000000u, c->r[1] = xb, c->r[5] = classe;
    if (neg) cobrar(c, 3), c->fr[3] = axb;
    cobrar(c, 2);
    bool fimPi4 = false;  // |x| = 1: o resultado vai direto em fr0
    if (classe & 1) {
        cobrar(c, 3);
        c->r[2] = 0x8C145430u, c->fr[5] = ns::bits(ler_f(c, 0x8C145430u)), c->t = 0;
    } else {
        cobrar(c, 12);
        const int32_t e = int32_t((axb & 0x7F800000u) >> 23) - 127;
        c->r[3] = 0x7F800000u, c->r[2] = uint32_t(-23), c->r[0] = 32, c->r[1] = uint32_t(e);
        if (e >= 32) {
            cobrar(c, 4 + 2);
            c->r[1] = 0x8C145458u, c->fr[5] = ns::bits(ler_f(c, 0x8C145458u)), c->t = !neg;
            if (neg) c->fr[5] ^= ns::kSinal;
        } else if (cobrar(c, 2), classe & 4) {
            cobrar(c, 2);
            c->fr[5] = c->fr[4], c->t = 0;
        } else {
            cobrar(c, 7);
            c->r[1] = 0x8C14543Cu, c->fr[3] = axb, fr(c, 5, ler_f(c, 0x8C14543Cu)), c->r[3] = 0x8C14545Cu;
            fr(c, 4, ler_f(c, 0x8C14545Cu));
            if (fr(c, 5) > ax) {
                cobrar(c, 4);
                c->r[2] = 0x8C145434u, fr(c, 14, ler_f(c, 0x8C145434u)), c->fr[15] = c->fr[3];
            } else if (cobrar(c, 2), ax == fr(c, 5)) {
                cobrar(c, 2 + (neg ? 1 : 0) + 2);
                if (neg) c->fr[4] ^= ns::kSinal;
                c->t = !neg, c->fr[0] = c->fr[4], fimPi4 = true;
            } else {
                cobrar(c, 8);
                c->r[2] = 0x8C14546Cu, c->fr[3] = axb, fr(c, 2, ler_f(c, 0x8C14546Cu));
                fr(c, 5, fr(c, 5) + ax), c->fr[14] = c->fr[4], fr(c, 2, fr(c, 2) + ax);
                c->fr[15] = c->fr[2], fr(c, 15, fr(c, 15) / fr(c, 5));
            }
            if (!fimPi4) {
                cobrar(c, 7);
                c->r[3] = 0x8C14522Cu, c->r[4] = sp + 4, c->r[2] = 0x8C1263C0u, fr(c, 5, ler_f(c, 0x8C14522Cu));
                c->fr[4] = c->fr[15];
                chamar(c, 0x8C1263C0u, &resto_com_quociente, 0x8C1261FAu);
                cobrar(c, 6);
                const uint32_t q = ler32(c, sp + 4);
                c->r[0] = q, c->r[3] = 0x8C14543Cu, c->r[14] = 0x8C12896Cu, fr(c, 13, ler_f(c, 0x8C14543Cu));
                c->t = q == 0;
                bool nucleo = true;
                if (q == 0) cobrar(c, 2), c->fr[4] = c->fr[15];
                else if (q <= 8) {
                    cobrar(c, 2 * q);
                    c->t = 1;
                    if (q <= 4) {  // t/4 ou t/2 pelo ldexpf
                        const bool  quarto = q <= 2;
                        cobrar(c, 11);
                        c->r[1] = quarto ? 0x8C145474u : 0x8C145470u, c->r[4] = quarto ? uint32_t(-2) : uint32_t(-1);
                        c->r[2] = quarto ? 0x8C145218u : 0x8C14521Cu;
                        fr(c, 2, ler_f(c, c->r[1])), fr(c, 3, ler_f(c, c->r[2])), fr(c, 2, fr(c, 2) + fr(c, 15));
                        c->r[3] = 0x8C1289D4u, fr(c, 14, fr(c, 14) + fr(c, 3)), c->fr[12] = c->fr[2], c->fr[4] = c->fr[15];
                        chamar(c, 0x8C1289D4u, &ldexpf_, quarto ? 0x8C126244u : 0x8C126262u);
                        cobrar(c, 4);
                        fr(c, 13, fr(c, 13) + fr(c, 0)), fr(c, 12, fr(c, 12) / fr(c, 13)), c->fr[4] = c->fr[12];
                    } else if (q <= 6) {  // 3/4: 1 + 0,75 t por fmac
                        cobrar(c, 13);
                        c->r[1] = 0x8C145228u, c->r[3] = 0x8C145224u, c->r[2] = 0x8C145220u;
                        fr(c, 2, ler_f(c, 0x8C145228u)), fr(c, 0, ler_f(c, 0x8C145224u)), c->fr[1] = c->fr[15];
                        fr(c, 1, fr(c, 1) + fr(c, 2)), fr(c, 3, ler_f(c, 0x8C145220u));
                        const float m = fr(c, 0) * fr(c, 15);
                        fr(c, 13, m + fr(c, 13)), fr(c, 14, fr(c, 14) + fr(c, 3)), c->fr[4] = c->fr[1];
                        fr(c, 4, fr(c, 4) / fr(c, 13));
                    } else {  // 1: (t - 1)/(1 + t), base + pi/4
                        cobrar(c, 9);
                        c->r[1] = 0x8C14546Cu, fr(c, 13, fr(c, 13) + fr(c, 15)), c->r[2] = 0x8C14545Cu;
                        fr(c, 2, ler_f(c, 0x8C14546Cu)), fr(c, 3, ler_f(c, 0x8C14545Cu)), fr(c, 2, fr(c, 2) + fr(c, 15));
                        fr(c, 14, fr(c, 14) + fr(c, 3)), c->fr[4] = c->fr[2], fr(c, 4, fr(c, 4) / fr(c, 13));
                    }
                } else {  // fora da tabela (t > 1, nunca na reducao): sem nucleo, fr5 fica como a reducao deixou
                    cobrar(c, 2 * 8 + 2);
                    c->t   = 0;
                    nucleo = false;
                }
                if (nucleo) {
                    cobrar(c, 2);
                    chamar(c, 0x8C12896Cu, &atan_nucleo, 0x8C12629Au);
                    cobrar(c, 2);
                    fr(c, 5, fr(c, 14) + fr(c, 0));
                }
                cobrar(c, 2 + (neg ? 1 : 0));
                c->t = !neg;
                if (neg) c->fr[5] ^= ns::kSinal;
            }
        }
    }
    if (!fimPi4) cobrar(c, 1), c->fr[0] = c->fr[5];
    cobrar(c, 9);
    c->r[13] = r13, c->r[14] = r14, c->fr[12] = f12, c->fr[13] = f13, c->fr[14] = f14, c->fr[15] = f15;
    c->r[15] = sp0, c->pr = pr0;
}

// 8C126030: arco-seno (sem o errno). NaN ou |x| > 1 dao o NaN do SDK; |x| = 1, +-pi/2; zero, x. Senao 2 * atan(t)
// (ldexpf(atan, 1)), t = x/(1 + sqrt(1 - x^2)) (|x| < 1/2) ou (1 - sqrt(1 - x^2))/x. Salva fr15.
constexpr Premissa kAsin[] = {
    {0x8C126100u, 0x7FFFFFFFu}, {0x8C126104u, 0x7F800000u}, {0x8C126108u, 0x8C145430u}, {0x8C12610Cu, 0x80000000u},
    {0x8C126110u, 0x8C14543Cu}, {0x8C126114u, 0x8C145458u}, {0x8C126118u, 0x8C145440u}, {0x8C12611Cu, 0x8C126124u},
    {0x8C126120u, 0x8C1289D4u},
};
void asin_interno(Sh4Context* c) {
    exigir_fpu_simples(c, 0x8C126030u);
    const uint32_t pr0 = c->pr, sp0 = c->r[15], f15 = c->fr[15];
    const uint32_t sp = sp0 - 16, xb = c->fr[4];  // fr15 e PR salvos e o quadro de 8 bytes
    const float    x  = ns::de_bits(xb);
    const bool     neg = int32_t(xb) < 0;
    c->r[15]           = sp;
    c->r[0] = 4, c->fr[5] = xb, c->r[3] = xb;
    cobrar(c, 10 + (neg ? 1 : 0) + 5);
    c->r[4] = neg ? 8 : 0, c->fr[3] = 0, c->r[7] = 4, c->r[6] = 1;
    if (x == 0.0f) cobrar(c, 2), c->r[4] |= 4;
    else {
        cobrar(c, 6);
        c->r[2] = xb & 0x7FFFFFFFu, c->r[3] = 0x7FFFFFFFu, c->r[1] = 0x7F800000u;
        if (c->r[2] == 0x7F800000u) cobrar(c, 3), c->r[0] = 2, c->r[4] |= 2;
        else if (cobrar(c, 2), x != x) cobrar(c, 1), c->r[4] |= 1;
    }
    const uint32_t classe = c->r[4], axb = xb & 0x7FFFFFFFu;
    const float    ax     = ns::de_bits(axb);
    cobrar(c, 3);
    c->r[5] = 0x8C145430u;
    if (classe & 1) {  // NaN: volta direto, sem passar por fr5
        cobrar(c, 5);
        fr(c, 0, ler_f(c, 0x8C145430u)), c->t = 0;
        c->r[15] = sp0, c->pr = pr0;
        return;
    }
    cobrar(c, 6 + (neg ? 3 : 0));
    c->r[3] = sp, c->r[2] = 0x80000000u, c->r[1] = xb;
    if (neg) c->fr[3] = axb;
    cobrar(c, 5);
    c->r[3] = 0x8C14543Cu, c->fr[3] = axb, fr(c, 5, ler_f(c, 0x8C14543Cu));
    if (ax > fr(c, 5)) {
        cobrar(c, 2);
        fr(c, 5, ler_f(c, 0x8C145430u)), c->t = 1;
    } else if (cobrar(c, 2), fr(c, 5) == ax) {
        cobrar(c, 5 + (neg ? 2 : 0));
        c->r[2] = 8, c->r[1] = 0x8C145458u, fr(c, 5, ler_f(c, 0x8C145458u)), c->t = !neg;
        if (neg) c->fr[5] ^= ns::kSinal;
    } else if (cobrar(c, 2), classe & 4) {
        cobrar(c, 2);
        c->fr[5] = c->fr[4], c->t = 0;
    } else {
        cobrar(c, 5);
        c->r[2] = 0x8C145440u, c->fr[3] = axb, fr(c, 2, ler_f(c, 0x8C145440u));
        const bool  pequeno = fr(c, 2) > ax;
        const float um      = fr(c, 5);
        fr(c, 1, x * x), c->fr[0] = c->fr[5];
        if (pequeno) {
            cobrar(c, 10);
            c->fr[15] = xb, fr(c, 0, fr(c, 0) - fr(c, 1)), c->fr[1] = ns::bits(um);
            fr(c, 0, std::sqrt(fr(c, 0))), fr(c, 1, fr(c, 1) + fr(c, 0)), fr(c, 15, fr(c, 15) / fr(c, 1));
        } else {
            cobrar(c, 9);
            fr(c, 0, fr(c, 0) - fr(c, 1)), c->fr[1] = ns::bits(um);
            fr(c, 0, std::sqrt(fr(c, 0))), fr(c, 1, fr(c, 1) - fr(c, 0)), c->fr[15] = c->fr[1];
            fr(c, 15, fr(c, 15) / x);
        }
        cobrar(c, 3);
        c->r[3] = 0x8C126124u, c->fr[4] = c->fr[15];
        chamar(c, 0x8C126124u, &atan_interno, 0x8C1260ECu);
        cobrar(c, 4);
        c->r[2] = 0x8C1289D4u, c->fr[4] = c->fr[0], c->r[4] = 1;
        chamar(c, 0x8C1289D4u, &ldexpf_, 0x8C1260F4u);
        cobrar(c, 1);
        c->fr[5] = c->fr[0];
    }
    cobrar(c, 5);
    c->fr[0] = c->fr[5], c->fr[15] = f15;
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
// Os envoltorios tem o mesmo codigo (conferido byte a byte, 0x38 bytes) e o pool em E + 0x38: a mascara do resultado,
// os codigos de erro, a funcao interna e a variavel de erro; o retorno da chamada a interna em E + 0x0A.
template <uint32_t E, uint32_t I>
constexpr Premissa kEnvoltorio[] = {{E + 0x38, 0x044D0009u}, {E + 0x3C, 0x0000044Cu}, {E + 0x40, I},
                                    {E + 0x44, 0x7F800000u}, {E + 0x48, 0x007FFFFFu}, {E + 0x4C, 0x8C30FA50u}};
template <uint32_t E, uint32_t I, Sh4Func F>
void envoltorio(Sh4Context* c) {
    com_errno(c, I, F, E + 0x0A);
}
template <uint32_t E, uint32_t I, Sh4Func F>
constexpr Elevada elevada_envoltorio(const char* nome) {
    return {E, nome, &envoltorio<E, I, F>, Retorno::Fr0, kEnvoltorio<E, I>, Contrato::Completo};
}

constexpr Elevada kLista[] = {
    {0x8C1263C0u, "resto_com_quociente", &resto_com_quociente, Retorno::Fr0, kResto, Contrato::Completo},
    {0x8C128990u, "cos_nucleo", &cos_nucleo, Retorno::Fr0, kCosNucleo, Contrato::Completo},
    {0x8C1289B0u, "sen_nucleo", &sen_nucleo, Retorno::Fr0, kSenNucleo, Contrato::Completo},
    {0x8C1264D0u, "cosseno_interno", &cosseno_interno, Retorno::Fr0, kCosseno, Contrato::Completo},
    elevada_envoltorio<0x8C1243B8u, 0x8C1264D0u, &cosseno_interno>("cosf"),
    {0x8C126744u, "seno_interno", &seno_interno, Retorno::Fr0, kSeno, Contrato::Completo},
    elevada_envoltorio<0x8C1246F8u, 0x8C126744u, &seno_interno>("sinf"),
    {0x8C1289D4u, "ldexpf", &ldexpf_, Retorno::Fr0, kLdexp, Contrato::Completo},
    {0x8C12896Cu, "atan_nucleo", &atan_nucleo, Retorno::Fr0, kAtanNucleo, Contrato::Completo},
    {0x8C126124u, "atan_interno", &atan_interno, Retorno::Fr0, kAtan, Contrato::Completo},
    {0x8C126030u, "asin_interno", &asin_interno, Retorno::Fr0, kAsin, Contrato::Completo},
    elevada_envoltorio<0x8C1241C0u, 0x8C126030u, &asin_interno>("asinf_1"),
    elevada_envoltorio<0x8C124210u, 0x8C126030u, &asin_interno>("asinf_2"),
    elevada_envoltorio<0x8C124300u, 0x8C126124u, &atan_interno>("atanf"),
    elevada_envoltorio<0x8C124368u, 0x8C1264D0u, &cosseno_interno>("cosf_2"),
    // 8C1246A8 (o segundo envoltorio do seno, mesmo codigo) fica gerado: a rota da area 1 nao o chama (sem conferencia)
};

} // namespace

std::span<const Elevada> elevadas_libm() { return kLista; }

} // namespace hh::transitional::elevacao
