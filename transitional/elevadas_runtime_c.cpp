// SPDX-License-Identifier: MIT
// HH15.11a: a biblioteca do compilador e do runtime C (faixa 8C123B38-8C1248E0) elevada. Identificacao: o desmonte de
// cada funcao (comportamento) e a conferencia contra a gerada em cada chamada da rota (contrato). Os auxiliares do
// compilador tem convencao propria (argumentos em r0-r2, registradores preservados), entao todas tem contrato completo:
// cada registrador e o T ficam como a gerada deixa. Custo e back-edges do caminho original na mesma ordem (cobrar/volta);
// cada escrita na ordem da gerada.
#include "elevacao.h"

#include "cpu/sh4_mem.h"

namespace hh::transitional::elevacao {
namespace {

uint32_t sext8(uint8_t b) { return uint32_t(int32_t(int8_t(b))); }

// 8C1247E8 strlen(r4): conta os bytes ate o zero. Deixa r3 = 0 (o ultimo byte lido), r4 depois do zero, r5 = r0 = n.
void strlen_(Sh4Context* c) {
    const uint32_t s = c->r[4];
    cobrar(c, 2 + 3);  // bra (com o delay slot) e o primeiro teste
    uint32_t n = 0;
    for (uint8_t b; (b = ler8(c, s + n)) != 0;) {
        c->r[3] = sext8(b), c->r[4] = s + n + 1, c->r[5] = n, c->t = 0;  // no back-edge: o byte, o ponteiro, a contagem
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
    c->r[3] = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (i) {  // no back-edge: r0 = o destino, r7 = o contador, T = 0 (cmp/hs que nao chegou)
            c->r[0] = d + i, c->r[7] = i, c->t = 0;
            volta(c, 0x8C12464Au);
        }
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
        c->t = 1;  // o tst r0,r0 da entrada (revisao Codex HH15.11f); M/Q intactos
        return;
    }
    cobrar(c, 70);
    c->r[0] = c->r[1] = a / b;
    // HH15.11f: o estado do SR que o laco (div0u, 32 x rotcl/div1, rotcl) deixa: M = 0, T = 0 e Q = 1 - bit 0 do
    // quociente (com M = 0 cada div1 da T = !Q, e o ultimo rotcl poe esse T no bit 0; conferido em 200 081 casos com o
    // DIV1 do runtime). O contrato desta elevada nao confere M/Q/T, mas o do strtol, que a chama, confere o SR inteiro.
    c->sr = (c->sr & ~(kSrM | kSrQ)) | ((c->r[0] & 1) ? 0u : kSrQ);
    c->t  = 0;
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

// ---- HH15.11c: copias de string, o envoltorio do strcmp e a copia por palavras de tamanho qualquer ----

// 8C1247C0: envoltorio do strcmp (salva o PR e chama 8C124068 pelo pool em 8C1247D0)
constexpr Premissa kStrcmpEnvoltorio[] = {{0x8C1247D0u, 0x8C124068u}};
void strcmp_envoltorio(Sh4Context* c) {
    const uint32_t pr0 = c->pr, sp0 = c->r[15];
    cobrar(c, 4);
    escrever32(c, sp0 - 4, pr0);  // sts.l pr,@-r15 (o valor fica na pilha: o jogo copia lixo de pilha para fora dela)
    c->r[15] = sp0 - 4;
    c->r[3]  = 0x8C124068u;
    chamar(c, 0x8C124068u, &strcmp_, 0x8C1247C8u);
    cobrar(c, 3);
    c->r[15] = sp0, c->pr = pr0;
}

// A parte comum de 8C123F78 e 8C123F7C a partir de 8C123F80 (o trecho linear ate 8C123F84 ja cobrado): copia a string
// de r1 para r0 (os dois alinhados) palavra a palavra, quatro por volta, ate uma palavra com zero (cmp/str com r6 = 0);
// as palavras se alternam entre r4 (indice par) e r5 (impar), cada uma lida antes de gravar a anterior; depois byte a
// byte nessa palavra ate o zero (o quarto byte sem teste). r3-r6 voltam da pilha (r3/r4 empilhados por quem entrou);
// r15 fica sp0 - 16 durante o corpo.
void strcpy_alinhada_corpo(Sh4Context* c, uint32_t sp0, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6) {
    const uint32_t d = c->r[0], s = c->r[1];
    escrever32(c, sp0 - 12, r5), escrever32(c, sp0 - 16, r6);  // mov.l r5/r6,@-r15
    c->r[15]         = sp0 - 16;
    c->r[3] = d, c->r[6] = 0;
    uint32_t w = ler32(c, s);
    c->r[4]    = w;
    uint32_t j = 0;  // a palavra com o zero
    cobrar(c, 2);
    if (!tem_zero(w)) {
        for (j = 1;; ++j) {
            cobrar(c, 4);
            const uint32_t prox = ler32(c, s + 4 * j);
            c->r[(j & 1) ? 5 : 4] = prox;
            escrever32(c, d + 4 * (j - 1), w);
            w = prox;
            if (j % 4) {
                if (tem_zero(w)) {
                    cobrar(c, j % 4 == 3 ? 1 : 2);  // o add do r0 (bra com o add no slot, ou so o add)
                    break;
                }
                continue;
            }
            // o back-edge: r0 = o destino da palavra j, r1 depois dela, T = 0 (a palavra j - 1 sem zero)
            c->r[0] = d + 4 * j, c->r[1] = s + 4 * (j + 1), c->t = 0;
            volta(c, 0x8C123F86u);
            cobrar(c, 2);
            if (tem_zero(w)) break;
        }
    }
    const uint32_t bd = d + 4 * j, bs = s + 4 * j;
    c->r[6]           = bd;
    uint32_t lidos = 0;
    bool     t     = false;
    for (uint32_t k = 0; k < 4; ++k) {
        cobrar(c, k == 0 ? 6 : k == 3 ? 2 : 4);
        const uint8_t b = ler8(c, bs + k);
        ++lidos;
        escrever8(c, bd + k, b);
        c->r[0] = sext8(b);
        if (k == 3) break;  // o quarto byte sem teste: T fica o do terceiro (0)
        t = !b;
        if (t) break;
    }
    cobrar(c, 6);
    c->r[0] = d, c->r[1] = bs + lidos, c->t = t;
    c->r[3] = r3, c->r[4] = r4, c->r[5] = r5, c->r[6] = r6, c->r[15] = sp0;
}

// 8C123F78 (auxiliar do compilador): strcpy com r0 = destino e r1 = origem alinhados; devolve o destino em r0
void strcpy_alinhada(Sh4Context* c) {
    cobrar(c, 7);
    escrever32(c, c->r[15] - 4, c->r[3]), escrever32(c, c->r[15] - 8, c->r[4]);  // mov.l r3/r4,@-r15
    strcpy_alinhada_corpo(c, c->r[15], c->r[3], c->r[4], c->r[5], c->r[6]);
}

// 8C124018 (auxiliar do compilador): strcpy com r0 = destino e r1 = origem. Os dois alinhados: salto de cauda para
// 8C123F7C (o meio de 8C123F78, depois de empilhar r3 e r4; pool em 8C124064). Senao byte a byte, quatro por volta.
// r3 e r4 voltam da pilha; devolve o destino em r0 e r1 depois do zero.
constexpr Premissa kStrcpyAuxiliar[] = {{0x8C124064u, 0x8C123F7Cu}};
void strcpy_auxiliar(Sh4Context* c) {
    const uint32_t d = c->r[0], s = c->r[1], r3 = c->r[3], r4 = c->r[4], sp0 = c->r[15];
    escrever32(c, sp0 - 4, r3), escrever32(c, sp0 - 8, r4);  // mov.l r3/r4,@-r15
    cobrar(c, 9);
    if (!((d | s) & 3)) {
        cobrar(c, 3 + 5);  // mov.l/jmp/nop e o trecho linear de 8C123F7C
        strcpy_alinhada_corpo(c, sp0, r3, r4, c->r[5], c->r[6]);
        return;
    }
    c->r[15] = sp0 - 8;
    c->r[3] = d, c->r[4] = d;
    cobrar(c, 3);
    uint32_t i = 0;  // bytes lidos
    uint8_t  b = ler8(c, s);
    for (uint32_t g = 0;; ++g) {
        const uint32_t base = d + 4 * g;
        bool           fim  = false;
        for (uint32_t k = 0; k < 4 && !fim; ++k) {
            if (k) b = ler8(c, s + 4 * g + k);
            cobrar(c, k ? 4 : 3);
            escrever8(c, base + k, b);
            if (!b) fim = true, i = 4 * g + k + 1;
        }
        if (fim) break;
        cobrar(c, 3);
        b = ler8(c, s + 4 * g + 4);
        // o back-edge: r0 = o byte lido, r1 depois dele, r3 = o destino do grupo seguinte, T = 0
        c->r[0] = sext8(b), c->r[1] = s + 4 * g + 5, c->r[3] = base + 4, c->t = 0;
        volta(c, 0x8C124030u);
    }
    cobrar(c, 4);
    c->r[0] = d, c->r[1] = s + i, c->t = 1;
    c->r[3] = r3, c->r[4] = r4, c->r[15] = sp0;
}

// 8C1247D4 strcpy(r4, r5): o auxiliar 8C124018 (pool em 8C1247E4) com r0 = r4 e r1 = r5
constexpr Premissa kStrcpy[] = {{0x8C1247E4u, 0x8C124018u}};
void strcpy_(Sh4Context* c) {
    const uint32_t pr0 = c->pr, sp0 = c->r[15];
    cobrar(c, 5);
    escrever32(c, sp0 - 4, pr0);  // sts.l pr,@-r15
    c->r[15] = sp0 - 4;
    c->r[3] = 0x8C124018u, c->r[1] = c->r[5], c->r[0] = c->r[4];
    chamar(c, 0x8C124018u, &strcpy_auxiliar, 0x8C1247DEu);
    cobrar(c, 3);
    c->r[15] = sp0, c->pr = pr0;
}

// 8C124784 strcat(r4, r5): acha o zero de r4 (r6) e copia r5 (r7) ate o zero inclusive; devolve r4. r2 = o byte.
void strcat_(Sh4Context* c) {
    const uint32_t d = c->r[4], s = c->r[5];
    cobrar(c, 3);
    c->r[7]    = s;
    uint32_t p = d;
    for (;;) {
        cobrar(c, 3);
        const uint8_t b = ler8(c, p);
        c->r[2]         = sext8(b);
        if (!b) break;
        c->r[6] = p, c->t = 0;  // no back-edge: r6 ainda no byte lido
        volta(c, 0x8C12478Au);
        cobrar(c, 1);
        ++p;
    }
    for (uint32_t i = 0;; ++i) {
        cobrar(c, 6);
        const uint8_t b = ler8(c, s + i);
        escrever8(c, p + i, b);
        c->r[2] = b, c->r[6] = p + i + 1, c->r[7] = s + i + 1, c->t = !b;  // extu.b; o add do slot
        if (!b) break;
        volta(c, 0x8C124792u);
    }
    cobrar(c, 2);
    c->r[0] = d;
}

// 8C123EB8 (auxiliar do compilador): copia r0 bytes de r2 para r1 por palavras; r3 preservado (pilha). Ate 64 bytes
// (cmp/hs sem sinal): pela tabela de saltos (pool em 8C123EC8, tabela em 8C123F14, entrada r0/4 -> rotulo que copia
// as palavras de r0 - 4 ate 0, da mais alta para a mais baixa), r0 = a ultima palavra copiada (ou 0) e T = 1. Acima:
// laco de duas palavras por volta, sempre lendo uma palavra antes de testar o fim (a ultima lida fica em r0 sem ser
// gravada); r2 volta da pilha, r1 avanca 8 por volta completa, T = 0.
constexpr Premissa kTabela64[] = {
    {0x8C123EC8u, 0x8C123F14u}, {0x8C123F14u, 0x8C123F0Eu}, {0x8C123F18u, 0x8C123F0Au}, {0x8C123F1Cu, 0x8C123F06u},
    {0x8C123F20u, 0x8C123F02u}, {0x8C123F24u, 0x8C123EFEu}, {0x8C123F28u, 0x8C123EFAu}, {0x8C123F2Cu, 0x8C123EF6u},
    {0x8C123F30u, 0x8C123EF2u}, {0x8C123F34u, 0x8C123EEEu}, {0x8C123F38u, 0x8C123EEAu}, {0x8C123F3Cu, 0x8C123EE6u},
    {0x8C123F40u, 0x8C123EE2u}, {0x8C123F44u, 0x8C123EDEu}, {0x8C123F48u, 0x8C123EDAu}, {0x8C123F4Cu, 0x8C123ED6u},
    {0x8C123F50u, 0x8C123ED2u}, {0x8C123F54u, 0x8C123ECEu},
};
void copiar_palavras_n(Sh4Context* c) {
    const uint32_t n = c->r[0], d = c->r[1], s = c->r[2], r3 = c->r[3], sp0 = c->r[15];
    escrever32(c, sp0 - 4, r3);  // mov.l r3,@-r15
    cobrar(c, 4);
    if (n <= 64) {
        if (n & 3) sh4_fatal(c, "elevacao: copia por palavras fora do contrato (tamanho nao multiplo de 4)", 0x8C123EB8u, n);
        cobrar(c, 4 + 2 * (n / 4) + 2);
        uint32_t r0 = n;
        for (uint32_t o = n; o >= 4; o -= 4) {
            r0 = ler32(c, s + o - 4);
            escrever32(c, d + o - 4, r0);
        }
        c->r[0] = r0, c->r[3] = r3, c->t = 1;
        return;
    }
    escrever32(c, sp0 - 8, s);  // mov.l r2,@-r15
    c->r[15] = sp0 - 8;
    cobrar(c, 3);
    const uint32_t fim = s + n;
    uint32_t       p = s, q = d, w = 0;
    for (;;) {
        cobrar(c, 3);
        w = ler32(c, p), p += 4;
        if (!(fim >= p)) break;
        cobrar(c, 4);
        escrever32(c, q, w);
        w = ler32(c, p), p += 4;
        if (!(fim >= p)) break;
        cobrar(c, 3);
        escrever32(c, q + 4, w);
        q += 8;
        c->r[0] = w, c->r[1] = q, c->r[2] = p, c->r[3] = fim, c->t = 1;  // no back-edge
        volta(c, 0x8C123F5Eu);
    }
    cobrar(c, 3);
    c->r[0] = w, c->r[1] = q, c->r[2] = s, c->r[3] = r3, c->r[15] = sp0, c->t = 0;
}

// ---- HH15.11d: memcmp e strncpy ----

// 8C124508 memcmp(r4, r5, r6): byte a byte (r2 = byte de s2, r3 = byte de s1, com sinal) ate a diferenca ou n; depois
// rele o ultimo par sem sinal e devolve s1[k] - s2[k]. r5 = os bytes iguais, r4 depois do par, r7 = s2 + k, r3 = s2[k]
// sem sinal; T = 0 na diferenca, 1 com os n iguais (cmp/hs) ou n = 0 (tst, r0 = 0).
void memcmp_(Sh4Context* c) {
    const uint32_t s1 = c->r[4], s2 = c->r[5], n = c->r[6];
    cobrar(c, 3);
    c->r[7] = s2;
    if (n == 0) {
        cobrar(c, 2);
        c->r[0] = 0, c->t = 1;
        return;
    }
    cobrar(c, 4);
    c->r[5] = 0, c->r[2] = 0;
    uint32_t i = 0;
    bool     t = false;
    for (;;) {
        cobrar(c, 4);
        const uint8_t b2 = ler8(c, s2 + i), b1 = ler8(c, s1 + i);
        c->r[2] = sext8(b2), c->r[3] = sext8(b1), c->r[4] = s1 + i + 1, c->r[7] = s2 + i + 1;
        if (b1 != b2) break;
        cobrar(c, 3);
        c->r[5] = ++i;
        if (i >= n) {
            t = true;
            break;
        }
        c->t = 0;  // no back-edge: o par i - 1 igual
        volta(c, 0x8C12451Au);
    }
    cobrar(c, 10);
    const uint32_t k = c->r[4] - 1 - s1;  // o ultimo par comparado
    c->r[7]          = s2 + k;
    c->r[3]          = ler8(c, s2 + k);
    c->r[0]          = uint32_t(ler8(c, s1 + k)) - c->r[3];
    c->t             = t;
}

// 8C124870 strncpy(r4, r5, r6): copia ate o zero (exclusive) ou n bytes; o resto ate n com zeros (r1 = 0). Devolve r4;
// r5 = n (ou os copiados), r7 = r4 + r5, r0 da origem no zero (ou depois do ultimo copiado), r3 = o ultimo byte lido, T = 1.
void strncpy_(Sh4Context* c) {
    const uint32_t d = c->r[4], s = c->r[5], n = c->r[6];
    cobrar(c, 5);
    c->r[1] = 0, c->r[0] = s;
    uint32_t i = 0;
    for (;;) {
        cobrar(c, 2);
        if (i >= n) break;  // cmp/hs r6,r5
        cobrar(c, 3);
        const uint8_t b = ler8(c, s + i);
        c->r[3]         = sext8(b);
        if (!b) {
            cobrar(c, 2);  // o cmp/hs de novo (falso)
            for (;;) {
                cobrar(c, 5);
                escrever8(c, d + i, 0);
                ++i;
                if (i >= n) break;
                c->r[5] = i, c->r[7] = d + i, c->t = 0;  // no back-edge
                volta(c, 0x8C124890u);
            }
            break;
        }
        c->r[0] = s + i, c->r[5] = i, c->r[7] = d + i, c->t = 0;  // no back-edge: o byte lido ainda nao copiado
        volta(c, 0x8C12487Au);
        cobrar(c, 4);
        const uint8_t r = ler8(c, s + i);  // a gerada rele o byte depois do back-edge (revisao Codex HH15.11d)
        c->r[3]         = sext8(r);
        escrever8(c, d + i, r);
        ++i;
        c->r[0] = s + i;
    }
    cobrar(c, 2);
    c->r[0] = d, c->r[5] = i, c->r[7] = d + i, c->t = 1;
}

// ---- HH15.11e: memmove (fora do contrato: a gerada) ----

uint32_t sext16(uint16_t h) { return uint32_t(int32_t(int16_t(h))); }

// 8C12453C memmove(r4, r5, r6): devolve r4; r11-r13 empilhados (gravados) e restaurados. r7 = r4 | r5 | r6 escolhe a
// largura (palavra longa com os tres multiplos de 4, palavra com os tres pares, senao byte). Destino abaixo da origem:
// para a frente; acima: para tras, a partir do fim. Cada unidade lida e gravada antes da seguinte (sobreposicao como a
// gerada). Nos lacos r6 conta as unidades restantes; r11 = n >> 1 e r12 = n >> 2 (as contagens), r13 = origem + n
// (para tras); T = 1 no fim do laco.
// HH15.11e: com um dos dois intervalos fora da RAM principal (a rota chama memmove(0, ...): a gerada grava na area 0
// pelo caminho de hardware), a chamada volta para a gerada, antes de qualquer efeito.
// n > 0. O intervalo inteiro no bloco de 16 MB de `a`, sem passar do fim dele: nem espelho nem volta nos 32 bits (revisao
// Codex HH15.11e: a forma anterior aceitava a = 0x8CFFFFFF, n = 0xFF000002, cujo fim da a volta e cai na RAM).
bool na_ram(uint32_t a, uint32_t n) { return sh4_is_ram(a) && n - 1 <= kRamMask - (a & kRamMask); }

void memmove_(Sh4Context* c) {
    const uint32_t d = c->r[4], s = c->r[5], n = c->r[6], sp0 = c->r[15];
    if (n && d != s && !(na_ram(d, n) && na_ram(s, n))) return delegar(c, 0x8C12453Cu);
    const uint32_t r11 = c->r[11], r12 = c->r[12], r13 = c->r[13];
    cobrar(c, 5);
    escrever32(c, sp0 - 4, r13), escrever32(c, sp0 - 8, r12), escrever32(c, sp0 - 12, r11);  // mov.l r13/r12/r11,@-r15
    c->r[15] = sp0 - 12;
    // mov.l @r15+,r11 / mov r4,r0 / mov.l @r15+,r12 / mov.l @r15+,r13: relidos da pilha, como a gerada (revisao Codex
    // HH15.11e: um destino sobre os empilhamentos muda o que volta)
    auto fim = [&] {
        cobrar(c, 5);
        c->r[11] = ler32(c, sp0 - 12), c->r[0] = d, c->r[12] = ler32(c, sp0 - 8), c->r[13] = ler32(c, sp0 - 4);
        c->r[15] = sp0;
    };
    if (d == s) {
        c->t = 1;
        fim();
        return;
    }
    cobrar(c, 3);
    c->r[2] = 0;
    if (n == 0) {
        c->t = 0;
        fim();
        return;
    }
    cobrar(c, 9);
    const uint32_t ou = d | s | n;
    c->r[7] = ou, c->r[11] = n >> 1, c->r[12] = n >> 2;
    if (d < s) {  // para a frente
        cobrar(c, 3);
        c->r[3] = 1;
        if (ou & 1) {  // bytes: r0 = a origem, r7 = o destino
            cobrar(c, 2);
            for (uint32_t k = 1;; ++k) {
                cobrar(c, 6);
                const uint8_t b = ler8(c, s + k - 1);
                escrever8(c, d + k - 1, b);
                c->r[0] = s + k, c->r[3] = sext8(b), c->r[6] = n - k, c->r[7] = d + k;
                if (k == n) break;
                c->t = 0;
                volta(c, 0x8C124596u);
            }
        } else {
            cobrar(c, 3);
            c->r[1] = 3;
            const uint32_t u = (ou & 3) ? 2 : 4, m = n / u;  // r5 = a origem, r7 = o destino
            cobrar(c, 2);
            for (uint32_t k = 1;; ++k) {
                cobrar(c, 6);
                const uint32_t o = u * (k - 1);
                if (u == 2) {
                    const uint16_t h = ler16(c, s + o);
                    escrever16(c, d + o, h);
                    c->r[3] = sext16(h);
                } else {
                    const uint32_t w = ler32(c, s + o);
                    escrever32(c, d + o, w);
                    c->r[3] = w;
                }
                c->r[5] = s + u * k, c->r[6] = m - k, c->r[7] = d + u * k;
                if (k == m) break;
                c->t = 0;
                volta(c, u == 2 ? 0x8C124582u : 0x8C12456Eu);
            }
        }
        cobrar(c, 2);  // bra para o epilogo
    } else {  // para tras: r0 = destino + n, r13 = origem + n
        cobrar(c, 7);
        c->r[2] = 1, c->r[0] = d + n, c->r[13] = s + n;
        if (ou & 1) {  // bytes: r7 = a origem, r5 = o destino; o laco cai direto no epilogo
            cobrar(c, 2);
            for (uint32_t k = 1;; ++k) {
                cobrar(c, 5);
                const uint8_t b = ler8(c, s + n - k);
                escrever8(c, d + n - k, b);
                c->r[3] = sext8(b), c->r[5] = d + n - k, c->r[6] = n - k, c->r[7] = s + n - k;
                if (k == n) break;
                c->t = 0;
                volta(c, 0x8C1245E6u);
            }
        } else {
            cobrar(c, 3);
            c->r[1] = 3;
            cobrar(c, 3);
            if (ou & 3) {  // palavras: r5 = a origem, r7 = o destino
                for (uint32_t k = 1, m = n / 2;; ++k) {
                    cobrar(c, 5);
                    const uint16_t h = ler16(c, s + n - 2 * k);
                    escrever16(c, d + n - 2 * k, h);
                    c->r[3] = sext16(h), c->r[5] = s + n - 2 * k, c->r[6] = m - k, c->r[7] = d + n - 2 * k;
                    if (k == m) break;
                    c->t = 0;
                    volta(c, 0x8C1245D4u);
                }
            } else {  // palavras longas: r7 = a origem, r5 = o destino
                for (uint32_t k = 1, m = n / 4;; ++k) {
                    cobrar(c, 5);
                    const uint32_t w = ler32(c, s + n - 4 * k);
                    escrever32(c, d + n - 4 * k, w);
                    c->r[3] = w, c->r[5] = d + n - 4 * k, c->r[6] = m - k, c->r[7] = s + n - 4 * k;
                    if (k == m) break;
                    c->t = 0;
                    volta(c, 0x8C1245C0u);
                }
            }
            cobrar(c, 2);  // bra para o epilogo
        }
    }
    c->t = 1;
    fim();
}

// ---- HH15.11f: strtol, atoi e o limite de 512 caracteres ----
// Transcricao bloco a bloco de generated/disc1/funcs_008.cpp (f_8C126A74, f_8C1248E0, f_8C12435C): cada cobrar() e o
// HH_CYCLES do bloco, cada volta() o HH_POLL, cada chamar() o HH_CALL (o jsr @r11 da gerada passa pelo sh4_dispatch, que
// chega a elevada pelo slot). Os rotulos L_xxxx sao os da gerada (8C12xxxx).

// 8C126A74 (do SDK): r4 = a posicao atual, r5 = o inicio da string. Se *r4 != 0 e r4 - r5 >= 0x200 (512), grava 0x450
// no errno (8C30FA50) e devolve 1; senao 0. O strtol a chama pelo ponteiro em r11 a cada caractere consumido.
// Premissas: o dword em 8C126A90 (nop + 0x0200 do mov.w), 8C126A94 (0x0450) e o endereco do errno.
constexpr Premissa kLimiteTexto[] = {{0x8C126A90u, 0x02000009u}, {0x8C126A94u, 0x00000450u}, {0x8C126A98u, 0x8C30FA50u}};
void limite_texto(Sh4Context* c) {
    cobrar(c, 3);
    const uint8_t b = ler8(c, c->r[4]);
    c->r[3]         = sext8(b);
    c->t            = b == 0;
    if (!c->t) {
        cobrar(c, 4);
        c->r[3] = 0x200;
        c->r[4] -= c->r[5];
        c->t = int32_t(c->r[4]) >= 0x200;
        if (c->t) {
            cobrar(c, 5);
            c->r[2] = 0x8C30FA50u, c->r[1] = 0x450;
            escrever32(c, 0x8C30FA50u, 0x450);
            c->r[0] = 1;
            return;
        }
    }
    cobrar(c, 3);
    c->r[0] = 0;
}

// 8C1248E0 strtol(r4 = s, r5 = endptr, r6 = base): espacos (bit 0x40 da tabela de tipos em 8C145118), sinal, prefixo
// 0/0x (base 0 deduzida: 8, 10 ou 16; base 16 aceita 0x), base fora de 2..36 grava 0x4B0 no errno, zeros a esquerda,
// digitos ate o maior da base (bits 7 da tabela; letras pelo bit 1 + 0x20), estouro pela divisao sem sinal 8C123BEC
// ((LONG_MAX + neg - digito) / base) com 0x44C no errno e LONG_MAX/LONG_MIN, sufixos u/l/ul/lu, *endptr e o limite de
// 512 caracteres por 8C126A74 (estouro devolve 0 sem *endptr). Quadro: r14..r8, pr e macl empilhados e 16 bytes
// (+0 = o resultado ou a marca de estouro, +4 = negativo, +8 = o maior digito, +12 = endptr).
constexpr uint32_t kErrnoStrtol = 0x8C30FA50u, kTiposCaractere = 0x8C145118u;
constexpr Premissa kStrtol[] = {
    {0x8C124ACCu, 0x8C126A74u}, {0x8C124AD0u, kTiposCaractere}, {0x8C124AD4u, 0x7FFFFFFFu}, {0x8C124AD8u, 0x8C123BECu},
    {0x8C124B90u, 0x044C6EF6u}, {0x8C124B94u, 0x000004B0u},    {0x8C124B98u, kErrnoStrtol}, {0x8C124B9Cu, 0x7FFFFFFFu},
    {0x8C124BA0u, 0x80000000u},
};
void strtol_(Sh4Context* c) {
    auto& r = c->r;
    // r4 = r14 no delay slot do jsr @r11; depois o tst r0,r0 (HH_CYCLES 2). true: passou do limite (r0 != 0)
    auto limite = [&](uint32_t retorno) {
        r[4] = r[14];
        chamar(c, 0x8C126A74u, &limite_texto, retorno);
        cobrar(c, 2);
        c->t = r[0] == 0;
        return !c->t;
    };
    auto tipo = [&](uint32_t ch) { return uint32_t(ler8(c, (ch & 0xFFu) + r[13])); };  // mov.b @(r0,r13) + extu.b
    cobrar(c, 22);
    uint32_t sp = r[15];
    for (int k = 14; k >= 8; --k) sp -= 4, escrever32(c, sp, r[k]);
    sp -= 4, escrever32(c, sp, c->pr);
    sp -= 4, escrever32(c, sp, c->macl);
    r[15] = sp - 16;
    r[3] = 0, r[11] = 0x8C126A74u, r[2] = 0, r[14] = r[4], r[13] = kTiposCaractere, r[9] = r[14];
    escrever32(c, r[15] + 12, r[5]);
    r[10] = r[6];
    escrever32(c, r[15], 0);
    escrever32(c, r[15] + 4, 0);
    r[12] = 0x40;
    goto L_491C;
L_490C:
    cobrar(c, 4);
    r[5] = r[9], r[14] += 1;
    if (limite(0x8C124914u)) {
        cobrar(c, 2);
        goto L_4B66;
    }
L_491C:
    cobrar(c, 6);
    r[0] = sext8(ler8(c, r[14])) & 0xFFu;
    r[3] = tipo(r[0]);
    c->t = (r[3] & r[12]) == 0;
    if (!c->t) {
        volta(c, 0x8C12490Cu);
        goto L_490C;
    }
    cobrar(c, 5);
    r[0]  = sext8(ler8(c, r[14]));
    r[12] = 1;
    c->t  = r[0] == 0x2Bu;  // '+'
    r[4]  = r[0];
    if (c->t) goto L_4938;
    cobrar(c, 3);
    r[0] = r[4];
    c->t = r[0] == 0x2Du;  // '-'
    if (!c->t) goto L_4950;
L_4938:
    cobrar(c, 4);
    r[0] = sext8(ler8(c, r[14]));
    c->t = r[0] == 0x2Du;
    {
        const bool menos = c->t;
        r[14] += 1;
        if (menos) cobrar(c, 1), escrever32(c, r[15] + 4, r[12]);
    }
    cobrar(c, 3);
    r[5] = r[9];
    if (limite(0x8C124948u)) {
        cobrar(c, 2);
        goto L_4B66;
    }
L_4950:
    cobrar(c, 2);
    c->t = r[10] == 0;
    if (!c->t) goto L_499E;
    cobrar(c, 3);
    r[0] = sext8(ler8(c, r[14]));
    c->t = r[0] == 0x30u;  // '0'
    if (!c->t) {
        cobrar(c, 2);
        r[10] = 10;
        goto L_49D4;
    }
    cobrar(c, 4);
    r[5] = r[9], r[14] += 1;
    if (limite(0x8C124962u)) {
        cobrar(c, 2);
        goto L_4B66;
    }
    cobrar(c, 6);
    r[4] = sext8(ler8(c, r[14]));
    r[0] = tipo(r[4]);
    c->t = (r[0] & r[12]) == 0;
    if (c->t) cobrar(c, 1), r[0] = r[4];
    else cobrar(c, 3), r[0] = r[4] + 0x20u;
    cobrar(c, 2);
    c->t = r[0] == 0x78u;  // 'x'
    if (!c->t) {
        cobrar(c, 2);
        r[10] = 8;
        goto L_49D4;
    }
    cobrar(c, 4);
    r[5] = r[9], r[14] += 1;
    if (limite(0x8C12498Au)) {
        cobrar(c, 2);
        goto L_4B66;
    }
    cobrar(c, 2);
    r[10] = 16;
    goto L_49D4;
L_499E:
    cobrar(c, 3);
    r[0] = r[10];
    c->t = r[0] == 0x10u;
    if (!c->t) goto L_49D4;
    cobrar(c, 3);
    r[0] = sext8(ler8(c, r[14]));
    c->t = r[0] == 0x30u;
    if (!c->t) goto L_49D4;
    cobrar(c, 7);
    r[14] += 1;
    r[4] = sext8(ler8(c, r[14]));
    r[0] = tipo(r[4]);
    c->t = (r[0] & r[12]) == 0;
    if (c->t) cobrar(c, 1), r[0] = r[4];
    else cobrar(c, 3), r[0] = r[4] + 0x20u;
    cobrar(c, 3);
    c->t = r[0] == 0x78u;
    {
        const bool x = c->t;
        r[5]         = r[9];
        if (x) cobrar(c, 1), r[14] += 1;
    }
    cobrar(c, 2);
    if (limite(0x8C1249CCu)) {
        cobrar(c, 2);
        goto L_4B66;
    }
L_49D4:
    cobrar(c, 3);
    r[3] = 2;
    c->t = int32_t(r[10]) >= 2;
    if (!c->t) {
        cobrar(c, 2);
        goto L_4B6A;
    }
    cobrar(c, 3);
    r[1] = 36;
    c->t = int32_t(r[10]) > 36;
    if (c->t) {
        cobrar(c, 2);
        goto L_4B6A;
    }
    cobrar(c, 3);
    r[3] = 10;
    c->t = int32_t(r[10]) > 10;
    if (c->t) cobrar(c, 3), r[0] = r[10] + 0x56u;
    else cobrar(c, 2), r[0] = r[10] + 0x2Fu;
    cobrar(c, 8);
    escrever8(c, r[15] + 8, uint8_t(r[0]));
    r[3] = 7;
    r[4] = sext8(ler8(c, r[14]));
    r[0] = r[4] & 0xFFu;
    r[5] = tipo(r[0]);
    c->t = (r[5] & r[3]) == 0;
    if (c->t) goto L_4AF2;
    cobrar(c, 3);
    r[2] = 1;
    c->t = (r[5] & r[2]) == 0;
    if (c->t) cobrar(c, 1), r[1] = r[4];
    else cobrar(c, 3), r[1] = r[4] + 0x20u;
    cobrar(c, 3);
    r[0] = sext8(ler8(c, r[15] + 8));
    c->t = int32_t(r[1]) > int32_t(r[0]);
    if (c->t) goto L_4AF2;
    cobrar(c, 2);
    goto L_4A30;
L_4A20:
    cobrar(c, 4);
    r[5] = r[9], r[14] += 1;
    if (limite(0x8C124A28u)) {
        cobrar(c, 2);
        goto L_4B66;
    }
L_4A30:
    cobrar(c, 3);
    r[0] = sext8(ler8(c, r[14]));
    c->t = r[0] == 0x30u;
    if (c->t) {
        volta(c, 0x8C124A20u);
        goto L_4A20;
    }
    cobrar(c, 4);
    r[2] = 0;
    escrever32(c, r[15], 0);
    r[8] = 0;
    goto L_4A96;
L_4A3E:
    cobrar(c, 3);
    r[2] = ler32(c, r[15]);
    c->t = r[2] == 0;
    if (!c->t) goto L_4A8A;
    cobrar(c, 6);
    r[0] = sext8(ler8(c, r[14])) & 0xFFu;
    r[0] = tipo(r[0]);
    c->t = (r[0] & 4u) == 0;
    if (!c->t) {
        cobrar(c, 3);
        r[4] = sext8(ler8(c, r[14])) + uint32_t(-48);
    } else {
        cobrar(c, 6);
        r[5] = sext8(ler8(c, r[14]));
        r[0] = r[5] & 0xFFu;
        r[4] = tipo(r[0]);
        c->t = (r[4] & r[12]) == 0;
        if (c->t) cobrar(c, 1), r[4] = r[5];
        else cobrar(c, 3), r[4] = r[5] + 0x20u;
        cobrar(c, 1);
        r[4] += uint32_t(-87);
    }
    cobrar(c, 7);
    r[1] = ler32(c, r[15] + 4);
    r[3] = 0x7FFFFFFFu;
    r[2] = 0x8C123BECu;
    r[1] -= r[4];
    r[1] += r[3];
    r[0] = r[10];
    chamar(c, 0x8C123BECu, &dividir_sem_sinal, 0x8C124A7Au);
    cobrar(c, 2);
    c->t = r[8] > r[0];
    if (c->t) {
        cobrar(c, 2);
        escrever32(c, r[15], r[12]);
        goto L_4A8A;
    }
    cobrar(c, 4);
    c->macl = r[8] * r[10];
    r[1]    = c->macl;
    r[8]    = r[1] + r[4];
L_4A8A:
    cobrar(c, 4);
    r[5] = r[9], r[14] += 1;
    if (limite(0x8C124A92u)) goto L_4B66;  // bf direto, sem o bra
L_4A96:
    cobrar(c, 7);
    r[4] = sext8(ler8(c, r[14]));
    r[3] = 7;
    r[0] = r[4] & 0xFFu;
    r[5] = tipo(r[0]);
    c->t = (r[5] & r[3]) == 0;
    if (c->t) goto L_4AB8;
    cobrar(c, 3);
    r[2] = 1;
    c->t = (r[5] & r[2]) == 0;
    if (c->t) cobrar(c, 1), r[1] = r[4];
    else cobrar(c, 3), r[1] = r[4] + 0x20u;
    cobrar(c, 3);
    r[0] = sext8(ler8(c, r[15] + 8));
    c->t = int32_t(r[1]) > int32_t(r[0]);
    if (!c->t) {
        volta(c, 0x8C124A3Eu);
        goto L_4A3E;
    }
L_4AB8:
    cobrar(c, 3);
    r[2] = ler32(c, r[15]);
    c->t = r[2] == 0;
    if (!c->t) goto L_4ADC;
    cobrar(c, 3);
    r[3] = ler32(c, r[15] + 4);
    c->t = r[3] == 0;
    if (!c->t) cobrar(c, 1), r[8] = 0u - r[8];
    cobrar(c, 2);
    escrever32(c, r[15], r[8]);
    goto L_4AF2;
L_4ADC:
    cobrar(c, 6);
    r[3] = kErrnoStrtol, r[2] = 0x44C;
    escrever32(c, r[3], r[2]);
    r[1] = ler32(c, r[15] + 4);
    c->t = r[1] == 0;
    if (c->t) {
        cobrar(c, 3);
        r[1] = 0x7FFFFFFFu;
        escrever32(c, r[15], r[1]);
    } else {
        cobrar(c, 2);
        r[2] = 0x80000000u;
        escrever32(c, r[15], r[2]);
    }
L_4AF2:
    // sufixos: u, ul, l, lu (o bit 0x01 da tabela e maiuscula: + 0x20)
    {
        auto letra = [&](bool avancar) {  // le r[14] (depois do avanco) em r5 e devolve a minuscula em r0
            if (avancar) r[14] += 1;
            r[5] = sext8(ler8(c, r[14]));
            r[0] = r[5] & 0xFFu;
            r[0] = uint32_t(ler8(c, r[0] + r[4]));
            c->t = (r[0] & r[12]) == 0;
            if (c->t) cobrar(c, 1), r[0] = r[5];
            else cobrar(c, 3), r[0] = r[5] + 0x20u;
            cobrar(c, 2);
        };
        cobrar(c, 7);
        r[4] = r[13];
        letra(false);
        c->t = r[0] == 0x75u;  // 'u'
        if (c->t) {
            cobrar(c, 7);
            letra(true);
            c->t = r[0] == 0x6Cu;  // 'l'
            if (c->t) cobrar(c, 1), r[14] += 1;
        }
        cobrar(c, 6);
        letra(false);
        c->t = r[0] == 0x6Cu;
        if (c->t) {
            cobrar(c, 7);
            letra(true);
            c->t = r[0] == 0x75u;
            if (c->t) cobrar(c, 1), r[14] += 1;
        }
    }
    cobrar(c, 3);
    r[5] = r[9];
    if (!limite(0x8C124B62u)) goto L_4B70;
L_4B66:
    cobrar(c, 2);
    r[0] = 0;
    goto L_4B7C;
L_4B6A:
    cobrar(c, 3);
    r[3] = kErrnoStrtol, r[2] = 0x4B0;
    escrever32(c, r[3], r[2]);
L_4B70:
    cobrar(c, 3);
    r[1] = ler32(c, r[15] + 12);
    c->t = r[1] == 0;
    if (!c->t) {
        cobrar(c, 2);
        r[1] = ler32(c, r[15] + 12);
        escrever32(c, r[1], r[14]);
    }
    cobrar(c, 1);
    r[0] = ler32(c, r[15]);
L_4B7C:
    cobrar(c, 11);
    sp      = r[15] + 16;
    c->macl = ler32(c, sp), sp += 4;
    c->pr   = ler32(c, sp), sp += 4;
    for (int k = 8; k <= 14; ++k) r[k] = ler32(c, sp), sp += 4;
    r[15] = sp;
}

// 8C12435C atoi(r4): strtol(r4, NULL, 10) por salto de cauda (pool em 8C124364); r3 = o endereco do strtol
constexpr Premissa kAtoi[] = {{0x8C124364u, 0x8C1248E0u}};
void atoi_(Sh4Context* c) {
    cobrar(c, 4);
    c->r[3] = 0x8C1248E0u, c->r[6] = 10, c->r[5] = 0;
    strtol_(c);
}

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
    // HH15.11c
    {0x8C1247C0u, "strcmp_envoltorio", &strcmp_envoltorio, Retorno::R0, kStrcmpEnvoltorio, Contrato::Completo},
    {0x8C123F78u, "strcpy_alinhada", &strcpy_alinhada, Retorno::R0, {}, Contrato::Completo},
    {0x8C124018u, "strcpy_auxiliar", &strcpy_auxiliar, Retorno::R0, kStrcpyAuxiliar, Contrato::Completo},
    {0x8C1247D4u, "strcpy", &strcpy_, Retorno::R0, kStrcpy, Contrato::Completo},
    {0x8C124784u, "strcat", &strcat_, Retorno::R0, {}, Contrato::Completo},
    {0x8C123EB8u, "copiar_palavras_n", &copiar_palavras_n, Retorno::R0, kTabela64, Contrato::Completo},
    // HH15.11d
    {0x8C124508u, "memcmp", &memcmp_, Retorno::R0, {}, Contrato::Completo},
    {0x8C124870u, "strncpy", &strncpy_, Retorno::R0, {}, Contrato::Completo},
    // HH15.11e
    {0x8C12453Cu, "memmove", &memmove_, Retorno::R0, {}, Contrato::Completo},
    // HH15.11f
    {0x8C126A74u, "limite_texto", &limite_texto, Retorno::R0, kLimiteTexto, Contrato::Completo},
    {0x8C1248E0u, "strtol", &strtol_, Retorno::R0, kStrtol, Contrato::Completo},
    {0x8C12435Cu, "atoi", &atoi_, Retorno::R0, kAtoi, Contrato::Completo},
};

} // namespace

std::span<const Elevada> elevadas_runtime_c() { return kLista; }

} // namespace hh::transitional::elevacao
