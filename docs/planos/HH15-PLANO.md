# HH15 — Fim do tempo guest e remoção do legado: plano

Data: 2026-10-27. Validação mínima do ROADMAP: **V3** (regressão integral antes e depois da remoção).
Referência MIT: N2c + N8 do Blue Stinger (`C:\Source\recomp-BStinger\docs\N8-PLANO.md`, `N8-1-PREPARACAO.md`,
`N8-2-SEPARACAO.md` … `N8-6-AUDIO.md`), só como projeto de referência.

Critério de saída (ROADMAP): V3 passa antes e depois da remoção; o build do produto falha se qualquer biblioteca
legada for religada.

## 1. Limite da etapa

O produto (`HeadHunter.exe`) deve usar só os provedores nativos de arquivos, tempo/eventos, controles, saves, vídeo,
áudio e render, sem depender em execução de GD-ROM/BIOS/flash por HLE, Maple/VMU, AICA, PVR/Holly/TA/VRAM, INTC/TMU/
DMAC/CCN/portas/store queues, ciclos estimados nem poll por ciclos. Esses módulos continuam num alvo de validação
separado e nas ferramentas (oráculos, testes históricos), nunca na distribuição.

**Contradição documental a resolver (D-HH15-1).** O ROADMAP põe na HH15 "remover contexto SH-4, RAM guest e
dispatch", e o `ARCHITECTURE.md` ("Critério operacional final") proíbe `Sh4Context`, despacho e RAM guest no produto
final. Mas todo o jogo ainda é C++ gerado do SH-4, que lê e escreve o `Sh4Context` e a RAM guest e chama por despacho;
a HH17 é a etapa que eleva o gameplay para `native/game`, e a HH16 (campanha) roda sobre o código gerado. Tirar o
substrato AOT na HH15 exigiria elevar o jogo inteiro antes da HH16/HH17. O BS teve a mesma contradição e o mantenedor
decidiu (D-N8-1, 2026-10-24): o N8 remove a simulação de hardware e os provedores legados; o substrato AOT (código
gerado, RAM guest, `Sh4Context`, `sh4_ops`, despacho e fibras) fica até a elevação (N10, equivalente à HH17). Esta
etapa executa primeiro tudo o que não depende da decisão (seção 6) e só então a apresenta.

## 2. Precedentes do BS adotados (decisões do mantenedor no projeto de referência)

- **Cadência B** (N8.1): o poll deixa de atender a cada quantum de ciclos; no tempo lógico o atendimento só roda
  quando pedido (espera que andou o instante, nível de interrupção entregável, evento no instante atual) e a
  vivacidade vai por um tick a cada N polls. O SPC de cada entrega não é relaxado: toda diferença precisa de exceção
  explícita, por diferença, provada pelo disassembly.
- **TMU, CCN não gráfico e porta do cabo** no tempo lógico por interceptação estreita do contrato medido; fora do
  contrato, erro explícito no produto (recuo para o modelo só na validação).
- **Ponte de interrupções:** reproduzir o contrato observável do tratador do SDK (INTEVT, SSR/SPC/SGR, `rte`), sem
  reescrever a lógica do SDK.
- **Macros do gerador** (`HH_CYCLES`, `HH_POLL`) continuam definidas: `HH_CYCLES` vira nada no produto; o gerador só
  muda depois da equivalência provada (o Builder embute o gerador e o projeto editável traz o próprio C++ gerado).
- **Alvo de validação separado** (D-N8-3): os modelos de hardware, o tempo estável por ciclos, os provedores
  `legado` e as baterias históricas ficam num build próprio (`HH_VALIDACAO`), nunca na distribuição.
- **Serviços nativos** no lugar do HLE (N8.3, N8.4, N8.6): o produto atende o contrato medido do jogo (syscalls,
  barramento de periféricos, área de som) e fora dele dá erro explícito.
- **Campanha completa** é critério da HH16, não pré-requisito desta etapa (D-N8-4).

## 3. Inventário

### 3.1 Sonda dinâmica no caminho nativo (HH15.0)

`HH_HW_PROBE` na rota `newgame_area1_nativo` (4 700 VBlanks; `--arquivos/--controles/--video/--audio/--render nativo`,
tempo estável; `analysis/hh15/sonda_*.tsv`). Acessos por região e quem os faz:

| Região | Acessos | Funções do guest | Dono |
|---|---|---|---|
| store queues (escrita na área SQ e `pref` para a RAM) | 19 521 443 + 2 288 440 | gráficos do jogo e do Kamui | HH15.7 |
| CCN | 510 061 | QACR0/1 (`0x8C090974`, `0x8C159E20`, `0x8C16BAA0`…: SQ, 15.7); INTEVT pelo tratador `0x8C00FA00`; CCR/MMUCR no boot | 15.1 (INTEVT, CCR) e 15.7 (QACR) |
| TMU | 241 098 | leitor `0x8C15E856`; esperas do boot `0x8C1696C0`/`0x8C16A5A0`; timeouts do Kamui (`0x8C16B516`, `0x8C16B60E`, `0x8C16C120`, `0x8C16C340`, `0x8C179520`, `0x8C17C600`); configuração `0x8C15E830` | 15.1 |
| Holly INTC | 94 360 | tratador `0x8C168160` (ISTNRM/ISTERR), servidor de periféricos `0x8C16E908`/`0x8C16E9EE`, máscaras no boot | 15.1 (ponte) e 15.7 |
| DMAC (canal 2) | 76 840 | Kamui `0x8C17C600` e tratador `0x8C168160` | 15.7 |
| G2 | 74 409 | trava/FIFO em volta do RTC da AICA (`0x8C183C6C`, `0x8C183C8C`, `0x8C183CD4`, 2 por VBlank); boot `0x8C15FAC8` | 15.6 |
| Holly SB | 38 132 | DMA do TA pelo Kamui `0x8C17C600`; boot | 15.7 |
| Maple | 18 591 | servidor de periféricos `0x8C17F0AA` (DMA por VBlank, mesmo com controles e saves nativos) | 15.4 |
| RTC da AICA | 18 592 | `0x8C1706E8` (2 leituras por VBlank) | 15.6 |
| SPG (PVR) | 13 935 | `0x8C16E908`, `0x8C16EF18` (SPG_STATUS, SPG_VBLANK) | 15.7 |
| INTC do SH-4, porta (PDTRA), arrays de cache, G1, PVR-DMA | 1–512 | boot (`0x8C15FAC8`, `0x8C15DF7A`, `0x8C15F460`) | 15.1/15.3 |

