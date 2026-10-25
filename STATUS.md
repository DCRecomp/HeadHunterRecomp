# STATUS — Head Hunter Recompiled

Atualizado em 2026-10-25. Este arquivo é a fonte autoritativa do checkpoint.

## Estado das etapas

| Etapa | Estado | Evidência/relatório |
|---|---|---|
| HH00 | CONCLUIDA | `docs/relatorios/HH00-RELATORIO.md` (correção do porta V0 documentada em HH01) |
| HH01 | CONCLUIDA | `docs/relatorios/HH01-RELATORIO.md` |
| HH02 | CONCLUIDA | `docs/relatorios/HH02-RELATORIO.md` |
| HH03 | CONCLUIDA | `docs/relatorios/HH03-RELATORIO.md` (decodificador 65.536 + semântica; 1 divergência 0xFFFD documentada) |
| HH04 | CONCLUIDA | `docs/relatorios/HH04-RELATORIO.md` (4.698 funções, 0 fins mortos/órfãs; C++ idêntico nos dois discos, compila e linka) |
| HH05 | CONCLUIDA | `docs/relatorios/HH05-RELATORIO.md` (boot até a tela-título sem CDI; trace e 1º quadro conferidos com o Flycast) |
| HH06 | CONCLUIDA | `docs/relatorios/HH06-RELATORIO.md` (roteiro até a 1ª área controlável em janela e headless; Builder do CDI ao Preview) |
| HH07 | CONCLUIDA | `docs/relatorios/HH07-RELATORIO.md` (fronteira de 286 entradas, 0 violações, 2 dependências de hardware com dono HH14; catraca; native/legacy/transitional; produto sem CDI) |
| HH08 | CONCLUIDA | `docs/relatorios/HH08-RELATORIO.md` (provedor nativo de arquivos padrão; P1 4 838 blocos idênticos, P2 0 setores; os dois discos numa instalação só, sem troca; falhas seguras) |
| HH09 | CONCLUIDA | `docs/relatorios/HH09-RELATORIO.md` (tempo nativo estável padrão: ordem, quadros e relógios idênticos ao legado; 7 esperas declaradas; experimental opcional) |
| HH10 | EM_ANDAMENTO | — |
| HH11 | PENDENTE | — |
| HH12 | PENDENTE | — |
| HH13 | PENDENTE | — |
| HH14 | PENDENTE | — |
| HH15 | PENDENTE | — |
| HH16 | PENDENTE | — |
| HH17 | PENDENTE | — |
| HH18 | PENDENTE | — |

## Fatos já verificados

- Há duas imagens locais, privadas e ignoradas: `ISO/HeadHunter (Disc 1).cdi` e `ISO/HeadHunter (Disc 2).cdi`.
- `C:\Source\recomp-BStinger` é a referência arquitetural principal, branch `master`, com uma alteração local preexistente em `docs/DISTRIBUICAO-COMPILADOR.md`; nunca alterar essa árvore.
- `dream-recomp/` está disponível como ferramenta GPL-2.0 externa e seus testes previamente preparados passam; ele não integra o produto MIT.
- OpenCode usa `opencode/glm-5.3-flash-free` para implementação e `opencode/deepseek-v4.1-flash-free` para planejamento, exploração e revisão; `ollama/devstral-small-2:24b` permanece disponível como fallback local.
- REA não suporta SH-4; a análise estática SH-4 usa Ghidra headless
  (`C:\Tools\ghidra_12.1.3_PUBLIC`, linguagem `SuperH4:LE:32:default`,
  script `DreamcastRE.java` presente em `~/ghidra_scripts`).
- HH01 concluída: metadados dos dois discos confirmados por duas ferramentas
  independentes (bs-extract MIT e `tools/forensics/hh_discinfo.py`, 372
  arquivos 100% verbatim em lba/size/sha256/crc32); `1ST_READ.BIN` é um
  binário único que diverge em um dword por disco (@0x00175E28 =
  `iso_root_lba + 143`); contrato de troca = `DISC/DISK{N}.ID`; Katana, não
  Windows CE; nomenclatura `hedhunter_v1006_a/b` (par de `known_versions.h`
  confirmado); configs preliminares em `config/discs/`.
