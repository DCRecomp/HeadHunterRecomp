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

// ---- HH15.11c: copias de string, o envoltorio do strcmp e a copia por palavras de tamanho qualquer ----

// 8C1247C0: envoltorio do strcmp (salva o PR e chama 8C124068 pelo pool em 8C1247D0)
constexpr Premissa kStrcmpEnvoltorio[] = {{0x8C1247D0u, 0x8C124068u}};
void strcmp_envoltorio(Sh4Context* c) {
    const uint32_t pr0 = c->pr, sp0 = c->r[15];
    cobrar(c, 4);
    c->r[15] = sp0 - 4;  // o PR salvo
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
    strcpy_alinhada_corpo(c, c->r[15], c->r[3], c->r[4], c->r[5], c->r[6]);
}

// 8C124018 (auxiliar do compilador): strcpy com r0 = destino e r1 = origem. Os dois alinhados: salto de cauda para
// 8C123F7C (o meio de 8C123F78, depois de empilhar r3 e r4; pool em 8C124064). Senao byte a byte, quatro por volta.
// r3 e r4 voltam da pilha; devolve o destino em r0 e r1 depois do zero.
constexpr Premissa kStrcpyAuxiliar[] = {{0x8C124064u, 0x8C123F7Cu}};
void strcpy_auxiliar(Sh4Context* c) {
    const uint32_t d = c->r[0], s = c->r[1], r3 = c->r[3], r4 = c->r[4], sp0 = c->r[15];
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
    c->r[15] = sp0 - 4;  // o PR salvo
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
    c->r[15] = sp0 - 8;  // r3 e r2 empilhados
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
};

} // namespace

std::span<const Elevada> elevadas_runtime_c() { return kLista; }

} // namespace hh::transitional::elevacao
