# Revisão geral do projeto — 2026-10-24

Revisão de todo o workspace no ponto HH04 `EM_ANDAMENTO`. Objetivo: confirmar
o estado declarado, listar inconsistências e dívidas e reorganizar o
planejamento das próximas etapas.

## 1. Estado verificado

| Item | Comando | Resultado |
|---|---|---|
| Build Release | `cmake --build build --config Release` (VS 18 2026, MSBuild 18.0.5) | ok, sem erros |
| Testes | `ctest --test-dir build -C Release --output-on-failure` | 4/4 passaram (`hh_smoke`, `hh_extract_tests`, `hh03_decoder_tests`, `hh03_semantics_tests`) |
| Git | `git status --short` | branch `master` **sem nenhum commit**; tudo não rastreado |
| Executor autônomo | `.opencode-autonomo.pid` | PID 6656 inexistente; nenhum arquivo do projeto alterado nas últimas 2 h |

O estado de HH00–HH03 em `STATUS.md` é consistente com build e testes. HH04
está marcado `EM_ANDAMENTO`, mas **não há nenhum código de HH04** em
`translator/` (só `translator/sh4`, que é HH03).

## 2. Inconsistências encontradas

| # | Onde | Problema | Ação |
|---|---|---|---|
| I1 | `docs/relatorios/HH00-RELATORIO.md` | Termina com "AGUARDANDO REVISÃO / não iniciarei HH01 sem APROVADO HH00", contrariando a política de autonomia; o toolchain citado (MSVC 17.7, CMake 3.27) não é o do build atual (VS 18 2026); afirma "licença MIT em todos os arquivos de código", o que não é verdade | Errata acrescentada ao relatório |
| I2 | `docs/ADRs/ADR-002-licensing.md` | Exige cabeçalho MIT em todo arquivo versionado; 26 de 27 fontes C++ não têm | HH04.0: acrescentar `SPDX-License-Identifier: MIT` |
| I3 | `ARCHITECTURE.md` | Cita `tools/extractor` e `config/headhunter_*.toml`; os reais são `tools/hh-extract` e `config/discs/hedhunter_v1006_{a,b}.toml` | Corrigido |
| I4 | `analysis/hh03/gen_decoder_table.py` (+ `atlas_shapes.txt`, `sleigh_truth2.txt`) | O gerador do arquivo versionado `translator/sh4/src/decoder_table.inc` vive em `analysis/`, que é ignorado: a tabela não é regenerável a partir do repositório | HH04.0: mover gerador e entradas sintéticas (sem bytes do jogo) para `tools/sh4-tablegen/` |
| I5 | `tools/dc-re-ghidra/` | Repositório Git aninhado de terceiros, sem licença confirmada, não registrado em `third_party/README.md`; seria adicionado como repositório embutido | Ignorado no `.gitignore` (ferramenta externa, como `dream-recomp/`) |
| I6 | `opencode-ios-pair.png` na raiz | Imagem de pareamento do OpenCode não ignorada | Ignorado no `.gitignore` |
| I7 | `tools/forensics/__pycache__/` | Bytecode Python não ignorado | Ignorado no `.gitignore` |
| I8 | `scripts/compare-trees.ps1` | Placeholder que só imprime "pending implementation"; a função real está em `tools/forensics/hh_discinfo.py` e `analysis/hh01/*.py` | HH04.0: remover ou implementar |
| I9 | `AGENTS.md` ("C++20, CMake e módulos nomeados") | Todo o código usa headers (como o Blue Stinger); nenhum módulo C++20 | Decisão do mantenedor (ver §5) |
| I10 | `README.md` | Em inglês, genérico, não aponta para STATUS/ROADMAP | Baixa prioridade; ajustar no HH18 ou quando houver commit |
| I11 | Raiz | `*.obj` e `opencode-autonomo*.log` soltos na raiz (já ignorados) | Sem ação obrigatória |
| I12 | Git | Branch atual `master` sem commits, enquanto a principal esperada é `main` | Decisão do mantenedor (ver §5) |

## 3. Pendências técnicas herdadas dos relatórios

- HH02: modo `--runtime` (verificação setor a setor) não implementado; testes
  sintéticos sem CDI multissessão (cobertura real só com os discos).
