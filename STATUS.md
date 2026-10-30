# STATUS — Head Hunter Recompiled

Atualizado em 2026-10-28. Este arquivo é a fonte autoritativa do checkpoint.

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
| HH07 | CONCLUIDA | `docs/relatorios/HH07-RELATORIO.md` (fronteira de 286 entradas, 0 violações, 2 dependências de hardware com dono HH13 (render); catraca; native/legacy/transitional; produto sem CDI) |
| HH08 | CONCLUIDA | `docs/relatorios/HH08-RELATORIO.md` (provedor nativo de arquivos padrão; P1 4 838 blocos idênticos, P2 0 setores; os dois discos numa instalação só, sem troca; falhas seguras) |
| HH09 | CONCLUIDA | `docs/relatorios/HH09-RELATORIO.md` (tempo nativo estável padrão: ordem, quadros e relógios idênticos ao legado; 7 esperas declaradas; experimental opcional) |
| HH10 | CONCLUIDA | `docs/relatorios/HH10-RELATORIO.md` (controles nativos padrão: pd 1.50 sem Maple; traço do PDS, info/name e quadros idênticos ao legado, inclusive desconexão; janela com teclado + SDL3 Gamepad e `controles.toml`) |
| HH11 | CONCLUIDA | `docs/relatorios/HH11-RELATORIO.md` (saves nativos: API bu 1.47 sobre arquivos; chamadas e arquivo iguais ao jogo original no Flycast com VMU; escrita atomica, reinstalacao mantem os saves; exportar/importar VMS) |
| HH12 | CONCLUIDA | `docs/relatorios/HH12-RELATORIO.md` (filmes nativos: 53 MP4 do Builder, open/play/close do objeto do filme 4X sem o decodificador nem a AICA, skip, áudio, legendas do jogo, mods; FFmpeg mantido por decisão do mantenedor) |
| HH13 | CONCLUIDA | `docs/relatorios/HH13-RELATORIO.md` (Vulkan sobre a IR do TA; provedor nativo da camada Kamui sem TA, registradores de render do PVR nem VRAM guest; hashes iguais ao legado e ao HH06) |
| HH14 | CONCLUIDA | `docs/relatorios/HH14-RELATORIO.md` (áudio nativo: driver AUDIO64.DRV reconstruído sem ARM7, motor de 64 canais, reverb do jogo, mods/dublagem; nenhum acesso à AICA no nativo além do RTC; sombra do driver igual à do Flycast) |
| HH15 | BLOQUEADA_DECISAO | `docs/relatorios/HH15-RELATORIO.md` (HH15.0–HH15.8 concluídas: tempo lógico sem ciclos, runtime por papel com catraca, syscalls, Maple, som e render por serviços nativos, toda a simulação de hardware e os provedores legados fora do produto, provas de mapa, imports, strings e sonda, V3 pós-corte 14/14; pendente D-HH15-1: substrato AOT até a HH17) |
| HH16 | PENDENTE | — |
| HH19 | PENDENTE | `docs/planos/HH19-PLANO.md` (remasterização, equivalente ao N9 do BS; incluída pelo mantenedor em 2026-10-28; depois do HH16 e antes do HH17; só começa com autorização do mantenedor) |
| HH17 | PENDENTE | — |
| HH18 | PENDENTE | — (Builder com Recompiler/Developer antecipado em 2026-10-25; distribuição final pendente) |

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
- Divergência conhecida (→ HH14/HH12): os vídeos 4X são abandonados. Mecanismo
  comprovado: a fila de comandos de som (32 slots na RAM da AICA) não é
  consumida sem o driver ARM7 e todo comando falha a partir do VBlank 56; no
  Flycast os slots são zerados. A correção é a reconstrução nativa do áudio no HH14 (o provedor nativo
  de vídeo do HH12 substitui a reprodução inteira e não depende dessa fila),
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
  0x8C0C4CE0: `[[dependencia_hw]]` com regiões, dono HH13 e evidência; confirmação só pela
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

### HH10 (2026-10-25)

- Portados do N3 do BS: `native/platform/{input.h,input_map,input_system}`, `transitional/{input_bridge,
  pad_contract,pad_adapter,pad_probe}`; contrato refeito para o `pd Ver 1.50` (gatilhos com histerese,
  travas na RAM do SDK). `--controles legado|nativo`; nativo padrão (C1–C9).
- Sonda de contrato: 0 divergências em 54 264 unidades; matriz: 0 diferenças em 53 520 casos contra
  o gerado. Coleta da amostra no próprio disparo (regra medida no HH, diferente do BS).
- Corrigido: a transição injetada dos roteiros `indice transicao` nunca andava no HH (arma no VBlank-In,
  conta no primeiro acesso ao MDST). Área nativa do BIOS HLE movida de 0x8C004000 (área do adaptador
  de depuração, 4 literais no executável) para 0x8C006000.
