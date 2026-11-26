# STATUS — Head Hunter Recompiled

Atualizado em 2026-11-26. Este arquivo é a fonte autoritativa do checkpoint.

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
| HH15 | EM_ANDAMENTO | `docs/planos/HH15-PLANO.md` (parte 1, HH15.0–HH15.8, concluída: tempo lógico, toda a simulação de hardware e os provedores legados fora do produto, V3 14/14, `docs/relatorios/HH15-RELATORIO.md`; D-HH15-1 decidida B em 2026-10-30: parte 2 em curso, remoção do substrato AOT com a elevação para `native/game`: HH15.9 inventário, HH15.10 infraestrutura e HH15.11a runtime C concluídos, HH15.11b biblioteca matemática com as 33 elevadas e a sonda dos back-edges feitas, HH15.11c cópias de string e por palavras (39 elevadas, 2026-11-18, regressão integral aprovada em 2026-11-25), HH15.11d `memcmp` e `strncpy` (41 elevadas, 2026-11-25), HH15.11e delegação fora do contrato e `memmove` (42 elevadas, 2026-11-26; regressão integral da HH15.11d/e aprovada), HH15.11f `strtol`, `atoi` e o limite de 512 caracteres (45 elevadas, 2026-11-26; regressão integral aprovada), HH15.11g `log10f` e os auxiliares da saída formatada (50 elevadas, 2026-11-26)) |
| HH16 | EM_ANDAMENTO | `docs/planos/HH16-PLANO.md` (campanha por gravação única do mantenedor, decidida em 2026-11-14). HH16.0 (gravação, estado por leitura, reprodução) e HH16.1 (monitor `tools/dev/campanha.py`) implementadas e testadas sem janela; P0 da passagem de disco e do LOAD do disco 2 corrigidos; placar dos portões na seção 9 do plano. **Falta: a sessão piloto do mantenedor (HH16.P, P3) e os testes de GPU**; depois, a gravação (HH16.2) |
| HH19 | PENDENTE | `docs/planos/HH19-PLANO.md` (remasterização, equivalente ao N9 do BS; incluída pelo mantenedor em 2026-10-28; depois do HH16 e antes do HH17; só começa com autorização do mantenedor) |
| HH17 | PENDENTE | — |
| HH18 | PENDENTE | — (Builder com Recompiler/Developer antecipado em 2026-10-25; distribuição final pendente) |
| HH20 | PROPOSTA | `docs/planos/PLANO-ETAPAS-RESTANTES.md` seção 5 (Linux nativo, equivalente ao N9-L do BS; proposta de 2026-11-14, D-P4 pendente; antes da HH19.1) |

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

### Primeira publicação (2026-11-01, pedido do mantenedor)

`main` e a tag `v0.11.0` (`1d61333`) publicados em `rodrigo-mendonca/HeadHunterRecomp`, que redireciona para
`DCRecomp/HeadHunterRecomp` (público). Só esses refs: `refs/codex/*` são locais e não sobem (guardam um PNG de pareamento).

- Auditoria antes do push: 1.071 blobs de `main` sem amostras de bytes dos `1ST_READ.BIN`/`IP.BIN`, sem blocos longos de hex,
  sem segredos; 13 strings do executável, curtas (diagnósticos citados como evidência ou preenchimento).
- Defeito achado pelo teste do pacote e corrigido: `third_party/vulkan` faltava no kit (C1083 em `vulkan/vulkan.h`). A
  Preview não mostrava porque usa o repositório como kit. `check_kit` agora exige o cabeçalho.
- Pacote `HeadHunter-Builder-v0.11.0-win64.zip`, de uma worktree limpa da tag (sem o WIP da HH15.11b e sem `icone-local`):
  o `hh-builder` do pacote, só com o próprio kit, foi dos dois CDIs ao `HeadHunter.exe` instalado e verificado. SHA-256
  `c133b99e0d44e68e6cce3f250f60880b794f25c073168fabdaf74158e9f842d8`.
- O mantenedor confirmou a migração para a organização `DCRecomp`; o `origin` aponta para lá. Release `v0.11.0` publicada
  como pré-lançamento, com o zip acima; o download público confere com o SHA-256.

### Segunda publicação, v0.12.0 (2026-11-22, pedido do mantenedor)

- README reescrito para quem vai testar (requisitos, passo a passo, controles, saves, o que testar, problemas
  conhecidos, como relatar, resumo em inglês) e com o selo "Get it on RecompLauncher", exatamente como o
  `recomplauncher-agent.md` pede, apontando para `recomplauncher.com/game/headhunterrecomp` (id achado no
  `pc-catalog-web.json` pelo `repo` `DCRecomp/HeadHunterRecomp`). Nenhuma issue nem PR aberta no repositório.
- Auditoria dos 81 blobs novos desde a `v0.11.0`: sem binários, blocos de hex nem segredos; janelas de 24 bytes do
  `1ST_READ.BIN`/`IP.BIN` só em textos curtos já citados como evidência (4 mensagens de diagnóstico e
  `acDigiGetCurrentReadPosition` no config, "can't send driver command" no STATUS), a mesma categoria aceita na v0.11.0.
