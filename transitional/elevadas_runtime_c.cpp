// SPDX-License-Identifier: MIT
// HH15.11a: a biblioteca do compilador e do runtime C (faixa 8C123B38-8C1248E0) elevada. Identificacao: o desmonte de
// cada funcao (comportamento) e a conferencia contra a gerada em cada chamada da rota (contrato). Os auxiliares do
// compilador tem convencao propria (argumentos em r0-r2, registradores preservados), entao todas tem contrato completo:
// cada registrador e o T ficam como a gerada deixa. Custo e back-edges do caminho original na mesma ordem (cobrar/volta);
// cada escrita na ordem da gerada.
#include "elevacao.h"

namespace hh::transitional::elevacao {
namespace {

uint32_t sext8(uint8_t b) { return uint32_t(int32_t(int8_t(b))); }

// 8C1247E8 strlen(r4): conta os bytes ate o zero. Deixa r3 = 0 (o ultimo byte lido), r4 depois do zero, r5 = r0 = n.
void strlen_(Sh4Context* c) {
    const uint32_t s = c->r[4];
    cobrar(c, 2 + 3);  // bra (com o delay slot) e o primeiro teste
    uint32_t n = 0;
    while (ler8(c, s + n)) {
        volta(c, 0x8C1247ECu);
        cobrar(c, 1 + 3);
        ++n;
    }
    cobrar(c, 2);
    c->r[0] = c->r[5] = n;
    c->r[3]           = 0;
    c->r[4]           = s + n + 1;
    c->t              = 1;
}

bool tem_zero(uint32_t w) { return !(w & 0xFF) || !(w & 0xFF00) || !(w & 0xFF0000) || !(w & 0xFF000000); }

// 8C124068 strcmp(r4, r5): devolve s1[j] - s2[j] (bytes com sinal). Alinhadas: palavra a palavra, quatro por volta, ate
// uma palavra de s2 com zero (cmp/str) ou diferente (a gerada ja le a palavra seguinte de s2 nesse teste); depois byte a
// byte nessa palavra, parando no zero de s2 ou na diferenca (o quarto byte sem teste). Desalinhadas: byte a byte, quatro
// por volta, parando no zero de s1 ou na diferenca.
// Os registradores ficam como na gerada, no retorno e em cada back-edge (o escalonador do jogo os salva na RAM).
void strcmp_(Sh4Context* c) {
    const uint32_t s1 = c->r[4], s2 = c->r[5];
    cobrar(c, 4);
    if ((s1 | s2) & 3) {  // r0 = byte de s1, r3 = byte de s2, r4/r5 depois deles
        for (uint32_t i = 0;; ++i) {
            if (i && !(i % 4)) {
                c->r[4] = s1 + i, c->r[5] = s2 + i, c->t = 1;
                volta(c, 0x8C1240DCu);
            }
            cobrar(c, 4);
            const uint8_t a = ler8(c, s1 + i), b = ler8(c, s2 + i);
            c->r[0] = sext8(a), c->r[3] = sext8(b);
            if (!a || (cobrar(c, 2), a != b)) {
                cobrar(c, 2);
                c->r[4] = s1 + i + 1, c->r[5] = s2 + i + 1, c->t = !a;
                c->r[0] = sext8(a) - sext8(b);
                return;
            }
        }
    }
    // alinhadas: as palavras de s2 se alternam entre r3 (indice par) e r1 (impar); r0 = palavra de s1; r2 = 0
    cobrar(c, 2);
    uint32_t k  = 0;
    uint32_t w2 = ler32(c, s2);  // a palavra de s2 e lida um passo antes (antes do back-edge)
    c->r[2] = 0, c->r[3] = w2;
    for (;;) {
        if (k && !(k % 4)) {
            c->r[4] = s1 + 4 * k, c->r[5] = s2 + 4 * (k + 1), c->t = 1;
            volta(c, 0x8C124074u);
        }
        cobrar(c, 3);
        const uint32_t w1 = ler32(c, s1 + 4 * k);
        c->r[0]           = w1;
        if (tem_zero(w2)) break;
        cobrar(c, 3);
        const uint32_t prox      = ler32(c, s2 + 4 * (k + 1));
        c->r[(k & 1) ? 3 : 1] = prox;
        if (w1 != w2) {
            cobrar(c, 1);
            break;
        }
        w2 = prox;
        ++k;
    }
    const uint32_t b1 = s1 + 4 * k, b2 = s2 + 4 * k;
    cobrar(c, 6);
    uint32_t r1 = sext8(ler8(c, b2)), r0 = sext8(ler8(c, b1)), lidos = 1;
    bool     t  = false;
    for (uint32_t j = 0; j < 3; ++j) {
        if (!r1) {
            t = true;  // cmp/eq r2,r1: o zero de s2
            break;
        }
        cobrar(c, 2);
        t = r0 == r1;
        if (!t) break;
        if (j == 2) {
            cobrar(c, 2);
            r0 = sext8(ler8(c, b1 + 3)), r1 = sext8(ler8(c, b2 + 3)), ++lidos;
            break;
        }
        cobrar(c, 4);
        r1 = sext8(ler8(c, b2 + j + 1)), r0 = sext8(ler8(c, b1 + j + 1)), ++lidos;
    }
    cobrar(c, 2);
    c->r[0] = r0 - r1, c->r[1] = r1;
    c->r[4] = b1 + lidos, c->r[5] = b2 + lidos, c->t = t;
}

// 8C124640 memset(r4, r5, r6): devolve r4; deixa r3 = 0 e r7 = n (contador), T = 1 (cmp/hs final ou de n = 0).
void memset_(Sh4Context* c) {
    const uint32_t d = c->r[4], n = c->r[6];
    const uint8_t  v = uint8_t(c->r[5]);
    cobrar(c, 5);
    for (uint32_t i = 0; i < n; ++i) {
        if (i) volta(c, 0x8C12464Au);
        cobrar(c, 5);
        escrever8(c, d + i, v);
    }
    cobrar(c, 2);
    c->r[0] = d;
    c->r[3] = 0;
    c->r[7] = n;
    c->t    = 1;
}

// 8C123FE0 (auxiliar do compilador): copia r0 bytes de r2 para r1, quatro por volta; r2-r4 preservados (pilha), r1 avanca
// 4 por grupo completo, r0 fica com o ultimo byte lido (com sinal), T = 0 (ou 1 com r0 = 0).
void copiar_bytes(Sh4Context* c) {
    const uint32_t n = c->r[0], s = c->r[2], sp0 = c->r[15], r4 = c->r[4];
    uint32_t       d = c->r[1];
    c->r[15]         = sp0 - 12;  // r2, r3 e r4 empilhados durante o corpo
    cobrar(c, 5);
    if (n == 0) {
        cobrar(c, 4);
        c->t = 1, c->r[15] = sp0;
        return;
    }
    cobrar(c, 2);
    const uint32_t fim = s + n;
    uint32_t       i = 0;
    uint8_t        b = 0;
    for (bool mais = true; mais;) {
        for (uint32_t k = 0; k < 4 && mais; ++k) {
            cobrar(c, k == 3 ? 5 : 4);
            b = ler8(c, s + i);
            escrever8(c, d + k, b);
            ++i;
            mais = fim > s + i;  // cmp/hi r2,r4
            if (k == 3) {
                d += 4;
                if (mais) {  // os registradores no back-edge: r0 = o byte, r1 = o destino, r2 = a origem, r4 = o fim
                    c->r[0] = sext8(b), c->r[1] = d, c->r[2] = s + i, c->r[4] = fim, c->t = 1;
                    volta(c, 0x8C123FEEu);
                }
            }
        }
    }
    cobrar(c, 4);
    c->r[0] = sext8(b);
    c->r[1] = d;
    c->t    = 0;
    c->r[2] = s, c->r[4] = r4, c->r[15] = sp0;  // r2 e r4 restaurados da pilha (r3 nao muda aqui)
}

// 8C123E2C: o ponteiro da tabela de saltos das copias por palavra; 8C123E74: a tabela (entrada i -> rotulo que grava o
// deslocamento 4i - 4; a 0 vai direto ao rts)
constexpr Premissa kTabela[] = {
    {0x8C123E2Cu, 0x8C123E74u}, {0x8C123E74u, 0x8C123E70u}, {0x8C123E78u, 0x8C123E6Eu}, {0x8C123E7Cu, 0x8C123E6Au},
    {0x8C123E80u, 0x8C123E66u}, {0x8C123E84u, 0x8C123E62u}, {0x8C123E88u, 0x8C123E5Eu}, {0x8C123E8Cu, 0x8C123E5Au},
    {0x8C123E90u, 0x8C123E56u}, {0x8C123E94u, 0x8C123E52u}, {0x8C123E98u, 0x8C123E4Eu}, {0x8C123E9Cu, 0x8C123E4Au},
    {0x8C123EA0u, 0x8C123E46u}, {0x8C123EA4u, 0x8C123E42u}, {0x8C123EA8u, 0x8C123E3Eu}, {0x8C123EACu, 0x8C123E3Au},
    {0x8C123EB0u, 0x8C123E36u}, {0x8C123EB4u, 0x8C123E32u},
};

// 8C123E14 / 8C123E20 (auxiliares do compilador): copiam r0 bytes (multiplo de 4, ate 64) de r2 para r1 por palavras,
// da mais alta para a mais baixa, lendo a proxima antes de gravar a anterior; as palavras se alternam entre r0 e r3 (a
// primeira em r0 pela 8C123E14, numero par de palavras; em r3 pela 8C123E20, impar). r3 preservado (pilha).
void copiar_palavras(Sh4Context* c, bool primeiraEmR0, uint32_t funcao) {
    const uint32_t s = c->r[0], d = c->r[1], src = c->r[2];
    if (s > 64 || (s & 3) || (s && ((s / 4) % 2 == 0) != primeiraEmR0))
        sh4_fatal(c, "elevacao: copia por palavras fora do contrato (tamanho ou paridade)", funcao, s);
    uint32_t r0   = s - 4;
    bool     emR0 = primeiraEmR0;
    auto     ler  = [&](uint32_t o) {
        const uint32_t v = ler32(c, src + o);
        if (emR0) r0 = v;
        emR0 = !emR0;
        return v;
    };
    if (s == 0) {  // a entrada 0 da tabela: so o rts (a carga do delay slot fica em r0 ou no r3 restaurado)
        cobrar(c, 6 + 2);
        ler(uint32_t(-4));
        c->r[0] = r0;
        return;
    }
    cobrar(c, 2 * (s / 4) + 7);
    uint32_t o    = s - 4;
    uint32_t prev = ler(o);
    while (o > 0) {
        const uint32_t prox = ler(o - 4);
        escrever32(c, d + o, prev);
        prev = prox;
        o -= 4;
    }
    escrever32(c, d, prev);
    c->r[0] = r0;
}

// Divisoes do compilador: r1 (dividendo) por r0 (divisor). Formulas fechadas do resultado, de r1 e do desvio de
// correcao do resto com sinal, deduzidas do desmonte por um modelo de analise (fora do produto) e conferidas em 400 mil
// entradas; o quociente com sinal e o truncado para zero calculado em 64 bits (INT_MIN / -1 da 0x80000000, como a
// gerada). Divisor zero: grava o codigo de erro (do pool) na variavel de erro (do pool) e devolve 0.
constexpr Premissa kErroSdiv[] = {{0x8C123BE4u, 0x8C30FA50u}, {0x8C123BE8u, 0x0000044Eu}};
constexpr Premissa kErroUdiv[] = {{0x8C123C8Cu, 0x8C30FA50u}, {0x8C123C90u, 0x0000044Eu}};
constexpr Premissa kErroSmod[] = {{0x8C123D54u, 0x8C30FA50u}, {0x8C123D58u, 0x0000044Eu}};
constexpr Premissa kErroUmod[] = {{0x8C123E0Cu, 0x8C30FA50u}, {0x8C123E10u, 0x0000044Eu}};
constexpr uint32_t kErrno = 0x8C30FA50u, kEdiv = 0x0000044Eu;

int64_t quociente(int32_t a, int32_t b) { return int64_t(a) / int64_t(b); }  // truncado para zero

// 8C123B38: quociente com sinal; r0 = r1 = q
void dividir(Sh4Context* c) {
    const int32_t a = int32_t(c->r[1]), b = int32_t(c->r[0]);
    cobrar(c, 3);
    if (b == 0) {
        cobrar(c, 6);
        escrever32(c, kErrno, kEdiv);
        c->r[0] = 0, c->r[1] = kErrno;
        return;
    }
    cobrar(c, 76);
    c->r[0] = c->r[1] = uint32_t(quociente(a, b));
}

// 8C123BEC: quociente sem sinal; r0 = r1 = q
void dividir_sem_sinal(Sh4Context* c) {
    const uint32_t a = c->r[1], b = c->r[0];
    cobrar(c, 3);
    if (b == 0) {
        cobrar(c, 6);
        escrever32(c, kErrno, kEdiv);
        c->r[0] = 0, c->r[1] = kEdiv;
        return;
    }
    cobrar(c, 70);
    c->r[0] = c->r[1] = a / b;
}

// 8C123C94: resto com sinal (o do dividendo); r1 = o quociente bruto (complemento de um) sem o ultimo passo; a correcao
// final (3 ciclos) quando o sinal do resto parcial (o do divisor se o ultimo digito e 1, o oposto se 0) difere do sinal
// do dividendo
void resto(Sh4Context* c) {
    const int32_t a = int32_t(c->r[1]), b = int32_t(c->r[0]);
    cobrar(c, 3);
    if (b == 0) {
        cobrar(c, 6);
        escrever32(c, kErrno, kEdiv);
        c->r[0] = 0, c->r[1] = kErrno;
        return;
    }
    const uint32_t q    = uint32_t(quociente(a, b));
    const uint32_t neg  = (a < 0) != (b < 0);
    const uint32_t qraw = q - neg;
    const uint32_t sa = a < 0, sb = b < 0;
    const bool     corrige = ((qraw & 1) ? sb : 1 - sb) != sa;
    cobrar(c, 77 + (corrige ? 3 : 0) + 6);
    c->r[0] = uint32_t(a) - q * uint32_t(b);
    c->r[1] = (qraw >> 1) | (neg << 31);
}

// 8C123D5C: resto sem sinal; r1 = q >> 1 (o laco sem o ultimo passo)
void resto_sem_sinal(Sh4Context* c) {
    const uint32_t a = c->r[1], b = c->r[0];
    if (b == 0) {
        cobrar(c, 2 + 7);
        escrever32(c, kErrno, kEdiv);
        c->r[0] = 0, c->r[1] = kErrno;
        return;
    }
    cobrar(c, 2 + 70 + 4);
    c->r[0] = a % b;
    c->r[1] = (a / b) >> 1;
}

void copiar_palavras_par(Sh4Context* c) { copiar_palavras(c, true, 0x8C123E14u); }
void copiar_palavras_impar(Sh4Context* c) { copiar_palavras(c, false, 0x8C123E20u); }

constexpr Elevada kLista[] = {
    {0x8C1247E8u, "strlen", &strlen_, Retorno::R0, {}, Contrato::Completo},
    {0x8C124068u, "strcmp", &strcmp_, Retorno::R0, {}, Contrato::Completo},
    {0x8C124640u, "memset", &memset_, Retorno::R0, {}, Contrato::Completo},
    {0x8C123FE0u, "copiar_bytes", &copiar_bytes, Retorno::R0, {}, Contrato::Completo},
    {0x8C123E14u, "copiar_palavras_par", &copiar_palavras_par, Retorno::R0, kTabela, Contrato::Completo},
    {0x8C123E20u, "copiar_palavras_impar", &copiar_palavras_impar, Retorno::R0, kTabela, Contrato::Completo},
    {0x8C123B38u, "dividir", &dividir, Retorno::R0, kErroSdiv, Contrato::Auxiliar},
    {0x8C123BECu, "dividir_sem_sinal", &dividir_sem_sinal, Retorno::R0, kErroUdiv, Contrato::Auxiliar},
    {0x8C123C94u, "resto", &resto, Retorno::R0, kErroSmod, Contrato::Auxiliar},
    {0x8C123D5Cu, "resto_sem_sinal", &resto_sem_sinal, Retorno::R0, kErroUmod, Contrato::Auxiliar},
};

} // namespace

std::span<const Elevada> elevadas_runtime_c() { return kLista; }

} // namespace hh::transitional::elevacao