- Revisão Codex: 11 achados, todos aceitos e corrigidos (relatório, seção 9).

### HH11 (2026-10-25)

- Contrato da `bu` 1.47 (14 entradas, duas evidencias cada) e oraculo do jogo original no Flycast com VMU
  (c0-c4): protocolo, VMS do HH (0x48 = bytes cobertos pelo CRC), op 0x0B = leitura do arquivo inteiro,
  `file_info` de 18 bytes, data = relogio do jogo.
- Portados do N4 do BS: `native/platform/saves*`, `transitional/{bu_machine,bu_adapter}`, `tools/dev/hh11_vmu.py`.
  `--saves legado|nativo`; nativo padrao na janela (`Documentos\DC-Recomp\Head Hunter\saves`); o `hh-run` e o
  `--headless` ficam no legado (roteiro de referencia sem memory card).
- Builder: `saves\` e `controles.toml` sobrevivem a reinstalacao. Revisao Codex: 5 achados, aceitos.
- Pendente para o HH16: saves de partida (`H_HUNTER.001..008`) em gameplay e a passagem real do disco 1 para o 2.

### HH15 (2026-10-28, BLOQUEADA_DECISAO em 2026-10-30)

- Plano `docs/planos/HH15-PLANO.md` (modelo: N2c + N8 do BS). Inventário (sonda na rota nativa): store queues, CCN
  (QACR, INTEVT), TMU, Holly INTC/SB, DMAC ch2, G2 (trava do RTC), Maple, RTC da AICA e SPG ainda alcançados; AICA não.
- HH15.0: `tools/dev/hh15_referencias.py` (quadros, trace de ordem sem ciclos, relógios, RAM em 5 VBlanks, áudio,
  resumo), casos `estavel`, `experimental` e `experimental_saves`, reproduzidos (6/6 por caso).
- HH15.1: espera do `bu` `0x8C118346` declarada (o experimental com saves nativos parava no VBlank 2 047); origem das
  entregas (`[irq]`); cadência B (atendimento só quando pedido no tempo lógico, despertar por nível entregável, tick de
  vivacidade); `runtime/memory/sys_logical` (TMU canal 0, CCN não gráfico, porta, ICR/IPR do boot) com 0 acessos fora
  do contrato. Estável idêntico; experimental idêntico fora de 7 SPCs da interrupção da AICA (lista fechada
  `config/hh15_excecoes_spc.toml`, comparador com negativos); ruído de ciclos sem efeito no experimental (o estável
  diverge, controle); TMU parado é acusado.
- HH15.1 concluída (2026-10-29): INTEVT também pelo serviço (INTC 30 191 acessos, 0 fora do contrato); despertar no
  `rte` com desvio; **tempo lógico padrão da janela** (`--headless` no estável) pelo portão `tests/diff/hh15_tempo.py`
  (modelo N2b2 do BS): T1 os 5 205 blocos lidos iguais em ordem; T2 1 779 trocas de quadro iguais até o pulo do último
  filme, fora da lista fechada `config/hh15_excecoes_quadros.toml` (entrada do logo no boot; tela de carga que só o
  estável desenha, porque a carga cobra ciclos estimados) e da sobra do filme; T3 2 594 comandos de áudio iguais; T6
  lógico 18,4 s contra 45,9 s. Janela: rota inteira no tempo lógico, Vulkan, XAudio2, saves nativos. Etapa de regressão
  `portoes_hh15`.
- Revisão Codex (`reviews/codex-hh15_1.log`), 6 achados, todos aceitos e corrigidos com contraexemplo nos testes
  sintéticos: T1 na direção errada, T2 sem fixar o lado lógico e com sobra ilimitada, T3 aceitando igualdade só até o
  corte, multiplicidade das exceções de SPC, arrays do cache fora da faixa do contrato, laços nativos do Kamui sem o
  tick de vivacidade.
- HH15.2: ponte de interrupções separada do modelo do INTC; runtime por papel (manifesto `runtime_papeis.tsv`);
  sonda, ruído, cadência de referência e mutações só no `hh-run` (o produto compila stubs e recusa as 32 variáveis de
  teste, classificação fechada em `runtime/debug/validacao_vars.h` conferida por varredura do código); catraca
  `hh15_produto_papeis` (manifesto, inventário das fontes, mapa do linker por campo, carimbo PE) com 9 negativos.
  Revisão Codex: 5 achados, todos corrigidos.
- HH15.3: serviço nativo das syscalls (`runtime/system/syscall_service`: GDROM, FLASHROM da partição 2 volátil,
  SYSINFO, fim do jogo; fora do contrato erro explícito); `--sistema hle|nativo` no `hh-run`; BIOS HLE, flash e drive
  fora do produto (papel `hle`). Referências 18/18 com o serviço; unitários de equivalência com o HLE e de recusas.
- HH15.4: barramento sem dispositivos (`runtime/system/maple_bus`) com o efeito do modelo sem controle; modelo do Maple
  fora do produto (papel `perifericos`); o produto recusa `--arquivos legado` e `--controles legado`.
- Regressão integral depois da HH15.4: 14/14 (`analysis/hh15/regressao.log`), inclusive `portoes_hh15`.
- Revisão Codex da HH15.3/15.4 (`reviews/codex-hh15_34.log`): 4 achados aceitos e corrigidos (comandos do GDROM
  validados no REQ_CMD; troca de presença do controle depois de um DMA aborta e o host zera o Maple antes de restaurar;
  o produto também recusa `--saves/--video/--audio legado`; unitário de equivalência barramento × modelo
  `hh15_maple_tests` e teste com dados dos 6 DMAREAD + GETSCD).
- HH15.5 (concluída, 2026-10-29): o `HeadHunter.exe --headless` do produto passa a usar saves (cartão temporário), vídeo e áudio
  nativos; referência própria do produto (`snapshots/hh15/produto_hashes.txt`, 94 hashes, roteiro
  `newgame_area1_nativo`) e o passo `builder` da regressão confere o executável instalado contra ela. Regressão
  integral 14/14 (`analysis/hh15/regressao2.log`, 2026-10-29). Recusas dos 5 provedores legados antes de qualquer
  dado, negativo `hh15_produto_recusa_legado`; ctest 63/63.
- HH15.6 (concluída, 2026-10-29): área de som nativa `runtime/system/sound_bus` (papel `nativo`) com o contrato
  medido na rota nativa (só o RTC `0x00710000/04`, de `0x8C1706E8`, 9 400 leituras cada): RTC no relógio do provedor
  de tempo com a liberação de escrita, controle guardado, `HH_CLOCK_LOG` `Rtc`/`Aud`; o resto da área ou outro tamanho
  é erro explícito. O modelo `aica/aica_regs.cpp` (papel novo `aica`, só verificação) delega controle e RTC ao serviço;
  o produto compila `system/aica_ausente.cpp` e não tem `aica_regs` no mapa (catraca com os negativos `com_aica`,
  `aica_solto`, `aica_outra_biblioteca`, `sem_stub_aica`). Unitários `hh15_som_tests` (roteiro do contrato, clock log
  com hashes calculados dos valores, reset entre sessões, 16 recusas). Revisão Codex (`reviews/codex-hh15_56.log`):
  aceitos [MÉDIA] recusas dos provedores legados contornáveis por `--verify-data` e pela exportação/importação de
  saves (agora exame do `argv` no começo do `main`; negativo nos 4 modos, que falhou antes com 15 casos), [MÉDIA]
  cobertura do reset e do `HH_CLOCK_LOG` no unitário, [BAIXA] catraca só com o prefixo `hh_runtime:`; rejeitado o caso
  do RTC sem valor definido como teste separado (mesmo código do reset; só existe antes do primeiro reset do processo).
  Regressão integral 14/14 (`analysis/hh15/regressao3.log` antes da revisão; `regressao4.log` depois); ctest 67/67.
- HH15.7a (concluído, 2026-10-29): renderizador em software sobre a IR (`native/render/soft`: o rasterizador de
  referência movido do `pvr_render` sem mudar as contas, e o `SoftwareDevice`); os dois renderizadores desenham a IR
  com o provedor nativo do Kamui (o software sobre a IR também é o recuo do Vulkan); um render que o renderizador da IR
  não desenhou é erro explícito; o rasterizador sobre o TA só nos modos de verificação. Rota da área 1: 2 250 renders
  pelo software sobre a IR, 94 hashes iguais ao HH06; `conferir` compara o software sobre a IR com o caminho do TA em
  todo render (negativo `HH_RENDER_SOFT_MUTACAO=textura`). Revisão Codex (`reviews/codex-hh15_7a.log`): aceitos [MÉDIA]
  R1 amostrado, [MÉDIA] falha do software caindo no caminho do TA em silêncio, [BAIXA] negativo sem distinguir falha
  de divergência; nenhum rejeitado. Regressão integral 14/14 (`analysis/hh15/regressao_7a.log`).
- HH15.7b (concluído, 2026-10-29): `native/render/kamui/list_sink` divide as listas da área 4 e as decodifica direto
  para a IR (as regras do Parser do `pvr_render` e da captura, num lugar só); com a captura como o renderizador, o
  STARTRENDER nativo desenha pela IR do ListSink, sem o decodificador do legado nem o TA, com erro explícito para
  vértice que a IR não representa ou render não desenhado; nos modos de verificação o caminho do TA é a referência e o
  comparador confere o ListSink em todo render (negativo `HH_LISTA_NATIVA_MUTACAO=vertice`, portão K5). Rota da área 1:
  2 250 renders pelo ListSink, 94 hashes iguais ao HH06, ListSink igual à captura em 2 250. Revisão Codex
  (`reviews/codex-hh15_7b.log`): aceitos [MÉDIA] ambiente de ponto flutuante do guest na decodificação, [BAIXA]
  `HH_TA_DUMP` no caminho nativo, [BAIXA] teste do comparador contra captura vazia, [BAIXA] contagem de vértices sem
  suporte fora do comparador; nenhum rejeitado. Regressão integral 14/14 (`analysis/hh15/regressao_7b.log`).
- HH15.7c (concluído, 2026-10-29): estado do quadro, memória das texturas, texels e imagem exibida como serviços
  nativos (`native/render/kamui`: `FrameState`, `TextureHeap`, `texture_source` com a paleta de 8 bits,
  `display_service`); o produto renderiza e exibe sobre eles, sem empréstimo ao banco do PVR nem à VRAM do runtime (o
  SPG continua lido do runtime até o 7d). Comparadores: no `conferir`, as funções nativas contra as do runtime em todo
  render, textura e VBlank; na sombra, o estado nativo contra o real do runtime (2 250 quadros, 39 422 texturas, 0
  divergências); negativos `HH_KAMUI_NATIVO_MUTACAO=quadro|textura|framebuffer|video` (portão K6). Revisão Codex
  (`reviews/codex-hh15_7c.log`): aceitos [ALTA] referência do `conferir` sobre o estado emprestado (corrigido pela
  sombra independente), [MÉDIA] unitário só com 565 progressivo, [BAIXA] K6 sem contagens exatas; nenhum rejeitado.
  Regressão integral 14/14 (`analysis/hh15/regressao_7c.log`).
- HH15.7d1 (concluído, 2026-10-29): DMA do canal 2 e system block como serviços nativos (`runtime/system/dma_bus` e
  `sb_bus`, papel `nativo`): o canal 2 entrega os blocos direto ao destino nativo das listas e texturas, sem o TA do
  runtime; os modelos `pvr/holly_sb` e `memory/sh4_dmac` vão para o papel `sb` (só verificação) e delegam o contrato;
  o produto liga os stubs `system/sb_ausente` e `system/dmac_ausente` (fora do contrato, erro explícito; catraca com os
  negativos `com_sb` e `sem_stub_sb`). Teste do contrato `hh15_dma_e_system_block_no_contrato` (modelos e stubs);
  K3 exige pedidos e bytes por destino iguais aos da sombra. Revisão Codex (`reviews/codex-hh15_7d1.log`): aceitos
  [MÉDIA] teste dirigido do contrato, [MÉDIA] parcial stubs sem teste sem dados, [BAIXA] contagem do K3; registrado que
  os negativos `HH_SB_MUTACAO` não têm portão desde o HH09. A regressão achou a escrita da tabela de inicialização em
  `0x005F6940` fora do contrato (corrigido, faixa `0x005F6940-0x005F69FF`). Regressão integral 14/14
  (`analysis/hh15/regressao_7d1.log`), ctest 70/70.
- HH15.7d2 (concluído, 2026-10-29): banco de status de interrupção nativo (`native/render/kamui/event_status`, do
  BS): ISTNRM/ISTEXT/ISTERR, máscaras IML*, entrega dos eventos vencidos, nível IRL e contagens. O produto liga
  `runtime/system/event_bus.cpp` (a API de `pvr/holly_intc.h` sobre o banco nativo, mesma ordem de efeitos);
  `pvr/holly_intc.cpp` vai para o papel `intc` (só verificação) e roda o banco em sombra, comparado a cada operação
  (rota da área 1: 83 990 operações, 0 diferentes; negativo `HH_EVENT_STATUS_MUTACAO=limpa` acusa). Catraca com os
  negativos `com_intc` e `sem_event_bus`; portão K7. Revisão Codex (`reviews/codex-hh15_7d2.log`): aceito [MÉDIA] teste
  da entrega dos eventos no serviço do produto (`hh15_intc_entrega_de_eventos_vencidos`, conferido por mutação manual);
  parcial [MÉDIA] a sombra compara só o banco (efeitos no teste dirigido e na rota do produto). Produto: 94 hashes
  iguais. Regressão integral 14/14 (`analysis/hh15/regressao_7d2.log`), ctest 73/73.
- HH15.7d3 (concluído, 2026-10-29): temporização do vídeo (SPG) como serviço nativo (`native/render/kamui/spg_timing`,
  as contas movidas de `pvr/pvr_regs.cpp` sem mudança; `runtime/system/spg_bus`, papel `nativo`): fonte única do SPG
  para os dois provedores de tempo, o provedor nativo do Kamui e o modelo dos registradores (que delega 0x0C8-0x0E0, o
  SPG_STATUS, o dump, o reset e o boot). Testes `tests/hh15_spg_tests.cpp` (grade contra a cópia das fórmulas
  anteriores, valores fixos, contrato; mutações manuais conferidas). Revisão Codex (`reviews/codex-hh15_7d3.log`):
  aceito [MÉDIA] o teste passa a ver `holly_intc_sync` antes do valor novo; [BAIXA] registrado: o SPG pelo funil do
  Kamui não passa mais pelos diagnósticos do modelo. Rota da área 1 e produto: 94 hashes iguais. Regressão integral
  14/14 (`analysis/hh15/regressao_7d3.log`), ctest 73/73.
- HH15.7d4a (concluído, 2026-10-30): o pedido de render (`runtime/system/render_bus`: contagem, gancho do hh-diff,
  `HH_STOP_ON_STARTRENDER`, `HH_TA_DUMP`, render na hora, prazo do fim avisado ao provedor de tempo, fim pendente
  consumido pelo banco de status) e o aviso de VBlank do host (`runtime/system/video_bus`), papel `nativo`, movidos de
  `pvr/pvr_regs.cpp` e `pvr/pvr_video.cpp` sem mudança. Teste `tests/hh15_render_bus_tests.cpp` nos dois executáveis.
  Revisão Codex (`reviews/codex-hh15_7d4a.log`): sem achados (falha de teste de som do sandbox somente leitura,
  rejeitada). Rota da área 1 e produto: 94 hashes iguais. Regressão integral 14/14
  (`analysis/hh15/regressao_7d4a.log`), ctest 73/73.
- HH15.7d4b (concluído, 2026-10-30): o desenho da IR separado da captura: `transitional/render_devices` (produto: os
  dispositivos Vulkan e software sobre a IR com os caches de textura, a fonte dos texels escolhida pelo dono, o
  resumo no mesmo formato e o renderizador do produto, que lê os texels só da memória nativa) e `render_capture` só com
  o caminho do TA (verificação), sobre os mesmos dispositivos com os texels do runtime. Teste
  `tests/unit/test_hh15_render_devices.cpp`. Revisão Codex (`reviews/codex-hh15_7d4b.log`): sem achados. Rota da área
  1 e produto: 94 hashes iguais. Regressão integral 14/14 (`analysis/hh15/regressao_7d4b.log`), ctest 73/73.
- HH15.7d4c (concluído, 2026-10-30): os modelos gráficos no papel `grafico` (só verificação); o produto compila
  `runtime/system/pvr_ausente.cpp` (só o SPG atendido, banco do boot para a instalação do Kamui, sem VRAM; o resto erro
  explícito, inclusive a área 4 pela store queue) e `transitional/render_capture_ausente.cpp`; `HH_RENDER_IR` e as
  variantes de comparação recusadas no produto. Catraca com o papel, os substitutos, `render_capture` proibido e
  `render_devices` obrigatório, e cinco negativos novos. Teste `tests/hh15_pvr_ausente_tests.cpp`. Revisão Codex
  (`reviews/codex-hh15_7d4c.log`): aceito, escrita em `SPG_STATUS` ignorada como no modelo (corrigido, com teste);
  rejeitado, `HH_RENDER_IR_GUARDAR` (observacional, do renderizador do produto). Rota da área 1 e produto: 94 hashes
  iguais. Regressão integral 14/14 (`analysis/hh15/regressao_7d4c.log`), ctest 79/79.
- HH15.7 concluída (2026-10-30): mapa do `HeadHunter.exe` sem `pvr_*`, `ta`, `vram`, `holly_intc`, `sh4_dmac` e
  `render_capture`; R1–R5 e K1–K4 do HH13 e as referências da HH15 na regressão integral.
- HH15.8 (concluída, 2026-10-30): prova de imports e strings `tests/hh15/check_fechamento.py` (imports e delay-load
  na lista fechada; nenhum dos 517 literais exclusivos das fontes fora do produto no executável, mínimo por grupo;
  negativos `dll_extra`, `sem_imports`, `string_<grupo>` e autoteste sintético); sonda pós-corte com as regiões do
  HH15.0, só o contrato medido e atendido por serviços nativos (PVR só SPG, DMAC só canal 2, sem VRAM nem área 4).
  Revisão Codex (`reviews/codex-hh15_8.log`, `codex-hh15_8b.log`): 7 achados aceitos e corrigidos, nenhum rejeitado.
  HH15.2–HH15.4 concluídas com os portões. Regressão integral V3 pós-corte 14/14 (`analysis/hh15/regressao_hh15_8.log`),
  ctest 92/92. Relatório `docs/relatorios/HH15-RELATORIO.md`.
- **DECISAO_NECESSARIA — HH15 (D-HH15-1):** o substrato AOT (código gerado, RAM guest, `Sh4Context`, despacho e
  fibras) fica no produto até a HH17, como no BS (D-N8-1), ou sai agora, como o texto do ROADMAP pede? Recomendação:
  manter até a HH17 e corrigir ROADMAP/ARCHITECTURE. Detalhes, opções e custos: relatório, seção 9.
- Limitação registrada: no tempo lógico a tela de carga antes da cena de abertura (anúncio do jogo) não aparece, porque
  a carga não consome tempo; o estável a mostrava só pelo custo estimado de ciclos da descompressão.

### HH14 (2026-10-27, concluída)

- Plano `docs/planos/HH14-PLANO.md`; inventário reproduzível `tools/dev/hh14_inventario.py` (streams, pacotes AHFF,
  resumo do `HH_FN_PERFIL`). Músicas: 21 + 17 streams CRI ADX; falas e efeitos: WAV com ADPCM da AICA (streams e
  amostras dos pacotes AHFF: 368 / 307 nomes distintos, 77 / 70 com conteúdo diferente entre pacotes); reverb
  `HHREV.FPB/FOB` (SFPB/SFOB), igual nos dois discos.
- Fronteira: camada `ac*` do Audio64 (0x8C120534–0x8C1232FE; nomes pelas mensagens de erro, ABI pela desmontagem),
  abaixo do motor de som do jogo (`SoundEngine_*`) e do CRI ADXT. Legado: sem o ARM7 a fila de comandos ao driver
  enche no VBlank 56 e os comandos seguintes são descartados (740 `can't send driver command` no roteiro): hoje o jogo
  roda com o som morto, e a referência do áudio nativo é o Flycast com o driver real, não o legado.