- Pacote `HeadHunter-Builder-v0.12.0-Windows-x64.zip` montado por `tools/dist/make_builder.ps1 -Zip` numa worktree
  limpa (`tmp/wt-v0.12.0`, commit `83781a9`; a tag ficou no commit seguinte, que só muda README e STATUS, fora do kit),
  com a conferência de conteúdo contra o executável do jogo. SHA-256
  `4d9e9d289d8cc6c870fb4004fa22cf21466d3b5ae717382467172ab038d3acf9`. De ponta a ponta: o zip descompactado, o
  `hh-builder` do pacote com os dois CDIs reais terminou com código 0 em 440 s; o jogo instalado tem os 53 filmes e o
  `--headless` do roteiro `newgame_area1_nativo` deu os 94 hashes de `snapshots/hh15/produto_hashes.txt`.
- `recomplauncher.json` do pacote e do repositório validados sem erros pelo esquema oficial baixado hoje.
- Não verificado: o próprio RecompLauncher (Developer tools, "Add a build") e uma máquina diferente desta.

### Ícones dos executáveis (2026-11-01, pedido do mantenedor)

Padrão do Blue Stinger (MIT), ampliado a todos os executáveis por `cmake/HHIcone.cmake`:

- `app/res/headhunter.ico` (versionado): desenho original, só geometria; é o das distribuições publicáveis.
- `icone-local/headhunter.ico` (ignorado, `LEGAL.md`): tela de título da cópia do usuário
  (`python tools/dev/gerar_icone.py --titulo analysis/hh05/run3/frame_010800.png`). Rosto com logotipo em 128/256 px;
  rosto de perto de 16 a 64 px. Usado nos builds locais; `-DHH_ICONE_LOCAL=OFF` o desliga.
- Executáveis: HeadHunter, HeadHunter-Recompiler, HeadHunter-Developer, hh-builder, hh-run, hh-diff, hh-recomp, hh-disasm,
  hh-hwscan, hh-extract. Recompiler e Developer agora usam o recurso 1 na classe da janela. `icone-local` entrou em
  `kKitEntries` (opcional).
- `tools/dist/make_builder.ps1` sai com o desenho original por padrão e confere o recurso dos executáveis montados;
  `-IconeLocal` leva a arte do jogo aos executáveis e ao kit, marca `VERSAO.txt` como uso local e **não se publica**.

Evidências: ctest `hh_icones` e `hh18_builder_tests` passam (`hh_icones` lê o recurso dos 10 executáveis e recusa um .ico
alterado em um byte). Builder sem `-IconeLocal`: 3 executáveis iguais a `app/res`, e diferentes do ícone local (saída 1).
`-NoBuild -IconeLocal` sobre executáveis originais é recusado (saída 1). `dist/` remontado em `be7de69-dirty`: Preview e
Builder com o ícone local conferido. Janelas abertas de Recompiler, Developer e HeadHunter mostram ícone próprio na classe
(`GetClassLongPtr`; o SDL3 usa o primeiro `RT_GROUP_ICON`). A regressão integral não foi rodada: a mudança só acrescenta recursos e troca o `hIcon`.
Limitações: a Preview e o Builder de `dist/` levam as alterações ainda não commitadas da HH15.11b. Hipótese aberta: as
texturas `TITLE_*` usam o pixel PVR código 5, ainda sem decodificação confiável, por isso a fonte é a captura composta.

### HH16 — preparação (2026-11-14, só CPU: a GPU estava ocupada pelo BS)

- Build Release (`cmake --build build --config Release -- /m:8`): sem erro nem aviso. Suíte sem GPU: `ctest -C Release
  -j2 -E "^(hh13_render_tests|hh13_capture_tests|hh13_render|hh15_tempo)$"` com os dados privados (`HH_DATA_DIR`,
  `HH_DATA_DIR2`, `HH_SNAPSHOT_DIR`, `HH_AREA1_HASHES`, `HH07_BIN`, `HH07_SONDA`, `HH11_ORACULO`, `HH14_*`): **90/90**
  antes das correções (2 112 s), **90/90** depois do slot do LOAD (1 998 s), **90/90** depois do slot do fim de
  disco (2 018 s) e, com todas as correções das duas revisões e os testes novos `hh16_campanha` e `hh16_reproducao`
  (`HH_DATA_UNICO`), **92/92** (2 222 s), sem SKIP. Os 4 testes excluídos usam o
  Vulkan e ficam para quando a GPU estiver livre. Esconder o driver com `VK_DRIVER_FILES` não funciona: o terminal
  roda elevado e o carregador do Vulkan ignora a variável (o `hh13_render_tests` e o `hh13_capture_tests` usaram a GPU
  por cerca de 4 s cada; o portão de desempenho em 4K falhou com a GPU ocupada, 16,9 ms contra 4 ms, sem regressão).
- Temporários dentro do workspace (regra de 2026-11-14): `tests/CMakeLists.txt` dá `TMP`/`TEMP` = `tmp/ctest` a todos
  os testes; `tools/dev/regressao.py` guarda o estado em `tmp/regressao` (o antigo foi copiado; nada apagado).
