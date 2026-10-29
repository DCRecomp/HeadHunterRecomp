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