- Fase A: cada `ac*` ligada ao opcode tratado pelo `AUDIO64.DRV` (`tools/dev/hh14_ac_opcodes.py`, `hh14_drv.py` com
  Capstone 5.0.7 fora do produto); porta = canal da AICA; modelo do canal sem envelope (liga e desliga sem rampa).
- Fase C: `hh_native_audio_core` (ADPCM, mixer, WAV, DSP portados do N6 do BS); leitor de WAV adaptado ao `fmt` de
  18 bytes com preenchimento e ao ADPCM estéreo com bytes alternados (correlação L/R 0,96–0,999 nas falas das cenas,
  `tools/dev/hh14_estereo.py`); `hh14_audio_tests` 22/22 nos dois discos (74 + 44 WAV, reverb `HHREV`); etapa
  `portoes_hh14` na regressão; `ctest` completo 46/46.
- Fases D–F: `transitional/audio_device` (driver reconstruído: comandos, boot, posições, eventos, interrupção pelo INTC do
  Holly, DMA do G2 pela fila do Audio64) e `native/audio/aica_channels` (64 canais na RAM de som, LEA exclusivo);
  `gdFsSetDir` nativo (streams em `\Streams`); `--audio nativo` padrão na janela, `hh-run`/`--headless` no legado;
  roteiro `newgame_area1_nativo` (filmes e áudio nativos); portão `tests/diff/hh14_audio.py` A1–A5 ok (oráculo do
  Flycast: sombra das portas da música igual); mods por `modsssets` e `hh-builder --audio-mod` (`docs/MODS-AUDIO.md`).