- `ReadRecorder` (`transitional/input_bridge.*`) e `--gravar`/`--reproduzir` (`app/hh_window.cpp`) compilados; unitário
  `input_native_read_recorder` aprovado. Sonda dos back-edges da HH15.11b compilada e coberta pela suíte.
- **LOAD de um save do disco 2 (P0):** slot nativo `0x8C09EC68` (getter do disco) no menu de LOAD (retorno
  `0x8C0EEC34`): com os discos juntos e o save do outro disco, o jogo passa para o disco do save. 7 chamadas do getter
  pelo slot, que cobra os 4 ciclos da gerada; hashes da suíte inalterados. Unitários `hh16_load_save_other_disc*` (sem o
  slot, 28 verificações falham). No produto sem janela: um save "Disc 2, Aquadome" carregado pelo título abre
  `SXX_AQ_MAIN.AHF` e o SCENE46_48.
- **Fim do disco 1 sem troca (P0):** o comando 596 abre o menu "End of Disc" e, depois do save, o jogo mostrava INSERT
  DISC 2 para sempre (no console: reiniciar pelo disco 2 e fazer LOAD; tampa aberta simulada leva ao BIOS). Slot
  nativo do save `0x8C09C34C` (retorno `0x8C086C18`): desliga o modo fim de disco e aplica a imagem recém-salva
  (`0x8C2F1748`, idêntica byte a byte à que o LOAD lê) pela função do próprio LOAD `0x8C09C6C2`. Cenário local (mod
  privado em `tmp/p1` que põe a área `S65_MOVIE` no lugar da primeira): SCENE43/44/45, save, `SXX_AQ_MAIN` e SCENE46_48
  sem tela de troca; a arena do disco 2 jogável. Unitários `hh16_end_of_disc_*` com 4 negativos.
- `generated/disc1` e `disc2` regenerados (`hh-recomp config/headhunter.toml ...`); o disco 2 estava atrasado desde antes
  da HH15.1 e recebeu os slots e a espera `0x8C118346` que já estavam na configuração.
- Revisão Codex (`reviews/codex-hh16_p1.log`), 5 achados, todos aceitos e corrigidos: (alta) a chamada sintética do
  `fSave` sem o poll de retorno do `HH_CALL` (poll acrescentado); (média) os testes `hh16_*` sem o contrato de chamada
  (o positivo agora confere `pr`, `ret_expect`, `ret_sp`, a restauração de `cur_fn` e o poll; os negativos continuam
  provando que nada muda fora do filtro); (média) `--gravar` com corrida entre conferir e abrir (criação exclusiva
  `"wx"`); (média) `--tempo` explícito diferente na gravação e na reprodução (recusado); (baixa) falhas de E/S da
  gravação silenciosas (marcadas e informadas no fim, rc 1).
- Segunda revisão Codex (`reviews/codex-hh16_p2.log`), 5 achados, todos aceitos: (média) a RAM nos back-edges por
  palavra dava divergência falsa com escritas sobrepostas de tamanhos diferentes (passou a ser por byte); (média) o
  estado era tirado também nas sondagens de presença da fonte de teste e não na gravação (agora um observador do
  `latch` do `InputSystem`, `set_latch_observer`, uma vez por amostra nos dois caminhos; unitário
  `input_native_latch_observer`), e uma sessão sem estado era aprovada (agora `REPROVADA`); (média) uma reprodução
  reprovada alimentava a sessão seguinte (agora só uma reprodução aprovada, pelo `resultado.json`); (média) os créditos
  aprovados só pelo fim do SCENE84, sem o "GameEnd" — resolvido: o comando 229 (`0x8C025F66`) tira o nome da pilha do
  script e chama `0x8C095444`, e "GameEnd" carrega `AHFDATA/GAMEEND.AHF`; medido no produto sem janela com a área
  `S92_MOVIE` no lugar da primeira (mod privado em `tmp/p2`): fim natural do SCENE84 no V12002, `GAMEEND.AHF` no V12003,
  `STREAMS/CREDITSGM.ADP` no V12006. O monitor exige os dois sinais (stderr e `HH_FILE_LOG` da reprodução); (baixa) o
  poll sintético sem o diagnóstico do `HH_WRITE_WATCH` (acrescentado). Monitor 12/12.
- Achados abertos da revisão `reviews/codex-hh15_11b5.log` (31/10), aceitos e corrigidos: a RAM escrita pela elevada
  passa a ser conferida em cada back-edge, byte a byte (orçamento de 2^18 bytes × back-edges por chamada; acima dele a
  chamada é contada como "acima do orçamento"), com a mutação `RamNoBackEdge` no unitário. Conferência no roteiro da
  área 1: 3 387 116 chamadas conferidas, 264 312 com a RAM nos back-edges (103 acima do orçamento), 0 divergências, os
  94 quadros iguais à referência; a sonda entra na lista antes de o gancho
  ser instalado (exceção no `push_back` não deixa o gancho). A etapa `elevacao` da regressão exige "RAM nos
  back-edges" > 0.