A AICA (RAM de som e registradores) já não aparece: só o RTC. Com `--saves nativo` o DMA do Maple continua (o servidor
`bu` nativo roda na passada da camada `per`, chamada pelo servidor de periféricos).

### 3.2 Mapa do linker do `HeadHunter.exe`

Objetos de `hh_runtime` ainda ligados: `aica_regs`, `bios`, `flashrom`, `gdrom`, `data_source`, `holly_intc`, `holly_sb`,
`maple`, `input`, `memory`, `native_boot`, `pvr_regs`, `pvr_render`, `pvr_texture`, `pvr_video`, `ta`, `vram`,
`sh4_ccn`, `sh4_dmac`, `sh4_intc`, `sh4_port`, `sh4_sq`, `sh4_tmu`, `sh4_service`, `sh4_threads`, `time_legacy` e os
diagnósticos (`call_trace`, `clock_log`, `event_trace`, `file_log`, `hw_probe`). Substrato AOT: `hh_generated`,
`hh_runtime_cpu` (`sh4_context`, `sh4_runtime`, `mod_api`). Adaptadores: os 13 objetos de `hh_transitional`.

### 3.3 Consumidores de tempo

No gerado: 67 093 `HH_CYCLES`, 2 571 `HH_POLL`, 22 704 `HH_CALL` (cada um com `HH_POLL_RET`), 265 `HH_IRQ_CHECK`, 8
`HH_WAIT` e 1 107 despachos indiretos.

## 4. Subetapas

| ID | Entrega | Portão |
|---|---|---|
| HH15.0 | Referências pré-corte (V3 do HH14, rotas, hashes, traces de ordem e entrega) e inventário (seção 3) | regressão integral ok; referências reproduzidas duas vezes |
| HH15.1 | Fim do tempo guest (N2c): cadência sem ciclos no tempo lógico; TMU, CCN não gráfico, porta e INTC pela ponte; experimental fechado (B1–B11 do HH09) e padrão da janela | referências iguais fora da lista fechada de exceções provadas; ruído de ciclos sem efeito; 0 acessos fora do contrato |
| HH15.2 | Alvos por papel: produto, substrato AOT, validação (`HH_VALIDACAO`), catraca de mapa do linker | build do produto falha ao religar biblioteca legada (negativo); manifesto × mapa |
| HH15.3 | Boot, arquivos, GD-ROM, BIOS e flash: serviço nativo das syscalls; HLE só na validação | zero setores/syscalls fora do contrato; bytes lidos iguais |
| HH15.4 | Controles, saves e Maple: barramento sem dispositivos; `--controles/--saves legado` só na validação | roteiros e saves iguais; zero Maple |
| HH15.5 | Vídeo: `--video legado` e conversões dependentes do PVR fora do produto | filmes iguais; nenhum caminho legado |
| HH15.6 | Áudio: área de som nativa (RAM de som e RTC do contrato); modelo da AICA e `--audio legado` só na validação | A1–A5 do HH14; zero AICA |
| HH15.7 | Render: IR sem SQ/DMAC/TA/VRAM/PVR no produto; renderizador software sobre a IR nativa; Holly/SB/SPG pela ponte | R1–R5 do HH13 e K1–K4; zero PVR |
| HH15.8 | Fechamento: mapa/imports/strings/sonda, regressão V3 pós-corte, Builder/Preview, Codex, decisão D-HH15-1 | critério de saída |

## 5. Regras

- Recompilar, nunca emular: nenhuma instrução interpretada; a validação reproduz o que o produto fazia antes, não
  simula hardware novo.
- Cada corte remove do caminho de execução do produto primeiro (com prova contra as referências) e do link depois.
- Não enfraquecer testes: as baterias que medem o modelo por ciclos vão para a validação com as referências delas.
- Saídas em `analysis/hh15/` e `reviews/` (ignorados).

## 6. Andamento

### HH15.0 (concluída, 2026-10-28)

- Regressão integral do fechamento do HH14 (`analysis/hh14/regressao.log`, 13/13) vale como V3 pré-corte.
- `tools/dev/hh15_referencias.py` (`capturar`/`conferir`): na rota `newgame_area1_nativo`, por caso, hashes de quadro a
  cada 50 VBlanks, trace de ordem sem ciclos (V, I com INTEVT e SPC, R, T, N, J), relógios por VBlank, SHA-256 da RAM
  em 5 VBlanks, SHA-256 do áudio e resumo. Casos `estavel` (saves nativos) e `experimental`; referências em
  `analysis/hh15/ref`, reproduzidas duas vezes (6/6 por caso). Inventário na seção 3.