- Revisão Codex (`reviews/codex-hh14.log`), 5 achados: 3 aceitos e corrigidos (Stop/MultiStop pelo LPCTL do canal,
  limpeza do DSP desliga o reverb, `--audio-mod` só mono/estéreo), 1 aceito em parte (disco 2 pelos formatos na
  regressão; cenas com a campanha, HH16; comparação byte a byte do anel rejeitada por ser diferença de tempo), 1
  rejeitado com evidência (LEA = tamanho − 1 é o que o driver grava; o hardware também não toca a última amostra).
- Limitações: cenas do disco 2 (HH16); gameplay sem trecho correspondente no Flycast; envelope/filtro/LFO não modelados
  (o driver os deixa em repouso); RTC da AICA fica com o tempo (HH15).

### HH13 (2026-10-26 a 2026-10-27, concluída)

- Renderizador nativo (`docs/planos/HH13-PLANO.md`, N7 do BS): `native/render` + `native/render/vulkan` (SPIR-V
  versionado, Vulkan-Headers Apache-2.0) e `transitional/render_capture` (IR a partir do decodificador do TA);
  `--render software|nativo`, nativo padrão na janela, headless e `hh-run` no software.
- Portões no roteiro da área 1 (`tests/diff/hh13_render.py`, etapa `portoes_hh13`): R1 2 250 renders conferidos, 0
  divergências (triângulos, imagens, texturas, quadros, volumes); R2 Vulkan × software com maior diferença de 1 nível;
  R4 duas execuções nativas iguais e iguais ao software nos 94 pontos; R5 Vulkan 72,9 s contra 200,3 s do software
  (headless sem pacing) e tempo real na janela. `hh13_render_tests` 36/36 (GPU), `hh13_capture_tests` 16/16.