- **Estado por leitura (HH16.0):** `HeadHunter.exe --estado <arquivo> [--estado-cada K]`, hash FNV-1a da região
  `0x8C147B20`–`0x8C32EF60` a cada K leituras do controle, na thread do jogo (`sh4_session_context`). As pilhas das
  threads ficam fora dela (despejo de hoje: `0x8CFFxxxx` e `0x8C00Fxxx`).
- **Portão da reprodução (`tests/diff/hh16_reproducao.py`, `HH_DATA_UNICO`): 10/10.** Duas reproduções sem janela do
  `newgame_area1_nativo` com 106 quadros e 90 estados idênticos; igual ao mesmo roteiro por `HH_INPUT_SCRIPT` no tempo
  lógico; uma entrada trocada na leitura 4500 dá estados iguais antes e o primeiro diferente na leitura 4560; `--tempo
  nativo` e `--gravar --headless` recusados; gravação existente intocada.
- **Monitor de campanha:** cadeia de saves entre sessões, créditos (fim natural do SCENE84), estado por leitura, sem
  remoção recursiva; `tests/campanha/test_campanha.py` com programa falso, 9/9 (ctest `hh16_campanha`).
- **Cópia de segurança automática (pedida pelo portão da HH16.P):** ao fim de cada `jogar`, cópia incremental para
  `campanhas/_copias/<id>` ou para `nova --copia <pasta>` (`HH_CAMPANHA_COPIA` pelo atalho), com `os.replace` por
  arquivo, nunca apagando no destino e conferindo os selos; comando `copiar [--para]`. A suíte achou um defeito antes
  da entrega: pastas vazias (a `saves_inicio` da sessão 1) não eram copiadas e a cópia não reproduzia. Três testes
  novos (a cópia sozinha verificada até os créditos, a cópia alterada consertada pelo `copiar`, a cópia dentro da
  campanha recusada): **15/15**. `docs/CAMPANHA.md` atualizado (fim do disco 1 sem troca, cópia de segurança).
- **Gravação pronta para a sessão piloto (2026-11-15, só CPU).** Monitor: trava de instância única com identidade de
  processo (PID + momento de criação), recuperação quando o monitor cai (sessão fechada normalmente vale inteira),
  recusa do `HH_INPUT_SCRIPT` na campanha real, variáveis `HH_` fora do jogo e das reproduções, linha pela metade
  tolerada nos três arquivos, `verificar --paralelo`, cobertura (áreas e filmes) por sessão, `jogar --criar` que nunca
  continua uma campanha de teste, janela simples (`campanha.py janela`), saves em subpastas (`saves/card0/...`), prova do
  contrato por comportamento ao criar a campanha.
- **Dois defeitos achados pelo teste novo com o jogo de verdade** (`tests/campanha/test_campanha_real.py`, ctest
  `hh16_campanha_real`), escondidos pelo programa falso: (1) o jogo grava os saves em `saves/card0/`, e o monitor tratava
  a pasta como plana (os selos quebravam já na primeira sessão); (2) a reprodução parava no último hash de quadro, e a
  sessão termina num VBlank qualquer: os estados finais faltavam e **toda sessão real sairia REPROVADA**. Correção no
  produto: o `--reproduzir` para na leitura seguinte ao comentário `# fim da sessao, ultima leitura N` que o `--gravar`
  já escrevia, e o executável informa a última leitura no fim (`app/hh_window.cpp`); o `hh16_reproducao` ganhou os casos
  (parada, estados iguais até ali, sem o comentário vale o `--vblanks`). O `dist/HeadHunter-Preview.hh16-bfe3231` não
  tem essa parada e é recusado pela prova do contrato: a campanha exige o `dist/` refeito.
- **Terceiro defeito, achado na revisão dos contratos:** os quadros da janela saem do Vulkan, que não é idêntico bit a
  bit ao software da reprodução (o HH13 compara por PSNR); toda sessão gravada na janela sairia REPROVADA pelos quadros.
  Agora os quadros da janela só informam, a primeira reprodução aprovada (estado e saves iguais) fixa a referência em
  software selada da sessão, e as seguintes têm de bater com ela. Gravar no software foi medido e descartado (53
  VBlanks/s sem janela, abaixo de 60).
- Teste com o jogo real: **11/11** (duas sessões REPRODUZIDA com 79 estados, quadros e saves iguais; cobertura com 8
  áreas e 3 filmes; `reproduzir-em`; entrada trocada acusada na leitura 4560; a cópia reproduz), repetido depois de
  todas as correções da revisão (inclusive a prova do contrato contra o executável real). `hh16_reproducao` **14/14**.
  Programa falso: **35/35**. Suíte sem GPU (`ctest -C Release -j2 -LE gpu`, com os dados privados): **93/93**
  (3 089 s, `tmp/cpu-only/ctest-cpu6.log`). Rótulos do ctest `gpu` e `campanha` (`ctest -LE gpu` é a rodada só na CPU).