- Porta V0 de HH00 consertada e verde (CMake raiz + `tests/smoke.cpp`).

- HH02 concluída: `hh-extract` C++20 implementado do zero (CDI v3/v3.5
  linear, ISO9660 sanitizado, IP.BIN, scramble/descramble, SHA-256/CRC32
  streaming, `known_versions` com o par `hedhunter_v1006_{a,b}` portão
  `--force`); extração dos dois discos byte-idêntica à referência (372
  arquivos, 0 divergências estruturais, dif em
  `analysis/hh02_compare.py`); manifest determinístico entre rodadas; 39
  verificações sintéticas de unidade em `hh_extract_tests` (exit 0).

- HH03 concluída (2026-10-24): decodificador declarativo completo
  (197 retângulos, 59.051 legais / 6.485 ilegais, 0 sobreposição de
  destino) e executor semântico de passo único com 4 reparos provados por
  probe (BRAF/BSRF r[n]; STC.L DBR via `(op&0xF0)==0xF0`; FSQRT single com
  `u32(dn(...))`; hook de cache com opcode cru). Testes exaustivos verdes
  (Decoder + Semantics) em Debug e Release (ctest 4/4);
  1 divergência documentada vs SLEIGH: 0xFFFD é ILEGAL
  (bit 8 = paridade do par DRn; MIT `1111nnn011111101` +
  dream-recomp `{0xF1FF,0xF0FD}`; v. `docs/relatorios/HH03-RELATORIO.md`
  §3.5 e `MANUAL_WINS` em `analysis/hh03/gen_decoder_table.py`).

## Revisão geral (2026-10-24)

Ver `docs/planos/REVISAO-GERAL-2026-10-24.md`. Build Release ok e ctest
4/4 confirmados; HH04 ainda sem código. Inconsistências corrigidas na
revisão: errata do HH00, caminhos em `ARCHITECTURE.md`, `.gitignore`
(`tools/dc-re-ghidra/`, `opencode-ios-pair.png`, `__pycache__/`).
Dívidas resolvidas na HH04.0 (mesmo dia): gerador da tabela SH-4 em
`tools/sh4-tablegen/` com regeneração idêntica (só cabeçalho muda);
SPDX MIT nos fontes e ADR-002 revisado; `scripts/compare-trees.ps1` e
`tools/parse_cdi.py` (protótipos sem uso) movidos para `analysis/obsoleto/`;
guarda `cmake/HHProductGuard.cmake` marca `sh4_core` como `HH_TOOL_ONLY`
com teste negativo; `third_party/README.md` lista as ferramentas externas;
`README.md` reescrito; regra de módulos em `AGENTS.md` alinhada a headers;
branch vazia renomeada para `main`. ctest 6/6 em Debug e Release.

### Subetapas do HH04

| Subetapa | Estado |
|---|---|
| HH04.0 Higiene herdada (tablegen versionado, SPDX, sh4_core fora do produto) | CONCLUIDA |
| HH04.1 Imagem e carga verificada por hash | CONCLUIDA |
| HH04.2 Descoberta de funções e pools | CONCLUIDA (lacunas em aberto, ver abaixo) |
| HH04.3 Jump tables | CONCLUIDA |
| HH04.4 Código relocado e regiões de dados | CONCLUIDA |
| HH04.5 `config/headhunter.toml` (dword por disco como dado) | CONCLUIDA |
| HH04.6 Codegen C++20 com despacho rastreável | CONCLUIDA |
| HH04.7 Testes sintéticos gerar→compilar→executar | CONCLUIDA |
| HH04.8 Primeira geração completa compila e linka | CONCLUIDA |
| HH04.9 Relatório de cobertura e `HH04-RELATORIO.md` | CONCLUIDA (revisão Codex: 3 achados aceitos e corrigidos) |

### Progresso do HH04 (2026-10-24)