### HH15.1 (concluída, 2026-10-29)

1. **Espera do `bu`** (`0x8C118346`, `[native] esperas`): `f_8C118334` repete enquanto o status `0x8C1603DC` devolve
   −1; a operação conclui na passada do servidor `bu`, chamada pela camada `per` a cada fim de DMA do Maple. Sem a
   declaração, o tempo experimental com `--saves nativo` parava no VBlank 2 047 (falta de progresso). Com ela: 4 700
   VBlanks, 2 SAVE ok, espera liberada por InputDone em 10/10 episódios; no estável o mesmo laço dava 4,1 milhões de
   voltas sem efeito. Caso `experimental_saves` acrescentado às referências.
2. **Origem das entregas** (`IrqOrigin`, linha `[irq]` no resumo, `HH_IRQ_QUANTUM`): no experimental, 29 790 entregas
   por atendimento pedido, 391 por `ldc sr` e 6 pelo quantum de ciclos; as 6 são a interrupção externa da AICA (ISTEXT
   bit 1, IML4EXT), acesa pelo provedor nativo de áudio dentro do comando ao driver (slot `0x8C122440`).
3. **Cadência B** (`sh4_poll_slow`, `sh4_irq_wakeup`, `Sh4Context::next_poll`): no tempo lógico o atendimento só roda
   quando pedido — espera que andou o instante, fim do DMA do canal 2, nível entregável depois de `holly_intc_raise`,
   da fonte externa da AICA ou de uma máscara IML* nova (sem avaliar eventos) — e um tick de vivacidade a cada 4 096
   polls mantém a guarda D8 e a parada pedida pelo host. Tempo estável e legado inalterados. `HH_CADENCIA=quantum`
   (diagnóstico) reproduz a referência sem nenhuma diferença.
   - Resultado: estável idêntico (6/6); experimental e experimental_saves idênticos fora de 7 SPCs cada, todos a mesma
     interrupção da AICA (`0x360`) indo para o retorno do slot (`8C121964`), na mesma posição; quadros, relógios, RAM,
     áudio e resumo iguais. Lista fechada `config/hh15_excecoes_spc.toml`, conferida por `hh15_referencias.py conferir
     --excecoes` (todas as listadas têm de aparecer); comparador com 8 negativos sintéticos (`hh15_excecoes_spc`).
   - Independência dos ciclos (`HH_CICLOS_RUIDO=5000`): experimental 12/12 iguais com as mesmas exceções; estável
     (controle) diverge em todos os artefatos.
4. **Serviços do tempo lógico** (`runtime/memory/sys_logical`): TMU canal 0 (contrato de `0x8C15E830`: TOCR = 0,
   TSTR &= ~1, TCR0 = 2, TCOR0 = TCNT0 = 0xFFFFFFFF, TSTR |= 1; leituras de TCNT0), CCN não gráfico (CCR, array de
   endereços do cache de operandos), PDTRA, ICR/IPR do boot e o INTEVT lido pelo tratador `0x8C00FA00`. Fora do
   contrato o modelo assume com o estado (`*_adopt`) e o acesso é contado. Na rota: 0 fora do contrato. Negativo:
   `HH_TMU_MUTACAO=parado` faz relógios e RAM divergirem na conferência. A entrega das interrupções (contrato do
   tratador: INTEVT, SSR/SPC/SGR, `rte` conferido) continua em `sh4_intc.cpp`; a separação física do modelo de
   registradores fica para a HH15.2.
5. **Promoção do tempo lógico na janela** (`tests/diff/hh15_tempo.py`, modelo N2b2 do BS; as cargas não consomem
   tempo lógico, D11, então se compara conteúdo e ordem, não o instante), estável × lógico na rota nativa:
   - T1 leituras: os 5 205 blocos do estável (arquivo, deslocamento, SHA-256) idênticos e na mesma ordem no lógico,
     que lê 18 a mais no fim (o stream da música adiante);
   - T2 quadros: 1 779 trocas iguais até o pulo do último filme, fora da lista fechada
     `config/hh15_excecoes_quadros.toml` — a entrada do primeiro logo durante o trabalho do boot (estável V111–V117) e
     a tela de carga antes da cena de abertura (estável V3888–V3925: o anúncio do jogo, desenhado enquanto a carga
     cobra ciclos estimados; no lógico a carga é instantânea e a tela não aparece) — e de uma sobra no fim (24 quadros
     a mais do filme no lógico: o START do roteiro é a n-ésima leitura do controle e cai noutro ponto do filme);
   - T3 áudio: os 2 594 primeiros comandos ao driver idênticos, do boot até depois do pulo;
   - T4 vivacidade nos dois modos; T6 desempenho: lógico 18,4 s contra 45,9 s do estável;
   - negativo: bloco corrompido reprova T1; comparadores com 13 casos sintéticos (`hh15_tempo_comparadores`).
   - `--tempo experimental` passa a ser o padrão da janela; o `--headless` continua no estável. Janela: a rota inteira
     com Vulkan, XAudio2 e saves nativos, 0 entregas pelo quantum, 0 acessos fora do contrato.
   - Regressão: etapa `portoes_hh15` (os três testes `hh15_*` e a conferência das referências com `HH15_REF`).