- Revisão Codex (`reviews/codex-hh16_p3.log`), 8 achados, todos aceitos e corrigidos com teste: (alta) a janela entre
  abrir o jogo e gravar o PID (a recuperação procura processos com o executável fixado da campanha); (alta) sessão com o
  monitor caído ficava interrompida para sempre mesmo fechada normalmente (o comentário de fim decide); (alta) a prova do
  contrato só por textos no executável (agora reproduz uma gravação mínima e exige a parada); (média) as reproduções
  herdavam as variáveis `HH_` do terminal; (média) PID reaproveitado na trava e na recuperação; (média) linha pela metade
  em `quadros.txt`/`estado.txt`; (média) `SEM_QUADROS` não reprovava (todo veredito fora dos aprovados reprova); (média)
  `reproduzir-em` sem trava.
- Revisão dos planos (2026-11-15): o "passo de jogo" dos planos da HH16/HH19/etapas restantes e do `ROADMAP.md` virou a
  leitura do controle (o HH não tem passo de jogo, HH10.6); situação e placar da HH16 (`HH16-PLANO.md` seção 9); a
  ordem da HH19 alinhada à revisão de 2026-11-14; HH20 na comparação com o BS; matriz dos testes futuros em
  `docs/planos/TESTES-FUTUROS.md`.
- `config/fronteiras.toml`: os 27 rótulos `legado` → `nativo` dos slots (só os rótulos; a geração completa esvaziaria
  listas de subsistemas), `hh07_fechamento` 0 falhas.
- **Gravador com tela (2026-11-15):** `tools/gravador` (alvo `HeadHunter-Gravador`, Win32 nativo, copiado para
  `dist/HeadHunter-Gravador.exe`). "Iniciar gravação" roda `campanha.py jogar --criar`, que abre o jogo gravando; fechar o
  jogo termina a sessão e a tela mostra o resultado e o status. "Parar gravação" manda WM_CLOSE às janelas dos processos
  filhos do monitor (o mesmo caminho do X); a caixa "Campanha de teste" usa `jogar --criar --teste` (novo: a última
  campanha de teste, ou cria uma; nunca a real); a tela não fecha com a gravação em andamento e não usa job object
  (fechá-la nunca derruba o jogo). Teste `tests/campanha/test_gravador.py` (ctest `hh16_gravador`, rótulo `campanha`) com
  o programa falso: **4/4** (cria, grava e continua; teste e real separadas; Parar fecha a janela, gancho `FAKE_JANELA`
  com tkinter; sem Python avisa e não cria nada). Com o jogo real (campanha de teste em `tmp/gravador-real`): o Parar
  fechou o jogo depois de 25 s, a gravação terminou com o comentário de fim (leitura 1319), sessão `gravada`, cópia com
  14 selos conferidos e `verificar` → **TESTE REPRODUZIDO**. `hh16_campanha` 35/35 depois da mudança no `jogar`.
- **GPU liberada (2026-11-16).** G1: `ctest -L gpu` **4/4** (`hh13_render` 902 s, `hh15_tempo` 89 s;
  `tmp/gpu/ctest-gpu1.log`). G3 (lição L5): `elevacao` medido sozinho em 175 s (limite 3 600 s), mesmas contagens da
  conferência (3 387 116 chamadas, 264 312 com a RAM nos back-edges, 0 divergências). Achado da G3: o passo `ctest` da
  regressão só limpava `HH_DATA_DIR`/`HH_SNAPSHOT_DIR` e rodaria os portões da HH16 (~22 min) dentro do limite de 1 h da
  suíte; agora limpa também `HH_DATA_DIR2`/`HH_DATA_UNICO`, e a nova etapa `portoes_hh16` (`-L campanha`, 3 600 s por
  teste, 4 h na etapa, sem SKIP) põe a reprodução e a campanha na regressão integral.
- G2, primeira rodada: `portoes_hh15` reprovou a RAM do tempo experimental contra `analysis/hh15/ref`. **Erro de
  configuração meu, não regressão:** essa pasta (28/10) guarda o hash da RAM inteira, anterior à máscara das pilhas; a
  regressão usa `analysis/hh15/ref_pilhas` (30/10), como a `regressao_11b4`. Provado por quatro medições: o `hh-run` de
  31/10 (worktree do `be7de69` com o gerado refeito do config daquela data, `tmp/build-be7de69`) dá o mesmo hash de hoje;
  o despejo de 30/10 (`analysis/hh15e/bis/gerado`) tem o hash mascarado de hoje (`d7756c…`) e o hash inteiro da
  referência antiga (`df9aef…`); `HH_ELEVACAO=gerado` e o adaptador sem os slots da HH16.P1 também dão `d7756c…`; o
  gerado de hoje difere do de 31/10 só em `HHG_FN` → `HHG_SLOT` nas chamadas dos dois slots. A docstring do
  `regressao.py` apontava a pasta antiga; corrigida, e as variáveis da regressão listadas em `TESTES-FUTUROS.md`.