- Porte MIT do Blue Stinger com atribuição (`third_party/licenses/`):
  `translator/{decoder,analysis,codegen,app}` (`hh-recomp`, `hh-disasm`),
  `runtime/cpu` (contexto, operações, despacho), `runtime/mods/mod_api`,
  testes `tests/unit` e `tests/codegen`. Hardware ainda ausente:
  `runtime/cpu/sh4_hw_m3.cpp` torna fatal todo acesso fora da RAM
  (`HH_TOOL_ONLY`, substituído no HH05). toml++ 3.4.0 (MIT) via FetchContent.
- Teste cruzado `tests/hh04_decoder_cross.cpp` (decodificador portado ×
  HH03): achou 50 opcodes das extensões do SH-4A (`synco`, `icbi`,
  `movua.l`, `fpchg`) que o HH03 aceitava por herança do SLEIGH e que são
  ilegais no SH7091 (também ausentes no oráculo dream-recomp). HH03
  corrigido: tabela 59.001 legais / 6.535 ilegais, `SH4A_ONLY` em
  `tools/sh4-tablegen`, testes negativos; o teste cruzado falhava antes.
- `config/headhunter.toml` aceita só os dois SHA-256 do `1ST_READ.BIN`
  (executável adulterado recusado, rc=1).
- **Resultado final** (`hh-recomp` nos dois discos, após HH04.3, HH04.4 e a
  revisão Codex): 4.698 funções (2.561 chamada, 102 ponteiro no pool,
  1.982 ponteiro em dados, 46 tabela de ponteiros, 6 config/relocado),
  27 jump tables (0 heurísticas), 1.073 desvios via despacho, **0 fins
  mortos, 0 órfãs**; relatórios, `functions.tsv`/`coverage.bin` e C++
  gerado (13 arquivos, 1.083.722 linhas, `generated/`, ignorado) idênticos
  entre discos; o dword dependente do disco (0x8C185E28) não é código;
  compila e linka (MSVC Release); `ctest` 10/10 com `-DHH_GENERATED_DIR`.
  (Checkpoint anterior, antes de HH04.3/HH04.4: 4.692 funções, 3 tabelas
  heurísticas, 1.067 via despacho.)
- HH04.3: as 3 jump tables sem limite reconhecido têm limite 17 provado no
  código (0x8C123EB8) e declarado no TOML. HH04.4: VBR = 0x8C00F400
  definido pelo IP.BIN; blocos relocados para VBR+0 (alvo do `jmp @VBR` em
  0x8C180D98), +0x100, +0x400 e +0x600; tratadores 0x8C180DB4/0x8C17028C.
  Bug herdado corrigido no `coverage.bin` (base deslocada com `[[relocated]]`).
- Cobertura: 64,0% código, 10,3% dado, 25,7% não classificado; 1.414
  candidatas por prólogo (`prologue_candidates.tsv`). Incluí-las todas
  quebrou as invariantes (22 fins mortos, 1.928 órfãs), então
  `prologue_scan = false`; elas entram pelo config guiadas pelo despacho em
  execução (método do Blue Stinger, R2). Aviso único: chamada a
  `0x8C008300` (rotina de saída do jogo, bootstrap do IP.BIN) → HLE no HH05.
- Revisão Codex (2026-10-24, `reviews/codex-hh04-2026-10-24-b.md`, depois de
  corrigida a permissão de `~/.codex/.sandbox-bin`): 3 achados, todos
  aceitos e corrigidos — [ALTA] VBR+0 é alvo de `jmp @VBR` e faltava em
  `[[relocated]]` (minha afirmação anterior estava errada); [MEDIA]
  `sleep`, esperas e pontos de reentrada retornavam em silêncio no stub
  sem hardware → agora fatais, com teste `hh04_m3_contact_points_are_fatal`
  (`sh4_service`/`sh4_irq_check` continuam sem efeito por serem exatos sem
  fontes de evento); [BAIXA] métricas intermediárias misturadas às finais
  neste arquivo → separadas. Detalhes em `docs/relatorios/HH04-RELATORIO.md`.