6. **Despertar no `rte` com desvio** (`sh4_rte_redirect`: troca de thread, continuação, thread nova): o SR restaurado pode
   desmascarar um nível pendente e esse caminho não tem `HH_IRQ_CHECK`; o atendimento é pedido para o próximo poll.
   Referências iguais (18/18).
7. **Revisão Codex** (`reviews/codex-hh15_1.log`; a primeira tentativa caiu por falta de capacidade do modelo), 6
   achados, todos conferidos no código, aceitos e corrigidos, com contraexemplo nos testes sintéticos:
   - [MEDIA] T1 aceitava o lógico com leituras a menos: agora o estável tem de ser prefixo do lógico;
   - [MEDIA] T2 só fixava o lado estável dos trechos e aceitava qualquer sobra no fim: os VBlanks do lado lógico entram
     na lista, e a sobra tem de ser do lado cujo último filme durou mais, dentro do intervalo do filme, com no máximo
     metade da diferença de duração + 2 trocas (na rota: 24, limite 26);
   - [MEDIA] T3 aceitava igualdade só até o VBlank do corte: agora exige comandos iguais depois dele;
   - [MEDIA] uma entrada da lista de SPCs cobria diferenças repetidas: conta por multiplicidade;
   - [MEDIA] acessos aos arrays do cache fora da faixa do contrato iam ao modelo sem contar: o serviço cobre
     `0xF0000000–0xF7FFFFFF` (só as escritas no array de endereços do cache de operandos no contrato);
   - [BAIXA] os laços nativos de cópia/zeragem do Kamui ignoravam o tick de vivacidade: usam a condição do `HH_POLL`.

### HH15.2 (em andamento)

Precedentes do N8.2 do BS adotados (decisões do mantenedor no projeto de referência, 2026-10-26): o tempo estável fica
nos dois executáveis (CLI e headless históricos); a instrumentação das referências fica no produto, inerte sem a
variável; só as mutações e sondas exclusivas de teste saem do produto.

1. **Ponte de interrupções separada** (`runtime/cpu/irq_bridge.cpp`): a entrega (contrato do tratador: INTEVT,
   SSR/SPC/SGR, `rte` conferido, reavaliação depois do `rte`) saiu de `sh4_intc.cpp`, que ficou só com o modelo dos
   registradores (ICR/IPR, registrador INTEVT), recuo do tempo estável e de fora do contrato.
2. **Runtime por papel** (`runtime/CMakeLists.txt`): bibliotecas de objetos `aot`, `nativo`, `simulacao`, `estavel`,
   `diagnostico` e `validacao`, montadas no mesmo `hh_runtime`; manifesto `runtime_papeis.tsv` no build.
3. **Validação fora do produto sem segundo build**: o executável de verificação (`hh-run`, `hh-diff`, testes) liga os
   objetos de validação (`debug/hw_probe`, `debug/validacao`: ruído de ciclos, cadência de referência, TMU parado,
   lista das entregas pelo quantum); o produto compila `debug/validacao_stubs.cpp` como fonte própria, e como os
   símbolos já estão definidos nele o linker não puxa os objetos reais da biblioteca. O stub recusa na partida, com
   saída 2, as variáveis de sonda, prova e mutação (`HH_HW_PROBE`, `HH_CICLOS_RUIDO`, `HH_CADENCIA`, as
   `HH_*_MUTACAO` dos adaptadores, `HH_RENDER_PROBE`, `HH_IRQ_QUANTUM`).
4. **Catraca** `hh15_produto_papeis` (`tests/hh15/check_papeis.cmake`): manifesto × mapa do linker do `HeadHunter.exe`
   — todo objeto dos papéis do produto no mapa (exceção fechada: `snapshot`, sem chamador), nenhum de validação, o
   stub presente, nenhum objeto do `hh_runtime` fora do manifesto, mapa não anterior ao executável. Negativos por
   mutação do mapa (`sem_objeto`, `com_sonda`, `objeto_extra`) reprovam pelo motivo certo. `hh15_produto_recusa`: o
   produto sai com 2 para cada variável de teste, e sem ela não.
5. **Revisão Codex da HH15.2** (`reviews/codex-hh15_2.log`), 5 achados, conferidos e corrigidos:
   - [MEDIA] o produto aceitava variáveis de teste fora da lista (`HH_STOP_ON_STARTRENDER`, `HH_AUDIO_SEM_DSP`):
     classificação fechada em `runtime/debug/validacao_vars.h` (32 de teste, recusadas; 52 observacionais) e o teste
     `hh15_variaveis_classificadas`, que varre o código e reprova leitura de `HH_*` sem classificação; o teste de
     recusa cobre as 32;
   - [MEDIA] a catraca não detectava manifesto incompleto: inventário independente das fontes de `runtime/` (toda fonte
     no manifesto ou na lista fechada do que não é do `hh_runtime`) e fonte duplicada ou inexistente;
   - [MEDIA] objetos com o mesmo nome contavam como um: nome repetido no manifesto reprova;
   - [BAIXA] busca por trecho de texto: comparação por campo do mapa (objeto precedido de espaço; biblioteca com outro
     nome reprova; os stubs como objetos do próprio produto);
   - [BAIXA] mapa velho e negativos frouxos: o carimbo `Timestamp is` do mapa tem de ser o `TimeDateStamp` do PE; cada
     negativo exige a mensagem do caso (`PASS_REGULAR_EXPRESSION`), agora 8.

### HH15.3 (em andamento)

