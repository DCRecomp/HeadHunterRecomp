# HH04 — Plano detalhado: análise e tradução AOT

Validação mínima: **V2**. Critério de saída (ROADMAP): zero fluxos
mortos/órfãos inexplicados; gaps conhecidos configuráveis e observáveis,
nunca silenciosos; primeira geração completa que compile e linke.

## Estratégia

O Blue Stinger (MIT) já tem analisador e codegen funcionando para um título
Katana SH-4 do mesmo período:

| Componente BS | Caminho | Tamanho aprox. | Uso no HH04 |
|---|---|---|---|
| Análise | `translator/analysis/analysis.{h,cpp}` | ~800 linhas | Portar com atribuição, adaptando ao decodificador do HH03 |
| Codegen | `translator/codegen/codegen.{h,cpp}` | ~440 linhas | Portar com atribuição |
| App/config | `translator/app/{main,recomp,config,disasm_main}.cpp` | ~370 linhas | Portar como `hh-recomp` / `hh-disasm` |
| Config por jogo | `config/bluestinger_us.toml` | — | Modelo do esquema `[load]`, `[analysis]`, `[codegen]`, `[threads]`, `[native]` |
| Verificação gerada | `tools/gen_check` | — | Avaliar |

O decodificador do HH03 (`translator/sh4`) continua sendo o do projeto, já
validado nos 65.536 opcodes; o analisador portado passa a consumi-lo. Ler o
código BS antes de cada subetapa; nunca alterar `C:\Source\recomp-BStinger`.
Cada arquivo portado recebe cabeçalho com a origem
(`Derivado de recomp-BStinger <caminho>, MIT`) e uma linha em
`third_party/README.md`.

## Subetapas

| ID | Entrega | Evidência de saída |
|---|---|---|
| HH04.0 ✔ | Higiene herdada: gerador da tabela SH-4 movido para `tools/sh4-tablegen/` e regenerando `decoder_table.inc` byte-idêntico; `SPDX-License-Identifier: MIT` nos fontes; `scripts/compare-trees.ps1` removido ou implementado; alvo `sh4_core` marcado como ferramenta (proibido no link do produto) | `git diff --stat` do `.inc` vazio após regeneração; testes 4/4 |
| HH04.1 ✔ | Imagem e carga: `Image` com base `0x8C010000`, leitura do `1ST_READ.BIN` extraído via manifesto do `hh-extract`, verificação de SHA-256 contra `config/discs/*.toml` | Teste sintético + execução real recusando hash errado |
| HH04.2 ✔ | Descoberta de funções: entry, chamadas diretas (`bsr`/`jsr` via pool), pools de constantes, ponteiros em dados e tabelas de ponteiros; origem de cada função registrada | Relatório de cobertura em `analysis/hh04/`; contagem comparada com Ghidra headless (projeto HH01) |
| HH04.3 ✔ | Jump tables (`braf`/`jmp` com tabela `mov.w`/`mov.l`), limites achados no código vs heurísticos, overrides no TOML | Lista de tabelas com `bounded`; zero heurística sem entrada no relatório |
| HH04.4 ✔ | Código relocado (vetores em VBR+0x100/0x400/0x600 e cópias para RAM) e regiões de dados declaradas | Seção `[relocated]`/`data_ranges` no TOML com evidência por endereço |
| HH04.5 ✔ | Config TOML do título: `config/headhunter.toml` com `[load]`, `[analysis]`, `[codegen]`, seeds, jump tables, e o dword dependente de disco (`0x00175E28`) como dado por disco, não como código | Os dois discos produzem a mesma análise; diferença só no dado declarado |
| HH04.6 ✔ | Codegen: uma função C++20 por função guest, comentário de disassembly, despacho por tabela rastreável, miss de despacho registrado (nunca silencioso), `funcs_per_file` | `generated/` (ignorado) |
| HH04.7 ✔ | Testes sintéticos de codegen: pequenos programas SH-4 gerados → C++ → compilados → executados e comparados ao executor do HH03 | Novo teste ctest, com caso negativo (instrução alterada falha) |
| HH04.8 ✔ | Primeira geração completa do `1ST_READ.BIN` que compila e linka contra um runtime stub mínimo | Build Release ok; métricas: funções, linhas, arquivos, tempo |
| HH04.9 ✔ | Relatório de cobertura: bytes de código cobertos vs Ghidra, funções órfãs, alvos indiretos não resolvidos, cada gap com causa ou entrada de config | `docs/relatorios/HH04-RELATORIO.md` |

Ordem: HH04.0 → HH04.1 → (HH04.2, HH04.3, HH04.4 iterativos) → HH04.5 →
HH04.6 → HH04.7 → HH04.8 → HH04.9.

## Questões a investigar (não são decisões humanas)

- Existe biblioteca de threads cooperativa por `setjmp`/`longjmp`, como no
  Blue Stinger? Identificar por assinatura + grafo de chamadas antes de
  configurar `[threads]`.
- O jogo escreve em palavras do pool em tempo de execução? Determina
  `inline_pool_constants`.
- Como `AUDIO64.DRV` é carregado (só dado para o ARM7, ou também código SH-4)?
- Funções SDK identificáveis com `dc-re-ghidra/sdk-symbols.txt` (CRC +
  tamanho) como segunda evidência para nomeação futura.

## Fora do escopo do HH04

Runtime, memória e hardware temporários (HH05); execução real do código
gerado além do stub de link (HH05); nomeação de gameplay (HH17).