### Progresso do HH05 (2026-10-24)

- HH05.1 ✔ `hh-extract --runtime/--verify-data` (porte do `runtime_data` MIT
  sobre o `hh_extract_core`): dados de execução dos dois discos (212/160
  arquivos, 42/39 setores de sistema) reconstroem todos os setores de dados
  iguais ao CDI; um byte alterado é recusado (rc=1).
- HH05.2 ✔ runtime MIT do Blue Stinger portado (`hh_runtime`: memória,
  Holly/PVR/TA, GD-ROM e BIOS HLE sobre `data/`, Maple, AICA, threads, boot,
  diagnósticos; sem `cri_diag`), compila sem avisos. Host headless
  `app/hh_run.cpp` (`hh-run`). Teste `hh_runtime_tests` (5 casos de longjmp).
- Bug do meu porte corrigido: rótulo `bs_reenter_` dentro de string no
  codegen não foi renomeado (o build com `[threads]` falhava);
  `codegen_setjmp_return_is_reentry_label` cobre.
- Descobertas em execução (método R2): `[[functions]]` 0x8C16AA00 (callback
  chamado por `jsr @r2` em 0x8C17C208); biblioteca de threads cooperativa
  `[threads]` setjmp 0x8C131F3C / longjmp 0x8C131E9C (leiaute simétrico do
  buffer; threads novas criadas pela tabela {pilha, entrada} em 0x8C1710D4).
- GD-ROM HLE: comando 0x22 `GETSCD` (formato 1, canal Q) implementado como
  "sem estado de áudio" (sem faixas CD-DA); a confirmar no oráculo.
- Oráculo Flycast funciona (`tools/dev/flycast_oracle.py`, porte MIT; Flycast
  externo em `C:\Sourcelycast-oracle`): na entrada PC=0xAC010000,
  PR=0xAC00E0B2, R15=0x8C00F400, como previsto pela análise do IP.BIN.
- **Marco:** `hh-run --data game-data/runtime/disc1 --vblanks 1800 --rtc
  1700000000` roda 1.800 VBlanks sem parada, 1.617 quadros não pretos desde
  o VBlank 127: tela "Presented by SEGA" e tela-título com "PRESS START
  BUTTON" (capturas em `analysis/hh05/run2`, privadas).
- HH05.4 ✔ saída do jogo (`MISC` 1/3, bootstrap do IP.BIN) como encerramento
  pelo gancho do host; teste `hh05_game_exit_requests_end_the_program`.
- HH05.5 ✔ trace do boot × Flycast (helpers folha 0x8C1793EA/0x8C159820
  excluídos dos dois lados): sequência de funções idêntica em 558 chamadas;
  diferenças classificadas (espelho P1/P2 do PR, contador de tempo, RTC da
  sessão do oráculo — some com o RTC alinhado — e um registrador morto);
  divergência na #558 = momento de entrega de interrupção. Portão
  `hh_boot_regression` com negativos.
- HH05.6 ✔ primeiro frame (logo SEGA) × Flycast: 94,54% dos pixels iguais e
  os demais a exatamente 1 passo de 5 bits (dither do PVR ligado, não
  modelado); 640×476 é o `FB_R_SIZE` do próprio jogo. Portão `hh_first_frame`
  com caso negativo.
- `GETSCD` conferido no oráculo (14 bytes: estado 0x15 e canal Q com a posição
  da cabeça de leitura) e implementado com teste.
- Divergência conhecida (→ HH13/HH12): os vídeos 4X são abandonados. Mecanismo
  comprovado: a fila de comandos de som (32 slots na RAM da AICA) não é
  consumida sem o driver ARM7 e todo comando falha a partir do VBlank 56; no
  Flycast os slots são zerados. A correção é a reconstrução nativa no HH13,
  nunca emular o ARM7 (regra "recompilar, nunca emular" em `AGENTS.md`).