Inventário (`HH_HLE_LOG`, rota nativa, 4 700 VBlanks; `analysis/hh15/hle_uso.txt`): o jogo só usa
- GDROM com r6 = 0: REQ_CMD (comandos INIT 0x18, GET_VERSION 0x28, 6 DMAREAD 0x11 de 1 setor no boot, LBA 0x385CD a
  0x385E2, e 781 GETSCD 0x22 formato 1), GET_CMD_STAT, EXEC_SERVER (5 492), INIT_SYSTEM e GET_DRV_STAT;
- FLASHROM só na partição 2: INFO, READ de `0x1C000` e `0x1FFC0` (0x40 bytes), WRITE e DELETE de `0x1C000`;
- SYSINFO INIT e ID; MISC 1/3 e o bootstrap (fim do jogo).

Precedente do N8.3 do BS (opção A, decisão do mantenedor): serviço nativo na própria fronteira das syscalls, fora do
contrato erro explícito, flash volátil, HLE só na verificação e nas ferramentas.

1. **`runtime/system/syscall_service`** (papel `nativo`): os mesmos endereços dos vetores (registrados de novo depois do
   `boot_prepare`), o mesmo efeito em r0 e na RAM do HLE no perfil instantâneo; setores do `RuntimeDisc` com o mesmo
   registro de leituras (origem `gdrom`); flash da partição 2 em memória, em branco a cada sessão; o resto é
   componente removido (`sh4_fatal`, contado). Setor ilegível é instalação incompleta (erro), nunca setor em branco.
2. **Seleção**: `--sistema hle|nativo` no `hh-run` (padrão nativo; o HLE fica implícito com `--arquivos legado` e
   `HH_GDROM_TIMING`); o produto não tem a escolha e recusa `--arquivos legado` (saída 2).
3. **Produto sem o HLE**: papel `hle` (`bios`, `flashrom`, `gdrom`, `data_source`) só na verificação e nas ferramentas;
   o `HeadHunter.exe` compila `runtime/system/hle_ausente.cpp`, que define os seis símbolos que o boot, o host e o poll
   referenciam, e o linker não puxa o HLE. A catraca exige o papel `hle` fora do mapa (negativo `com_hle`).
4. **Equivalência**: as referências da HH15.0 (capturadas com o HLE) dão 18/18 com o serviço nativo; unitários
   `hh15_servico_nativo_igual_ao_hle_no_contrato` (o roteiro do contrato no HLE e no serviço: r0 e RAM iguais; uma
   mutação no texto da versão do GDROM reprova) e `hh15_servico_nativo_recusa_fora_do_contrato` (16 recusas).

### HH15.4 (em andamento)

No HH o modelo do Maple nunca teve VMU (M4) e, com os controles nativos (HH10, padrão do produto), fica também sem o
controle da porta A: no caminho do produto ele só respondia "nenhum dispositivo" à sondagem da camada `per` e agendava
o fim do DMA, que solta a passada dela (servidor `bu` nativo, espera `0x8C118346`). Precedente do N8.4 do BS:
barramento sem dispositivos com o mesmo efeito do modelo sem controle; o modelo só na verificação.

1. **`runtime/system/maple_bus`** (papel `nativo`): registradores, DMA que segue a tabela até o LAST e grava
   `0xFFFFFFFF` em cada buffer de recepção, fim em 100 ciclos de 200 MHz por bit (o do modelo), ISTNRM bit 12, os
   mesmos contadores e a transição injetada dos roteiros no primeiro acesso ao MDST depois do VBlank-In. Fora do
   contrato (padrão de transferência ≠ 0, tabela sem LAST, DMA fora da RAM, disparo por VBlank `MDTSEL=1`, que o jogo
   nunca pede) é erro explícito.
2. O modelo (`maple/maple.cpp`, papel `perifericos`, só na verificação) encaminha ao barramento quando não há
   controle, então as referências do `hh-run` (controles nativos) exercitam o código do produto; com `--controles
   legado` ele continua servindo o controle.
3. O produto compila `runtime/system/maple_ausente.cpp` (a API do modelo direto ao barramento) e recusa `--controles
   legado` (saída 2). `runtime/input` (roteiros, índice de transição, modo espectador) continua no produto: não é
   simulação de hardware. Catraca: o papel `perifericos` fora do mapa (negativo `com_maple`).
4. **Revisão Codex da HH15.3/HH15.4** (`reviews/codex-hh15_34.log`), 4 achados, conferidos e corrigidos:
   - [MEDIA] comando de GD-ROM fora do contrato com o drive ocupado era devolvido como "ocupado" em silêncio: o comando
     é conferido já no REQ_CMD (recusa nos unitários);
   - [MEDIA] trocar a presença do controle no meio da sessão separaria os estados do modelo e do barramento: a troca
     depois de um DMA é erro explícito e o host zera o barramento no fim da sessão antes de restaurar a presença;
   - [MEDIA] o produto aceitava `--saves legado` (e o `--headless` o escolhia): recusado, com o `--headless` nos saves
     nativos num cartão temporário (HH15.5 abaixo);
   - [BAIXA] cobertura: `hh15_servico_nativo_le_setores_como_o_hle` (os 6 DMAREAD do boot e o GETSCD, com os dados: r0
     e a RAM inteira iguais ao HLE), a RAM inteira no teste do roteiro e `hh15_barramento_igual_ao_modelo_sem_dispositivo`
     (buffers, prazo, MDST antes e depois do prazo, segundo disparo e contadores contra o modelo com o controle
     desconectado; uma mutação de 1% no prazo reprova).

