# HH04 — RELATÓRIO: Análise e tradução AOT

Data: 2026-10-24 · Nível de validação: V2 · Estado: **CONCLUIDA**

## 1. Objetivo e escopo realizado

Transformar o `1ST_READ.BIN` dos dois discos em C++20 nativo, uma função C++
por função SH-4, com descoberta de funções, pools, jump tables, código
relocado, despacho rastreável e configuração por TOML.

- Porte com atribuição do tradutor MIT do Blue Stinger (decodificador,
  análise, gerador, `hh-recomp`, `hh-disasm`) e do núcleo de CPU usado pelo
  código gerado (`runtime/cpu`, `runtime/mods`), com os testes do tradutor.
- Sem hardware ainda: `runtime/cpu/sh4_hw_m3.cpp` torna fatal todo acesso fora
  da RAM, store queue e troca de contexto (equivalente ao M3 do Blue Stinger).
- Configuração do título `config/headhunter.toml`: hashes aceitos, carga,
  jump tables com limite declarado, vetores VBR relocados e tratadores.
- C++ do jogo inteiro gerado, idêntico para os dois discos, compilado e linkado.

## 2. Arquivos alterados

| Arquivo | Mudança |
|---|---|
| `translator/{decoder,analysis,codegen,app}/*` | Portados do Blue Stinger (cabeçalho de origem); `hh::isa` para não colidir com o `hh::sh4` do HH03 |
| `translator/analysis/analysis.{h,cpp}` | Novo: `hasPrologue`, `prologue_candidates`, `Config::prologue_scan`, `Origin::Prologue` |
| `translator/app/{config,recomp}.{h,cpp}` | Novo: `[input] sha256` e recusa de executável fora da lista; relatório `prologue_candidates.tsv` |
| `translator/CMakeLists.txt` | `hh_translator`, `hh_recomp_lib`, `hh-recomp`, `hh-disasm` (`HH_TOOL_ONLY`) |
| `runtime/cpu/{sh4_context,sh4_ops,sh4_gen,sh4_mem,sh4_runtime,sh4_intc,write_watch}.*`, `runtime/mods/mod_api.*` | Portados |
| `runtime/cpu/sh4_hw_m3.cpp`, `runtime/CMakeLists.txt` | Novos (pontos de contato fatais; `sh4_sleep` movido para cá e fatal) |
| `config/headhunter.toml` | Novo |
| `tests/unit/*`, `tests/codegen/*`, `tests/fixtures/*` | Portados |
| `tests/hh04_decoder_cross.cpp`, `tests/hh04_generated_link.cpp`, `tests/hh04_runtime_m3_tests.cpp`, `tests/CMakeLists.txt` | Novos |
| `tools/sh4-tablegen/gen_decoder_table.py`, `translator/sh4/src/{cpu.cpp,decoder_table.inc}`, `translator/sh4/include/sh4/{cpu,op_enum}.hpp`, `tests/hh03_{decoder,semantics}_tests.cpp` | Correção SH-4A (§3.2) |
| `CMakeLists.txt` | toml++ 3.4.0 via FetchContent, `/utf-8` |
| `third_party/README.md`, `third_party/licenses/recomp-BStinger-LICENSE.txt` | Registro do código portado e do toml++ |

## 3. Evidências e decisões

### 3.1 Reuso em vez de reescrita
O decodificador do HH03 foi mantido como oráculo independente; a análise e o
gerador usam o decodificador do Blue Stinger, já validado num jogo Katana real.
Os dois foram construídos de forma independente e são comparados opcode a
opcode (`tests/hh04_decoder_cross.cpp`).

### 3.2 Extensões do SH-4A eram aceitas pelo HH03
O teste cruzado falhou na primeira execução: 50 opcodes legais no HH03 e
ilegais no Blue Stinger, todos de `synco` (1), `icbi` (16), `movua.l` (32) e
`fpchg` (1), instruções do SH-4A que o módulo SLEIGH SuperH4 também
decodifica. Terceira fonte: o decodificador do oráculo externo dream-recomp
não tem nenhuma delas. Correção: `SH4A_ONLY` no gerador da tabela (59.001
legais / 6.535 ilegais), casos removidos do executor, testes positivos
substituídos por negativos (`testSh4aIlegais`, spot-checks no teste do
decodificador). A única outra diferença é de nome (`fmov` × `fmov.s` nos 256
opcodes `fmov FRm,FRn`), aceita explicitamente no teste.

### 3.3 Identidade do executável
`[input] sha256` aceita só `aa5853…640d7` (disco 1) e `2d1ead…78b0a`
(disco 2). Teste negativo: um byte invertido no offset 100 → `hh-recomp`
recusa com rc=1.

### 3.4 Um código para os dois discos
`functions.tsv`, `coverage.bin`, `jump_tables.tsv` e o C++ gerado são
idênticos byte a byte entre os discos (`cmp`, `diff -rq`). O dword
dependente do disco (0x8C185E28) não é código (classe 0 em `coverage.bin`) e
o pool é lido da RAM (`inline_pool_constants = false`).