- Correção própria (herdada do BS): textura linear com stride menor que a largura lia além do buffer; teste com mutação.
- Revisão Codex (`reviews/codex-hh13.log`), 7 achados, todos aceitos e corrigidos: regra linear do `texture_source` só
  nos formatos de 16 bits sem mipmap (paleta), R4 com duas execuções nativas e R2 com `comparados == renders`, `h == 0`
  sem underflow, testes determinísticos do alcance (sem depender de comportamento indefinido), SKIP só com o Vulkan
  indisponível e R5 exigindo Vulkan em todos os renders, resumo sempre que instalado, atribuição do código portado.
- Regressão integral ok (`analysis/hh13/regressao.log`).
- HH13.6 (decisão B): provedor nativo da camada Kamui (`transitional/kamui_device`) sobre os cinco funis do SDK
  (0x8C1793E0, 0x8C1793EA, 0x8C17C600, 0x8C159820, 0x8C159874). Sombra: 0 divergências em 2 250 renders. Nativo
  (padrão de `--render nativo`; `HH_KAMUI=legado|sombra|nativo`): registradores no banco nativo, dados da área 4 no
  destino nativo (`ta_set_sink`), cópia/zeragem na memória nativa, render e quadro com o estado nativo emprestado; o
  slot reproduz o tempo do código gerado substituído (ciclos e pontos de atendimento). Portão `tests/diff/hh13_kamui.py`:
  K1 sombra 0 divergências, K2 94 hashes iguais ao legado e ao HH06, K3 sonda sem TA/VRAM e do PVR só SPG (HH15), K4
  IR conferida com o estado nativo. Prova negativa: sem o gasto de ciclos, hashes divergem no VBlank 150.