- Revisão Codex: 8 achados, todos aceitos e corrigidos (portões endurecidos,
  comparadores com 16 casos sintéticos, integridade por hash na abertura,
  `GETSCD` real, pasta de saída segura). ctest 14/14 (2 SKIP sem dados; 2/2
  com eles).

### HH06 (2026-10-25)

- Roteiro `tools/dist/roteiros/newgame_area1.txt`: título → START → aviso de memory card
  (sem VMU) → NEW GAME → TRAINING MISSION → SKIP → primeira área (Jack Wade, HUD) →
  movimento pelo analógico. Determinístico (94 hashes iguais em todas as execuções).
- Switch por `braf` em tabela de stubs `bra; nop` (0x8C010AD8, 8 entradas) suportado no
  analisador; 4.699 funções, 28 tabelas, 0 fins mortos/órfãs.
- Núcleo `app/hh_host`; Preview `HeadHunter.exe` (SDL3, teclado/XInput, `--headless`);
  janela, headless e instalação do Builder com os mesmos hashes; sem interpretador
  (guarda adiada para o fim da configuração).
- `hh-builder`: CDI → dados → C++ → CMake → instalação transacional (marcador, rollback,
  trava) verificada pelo próprio `HeadHunter.exe`.
- `tools/dev/regressao.py`: regressão retomável (chave = fontes + toolchain + executáveis +
  entradas privadas). Revisão Codex: 6 achados aceitos e corrigidos.

### HH07 (2026-10-25)

- `hh-hwscan` portado do N0 do BS. Defeito corrigido na identificação do SDK: nomes por
  mensagem de chamador ("X() failed", "(ID)") e semente Kamui falsa em 0x8C090B1C. Nome agora
  exige candidato único mais evidência de outro tipo (13 funções nomeadas). Camadas no disco 1:
  jogo 3 032, SDK 1 515, desconhecida 151, conflito 1.
- Fronteira `config/fronteiras.toml` (286 entradas) e fechamento com a sonda do roteiro
  `newgame_area1`: 0 violações. O jogo acessa store queues/QACR diretamente em 0x8C090974 e
  0x8C0C4CE0: `[[dependencia_hw]]` com regiões, dono HH14 e evidência; confirmação só pela
  sonda; catraca em `config/metricas_base.toml` (96 chaves).
- Alvos `hh_native_api` (interfaces, sem Dreamcast, conferido também no CMake),
  `hh_legacy`, `hh_transitional`; opção `HH_MODS` (slots, custo dentro do ruído, hashes
  iguais); ordem de eventos determinística (SHA igual em duas execuções); `HeadHunter.exe`
  sem leitor de CDI (mapa do linker).
- Comandos: `ctest --preset vs-release` (25 testes); `tools/dev/regressao.py` com
  `HH07_BIN`/`HH07_SONDA` (etapa `portoes_hh07`).
- Revisão Codex (`reviews/codex-hh07.log`): 5 achados aceitos e corrigidos (dependência
  limitada por região; obsoleta só pela sonda; portões com motivo e mínimos; chave da
  regressão com `hh-hwscan` e a árvore de dados; `native` limpo no CMake). 2 rejeitados com
  motivo (relatório HH07, seção 9). A primeira execução do Codex ficou presa esperando stdin
  por 7 h; rodar sempre com `< /dev/null`.

### HH08 (2026-10-25)

- gdFs 1.09: as funções por GDFS são chamadas diretas (layout do 1.05); as por nome e as de drive
  passam pela tabela do driver 0x8C185DAC (`*0x8C188D08`). Contratos no HH08-PLANO.
- Adaptador gdFs nativo (`transitional/gdfs_adapter`, 18 slots) sobre `native/platform/data_files`
  é o padrão; `--arquivos legado` fica para referência (só o disco 1). P1: 4 838 blocos
  idênticos; P2: 0 setores de dado pelo GD-ROM HLE; hashes do roteiro iguais à referência do HH06.