### 3.5 Jump tables (HH04.3)
Três tabelas não tinham limite reconhecido (0x8C123E1C, 0x8C123E28 e
0x8C123EC4: cópias desenroladas `mov.l @(r0,r3),r3; jmp @r3`). O limite está
no código de 0x8C123EB8: `mov #64,r3; cmp/hs r0,r3; bf` → índice ≤ 64 bytes →
17 entradas; a 18ª palavra de cada tabela já é o prólogo da função seguinte.
`boundBefore` não reconhece a comparação invertida; os limites foram
declarados em `[[jump_tables]]`. Resultado: 27 tabelas, 0 heurísticas.

### 3.6 Vetores de exceção/interrupção (HH04.4)
- O bootstrap do IP.BIN define SR=0x700000F0, r15 = VBR = 0x8C00F400 e
  FPSCR=0x00040001 (IP.BIN 0x8C00E07C–0x8C00E0A8). O jogo não tem nenhum
  `ldc rN,vbr` no código alcançado; só lê o VBR (`stc vbr`) e o restaura de um
  contexto salvo (0x8C180D7A).
- 0x8C1703A4 grava o tratador no pool dos stubs (0x8C180C4C, ou 0x8C180DB4
  quando r4 = 0x80000000) e copia os stubs para VBR+0x100/+0x400 (origem
  0x8C180B88, 0x3C bytes, lê EXPEVT) e VBR+0x600 (0x8C180BC0, 0x60 bytes, lê
  INTEVT); a cópia vai até passar do fim inclusivo (`cmp/hi`), por isso o
  tamanho é fim − início + 4 (`analysis/hh04/vbr_blocks.py`). Também copia
  0x8C180C1C..0x8C180C4C (0x34 bytes, começa com `rte`) para VBR+0. Nenhum
  vetor do hardware entra em VBR+0, mas o jogo salta para lá na saída do
  tratador (0x8C180D96–0x8C180D98: `stc vbr,r0; jmp @r0` quando o código do
  evento ≥ 0x200). A primeira versão deste relatório omitiu esse bloco; a
  revisão Codex apontou e ele foi declarado (§9).
- 0x8C17053A–0x8C170550 preenche a tabela em VBR+0x200 com 0x8C17028C.
- Declarados: quatro `[[relocated]]` (VBR+0, +0x100, +0x400, +0x600) e
  `[[functions]]` 0x8C180DB4 e 0x8C17028C.

### 3.7 Saída do jogo
O único aviso restante (`0x8C008300` a partir de 0x8C15E59E) é a rotina de
saída: lê flags do IP.BIN em 0x8C0080FC/FE e salta para o bootstrap do IP.BIN
ou para a syscall em `[0x8C0000E0]`. No executável nativo vira encerramento
do programa (HLE no HH05).

### 3.8 Bug herdado no mapa de cobertura
`writeReport` calculava a base do `coverage.bin` como o menor endereço de
código arredondado a 64 KB. Com os `[[relocated]]` em 0x8C00F500 a base virou
0x8C000000 e o mapa inteiro ficou deslocado em 0x10000 (o dword 0x8C185E28
apareceu como "código"). Correção: o mapa é ancorado em `img.base`, tem o
tamanho da imagem e ignora destinos relocados. Depois da correção: 772.512
halfwords, 64,0% código, 10,3% dado, 25,7% não classificado, dword do disco
de volta à classe 0. O mesmo código existe no Blue Stinger (não alterado,
árvore de referência).

### 3.9 Varredura por prólogo
1.414 inícios plausíveis (fronteira, prólogo do SHC, corpo decodificável) em
regiões não classificadas. Incluí-los todos de uma vez produziu 22 fins
mortos e 1.928 órfãs: há candidatas dentro do corpo de outras. Decisão:
só relatório (`prologue_candidates.tsv`); funções alcançadas por ponteiro
calculado entram pelo config quando o despacho as registrar em execução
(método do Blue Stinger, R2), e o despacho nunca é silencioso.

## 4. Comandos de build/teste e resultados

```powershell
cmake --preset vs -DHH_GENERATED_DIR=C:/Source/recomp-head-hunter/generated/disc1
cmake --build --preset vs-release -- -m        # sem erros nem avisos C4
ctest --preset vs-release                      # 10/10
build\translator\Release\hh-recomp.exe config\headhunter.toml --game-data game-data\hh02\disc1 --analysis analysis\hh04\disc1 --out generated\disc1
build\translator\Release\hh-recomp.exe config\headhunter.toml --game-data game-data\hh02\disc2 --analysis analysis\hh04\disc2 --out generated\disc2
python -I analysis\hh04\coverage_gaps.py game-data\hh02\disc1\1ST_READ.BIN analysis\hh04\disc1\coverage.bin
```

| Teste | Resultado |
|---|---|
| `hh_smoke`, `hh_extract_tests`, `hh03_decoder_tests`, `hh03_semantics_tests` | ok |
| `hh_product_guard_rejects` / `_accepts` | ok |
| `hh_translator_tests` (34 casos: decodificador, análise, análise M4, operações, teste cruzado, extensões SH-4A ilegais, pontos de contato fatais sem hardware) | ok |
| `hh_codegen_exec_0` / `_1` (gera, compila e executa o programa sintético, pool embutido ou não) | ok |
| `hh_generated_link` (C++ do jogo inteiro; entry em P0/P1/P2 e os 4 vetores VBR no despacho; meio de instrução recusado) | ok |