- Revisão Codex do HH13.6 (`reviews/codex-hh13_6.log`), 4 achados, todos aceitos e corrigidos: laços de cópia/zeragem
  com os pontos de atendimento e o `pc` de retomada do original (e `pc` preservado com `redirect`); leitura gasta os 3
  ciclos antes de ler; escrita devolve `r0 = 1`; referência do HH06 obrigatória na regressão e K4 com o nativo.
- Fica para o HH15: INTC do Holly, SB, DMAC, SPG, store queues para a RAM, o código de `runtime/pvr` usado com estado
  emprestado e o custo em ciclos dos slots.
- Regressão integral ok após o HH13.6 (`analysis/hh13/regressao.log`, `portoes_hh13` 5/5).

### HH12 (2026-10-26)

- Contrato (HH12.0, `docs/planos/HH12-PLANO.md`): PlayScene 0x8C09E904 continua original; o provedor nativo
  (`transitional/video_adapter`, `--video nativo`) atende open/áudio/play/close do objeto do filme 4X (slots
  0x8C11E9D2, 0x8C11EBE0, 0x8C11ECA4, 0x8C11EB32). O play repete a ordem do laço original (quadro, efeitos por quadro
  0x8C14A220/0x8C2388DC/0x8C016DF8, callback do jogo, espera do próximo instante no ponto declarado 0x8C11FC3A, fim ou
  skip por obj+0x33C) e o host apresenta o quadro do filme no lugar do quadro do PVR, com as legendas do próprio jogo
  (lista *0x8C151E10, opção *0x8C151E14) desenhadas por DirectWrite. Áudio pelo XAudio2 (saída nula no headless).