- **Sem troca de disco** (mantenedor, 2026-10-25): o Builder junta os dois discos numa `data\`
  (`arquivos.manifest`, variante do disco 2 em `disc\_DISCO2`); o provedor mostra o disco em que
  o jogo está pela variável dele (0x8C15174C) e o drive está sempre pronto. A passagem real do
  disco 1 para o 2 e o save do disco 2 são verificados no HH11/HH16.
- Falhas seguras: dados de outro disco (versão pelo hash do executável), disco 2 de outra versão,
  dados/variante/`DISK2.ID` ausentes, arquivo alterado/truncado/ilegível, mod inválido.
- Revisões Codex: 8 achados na primeira entrega (5 aceitos, 3 em parte) e 4 no retrabalho
  (aceitos); relatório, seção 9.

### HH09 (2026-10-25)

- Portados do N2a/N2b do BS: `native/platform/{time.h,clock,event_queue,progress}` e
  `transitional/time_adapter` (sem o CRI). `--tempo legado|nativo|experimental`; nativo estável
  padrão (A1/A2/A3/A5 iguais ao legado).
- Esperas (HH_WW_VOLTAS + modo sombra): 0x8C08ED26 (pausa de N quadros, VBlank) e 0x8C16B7CE (fim
  do render, Kamui); boot: 0x8C16A5E4/608 e 0x8C169704/728 (SPG), 0x8C11FC3A (filme 4X).
- Experimental: o roteiro chega à primeira área, determinístico e 20% mais rápido; a
  equivalência formal (B1–B6, B11) e o prazo do TMU nas esperas de SPG ficam para o HH15.
- Catraca `tempo.consumidores.ciclos` 2 → 0 (artefato do classificador), varrendo também
  `transitional/`.

### Formatos dos discos (investigação 2026-10-24)

Relatório e scripts em `analysis/formatos/` (privado). Verificado em todos
os arquivos de cada tipo e conferido de forma independente (AHF e 4XM):
`.AHF` = contêiner `AHFF` v2 (141 arquivos, 48.067 entradas, sistema de
arquivos virtual `HEADHUNTER\...`); `.PVR` = `PVRT` VQ quadradas;
`.4XM` = RIFF `4XMV` (53 filmes, 480 px de largura, 30 fps, decodificador 4X
dentro do executável, sem Sofdec); `.ADP` = música CRI ADX ou RIFF
ADPCM/PCM8; `.FPB/.FOB` = programa/mixagem do DSP AICA (reverb);
`AUDIO64.DRV` = driver ARM7 Sega Audio64 1.90f.2; `DISKn.ID` = identidade do
disco (`DISC\disk%d.id` no executável). A sessão 1 `CDROM` só repete
`MOVIES` e `STREAMS` da sessão 2: nada novo para o jogo. Hipóteses abertas:
pixel PVR código 5, variante ADPCM dos RIFF, gramática interna dos tipos
dentro dos AHF.

## Decisões pendentes

Nenhuma decisão humana está pendente. Descobertas que contradigam estas premissas podem exigir `BLOQUEADA_DECISAO`, mas somente depois de esgotar investigação e testes independentes:

- um middleware essencial exige incorporação incompatível com MIT;
- a arquitetura observada exige código dinâmico/overlay não coberto pelo plano.

Decisão do mantenedor (2026-10-24): o FFmpeg (LGPL) pode ser usado como
ferramenta externa de conversão dos vídeos 4X Movie no Builder (ARCHITECTURE.md
A11): processo separado, sem link nem código copiado. A forma de distribuição
do binário com o Builder fica para o HH18.

Questões da revisão geral resolvidas pelo mantenedor ("arruma todos os
problemas", 2026-10-24): headers aceitos no lugar de módulos nomeados;
branch `main`. O primeiro commit continua aguardando pedido explícito.

## Próxima ação permitida

Prosseguir pelo **HH10** (controles nativos) e as etapas seguintes. O teste da passagem real do
disco 1 para o 2 fica para depois (HH11/HH16). Commits por etapa autorizados pelo mantenedor
(2026-10-25).
Parar somente em `DECISAO_NECESSARIA` legítima ou quando
o mantenedor enviar `PAUSAR`.