Análise (idêntica nos dois discos): 4.698 funções (2.561 chamada, 102
ponteiro no pool, 1.982 ponteiro em dados, 46 tabela de ponteiros, 6
config/relocado), 494.718 instruções únicas, 27 jump tables (0
heurísticas), 19.352 desvios indiretos resolvidos, 1.073 via despacho,
**0 fins mortos, 0 órfãs**, 1 aviso (§3.7). Geração: 13 arquivos, 4.698
funções, 1.083.722 linhas; build Release do executável de teste em ~47 s.

## 5. Comparação com referência/oráculo

- Decodificador portado × HH03: concordância total após §3.2.
- dream-recomp (oráculo GPL externo, só consulta): confirma §3.2.
- IP.BIN e instalador dos vetores: disassembly próprio; confirmação em RAM
  do Flycast fica para o HH05 (o Blue Stinger fez a mesma conferência).

## 6. Riscos, limitações e trabalho deixado para depois

- Cobertura: 64,0% código, 10,3% dado, 25,7% não classificado (inclui
  tabelas, floats, strings e as funções ainda não descobertas de §3.9).
- 1.073 desvios indiretos dependem do despacho em execução.
- Hardware, interrupções, threads/setjmp e HLE de BIOS ausentes até o HH05.
- Biblioteca de threads cooperativa (`[threads]`) ainda não investigada.

## 7. Inventário de dados privados

`game-data/`, `generated/`, `analysis/hh04/` e `reviews/` ignorados
(`git check-ignore`). Versionados só metadados: endereços, tamanhos, hashes e
descrições próprias no TOML e neste relatório.

## 8. Checklist do critério de saída

- [x] Descoberta recursiva, pools, jump tables, ponteiros, callbacks e código relocado.
- [x] Config TOML de seeds, fronteiras e módulos dos dois discos.
- [x] Uma função C++20 por função guest, despacho rastreável, relatórios de cobertura.
- [x] Testes sintéticos que geram, compilam e executam C++.
- [x] Primeira geração completa que compila e linka.
- [x] Zero fins mortos e zero órfãs; gaps configuráveis e observáveis (despacho fatal com PC).
- [x] Revisão independente (Codex) tentada e registrada — §9.

## 9. Revisão independente e próximos passos

Revisão independente: `codex exec --sandbox read-only --ephemeral`
(codex-cli 0.153.2), prompt e saídas em `reviews/` (ignorado).

- 1ª tentativa: o sandbox somente leitura falhou antes de ler qualquer
  arquivo (`helper_sandbox_lock_failed`: o usuário não tinha controle total
  de `~/.codex/.sandbox-bin`). O único ponto levantado (outras extensões do
  SH-4A: `movco.l`, `movli.l`, `prefi`) foi verificado: já eram ilegais; o
  teste `hh04_sh4a_extensions_are_illegal` cobre as sete formas.
- 2ª tentativa, após o mantenedor corrigir a permissão
  (`reviews/codex-hh04-2026-10-24-b.md`), com 3 achados, todos aceitos:
  - **[ALTA] VBR+0 omitido de `[[relocated]]`.** Confirmado no disassembly
    (0x8C180D96 `stc vbr,r0`, 0x8C180D98 `jmp @r0`). Corrigido: bloco
    declarado, `f_8C00F400` no despacho, verificado por `hh_generated_link`.
    Antes da correção `functions.tsv` não continha 0x8C00F400.
  - **[MEDIA] Caminhos silenciosos no stub sem hardware.** `sh4_sleep` (que
    retornava via `sh4_service`), `sh4_wait_point`, `sh4_wait_end` e
    `sh4_set_reentry_points` com contagem ≠ 0 agora são fatais; teste
    `hh04_m3_contact_points_are_fatal` (falharia antes: os quatro
    retornavam). Mantidos sem efeito, por serem exatos num sistema sem fontes
    de evento: `sh4_service` (só reprograma o quantum dos back-edges, usado
    pelos testes sintéticos) e `sh4_irq_check` (nada pode estar pendente).
  - **[BAIXA] Métricas intermediárias e finais misturadas no `STATUS.md`.**
    Separadas.
- Pontos que o Codex conferiu sem problema: exclusões SH-4A (inclusive
  `ldc/ldc.l SGR` ilegais), teste cruzado, `prologue_scan` só relatório,
  hashes, as três jump tables (17 entradas), tamanhos e destinos dos blocos
  VBR+0x100/+0x400/+0x600, VBR no IP.BIN e os dois tratadores.
- Não verificado (fica para o HH05 com o Flycast): se o `jmp @VBR` ocorre em
  jogo e a RAM do VBR após as cópias.

Etapa marcada `CONCLUIDA`; próxima: HH05 (runtime híbrido, boot e primeiro
frame), começando pelo porte do runtime MIT do Blue Stinger.