- HH03: `fsca` sem teste numérico; `sleep`, `ldtlb`, `trapa` e hooks de página
  sem oráculo diferencial; divergência 0xFFFD vs SLEIGH resolvida por análise
  estática sem execução isolada.
- HH03: o executor de passo único (`translator/sh4/src/cpu.cpp`) é um
  interpretador. Ele serve de referência semântica para o codegen e de
  oráculo nos testes; **não pode ser ligado a `HeadHunter.exe`** (critério
  operacional final). O HH04 deve manter isso explícito no CMake.

## 4. Fatos de entrada para as próximas etapas

Já provados em HH01/HH02 e relevantes para o planejamento:

- `1ST_READ.BIN` é um binário único (1.545.024 bytes, carga e entry em
  `0x8C010000`, Katana, sem Windows CE), igual nos dois discos exceto um
  dword em `0x00175E28` (`iso_root_lba + 143`). **Uma única geração de código
  serve aos dois discos**; esse dword é dado dependente do disco.
- Nenhum overlay fora da árvore ISO. `AUDIO64.DRV` (driver ARM7, 18.608
  bytes) é idêntico nos dois discos → HH13.
- Vídeos em `MOVIES/*.4XM` (formato 4X Movie); `SCENE0.4XM` difere por disco → HH12.
- Contrato de troca de disco: `DISC/DISK{N}.ID` + o dword acima → HH08/HH11.

## 5. Decisões que dependem do mantenedor

Nenhuma bloqueia o HH04. Registradas aqui para não se perderem:

1. **Módulos C++20 (I9):** manter a regra de `AGENTS.md` e migrar para módulos
   nomeados, ou alinhar com o Blue Stinger e aceitar headers. Recomendação:
   headers (o código MIT a reutilizar no HH04 usa headers; MSVC + módulos +
   código gerado de milhares de funções aumenta risco de build).
2. **Primeiro commit e branch (I12):** o repositório não tem commits. Nenhum
   commit é feito sem pedido explícito.

## 6. Reorganização do planejamento

- `docs/planos/HH04-PLANO.md`: plano detalhado do HH04 em subetapas
  verificáveis, com mapa de reuso do Blue Stinger.
- `STATUS.md`: subetapas do HH04 rastreadas e dívidas listadas.
- `ROADMAP.md`: seção "Reuso da referência MIT e entradas conhecidas por etapa".

## 7. Correções aplicadas (2026-10-24, mesmo dia)

A pedido do mantenedor ("arruma todos os problemas"):

| # | Resultado |
|---|---|
| I1 | Errata no `HH00-RELATORIO.md` |
| I2 | `SPDX-License-Identifier: MIT` em 25 fontes C++, nos `.py`, `.ps1` e `.java`; gerador emite SPDX nos arquivos gerados; ADR-002 revisado |
| I3 | `ARCHITECTURE.md` com os caminhos reais |
| I4 | `tools/sh4-tablegen/` (gerador, atlas, binário sintético, script Ghidra, conversor de log, README). Verificado: `all_ops2.bin` e `sleigh_truth2.txt` reproduzidos; `.inc`/`op_enum.hpp` regenerados idênticos fora do cabeçalho |
| I5–I7 | `.gitignore` atualizado; `__pycache__` removido |
| I8 | `scripts/compare-trees.ps1` e `tools/parse_cdi.py` movidos para `analysis/obsoleto/` (ignorado, preservado) |
| I9 | `AGENTS.md`: headers, módulos só com ADR |
| I10 | `README.md` reescrito em português |
| I11 | `*.obj` da raiz movidos para `build/stray/` |
| I12 | `HEAD` aponta para `main` (não havia commits); nenhum commit criado |
| §3 | `sh4_core` marcado `HH_TOOL_ONLY` por `cmake/HHProductGuard.cmake`; `hh_assert_product_clean()` disponível para o alvo `HeadHunter.exe`; testes `hh_product_guard_rejects` (negativo, transitivo) e `hh_product_guard_accepts` |

Validação: `cmake --build build --config Release|Debug` sem erros;
`ctest -C Release` e `ctest -C Debug`: 6/6.

Continuam abertas, nas etapas previstas: `--runtime` do `hh-extract`
(HH05), testes numéricos de `fsca` e oráculo de `sleep`/`ldtlb`/`trapa`
(HH05, com o Flycast).