- Builder: `builder/video` (FFmpeg só decodifica, como processo; Media Foundation codifica H.264 + AAC 48 kHz),
  `data\video\manifest.toml`, reaproveitamento pelo SHA-256 real da fonte; `hh-builder --filmes <data>` e `--ffmpeg`.
  Os 53 filmes: 5 min 52 s, quadros == vtrk[4] em todos, PSNR médio 36,6–56,2 dB contra o FFmpeg, áudio com atraso 0
  e SNR ≥ 28,4 dB.
- Portões `tests/diff/hh12_video.py` (ctest `hh12_video`, etapa `portoes_hh12` da regressão): V0 slot; V1 0 entradas
  no stream e no decodificador 4X no nativo (o legado cria e abre o stream); V2 SCENE0 do VBlank 282 ao fim natural
  (1 023 VBlanks), skip por START, SCENE01_06 depois, ausente, MP4 alterado recusado pelo SHA-256, mod pelo nome,
  60 legendas do jogo no SCENE01_06; V3 os 25 + 28 filmes; V4 determinismo e tempo experimental. `hh12_video_tests`
  7/7 (com a conversão do SCENE0 do disco).
- V3 contra o decodificador do próprio jogo (Flycast rodando livre, `hh12_v3_livre.py`): quadros estáticos iguais à
  quantização ARGB1555 (erro máximo 9); quadros com movimento com o mesmo conteúdo e diferença média de 3–5 níveis
  (o 4xm do FFmpeg não é bit a bit o do jogo nos quadros P). Decisão do mantenedor: manter o FFmpeg (opção A).
- Promoção: `--video nativo` padrão na janela; `--headless` e `hh-run` no legado (roteiro de referência). O legado não
  toca os filmes (o laço sai no VBlank 312 sem o ARM7).