### HH15.5 (concluída, 2026-10-29)

O vídeo legado não tem módulo próprio no runtime: é o decodificador 4X do código gerado sobre a AICA e o PVR
simulados. O que o mantinha no produto era o `--headless` (verificação da instalação contra a rota do HH06, gravada com
os filmes, o áudio e os saves legados). Precedente do N8.5/N8.6 do BS: o produto só com os provedores nativos e a
verificação do produto rebaseada no caminho nativo; as rotas históricas ficam na verificação.

1. O produto recusa `--saves legado`, `--video legado` e `--audio legado` (saída 2; além de `--arquivos` e
   `--controles legado`); o `--headless` usa saves nativos num cartão temporário, filmes e áudio nativos (sem
   dispositivo) e o render por software.
2. Referência do produto: o roteiro `newgame_area1_nativo` com esses provedores, gravada pelo `hh-run`
   (`regressao.py --gravar-produto`, `HH15_PRODUTO_HASHES`, privada em `snapshots/hh15`); a etapa `builder` da
   regressão compara o produto instalado pelo Builder com ela. A rota histórica do HH06 continua na etapa
   `roteiro_area1` (`hh-run`).
3. **Recusa antes de qualquer dado**: as cinco recusas (`--arquivos/--controles/--saves/--video/--audio legado`) vêm
   antes da conferência da instalação; negativo `hh15_produto_recusa_legado` (saída 2 e mensagem para cada uma, e a
   mesma chamada com `nativo` não sai com 2), que reprovava antes da mudança (saída 1, a instalação era conferida
   primeiro).
4. **Filmes**: o adaptador nativo não tem retorno ao decodificador 4X (filme ausente é o `open` original falhando,
   `ret 1`, nunca o caminho legado). Na rota do produto: 3 `open`, 3 `play`, 732 quadros, 0 ausentes, 0 pulados, 60
   legendas; os 94 hashes da referência do produto (que atravessam os filmes) iguais no `hh-run` e no
   `HeadHunter.exe` instalado (etapa `builder`, `analysis/hh15/regressao2.log`). Regressão integral 14/14; ctest
   63/63.
5. Fica para a HH15.7: o quadro do filme é composto no lugar do quadro do PVR e o ritmo do filme lê
   `holly_vblank_count()`; os dois saem com o corte do PVR/Holly (ponte do SPG).

### HH15.6 (concluída, 2026-10-29)

Inventário (sonda da HH15.0, rota nativa, 4 700 VBlanks): com o áudio nativo do HH14 a CPU não toca a RAM de som nem
registrador de canal, DSP, temporizador ou interrupção do ARM7; na área da AICA só `0x8C1706E8` lê o RTC
(`0x00710000`/`04`, 9 400 vezes cada, duas por VBlank). Os registradores do G2 (`0x005F7800-0x005F78FF`: a tabela de
inicialização em `0x8C15FAC8` e `0x8C18113C`/`0x8C181278`, a suspensão em `0x8C183C6C`/`0x8C183C8C` a cada leitura do
RTC) e o estado da FIFO (`0x005F688C`, `0x8C183CD4`) são do `holly_sb` e saem com a HH15.7. Precedente do N8.6 do BS:
área de som nativa com o contrato medido, modelo só na validação; diferente do BS, a RAM de som fica fora do contrato,
porque no HH o provedor nativo de áudio tem a sua e o SDK não a toca pela CPU.

1. **`runtime/system/sound_bus`** (papel `nativo`): RTC no relógio do provedor de tempo (alto, baixo e a liberação de
   escrita em `0x00710008`, como o modelo), controle `0x00702C00` guardado, a mesma instrumentação (`HH_CLOCK_LOG` `Rtc`
   e `Aud`); qualquer outro acesso à área (`0x00700000-0x0071FFFF`, `0x00800000-0x00FFFFFF`) ou outro tamanho é erro
   explícito.
2. O modelo (`aica/aica_regs.cpp`, papel novo `aica`, só na verificação) delega controle e RTC ao serviço e guarda a RAM
   de som do `--audio legado` e os diagnósticos `HH_AICA_*`; então as referências do `hh-run` exercitam o código do
   produto.
3. O produto compila `runtime/system/aica_ausente.cpp` (a API do modelo direto ao serviço; sem `aica_ram`) e recusa
   `--audio legado` (HH15.5). Catraca: o papel `aica` fora do mapa e o stub no mapa (negativos `com_aica` e
   `sem_stub_aica`).
4. Unitários `hh15_area_de_som_igual_ao_modelo_no_contrato` (o roteiro do RTC e do controle no serviço e no modelo, com
   os valores esperados pela contagem do provedor estável) e `hh15_area_de_som_recusa_fora_do_contrato` (16 recusas:
   canais, DSP, temporizadores, RAM de som e espelho, tamanhos 1 e 2).