- G2, segunda rodada: `portoes_hh09` reprovou com 10 falhas; o registro do Windows tem **uma** queda do `hh-run` às
  22:15 (`abort()`: `0xc0000409` no `ucrtbase`, dados 7 = FAST_FAIL_FATAL_APP_EXIT; iniciado às 22:12:19, uma das 6
  execuções paralelas do `hh09_tempo`), a única queda de `hh-run`/`HeadHunter` no registro dos últimos 20 dias. Não
  reproduziu: o teste sozinho, com carga (junto com a HH08) e 6 rodadas seguidas (36 execuções) passaram sem queda.
  **Em aberto, sem causa conhecida.** Diagnóstico acrescentado (os critérios não mudam): o `hh09_tempo` mostra o código e
  o fim do stderr de cada execução quando reprova, e a regressão guarda a saída inteira de cada comando em
  `tmp/regressao/saidas/` (antes só as últimas 12 linhas).
- **G2 aprovada (2026-11-17): regressão integral 16/16** (terceira rodada, `tmp/regressao/g2-hh16c.log`, com a saída
  de cada comando em `tmp/regressao/saidas/`): build 10 s, ctest 144 s, HH05–HH16 (HH13 2 023 s, HH15 406 s com a
  `ref_pilhas`, HH16 1 161 s), roteiro da área 1 143 s, elevação 159 s, Builder 542 s. A queda da segunda rodada não
  voltou (o `hh09_tempo` passou em 191 s).
- **RecompLauncher (2026-11-16/17).** Padrões conferidos em `docs/DEVELOPERS.md` e `SETUP-INPUTS.md` do
  `elripalda/RecompLauncher` e no esquema `recomplauncher.com/schema/recomplauncher.json` (a página `recompstore.com/how`
  é o guia do jogador). Feito: `tools/dist/recomplauncher.json` (os dois CDIs como inputs `disc`; `prepare` = `hh-builder`
  com `--documentos --progresso-launcher`, 2 h de limite, `done` = o executável e o manifesto dos dados, `version` = a
  versão do produto; `start` no `HeadHunter/HeadHunter.exe`; `saves`; `exit_codes` 0 e 3 = saída normal), validado sem
  erros pelo esquema oficial (e o validador reprova três perfis errados de propósito). `hh-builder --progresso-launcher`
  imprime `progress <0..1> <passo>`. `make_builder.ps1` põe o perfil no topo com a versão e, com `-Zip`, gera
  `HeadHunter-Builder-v<versão>-Windows-x64.zip`. Teste `hh18_recomplauncher` **8/8** (com o esquema). De ponta a ponta,
  como a loja faria: o zip descompactado, a etapa `prepare` com os argumentos do perfil, os dois CDIs reais (nomes com
  espaço) e sem console terminou com código 0 em 430 s, 79 linhas de progresso, os dois arquivos de `done` presentes; o
  `HeadHunter.exe` instalado abriu da raiz da instalação (`--headless`, 600 VBlanks, código 0) com os saves em Documentos.
  Não testado: o próprio launcher (Developer tools, "Add a build"). Pendente do mantenedor: inscrição no catálogo (com o
  requisito de Build Tools), release completa (pré-lançamento vira beta). O que depende do jogo está na seção 5.7 do
  `HH19-PLANO.md`.
- Pendências: a gravação de verdade na janela (sessão piloto da HH16.P); a passagem real do disco pela campanha, sem o
  mod de teste.

### HH15 (2026-10-28, em andamento; parte 2 desde 2026-10-30)

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
- **D-HH15-1 decidida pelo mantenedor (2026-10-30): opção B** ("começa agora a remoção e deixar totalmente nativo"):
  o substrato AOT sai do produto na HH15; a elevação para `native/game` foi antecipada da HH17 (ROADMAP atualizado;
  plano, seção 7). Medição de partida: 4 699 funções por disco (3 521 do jogo, 1 178 do SDK); a rota da área 1 executa
  1 428 do jogo e 433 do SDK (`analysis/hh15e/perfil_area1.tsv`). Texto da decisão apresentada:
- (histórico) DECISAO_NECESSARIA — HH15 (D-HH15-1): o substrato AOT (código gerado, RAM guest, `Sh4Context`, despacho e
  fibras) fica no produto até a HH17, como no BS (D-N8-1), ou sai agora, como o texto do ROADMAP pede? Recomendação:
  manter até a HH17 e corrigir ROADMAP/ARCHITECTURE. Detalhes, opções e custos: relatório, seção 9.
- HH15.9 (concluída, 2026-10-30): inventário da elevação (`tools/dev/hh15_elevacao_inventario.py`).
- HH15.10 (concluída, 2026-10-30): infraestrutura da elevação: `native/game`, `transitional/elevacao` (elevadas pelo
  slot da gerada, custo do caminho original, premissas do literal pool, `HH_ELEVACAO=gerado|ativo|conferir` só na
  verificação) e as duas primeiras elevadas (`8C06F62C`, `8C073980`). Conferência na rota: 333 227 chamadas conferidas,
  0 divergências, hashes iguais; produto: 94 hashes iguais. Revisão Codex (`reviews/codex-hh15_10.log`,
  `codex-hh15_10b.log`): 7 achados aceitos e corrigidos, parte de 1 rejeitada (`t`, `fpul`, pilha morta). Regressão
  integral 15/15 com o passo novo `elevacao` (`analysis/hh15e/regressao_10.log`), ctest 93/93.