- Revisão Codex (`reviews/codex-hh12.log`), 8 achados. Aceitos e corrigidos: legendas (agora as do jogo, e mods SRT/VTT),
  SHA-256 do MP4 no open e erro do decodificador não conta como fim natural, SHA-256 real da fonte no reaproveitamento
  (teste negativo), cancelamento do FFmpeg por thread vigia e espera limitada, mod por nome não sai da pasta por
  link/junção, áudio com controle de fluxo e troca para a saída nula se o dispositivo cair, evidência do 0x8C142E78 no
  plano, os dois discos no portão. A promoção (ALTA) foi resolvida pela decisão do mantenedor (opção A).

### Builder do HH18 antecipado e reorganização (2026-10-25/26, pedido do mantenedor)

- Builder refeito com a arquitetura do Builder do Blue Stinger (MIT, portado com cabeçalho de origem):
  `builder/builder_core` + `HeadHunter-Recompiler.exe` (os dois CDIs -> C++ -> HeadHunter.exe instalado),
  `HeadHunter-Developer.exe` (exportar o projeto C++, compilar projeto com mods, inspecionar recursos) e
  `hh-builder` (linha de comando). Troca segura de instalação (`install_swap`, diário `.<nome>.troca`, a `.anterior`
  antiga nunca apagada), `data.ok` com SHA-256 dos manifestos, `instalacao.toml` (saves portáteis/Documentos),
  `HeadHunter.exe --version` e `--verify-data`. Revisão Codex (`reviews/codex-builder.log`): 7 achados, todos aceitos e
  corrigidos. Testes: CLI ponta a ponta igual à referência HH06 (94 hashes), exportar e compilar o projeto exportado
  sem CDI com o mod de exemplo, Recompiler `--auto`, ctest 37/37.
- Ordem HH13/HH14 trocada pelo mantenedor (render Vulkan antes do áudio, por desempenho, como no BS).
- `dist/` no leiaute do BS: `tools/dist/make_builder.ps1` -> `dist/HeadHunter-Builder` (compilado em `build/dist-builder`
  com `HH_BUILDER_DEV_KIT=OFF`: sem caminhos da máquina; confere ausência de conteúdo do jogo e dependências);
  `tools/dist/make_preview.ps1` -> `dist/HeadHunter-Preview`; cópia por etapa `HeadHunter-Preview.hh11-e26d1cf`.
  A regressão instala em `build/regressao`. O `hh-builder` da distribuição, só com o próprio kit, compilou e instalou o
  jogo dos dois CDIs (`--version` 0.11.0).
- Correção: `.gitignore` tinha `dist/` e `analysis/` sem âncora, que escondiam `tools/dist/` e o código-fonte
  `translator/analysis/` (nunca versionados; um clone novo não compilaria). Agora `/dist/` e `/analysis/`; os dois
  entram no repositório. Encontrado pelo teste ponta a ponta do kit da distribuição.
- Correção (regressão de 2026-10-26): a troca a partir de uma instalação do Builder antigo (marca `.hh-builder`, sem
  `.hh-package`) levava a marca antiga e o `data\` velho como se fossem do usuário. `is_package_file` agora os trata
  como do pacote; teste `hh18_builder_tests` (falha sem a correção: 4 falhas; passa com ela).
- Regressão integral (`python -I tools/dev/regressao.py`, `analysis/hh18/regressao2.log`): build, ctest (38/38),
  portões HH05/HH07–HH11, roteiro da área 1 e Builder (reinstalação com `.anterior` e dados do usuário) — todas ok.

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

Decisão do mantenedor (2026-10-27, HH13): opção B, os provedores nativos da camada Kamui (sem TA, PVR, Holly nem
VRAM guest no caminho nativo) são feitos ainda no HH13, antes do áudio.

Decisão do mantenedor (2026-10-26, HH12): os filmes continuam decodificados pelo FFmpeg (opção A). O 4xm do FFmpeg
reproduz o conteúdo do decodificador do jogo (estáticos iguais à quantização ARGB1555; com movimento, diferença média
de 3–5 níveis); a alternativa do decodificador do próprio jogo no Builder foi recusada pelo custo.

Decisão do mantenedor (2026-10-24): o FFmpeg (LGPL) pode ser usado como
ferramenta externa de conversão dos vídeos 4X Movie no Builder (ARCHITECTURE.md
A11): processo separado, sem link nem código copiado. A forma de distribuição
do binário com o Builder fica para o HH18.

Questões da revisão geral resolvidas pelo mantenedor ("arruma todos os
problemas", 2026-10-24): headers aceitos no lugar de módulos nomeados;
branch `main`. O primeiro commit continua aguardando pedido explícito.

## Próxima ação permitida

Prosseguir pelo **HH15** (fim do tempo guest e remoção completa do legado) e as etapas seguintes. O teste da passagem real do
disco 1 para o 2 fica para depois (HH11/HH16). Commits por etapa autorizados pelo mantenedor
(2026-10-25).
Parar somente em `DECISAO_NECESSARIA` legítima ou quando
o mantenedor enviar `PAUSAR`.