5. Revisão Codex (`reviews/codex-hh15_56.log`), três achados aceitos e corrigidos:
   - [MÉDIA] `--verify-data` e a exportação/importação de saves saíam antes das recusas
     (`HeadHunter.exe --verify-data --audio legado` conferia os dados). As recusas passam a ser um exame do `argv` no
     começo do `main`, válido em todos os modos; o negativo `hh15_produto_recusa_legado` cobre os 4 modos (falhou antes
     com 15 casos);
   - [MÉDIA] o unitário não cobria o reset entre sessões nem o `HH_CLOCK_LOG`. Novos
     `hh15_area_de_som_clock_log_do_contrato` (contagem e hash de `Rtc` e `Aud` calculados dos valores do contrato, no
     serviço e no modelo) e `hh15_area_de_som_reset_entre_sessoes` (RTC na hora do host, controle e liberação
     desligados). Parte rejeitada: o RTC sem valor definido só existe antes do primeiro reset do processo, e o código é
     o mesmo do reset (`rtcNow` define a hora do host); os valores esperados já eram fixos no teste, não do modelo; a
     ordem do `HH_CLOCK_LOG` e os diagnósticos na rota completa ficam com as referências da HH15 e os portões do HH09 e do
     HH14, capturados com o modelo antigo;
   - [BAIXA] a catraca só reconhecia `hh_runtime:aica_regs.obj`. Os objetos dos papéis fora do produto passam a ser
     reconhecidos sem biblioteca ou de qualquer uma; negativos `aica_solto` e `aica_outra_biblioteca`, que passavam na
     catraca sem a correção.
   - Sem achados de diferença de valor do RTC, símbolo faltando ou acesso do produto às recusas novas.

Resultado: regressão integral 14/14 (`analysis/hh15/regressao3.log`, 2026-10-29, antes da revisão; as correções mudam
só as recusas de opções, testes e a catraca), ctest 67/67, `hh_runtime_tests` 16 ok e 1 sem dados.

### HH15.7 (em andamento): plano

Ponto de partida, diferente do BS no N8.7: o HH13.6 já intercepta os cinco funis do Kamui (`0x8C1793E0` escrita e
`0x8C1793EA` leitura de registrador, `0x8C17C600` DMA, `0x8C159820` cópia e `0x8C159874` zeragem), com slots
gerados (o equivalente da D-K1 (b) do BS, já em uso desde o HH13). No modo nativo esses funis não tocam o PVR; o que
ainda passa pelo hardware simulado no produto é:
- o render e o quadro exibido: `pvr_start_render` (o decodificador e o rasterizador do `pvr_render` mais a captura do
  `render_capture`) e `pvr_video_frame` sobre o estado nativo emprestado (`withNative`: `pvr_regs_set_override`,
  `vram_set_override`, `ta_set_override`);
- o `--render software` (padrão do headless e do `hh-run`) sem o provedor nativo do Kamui: TA, VRAM e registradores
  do runtime;
- o DMA: o original programa o DMAC (`FFA00020/28/2C/40`) e o SB (`SB_C2DSTAT/LEN/ST`), e o modelo entrega os bytes
  ao `areaFourSink`; o fim do DMA e o fim de lista vêm do `holly_intc`;
- o SPG (`0x0C8-0x0E0` e `SPG_STATUS`) no `pvr_regs`, lido pelo tempo e pelo Maple; o ritmo do filme e o adaptador
  de vídeo leem `holly_vblank_count()`;
- o tratador `0x8C168160` lê e limpa `ISTNRM`/`ISTERR` e lê o `CHCR2`.

Decisões herdadas do BS (seção 2, mesmas regras): sem regenerar o AOT nem mudar o gerador; a memória de texturas
endereçada como a VRAM é memória do guest (D-K2); banco mínimo de status alimentado por eventos nativos, com o
tratador original do Kamui (D-K3 (a)); tempo estável e lógico sem mudança de relógio, ordem ou prazo (D-K4). O custo do
fim de render continua `min(450000 + bytes do TA * 100, 1500000)`.

Incrementos, cada um com o padrão do BS: o serviço nativo no produto; na validação o modelo antigo roda junto e é a
referência; um comparador por acesso ou por render com negativo por mutação; a catraca proíbe o modelo no produto;
referências da HH15, Codex e commit próprio.

0. **Medição** (sonda gráfica só na validação, recusada no produto): na rota do produto, com o provedor nativo do Kamui
   e os dois renderizadores, confirmar que todo byte do TA chega pelo funil de DMA (nenhum SQ para a área 4), os
   valores de `FB_W_SOF1` (sem render para textura), quem lê a VRAM pela CPU, os registradores que o tratador e o jogo
   leem e a sequência de status por render (fim de DMA, fins de lista, fim de render, VBlank).
1. **7a, software sobre a IR**: `native/render/soft` (`soft_raster` movido do `pvr_render` sem mudar as contas, texels
   por uma fonte; `soft_device` como o Vulkan, sobre a `CommandList`). O produto passa a usar o provedor nativo do
   Kamui também no software, e o software sobre a IR é o recuo do Vulkan. Prova: a imagem do software sobre a IR igual
   à do rasterizador sobre o TA em todo render da rota (modo `conferir`), os 94 hashes da referência do produto e as
   referências da HH15 com o `hh-run`.
2. **7b, ListSink**: a divisão dos parâmetros (`Params` do `kamui_device`) e o decodificador das listas viram
   `native/render/kamui/list_sink`, que produz a IR direto; `ta`, o decodificador do `pvr_render` e o `render_capture`
   saem do produto. Comparador: a IR do ListSink igual à do caminho do TA render a render.
3. **7c, FrameState, TextureHeap e DisplayService**: o banco de registradores do Kamui (com o SPG num módulo
   compartilhado, sem mudar as contas), a memória de texturas com gerações por página e a imagem exibida pelos
   registradores `FB_R_*`; `pvr_regs`, `vram`, `pvr_texture` e `pvr_video` saem do produto. Comparadores por acesso
   (FrameState), por página tocada (TextureHeap) e por quadro inteiro (DisplayService).