- HH15.11a (concluído, 2026-10-30): biblioteca do compilador e do runtime C elevada (`strlen`, `strcmp`, `memset`,
  cópias do compilador, divisões com fórmulas fechadas deduzidas por modelo de análise fora do produto); 12 elevadas,
  cerca de 3 milhões de chamadas conferidas por rota, 0 divergências. Referências de RAM da HH15 sem a pilha morta das
  threads (`analysis/hh15/ref_mascara`, recapturadas com as geradas: a RAM inteira idêntica à pré-corte). Revisão Codex:
  1 achado aceito e corrigido. Regressão integral 15/15 (`analysis/hh15e/regressao_11a2.log`), ctest 93/93.
- HH15.11b parte 1 (2026-10-30): cadeia trigonométrica do SDK elevada (redução, núcleos, seno e cosseno internos,
  `sinf`/`cosf` com errno). Critério endurecido: contrato completo de registradores para todas (o escalonador do jogo
  salva os registradores "mortos" na RAM) e a pilha inteira de cada thread fora do hash de RAM (referências
  `analysis/hh15/ref_pilhas`, recapturadas com as geradas: a RAM inteira idêntica à pré-corte). 19 elevadas, 0
  divergências. Revisão Codex: 2 achados aceitos, 1 rejeitado com evidência. Regressão integral 15/15
  (`analysis/hh15e/regressao_11b2.log`), ctest 93/93.
- HH15.11b parte 2 (2026-10-30): `ldexpf`, arco-tangente e arco-seno internos, núcleo do arco-tangente e os
  envoltórios com errno num modelo; 27 elevadas, 0 divergências com o contrato completo. Regressão integral 15/15
  (`analysis/hh15e/regressao_11b3.log`).
- Limitação registrada: no tempo lógico a tela de carga antes da cena de abertura (anúncio do jogo) não aparece, porque
  a carga não consome tempo; o estável a mostrava só pelo custo estimado de ciclos da descompressão.
- HH15.11c (2026-11-18, só CPU: a GPU estava ocupada por um processo do BS): `strcpy` (8C1247D4) e os auxiliares do
  compilador 8C124018 e 8C123F78, `strcat` (8C124784), o envoltório do `strcmp` (8C1247C0) e a cópia por palavras
  8C123EB8 elevados (`docs/planos/HH15-PLANO.md`, HH15.11c). `generated/` regenerado nos dois discos (só `HHG_FN` →
  `HHG_SLOT` nas 522 chamadas). Build Release sem aviso. Conferência na rota da área 1: 39 elevadas, 3 432 616
  chamadas conferidas, 291 395 com a RAM nos back-edges, 0 divergências, 94 quadros iguais; produto 94 hashes iguais;
  dois negativos por mutação reprovam pelo motivo certo. Suíte sem GPU (`ctest -C Release -j2 -LE gpu`, dados privados):
  **95/95** (2 887 s, `tmp/hh15_11c/ctest-cpu.log`). Revisão Codex (`reviews/codex-hh15_11c.log`): sem achados.
  GPU livre em 2026-11-24: `ctest -L gpu` **4/4**; a regressão integral reprovou a RAM do tempo experimental na
  `portoes_hh15` (lixo de pilha empilhado pela gerada no auxiliar do `strcpy` e copiado pelo jogo para fora da pilha;
  `docs/planos/HH15-PLANO.md`, HH15.11c). Corrigido (as elevadas gravam os empilhamentos); referências da HH15 e
  conferência na rota iguais. Regressão integral com a correção (2026-11-25, `tmp/v3-11c/regressao2.log`) **aprovada,
  rc=0**: build e ctest, portões HH05 (8 s), HH07 (286 s), HH08 (341 s), HH09 (281 s), HH10 (709 s), HH11 (533 s),
  HH12 (188 s, rodada anterior com a mesma chave), HH13 (1 938 s), HH14 (267 s), HH15 (233 s), HH16 (1 124 s), roteiro
  da área 1 (135 s), elevação (164 s) e Builder (154 s).
- HH15.11d (2026-11-25): `memcmp` (8C124508) e `strncpy` (8C124870) elevados (`docs/planos/HH15-PLANO.md`,
  HH15.11d). `generated/` regenerado nos dois discos (só as 39 chamadas). Conferência na rota da área 1: 41 elevadas,
  3 438 240 chamadas conferidas, 310 088 com a RAM nos back-edges, 0 divergências, 94 quadros iguais; produto 94 hashes
  iguais; referências da HH15 18/18; dois negativos por mutação reprovam pelo motivo certo. O `memmove` ficou de fora: a
  rota o chama com destino 0 (área 0, caminho de hardware da gerada); proposta no plano. A consulta da tabela de tipos
  de caractere (8C1244A8) não é chamada na rota; o `strtol` fica para lote próprio. `ctest -LE gpu` **95/95** (2 836 s),
  `ctest -L gpu` **4/4**. Revisão Codex (`reviews/codex-hh15_11d.log`): 1 achado aceito e corrigido (o `strncpy` relê o
  byte depois do back-edge, como a gerada); revisão da correção sem achados. Falta a regressão integral (V3).
