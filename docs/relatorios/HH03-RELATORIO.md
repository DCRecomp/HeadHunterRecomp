# HH03 — RELATÓRIO: Núcleo SH-4 (Decodificador + Executor Semântico)

Data: 2026-10-24 · Nível de validação: V2 · Estado: **CONCLUIDA**

## 1. Objetivo e escopo realizado

- Tabela declarativa completa das formas SH-4 usadas pelo SH7091
  (`translator/sh4/src/decoder_table.inc` + enum `Op` gerado em
  `translator/sh4/include/sh4/op_enum.hpp`): 197 retângulos
  (`mask,value)->(mnemônico,fmt)` cobrindo 65.536 opcodes (59.051 legais,
  6.485 ilegais) sem sobreposição de destino.
- Executor semântico de passo único (`translator/sh4/src/cpu.cpp`):
  aritmética/carry, shifts/rotações, divisões (rotinas div0u/div0s/rotcl /
  div1/Addc do manual), stores/loads com extensão de sinal, GBR, pilha,
  registradores de controle (SR/GBR/VBR/SSR/SPC/SGR/DBR/MACH/MACL/PR/FPUL/
  FPSCR/bancos), branches com delay slot e latch pré-slot, FPU single/par
  (PR/SZ), `fipr`/`ftrv`, `fsca`, cache-aware hooks e illegal hooks.
- Testes exaustivos: classificação dos 65.536 opcodes (legais/ilegais por
  major, pureza por fmt, spot-checks) + bateria semântica com contrato por
  opcode e exercício integral sem dados do jogo.
- Regeneração do pipeline do atlas com 1 divergência documentada (§3.5).

## 2. Arquivos alterados

| Arquivo | Mudança |
|---|---|
| `translator/sh4/src/cpu.cpp` | 4 correções do executor (§3.1–3.4) |
| `translator/sh4/include/sh4/op_enum.hpp` | regenerado (130 ops) |
| `translator/sh4/src/decoder_table.inc` | regenerado (197 linhas; 0xFFFD ilegal) |
| `analysis/hh03/atlas_shapes.txt` | retângulo `fsca@0xFFFF/0xFFFD` removido + comentário de divergência; parsers agora pulam `#` |
| `analysis/hh03/gen_decoder_table.py` | `MANUAL_WINS` documentado; gates de divergência |
| `analysis/hh03/verify_atlas.py` | mesma exceção documentada |
| `analysis/hh03/check_conflict.py` | mesma exceção documentada |
| `tests/hh03_decoder_tests.cpp` | contagens 59051/6485, major F=305, spot-checks 0xFFFD ilegal / 0xFEFD fsca |
| `tests/hh03_semantics_tests.cpp` | correções extensivas de contratos de teste (§3.6) |

## 3. Evidências e decisões

Regra aplicada: fix do **executor** exige prova (MIT + SLEIGH ou probe
reprodutível); falha de teste com exec correto corre o **teste**. Cada
correção abaixo tem: fontes, comando, endereço e confirmação independente.

### 3.1 BRAF/BSRF usavam r[m], o correto é r[n]
- Encoding `0000nnnn00100011`/`0000nnnn00000011`: campo é `n` (bits 11‑8)
  no MIT (`sh4_isa.inc`, `"%n"`) e no SLEIGH (`N_0`). O executor saltava
  para `addr+4+r[m]`.
- Probe: `hh03_probe.exe op=0x0F23 r15=0x30` → PC = `A+4+0x30` pós-slot com
  slot mexendo em r15 (latch antes do slot) — antes do fix leitura r[m]=r0=0.
- Ajustes de teste: `bsrf` 0x0F33 (incorreto, sem encoding) → 0x0F03.

### 3.2 `creg_of` perdia STC.L DBR (0x4FF2)
- Escrita original `(op >> 4) == 0xF` era 1 bit além; correto é
  `(op & 0xF0) == 0xF0` (bits 7‑4 = 0xF).
- Probe: `hh03_probe.exe op=0x4FF2 r15=0x8000 step=1` → antes: memória em
  0x7FFC = 0 (DBR não reconhecido); após fix: valor do DBR empilhado e
  `r15 = 0x7FFC`.
- Fontes: MIT `STC_L_DBR` + tabela do decodificador (mask 0xF0FF value 0x4FA2
  via famílias STCL).

### 3.3 FSQRT single truncava para 32 bits
- Segundo `set_fr(n, dn(...))` sem `u32(...)` passava `float` e truncava o
  padrão de bits. Probe: entrada fr9 = 0x40800000 (4.0f) → saída
  `fr9 = 0x00000002` (correto após fix: 0x3FB504F3 √4 → 1.41421356f...);

### 3.4 Hook `on_cache` recebia o enum `row.op`, não o opcode cru
- Contrato do hook = **opcode cru de 16 bits** (especificação viva no teste
  de hooks). Fix: `on_cache(*this, r[n], op)`. Hook conta exatamente 1
  consulta por `step`, kinds observados = opcodes de 16 bits.

### 3.5 Divergência documentada vs SLEIGH: 0xFFFD é ILEGAL
- SLEIGH (`sleigh_truth2.txt`) etiqueta 0xFFFD como `fsca FPUL,dr14`.
- Evidência contrária (3 fontes independentes):
  1. MIT `sh4_isa.inc`: `FSCA = "1111nnn011111101"` — bit 8 = 0 obrigatório
     (paridade do par DRn: encodings pares `0xF0FD…0xFEFD`).
  2. `dream-recomp` (oráculo externo GPL-2.0, decodificador real): FSCA =
     `{0xF1FF, 0xF0FD}`, sem linha para 0xFFFD → ILEGAL.
  3. Análise do `.sinc`: o símbolo `FPUL` não restringe bits e a tripla
     sobreposição `DRN_0/FRN_1/FRN_2` em (9,11) com listas de anexos
     pares/ímpares resolve para o mesmo `dr14` em 0xFFFD (duplicata
     artificial); nenhum outro padrão MIT casa 0xFFFD (FTRV exige bits
     9‑8 = 01; FRCHG/FSCHG têm encodings únicos diferentes).
- Decisão: manual/MIT/uso real vencem → 0xFFFD **ILEGAL** na nossa tabela.
- A diferença com o oráculo SLEIGH fica em `MANUAL_WINS` = {0xFFFD} em
  `gen_decoder_table.py`/`verify_atlas.py`/`check_conflict.py`, no
  comentário do atlas e nos testes C++ (1 divergência em 65.536, 0 fora
  dela, provado pelos gates dos scripts).

### 3.6 Correções de teste (execução confirmada correta antes)
- `mov #imm` não altera T (probe): blocos bt/bf/bt/s/bf/s passam a usar
  `tst #0,r0` (0xC800, r0=0) para setar T.
- Alvo de branches `addr+4+2*disp` (manual): bt/bf +1 tomado → A+8;
  bt/s +2 → A+10; bf/s não-tomado linear.
- Troca de banco SR: `set_sr` troca quando (MD ∧ RB) muda; 0x70000123
  (MD=1,RB=1) **não** troca; `rte` com `ssr=0x500001C0` (MD=1,RB=0) troca
  (o comentário antigo do teste mensurava errado os bits do SSR).
- `ftrv`: fonte = destino = FVn (comentário MIT "FVn = XMTRX * FVn,
  a = n*4" + SLEIGH lê/escreve FR[FVN+k]); FV3 com XF=diag(2): fr12..15 =
  (10,12,14,16) e FV0 intocado.
- SHAD/SHLD: s≥0 desloca por `s & 31` (MIT + SLEIGH `s & 0x1f`); s=32 ⇒ sem
  deslocamento (r1 inalterado em 0x40000000); shar de 0x80000000 →
  0xC0000000, T=0 (bit0 da fonte).
- Rotina do manual de divisão exige op por slot: 32/16 (操 shll16, div0u,
  16×div1, rotcl, extu.w — **20 instruções**; com 19 o `extu.w` não roda e
  r1 sai 0xFFF8xxxx — probe confirma) e 32/32 sinalizado (72 slots; um só
  `CHECK` final após `addc r3,r1`, protocolo MIT `test_ops.cpp`).
- Registradores de controle: leitura/escrita/push/pop separados por família
  (st plain, ld plain, stc.l/sts.l push, ldc.l/lds.l pop); pop por
  pós-incremento precisa `r15 = 0x7FFC` antes (lê `[0x7FFC]`, volta a 0x8000).
- Stores/loads tamanho .b/.w intercalados com passos (mov.b antes do store
  .w sobre 0x4000); `ld disp .w` usa disp×2 (alvo 0x6010).
- Div 32/32 sinal: sem `subc` intermediário antes do check (o protocolo MIT
  computa com `subc` dentro do setup, não entre rotcl e addc).

## 4. Comandos de build/teste e resultados completos

```
cmake --build build --config Debug --target sh4_core hh03_probe hh03_semantics_tests   → OK
cmd /c "build\tests\Debug\hh03_semantics_tests.exe > build\hh03_sem.out 2>&1"           → OK - hh03_semantics (0 FALHA)
cmd /c "build\tests\Debug\hh03_decoder_tests.exe"                                      → OK - hh03_decoder
python analysis/hh03/gen_decoder_table.py
    shapes mapeados: 175/175; diferenças fora da exceção: 0;
    exceção aplicada: 0xFFFD ilegal; linhas: 197; 130 ops
python analysis/hh03/verify_atlas.py
    retângulos lidos: 188; impuros: 0; sem-cluster: 0; 2+ destinos: 0
python analysis/hh03/check_conflict.py
    sobreposições mesmo-destino: 0; opcodes com conflito de destino: 0
python analysis/hh03/check_fmt_merges.py                                              → OK (agrupamentos coerentes)
ctest --test-dir build -C Debug                                                       → 4/4 passed
cmake --build build --config Release                                                  → OK
ctest --test-dir build -C Release                                                     → 4/4 passed
```

Saída semântica completa preservada em `build\hh03_sem.out` (saída local
ignorada). Probes recorrentes via `build\tests\Debug\hh03_probe.exe`
(endereços 0x90000+, slots `opA…`, passos `step=N`).

## 5. Comparação com referência/oráculo

| Fonte | Escopo | Resultado |
|---|---|---|
| SLEIGH/Ghidra SuperH4 (sleigh_truth2.txt) | 65.536 opcodes | 0 diferenças fora de 0xFFFD; 1 divergência documentada (execução SLEIGH fica com fsca em 0xFFFD por símbolo FPUL sem restrição de bits) |
| MIT recomp-BStinger (sh4_isa.inc \| test_ops.cpp) | encodings + rotinas de divisão | concorda 100% nos patterns; rotinas div reproduzidas e passando (400× 32/16, 64/32, 600× 32/32 sinalizado) |
| dream-recomp ( GPL-2.0 externo, decoder map) | encodings FPU | FSCA/FTRV/FCNVSD/FIPR idênticos ao nosso atlas; 0xFFFD ilegal |
| Manual Renesas (via transcrição MIT) | regras semânticas | troca de banco SR, alvo de branches, SHAD/SHLD, FTRV — vencedores nos conflitos teste×exec |

## 6. Riscos, limitações e trabalho deixado para depois

- Raiz causa da divergência SLEIGH em 0xFFFD (símbolo `FPUL` sem restrição +
  sobreposição horizontal de campos) está documentada por análise estática;
  uma execução SLEIGH isolada do op não foi obtida (as 4-5 varreduras
  anteriores divergem por contexto de fluxo do binário varrido). A decisão
  legislativa usa trio MIT + dream-recomp + análise do padrão.
- `fsca` ainda não tem teste semântico numérico (depende do suporte
  cosseno-lut e do estado FPSCR); o HH04/HH05 comparará contra oráculo.
- `sleep`, `ldtlb`, `trapa` e hooks de página têm paths implementados mas
  sem oráculo diferencial ainda (cobertura real virá nos HH05+).
- Execução de `illegal` em passos posteriores marca `res.illegal` + hook
  (cobertura provada pelos 6.485 opcodes no teste exaustivo semântico).

## 7. Inventário de dados privados

- Nenhum byte do jogo, imagem de disco, BIÓS ou asset extraído versionado.
- `analysis/` e `build/` estão no `.gitignore` e validados em amostras:
  `git check-ignore -v analysis/hh03/oracle_run5.txt` (.gitignore:7),
  `build/translator/sh4/Debug/sh4_core.lib` (.gitignore:2), `all_ops.bin`
  (sob `analysis/`).
- `git status --short` pós-etapa: só fontes/documentos novos, sem conteúdo
  proprietário (inventário completo em §2). Sem commits executados.
- `opencode-ios-pair.png` é arquivo pré-existente do usuário (não tocado,
  não ignorado — decisão do mantenedor se deve entrar no git).

## 8. Checklist do critério de saída

- [x] Todos os 65.536 opcodes classificados sem sobreposição
      (0 contraditórios; teste exaustivo + `check_conflict.py`).
- [x] Divergências externas revisadas e documentadas (0xFFFD, §3.5, com
      trio de evidências e gates de geração).
- [x] Testes exaustivos + bordas + comparação independente (SLEIGH/MIT/
      dream-recomp/probes).
- [x] Zero dependência de dados do jogo nos unitários.
- [x] 4/4 testes nas configurações Debug e Release (build limpo).
- [x] Nível V2: integração dirigida (hook/illegal/fpu-estado) + casos
      negativos (teste com contagem exata de ilegais e hamming por major).

## 9. Próximos passos

Executados automaticamente: `STATUS.md` atualizado (HH03 CONCLUIDA, HH04
EM_ANDAMENTO). A etapa HH04 (Análise e tradução AOT) segue do estado em
disco, conforme protocolo do AGENTS.md — nenhuma decisão humana pendente.