4. **7d, eventos e DMA**: o banco de status do Holly (`holly_intc` fora do produto); o DMA do canal 2 feito pelo funil
   nativo, com o contrato medido do DMAC e do SB (o que o tratador e o Kamui leem e os mesmos prazos); a parte do
   `holly_sb` que não é gráfica (Maple, G2, `SB_FFST`) num serviço do sistema; `sh4_dmac` fora do produto. As store
   queues para a RAM são semântica da CPU e ficam; para a área 4, erro explícito.

Portão: R1–R5 do HH13 e K1–K4 com o caminho novo; referências da HH15 (rotas, hashes e traces de ordem e entrega);
sonda sem acesso ao PVR, à VRAM, ao TA, ao DMAC ou ao SB gráfico; catraca do mapa sem `pvr_*`, `ta`, `vram`,
`holly_intc`, `sh4_dmac` e `render_capture`.

#### HH15.7, medição (passo 0, 2026-10-29)

Sonda `HH_HW_PROBE` da rota nativa (`analysis/hh15/sonda_nativo.tsv`, provedor nativo do Kamui): o guest não acessa a
VRAM (área 1), o TA (área 4) nem os registradores do PVR além do SPG (`0x005F810C` por `0x8C16E908` e `0x8C16EF18`,
`0x005F80DC` por `0x8C16EF18`, 4 645 cada); as store queues só vão para a RAM (`0x0C4xxxxx`–`0x0C6xxxxx`). O resto do
contrato gráfico é o DMA do canal 2 pelo funil `0x8C17C600` (DMAC `FFA00020/28/40` e `CHCR2` `FFA0002C`; SB
`SB_C2DSTAT/LEN/ST` `0x005F6800/04/08`; 9 605 pedidos) e o tratador `0x8C168160`, que lê e limpa `ISTNRM` e lê
`ISTERR` e o `CHCR2` (25 207 / 9 605 vezes). Fora do gráfico, no SB: `SB_FFST` (`0x005F688C`, `0x8C183CD4`) e a
inicialização (`0x8C15FAC8`).

#### HH15.7a (concluído, 2026-10-29): software sobre a IR

- `native/render/soft/soft_raster` (o rasterizador do `pvr_render` movido sem mudar as contas; texels por uma
  `TexelSource`, com o handle da textura na chave do cache) e `soft_device` (`IRenderDevice` sobre a `CommandList`,
  conversões da IR para as palavras do hardware como as da captura). O `pvr_render` decodifica e desenha com o
  `soft::rasterize` e os texels da VRAM (o caminho do TA, referência das comparações).
- `render_capture`: `Mode::Software` (o software do produto) desenha a IR no dispositivo em software, com as texturas
  pelas conversões nativas e cache pelos bytes da textura; o `Mode::Native` cai no software sobre a IR quando o Vulkan
  falha. O host usa o provedor nativo do Kamui e a IR nos dois renderizadores; o rasterizador sobre o TA só roda nos
  modos de verificação (`HH_RENDER_IR`).
- Comparador: o `conferir` desenha cada render conferido também no software sobre a IR e compara a imagem inteira;
  negativo `HH_RENDER_SOFT_MUTACAO=textura` (o primeiro texel de cada textura enviada muda). Portão R1 do
  `hh13_render` com os dois; unitários `soft_device_conversions_match_the_capture` e
  `soft_mode_writes_the_same_framebuffer_as_the_ta_rasterizer`, e o `expectClean` das cenas exige o software igual.
- Rota da área 1 (4 700 VBlanks, `hh-run` padrão): 2 250 renders pelo software sobre a IR, 0 falhas, 80 texturas
  enviadas (nenhuma pelo decodificador do legado); os 94 hashes iguais à referência do HH06; tempo 144 s (o passo
  `roteiro_area1` levava 149 s). `conferir` (1 render a cada 5): 450 conferidos, 0 imagens divergentes; com a
  mutação, 130 dos 135 conferidos acusados.
- Revisão Codex (`reviews/codex-hh15_7a.log`), três achados aceitos e corrigidos; sem diferença de conta no
  rasterizador movido, nas conversões nem no cache de texturas:
  - [MÉDIA] o R1 conferia o software sobre a IR só nos renders amostrados (1 a cada 5): a comparação dele passa a rodar
    em todo render do `conferir`, e o portão exige todos os renders da rota comparados;
  - [MÉDIA] uma falha do software sobre a IR (ou do recuo do Vulkan) caía no rasterizador sobre o TA em silêncio: agora
    o destino da captura declara que é o renderizador (`renders()`) e um render que ele não desenhou é `sh4_fatal`, com
    o motivo no log na primeira falha; unitário `soft_mode_failure_is_fatal_not_the_ta_rasterizer` (falha injetada
    `Fault::Soft`), que reprovava sem a correção;
  - [BAIXA] no `conferir` a falha do software contava como imagem divergente, e o negativo aceitava isso: falhas e
    divergências separadas, e o negativo exige renders comparados e 0 falhas.
- Regressão integral 14/14 com as correções (`analysis/hh15/regressao_7a.log`; `portoes_hh13` com o R1 novo em todo
  render e o negativo), ctest 67/67.
- Fica para o 7b: o produto ainda aceita `HH_RENDER_IR` (observacional; os modos de verificação com o caminho do TA;
  o `HH_KAMUI` já é recusado); o decodificador e o rasterizador sobre o TA continuam ligados até o ListSink.