- HH15.11e (2026-11-26): delegação para a gerada nos casos fora do contrato e o `memmove` (8C12453C), que delega quando
  um ponteiro está fora da RAM principal (`docs/planos/HH15-PLANO.md`, HH15.11e). Conferência na rota: 42 elevadas,
  3 442 083 conferidas, 9 delegadas, 0 divergências, 94 quadros; produto 94 hashes; referências da HH15 iguais;
  unitário novo e dois negativos. Regressão integral da HH15.11d e da HH15.11e (2026-11-26, `tmp/v3-11e/regressao.log`)
  **aprovada, rc=0**: build (9 s), ctest (145 s), portões HH05 (7 s), HH07 (284 s), HH08 (321 s), HH09 (200 s), HH10
  (503 s), HH11 (421 s), HH12 (93 s), HH13 (2 020 s), HH14 (256 s), HH15 (230 s, as três referências iguais), HH16
  (1 104 s), roteiro da área 1 (134 s), elevação (162 s) e Builder (155 s). Revisão Codex (`reviews/codex-hh15_11e.log`):
  2 achados aceitos e corrigidos com teste (o `memmove` relê r11–r13 da pilha, como a gerada; `na_ram` não aceita mais
  um intervalo que dá a volta nos 32 bits); depois, conferência na rota, 94 quadros e referências da HH15 iguais.
- HH15.11f (2026-11-26): `strtol` (8C1248E0), `atoi` (8C12435C) e o limite de 512 caracteres (8C126A74) elevados
  (`docs/planos/HH15-PLANO.md`, HH15.11f). `generated/` regenerado nos dois discos (só as 23 chamadas e o salto de
  cauda). A elevada da divisão sem sinal passa a deixar M/Q/T como o laço da gerada (o contrato completo do `strtol`
  acusou o Q). Conferência na rota: 45 elevadas, 3 445 756 conferidas, 0 divergências (`limite_texto` 2 424, `strtol` e
  `atoi` 779 cada), 94 quadros; produto 94 hashes; referências da HH15 iguais; dois negativos por mutação. Revisão Codex:
  1 achado aceito e corrigido com teste (T = 1 no divisor zero); revisão da correção: só o empilhamento de r2 da divisão,
  anterior ao lote, registrado. Regressão integral (tmp/v3-11f/regressao.log, 2026-11-26) **rc=0**: build (9 s), ctest
  (142 s), portões HH05 (7 s), HH07 (280 s), HH08 (326 s), HH09 (199 s), HH10 (502 s), HH11 (430 s), HH12 (90 s), HH13
  (1 939 s), HH14 (256 s), HH15 (228 s), HH16 (1 109 s), roteiro da área 1 (134 s), elevação (165 s) e Builder (150 s).
- HH15.11g (2026-11-26): `log10f` (8C1244B8) e o interno (8C1264B4), e os auxiliares da saída formatada (8C125FEA,
  8C125FFA, 8C125F92, este delegando os descritores que não são de string) (`docs/planos/HH15-PLANO.md`, HH15.11g); o
  modelo dos envoltórios com `errno` passou a gravar o PR e o resultado na pilha. Conferência na rota: 50 elevadas,
  3 460 040 conferidas, 0 divergências, 94 quadros; produto 94 hashes; referências da HH15 iguais; dois negativos.
  Revisão Codex sem achados. Regressão integral em curso.

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

**Revisão do planejamento (2026-11-14, pedido do mantenedor):** `docs/planos/PLANO-ETAPAS-RESTANTES.md`, comparado com o
BS em `fbc6761` (2026-11-12). Decisões levantadas, nenhuma bloqueando a HH15.11, a HH16.0 e a HH16.1:
- **D-P1 decidida (mantenedor, 2026-11-14): A.** "Vou gravar a campanha inteira para testar o jogo completo. Para não
  precisar zerar mais de uma vez." A gravação vem antes da HH15.F e dá a cobertura das funções fora da área 1.
- **D-P2 decidida (2026-11-14): sim.** Campanha como gravação humana única, reproduzida em cada programa seguinte.
  Plano: `docs/planos/HH16-PLANO.md`.
- **D-P3:** na Remastered, saves do jogo sem sistema novo (o BS cancelou o N9.3). Recomendado: sim.
- **D-P4:** HH20 (Linux) antes da HH19.1. Recomendado: incluir.
- **D-P5:** codec dos filmes convertidos no Linux (hoje H.264/AAC pelo Media Foundation). Recomendado: decidir na HH20.L4
  com medição.

Até esta revisão, nenhuma decisão humana estava pendente. Descobertas que contradigam estas premissas podem exigir `BLOQUEADA_DECISAO`, mas somente depois de esgotar investigação e testes independentes:

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
