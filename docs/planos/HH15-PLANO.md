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

### HH15.2 (concluída, 2026-10-30, no fechamento V3 da HH15.8)

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

### HH15.3 (concluída, 2026-10-30, no fechamento V3 da HH15.8)

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

### HH15.4 (concluída, 2026-10-30, no fechamento V3 da HH15.8)

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

### HH15.7 (concluída, 2026-10-30): plano

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

#### HH15.7b (concluído, 2026-10-29): ListSink

- `native/render/kamui/list_sink` (portado do BS sem o fog, a sombra e os campos de fechamento de volume que o HH não
  tem): divide os blocos da área 4 em parâmetros (a regra do `Params` do `kamui_device`) e decodifica direto para a IR
  com as regras do Parser do `pvr_render` e da montagem da captura, num decodificador só; `draw_state` e `ir_vertex`
  ficam nele, e a captura delega.
- `kamui_device` (nativo): o ListSink no lugar do `Params`. No STARTRENDER, com a captura como o renderizador (o
  software sobre a IR ou o Vulkan), `pvr_start_render_with` (a mesma contagem, gancho e prazo do fim de render pelos
  bytes do ListSink) e o render pela IR do ListSink: parâmetros do quadro, listas decodificadas direto, o renderizador
  da captura e a imagem no framebuffer. Vértices de estados que a IR não representa (textura que o decodificador não lê,
  dois volumes) e render não desenhado são erro explícito. Nos modos de verificação (`HH_RENDER_IR`), o caminho do TA
  sobre a cópia das mesmas listas, e a captura confere o ListSink com ele.
- Comparador `check_list_sink` (em todo render do caminho do TA com o ListSink registrado: quadro, vértices, desenhos,
  volumes, texturas e contagens); negativo `HH_LISTA_NATIVA_MUTACAO=vertice`. Portão K5 do `hh13_kamui` e a cena dos
  unitários da captura alimentando também um ListSink (o `expectClean` exige a mesma IR em todas).
- Rota da área 1: o padrão (o produto) desenha os 2 250 renders pela IR do ListSink, com os 94 hashes iguais à
  referência do HH06; `conferir` no nativo: ListSink igual à captura nos 2 250 renders e o software sobre a IR igual
  ao caminho do TA nos 2 250; com a mutação, 674 de 674 acusados.
- Revisão Codex (`reviews/codex-hh15_7b.log`), quatro achados aceitos e corrigidos; sem diferença na divisão de 32/64
  bytes, `TA_LIST_CONT`, fim de lista, ordem dos eventos, chaves de textura, prazo, fundo ou framebuffer:
  - [MÉDIA] o render pela IR do ListSink decodificava no ambiente de ponto flutuante do guest (arredondamento para zero,
    FTZ/DAZ), e o caminho do TA no padrão (`FpEnv` antes do Parser): o `renderList` instala o `FpEnv` antes dos
    parâmetros do quadro e da decodificação;
  - [BAIXA] o `HH_TA_DUMP` sumia no caminho nativo: o dump fica nos dois caminhos (os parâmetros pelo chamador);
  - [BAIXA] o teste do comparador comparava com uma captura vazia: as duas listas recomeçam, um strip a mais só no
    ListSink, e o teste exige a cena conferida;
  - [BAIXA] o comparador não conferia a contagem de vértices sem suporte (a que para o produto): `unsupported` contra
    os omitidos da captura, com o unitário `list_sink_counts_unsupported_like_the_capture`; as contagens sem uso saíram.
- Regressão integral 14/14 com as correções (`analysis/hh15/regressao_7b.log`, com o K5), ctest 67/67.
- A remoção do decodificador, do `ta` e da captura do link fica para depois dos cortes 7c e 7d (as funções de quadro,
  fundo e framebuffer ainda leem o banco do PVR e a VRAM emprestados), com a catraca do mapa no fim, como no BS.

#### HH15.7c (concluído, 2026-10-29): estado do quadro, memória das texturas e imagem exibida

- Serviços nativos em `native/render/kamui`: `FrameState` (o banco do funil, com a semântica de comando que estava no
  `kamui_device`; a paleta; os parâmetros do quadro da IR; a cor do fundo; o aviso do Region Array; a gravação da
  imagem no framebuffer), `TextureHeap` (8 MB no layout do caminho de 32 bits, com a vista de 64 bits),
  `texture_source` (a conversão das texturas da captura sobre a memória nativa, mais a paleta de 8 bits por
  `convert_pal8`, que antes ia ao decodificador do legado) e `display_service` (a saída de vídeo do `pvr_video`).
- O produto (o render pela IR do ListSink e o quadro exibido) roda sobre o estado nativo, sem empréstimo ao banco do PVR
  nem à VRAM do runtime; uma textura sem conversão nativa não é desenhada (o render falha com erro explícito). O SPG
  (SPG_STATUS e o SPG_CONTROL do vídeo) continua lido do runtime até o 7d.
- Nos modos de verificação o caminho do runtime continua a referência, e quatro comparadores conferem o nativo: os
  parâmetros do quadro e os bytes e texels de cada textura (`[render-nativo]`), a gravação do framebuffer (por render)
  e o quadro exibido (por VBlank) (`[kamui]`); negativos `HH_KAMUI_NATIVO_MUTACAO=quadro|textura|framebuffer|video`,
  cada um acusado só pelo seu comparador (portão K6). Unitário `native_frame_state_matches_the_runtime` (16 bits, paletas
  de 4 e de 8 bits contra o decodificador do legado, framebuffer, vídeo).
- Rota da área 1: o padrão com os 94 hashes iguais ao HH06; `conferir`: 2 250 quadros, 39 422 texturas, 2 250
  framebuffers e 4 700 quadros exibidos iguais (0 sem conversão nativa); cada mutação acusada em todos os conferidos
  (674 renders ou 1 500 VBlanks) e só no seu comparador.
- Revisão Codex (`reviews/codex-hh15_7c.log`), três achados aceitos e corrigidos; sem diferença de conta na paleta,
  fundo, parâmetros do quadro, Region Array, texturas, framebuffer ou saída de vídeo:
  - [ALTA] no `conferir` a referência lia o próprio estado nativo emprestado: provava as funções, não o estado. A sombra
    (o runtime no caminho real, com o banco e a VRAM dele) passa a aplicar a semântica de escrita do estado nativo
    (`FrameState::write`, conferida com o banco do runtime em cada STARTRENDER pelo K1) e a registrar o estado nativo
    na captura, que confere o quadro e as texturas lidos dele com os lidos do runtime: na rota, 2 250 quadros e 39 422
    texturas iguais, registradores e memória com 0 divergências (K6 na sombra);
  - [MÉDIA] o unitário cobria só 565 progressivo: novos `native_frame_state_writes_like_the_runtime_bank` (comandos,
    só de leitura, ponteiros do TA, `TA_LIST_CONT`, leituras do funil) e
    `native_framebuffer_and_scanout_formats_match_the_runtime` (os 7 formatos do framebuffer com K, limiar, recorte,
    passo e volta; os 4 formatos de leitura com linhas duplicadas, entrelaçado e blank);
  - [BAIXA] o K6 aceitava texturas e quadros exibidos maiores que zero: exige as texturas conferidas iguais ao total
    dos renders (contado à parte) e um quadro exibido conferido por VBlank.
- Regressão integral 14/14 com as correções (`analysis/hh15/regressao_7c.log`, com o K6), ctest 67/67.

#### HH15.7d1 (concluído, 2026-10-29): DMA do canal 2 e system block como serviços nativos

- `runtime/system/dma_bus` (papel `nativo`): DMAOR e os registradores do canal 2 guardados (o contrato medido: o funil
  `0x8C17C600` e o tratador `0x8C168160`); a transferência entrega os blocos da RAM, na ordem, direto ao destino nativo
  das listas e texturas, sem o TA do runtime, com o efeito do modelo de antes no fim (SAR2, DMATCR2, CHCR2.TE).
- `runtime/system/sb_bus` (papel `nativo`): o contrato medido do system block (leituras de C2DSTAT/C2DLEN/C2DST, SB_FFST
  e SB_SBREV; escritas guardadas do sistema, do G1, do G2 e do DMA do PVR); SB_C2DST = 1 dispara o DMA do canal 2 e acende
  o bit 19 do ISTNRM, como antes.
- Os modelos `pvr/holly_sb.cpp` e `memory/sh4_dmac.cpp` vão para o papel novo `sb` (só verificação) e delegam o
  contrato aos serviços, então as referências do `hh-run` exercitam o código do produto; o modelo guarda o resto da
  região, os negativos `HH_SB_MUTACAO` e os diagnósticos, e entrega o canal 2 ao TA do runtime sem o destino nativo
  (`HH_KAMUI=legado` ou `sombra`).
- O produto compila `system/sb_ausente.cpp` e `system/dmac_ausente.cpp` (a API dos modelos direto aos serviços; fora do
  contrato, erro explícito). Catraca: o papel `sb` fora do mapa e os stubs no mapa (negativos `com_sb` e `sem_stub_sb`).
- Rota da área 1: o padrão e a sombra com os 94 hashes iguais ao HH06; no nativo os 14 534 pedidos ao funil de DMA entregaram
  301 857 856 bytes de listas direto ao destino nativo; ctest 69/69.
- Revisão Codex (`reviews/codex-hh15_7d1.log`), sem diferença de efeito apontada nos serviços:
  - [MÉDIA] aceito: faltava um teste dirigido do contrato (os modelos delegam aos serviços, então não são uma referência
    independente nele): `hh15_dma_e_system_block_no_contrato` com os valores fixos do modelo de antes (canal
    desabilitado, blocos na ordem, SAR2/DMATCR2/TE, C2DSTAT/C2DLEN/C2DST, bit 19, `next_event`, caminho de textura,
    SB_SBREV/SB_FFST), compilado com os modelos (`hh_runtime_tests`) e com os stubs do produto
    (`hh15_produto_sb_tests`: fora do contrato, erro explícito). Fica registrado, sem mudança nesta subetapa, que os
    negativos `HH_SB_MUTACAO` (do modelo, aplicados antes da delegação) não têm portão desde o HH09;
  - [MÉDIA] parcial: os stubs do produto só rodavam com dados no passo `builder` da regressão (o `HeadHunter.exe`
    instalado na rota do produto, 94 hashes), não em teste sem dados; o `hh15_produto_sb_tests` cobre agora o contrato
    deles;
  - [BAIXA] aceito: o K3 usava a contagem de pedidos do funil nativo, que contava todo pedido como de listas; o funil
    nativo classifica pelo destino do descritor, como a sombra, e o K3 exige os pedidos e os bytes por destino iguais
    aos da sombra.
- A regressão (passo `builder`) parou o boot de teste do `HeadHunter.exe` instalado numa escrita da tabela de
  inicialização em `0x005F6940`, fora da lista de escritas do serviço: a sonda classificava `0x005F6940-0x005F6954`
  (logo depois do INTC) como `holly_intc`. O contrato de escrita ganhou `0x005F6940-0x005F69FF`, e todo acesso da sonda
  da rota nativa à região do system block foi conferido contra o contrato (nenhum fora); o unitário escreve a tabela
  inteira. Com isso o `HeadHunter.exe --headless` cumpre a rota do produto (4 700 VBlanks) com os 94 hashes da
  referência do produto.
- Regressão integral 14/14 (`analysis/hh15/regressao_7d1.log`), ctest 70/70; no passo `builder` o produto instalado
  cumpre a rota sem acesso recusado.

#### HH15.7d2 (concluído, 2026-10-29): banco de status de interrupção nativo

- `native/render/kamui/event_status` (derivado do BS, N8.7 4a; deslocamentos 0x00-0x3F): o banco mínimo de status que o
  tratador `0x8C168160`, a Ninja e o jogo leem e limpam: ISTNRM/ISTEXT/ISTERR e as máscaras IML*, a entrega dos eventos
  vencidos na ordem do controlador anterior (VBlank-In com a contagem dos fundidos, VBlank-Out, fim de render, fim do
  DMA do Maple), os levantados na hora, o nível da fonte externa da AICA (ISTEXT: só a fonte apaga; o ISTNRM lido com o
  bit 30), o nível IRL e as contagens. O prazo de cada evento continua com o provedor de tempo (D-K4).
- `runtime/system/event_bus.cpp`, compilado só no produto (como os stubs): a API de `pvr/holly_intc.h` sobre o banco
  nativo, com os mesmos efeitos e a mesma ordem no `update` (`maple_vblank_in`, traces, `pvr_render_done_take`,
  `maple_dma_take`, `pvr_video_vblank` por último).
- `pvr/holly_intc.cpp` vai para o papel novo `intc` (só verificação): é a referência e roda o banco nativo em sombra,
  comparado depois de cada operação (os 16 registradores, o ISTNRM lido, o nível IRL e as contagens;
  `holly_event_status_summary`, impresso pelo host); negativo `HH_EVENT_STATUS_MUTACAO=limpa` (a sombra ignora a limpeza
  do bit 19).
- Catraca: `intc` fora do mapa e `event_bus` como objeto do próprio produto (negativos `com_intc` e `sem_event_bus`).
  Testes (`tests/hh15_intc_tests.cpp`, no `hh_runtime_tests` com a referência e no `hh15_produto_sb_tests` com o serviço
  do produto): o banco puro, o contrato pela API com valores fixos e a entrega dos eventos vencidos com um provedor de
  tempo roteirizado; negativo `hh15_event_status_mutacao`. Portão K7 em `tests/diff/hh13_kamui.py`.
- Rota da área 1 (`hh-run`): 94 hashes iguais ao HH06; a sombra comparou 83 990 operações, 0 diferentes; o negativo
  `limpa` acusa (300 VBlanks: 2 632 diferentes, a primeira na limpeza da operação 173). Produto (`HeadHunter.exe
  --headless`, rota do produto): 94 hashes iguais à referência do produto. ctest 73/73.
- Revisão Codex (`reviews/codex-hh15_7d2.log`), sem diferença apontada nos valores, bit 30, reset, contagens, faixas ou
  símbolos:
  - [MÉDIA] aceito: o K7 só executa o controlador da verificação (o `hh-run` não liga o `event_bus`), e o teste do
    produto não entregava eventos vencidos; remover a entrega ou um efeito do `event_bus` não reprovava nada sem dados.
    Novo `hh15_intc_entrega_de_eventos_vencidos` (nas duas versões): provedor roteirizado, render agendado e DMA do Maple
    em curso; confere bits, contagens, fundidos, instante do último VBlank-In, consumo do fim de render e do Maple e a
    apresentação do quadro depois do estado atualizado. Conferido por mutação manual do `event_bus` (sem
    `pvr_render_done_take`; sem `pvr_video_vblank`): o teste reprova nos dois casos. O K7 passa a dizer o que confere;
  - [MÉDIA] parcial: a sombra compara o banco, não os efeitos em volta (instante do VBlank, despertar, Maple, traces,
    consumo dos prazos, apresentação). Esses efeitos ficam no teste dirigido acima, nas duas versões, e na rota do
    produto do passo `builder`; a comparação por operação continua só do banco, como no BS.
- Regressão integral 14/14 (`analysis/hh15/regressao_7d2.log`), ctest 73/73.

#### HH15.7d3 (concluído, 2026-10-29): temporização do vídeo (SPG) como serviço nativo

- `native/render/kamui/spg_timing` (novo): as contas do SPG movidas de `pvr/pvr_regs.cpp` sem mudança (fase do feixe
  pelo SPG_STATUS do boot, cruzamentos e próximo cruzamento de linha, próxima troca de linha, posição e SPG_STATUS;
  guarda 0x0C8-0x0E0). `runtime/system/spg_bus` (papel `nativo`, no produto): a instância com o campo
  `kCompatTicksPerField`; SPG_STATUS no instante atual (com o clock_log); SPG_LOAD e SPG_VBLANK_INT entregam os eventos
  vencidos com a configuração anterior (`holly_intc_sync`) antes do valor novo e avisam o provedor de tempo depois.
- Fonte única do SPG: o tempo (`cpu/time_legacy`, `transitional/time_adapter`) usa `spg_*`; o provedor nativo do Kamui
  escreve e lê o SPG pelo serviço (antes pelo modelo dos registradores); `pvr/pvr_regs.cpp` delega ao serviço `reg()` em
  0x0C8-0x0E0 (também com o banco emprestado, que tinha a mesma cópia), as escritas, o SPG_STATUS, o banco impresso, o
  reset e o estado do boot. Saem do `pvr_regs.h`: `pvr_spg_*`, `pvr_vblank_*_line`, `SpgPos`, `kSpgNever`.
- Testes (`tests/hh15_spg_tests.cpp`, no `hh_runtime_tests` e no `hh15_produto_sb_tests`): as contas contra uma cópia
  das fórmulas anteriores numa grade (4 campos, 5 SPG_LOAD, 5 SPG_STATUS, 3 SPG_VBLANK, entrelaçado ou não; 8 instantes,
  8 linhas, 5 intervalos), valores fixos num campo de conta exata e o contrato pela API (delegação, só leitura, dump,
  boot, `spg_changed` só em reset/load/SPG_LOAD/SPG_VBLANK_INT, eventos vencidos avaliados com o valor anterior).
  Mutações manuais conferidas: arredondamento da fase (reprova a grade) e `holly_intc_sync` depois do valor novo
  (reprova o contrato).
- Rota da área 1 (`hh-run`): 94 hashes iguais ao HH06 (o 3800 aparece partido por um diagnóstico do stderr no mesmo
  log, mesmo valor); sombra do banco de status: 83 990 operações, 0 diferentes. Produto (`HeadHunter.exe --headless`):
  94 hashes iguais à referência do produto. ctest 73/73.
- Revisão Codex (`reviews/codex-hh15_7d3.log`), sem diferença de valores, arredondamento, fase, instantes ou contagem
  de `spg_changed` apontada:
  - [MÉDIA] aceito: o teste do contrato não via a chamada nem a ordem de `holly_intc_sync` (o provedor devolvia 0 e o
    teste só olhava o valor guardado). O provedor do teste agora guarda o SPG visto em cada avaliação dos eventos
    vencidos; conferido pela mutação acima;
  - [BAIXA] aceito como limitação: as escritas e leituras do SPG pelo funil do Kamui não passam mais pelos diagnósticos
    do modelo (`HH_TA_FONTES`, `HH_PVR_DEBUG`, `HH_PVR_WATCH`), que só existem nele; o modelo sai do produto no 7d4 e
    esses diagnósticos continuam no caminho do guest pela memória.
- Regressão integral 14/14 (`analysis/hh15/regressao_7d3.log`), ctest 73/73.

#### HH15.7d4 (concluído, 2026-10-30): modelos gráficos fora do produto

Medição (mapa do `HeadHunter.exe` em ba0a110): ainda entram `pvr_regs`, `pvr_render`, `pvr_texture`, `pvr_video`, `ta`,
`vram`, `sh4_sq` e `render_capture` (729 símbolos). O BS deixou o `render_capture` no produto; o portão desta etapa o
exclui, então o 7d4 tem três passos: 7d4a, o que o produto usa dos modelos vira serviço nativo compartilhado; 7d4b, o
desenho do produto (renderizador em software e Vulkan sobre a IR) separado da captura e das comparações; 7d4c, os
substitutos do produto (só o contrato medido do guest, o SPG, atendido; o resto erro explícito; store queue para a
área 4 erro explícito), a catraca e os negativos.

- 7d4a (2026-10-29): `runtime/system/render_bus` (o pedido de render: contagem, gancho do hh-diff,
  `HH_STOP_ON_STARTRENDER`, `HH_TA_DUMP`, render na hora, prazo `min(450000 + bytes * 100, 1500000)` avisado ao
  provedor de tempo, o fim pendente que o banco de status consome) e `runtime/system/video_bus` (o aviso de VBlank do
  host), papel `nativo`, movidos de `pvr/pvr_regs.cpp` e `pvr/pvr_video.cpp` sem mudança. O provedor nativo do Kamui, o
  caminho do TA (`pvr_start_render`, que lê os bytes do TA antes do render; o `pvr_render` não muda o TA), as duas
  versões do banco de status, o host e o hh-diff usam os serviços. Teste `tests/hh15_render_bus_tests.cpp` (nos dois
  executáveis: ordem gancho/render/prazo, mínimo, teto, consumo, reset; aviso de VBlank). Rota da área 1: 94 hashes
  iguais ao HH06; produto: 94 iguais à referência do produto; ctest 73/73. Revisão Codex (`reviews/codex-hh15_7d4a.log`):
  sem achados; a falha que ele viu em `hh15_area_de_som_clock_log_do_contrato` é do sandbox somente leitura (o teste
  grava no diretório temporário), rejeitada: passa fora do sandbox.
  Regressão integral 14/14 (`analysis/hh15/regressao_7d4a.log`).
- 7d4b (2026-10-30): `transitional/render_devices` (produto): os tipos comuns, os ajustes globais (escala, substituições
  HD, falhas injetadas, memória nativa), os dispositivos sobre a IR (`Devices`: Vulkan com recuperação e resolução
  interna, software sobre a IR, caches de textura) com a fonte dos texels escolhida pelo dono (`TextureSource`), o
  arquivo da IR, o resumo (`format_summary`, as mesmas linhas que os portões leem) e o renderizador do produto
  (`install_renderer`, `draws_lists`, `draw_list`), movidos de `render_capture.cpp` sem mudança. O renderizador lê os
  texels só da memória nativa (antes, sem ela, caía na VRAM do runtime; agora a textura não é enviada e o render para
  com erro explícito). A captura fica com o caminho do TA (montagem, conferir, comparar, ListSink, estado nativo) e uma
  instância dos mesmos dispositivos com os texels do runtime; o host instala os dois no mesmo modo fora de
  `HH_RENDER_IR` e imprime o resumo de quem desenhou. Teste `tests/unit/test_hh15_render_devices.cpp` (a IR igual ao
  dispositivo direto, contagens e resumo, sem memória nativa o render falha, com ela os texels vêm dela, instalação).
  Rota da área 1 e produto: 94 hashes iguais; ctest 73/73. Revisão Codex (`reviews/codex-hh15_7d4b.log`): sem achados.
  Regressão integral 14/14 (`analysis/hh15/regressao_7d4b.log`).
- 7d4c (2026-10-30): os modelos gráficos (`pvr_regs`, `pvr_render`, `pvr_video`, `pvr_texture`, `vram`, `ta`) viram o
  papel `grafico` do runtime, só da verificação e das ferramentas. O produto compila `runtime/system/pvr_ausente.cpp`
  no lugar: só o SPG é atendido (pelo `spg_bus`; a escrita em `SPG_STATUS`, só leitura, é ignorada como no modelo), o
  banco que o boot deixa fica guardado só para a instalação do provedor nativo do Kamui, `vram_data()` devolve nulo (a
  memória das texturas parte zerada, como a VRAM depois do reset) e o resto do banco, a VRAM (área 1), a área 4 pela
  store queue, o STARTRENDER pelo caminho do TA, a saída de vídeo do modelo e os empréstimos da verificação são erro
  explícito. E `transitional/render_capture_ausente.cpp`: os modos da captura lançam erro; `HH_RENDER_IR` e as
  variantes de comparação passam a variáveis de teste, recusadas na partida do produto (saída 2;
  `HH_RENDER_IR_GUARDAR` fica observacional, é do renderizador do produto). Catraca: `grafico` fora do produto, os dois
  substitutos obrigatórios, `render_capture` proibido de qualquer biblioteca e `hh_transitional:render_devices`
  obrigatório, com os negativos `com_grafico`, `sem_stub_pvr`, `com_captura`, `sem_stub_captura` e
  `sem_render_devices` (as mutações dos substitutos trocam também as linhas das funções inline, `f i <obj>`). Teste
  `tests/hh15_pvr_ausente_tests.cpp` (executável `hh15_produto_pvr_tests`, com o substituto como no produto: só o SPG,
  banco do boot, só 32 bits, fora do contrato fatal, sem VRAM, área 4 pela store queue fatal). Mapa do
  `HeadHunter.exe`: nenhum objeto `pvr_*`, `ta`, `vram`, `holly_intc`, `sh4_dmac` ou `render_capture`. Rota da área 1 e
  produto: 94 hashes iguais; ctest 79/79. Revisão Codex (`reviews/codex-hh15_7d4c.log`): aceito, a escrita em
  `SPG_STATUS` dava erro no substituto e o modelo a ignora (corrigido, com teste); rejeitado, `HH_RENDER_IR_GUARDAR`
  continuar aceito no produto (só grava o arquivo da IR do próprio renderizador do produto, classificação
  observacional). Regressão integral 14/14 (`analysis/hh15/regressao_7d4c.log`).

Fechamento do HH15.7: portão atendido. R1–R5 do HH13 e K1–K4 com o caminho novo (`portoes_hh13` da regressão
integral); referências da HH15 (`portoes_hh15` e `roteiro_area1`); o produto não tem PVR, VRAM, TA, DMAC nem SB
gráfico (os substitutos param com erro explícito em qualquer acesso fora do SPG e do contrato do DMA/SB nativos, e a
rota do produto chega aos 94 hashes); catraca do mapa sem `pvr_*`, `ta`, `vram`, `holly_intc`, `sh4_dmac` e
`render_capture`.

### HH15.8 (concluída, 2026-10-30): fechamento

- **Prova de imports e strings** `tests/hh15/check_fechamento.py` (testes `hh15_produto_fechamento*`): as DLLs das
  tabelas de imports e de delay-load do PE (parser próprio, limitado aos dados das seções) na lista fechada do produto
  (CRT do MSVC, Win32, Media Foundation, Direct2D/DirectWrite, `winmm`, `xinput1_4`), com as obrigatórias presentes;
  nenhum dos 517 literais exclusivos das fontes fora do produto (lexer de C++; mínimo de 3 por grupo: os sete papéis só
  da verificação, a captura da IR e o tradutor com o interpretador) nos bytes do executável. Negativos: `dll_extra`,
  `sem_imports` e `string_<grupo>` para os nove grupos; autoteste sintético do parser e do lexer (reprova as mutações
  sem delay-load, cauda virtual, sem comentário de bloco e sem raw string). Controle: no `hh-run` a prova acusa os oito
  grupos do runtime.
- **Sonda pós-corte** (`analysis/hh15/sonda_fech_{legado,experimental}.tsv`): as mesmas regiões das sondas do HH15.0;
  PVR só o SPG, DMAC só o canal 2, SB só o DMA do canal 2, `SB_FFST` e a inicialização, nenhum acesso à VRAM ou à
  área 4, todos atendidos por serviços nativos.
- **Revisão Codex** (`reviews/codex-hh15_8.log`): 5 achados aceitos e corrigidos (imports vazios passariam, lexer
  ingênuo, piso só global, delay-load ignorado, RVA na cauda virtual); segunda passada (`reviews/codex-hh15_8b.log`):
  os 5 corrigidos, 2 achados novos aceitos (testes sem o delay-load e os limites de RVA; lexer sem teste sintético),
  resolvidos com o autoteste.
- HH15.2–HH15.4 concluídas com os portões (relatório, seção 3). Regressão integral V3 pós-corte 14/14
  (`analysis/hh15/regressao_hh15_8.log`), ctest 92/92. Relatório `docs/relatorios/HH15-RELATORIO.md`.
- **D-HH15-1** apresentada (relatório, seção 9): a etapa fica `BLOQUEADA_DECISAO`.

## 7. Parte 2: remoção do substrato AOT (D-HH15-1 = B)

**Decisão do mantenedor (2026-10-30):** o substrato AOT sai do produto nesta etapa ("começa agora a remoção e deixar
totalmente nativo"). A HH15 volta para `EM_ANDAMENTO`; a parte 1 (HH15.0–HH15.8: tempo guest, simulação de hardware e
provedores legados fora do produto) está concluída e provada (`docs/relatorios/HH15-RELATORIO.md`). Critério de saída
da parte 2: o critério operacional final do `ARCHITECTURE.md` no `HeadHunter.exe` (sem `Sh4Context`, despacho por
endereço, RAM guest nem código gerado), com a Preview jogável e equivalente às referências em cada passo.

### 7.1 Medição de partida (2026-10-30, `analysis/hh15e/`)

- Código gerado: 4 699 funções por disco (mesmos endereços nos dois discos; os binários diferem em dados), cerca de
  1,1 milhão de linhas de C++ por disco, 45 380 chamadas diretas e 2 214 despachos indiretos.
- Pelas fronteiras já nomeadas (`config/fronteiras.toml`), o código do jogo fica abaixo de `0x8C1586A8` (3 521
  funções, com bolsões de som e do runtime C) e o SDK acima (1 178: CRI, Kamui, Shinobi, BIOS).
- Rota `newgame_area1_nativo` (4 700 VBlanks, `HH_FN_PERFIL`): 1 428 funções do jogo e 433 do SDK executadas.
- HH15.9, `tools/dev/hh15_elevacao_inventario.py` sobre o `hh-hwscan` (classificação por banners e assinaturas do SDK)
  e o perfil da rota: jogo 3 032 funções (1 113 executadas na área 1), SDK 1 515 (Shinobi 390, CRI 272, Kamui 58, som
  33, BIOS 18, runtime C 7, 737 ainda sem família; 649 executadas), 151 desconhecidas (98 executadas). O jogo chama
  diretamente 187 funções do SDK (a superfície a substituir: Shinobi 53, CRI 13, Kamui 9, som 9, runtime C 4, BIOS 2,
  97 sem família). Por nível no grafo de chamadas diretas, 673 funções do jogo são folhas e 1 041 estão a 10 níveis
  ou mais (ciclos e cadeias longas); 64 funções do jogo fazem 114 chamadas indiretas.

### 7.2 Método (o do N10 do BS, sem precedente executado lá)

- Elevação por sistema, não por função: módulos nomeados em `native/game` (e o SDK substituído em `native/sdk`), com
  nomes e estruturas sustentados por duas evidências (regra do projeto).
- O código do SDK já substituído por serviço nativo e o código morto são descartados, não elevados; a meta é a ausência
  de dependência operacional, não 100% das funções.
- Cada função ou sistema elevado entra pelo slot da função gerada (`mods::replace`, o mesmo mecanismo dos mods); na
  verificação, um modo de conferência roda a função gerada sobre uma cópia do estado e compara o estado de saída (RAM,
  registradores do contrato, chamadas aos serviços) em cada chamada da rota, com negativo por mutação.
- Fase intermediária: a função elevada ainda lê e escreve a RAM guest por acessores tipados (equivalência provável por
  comparação direta). Fase final: as estruturas do jogo migram para memória nativa e a RAM guest, o `Sh4Context`, o
  despacho e as fibras saem; a catraca proíbe `hh_generated` e `hh_runtime_cpu` no produto.
- Cobertura: as funções fora da rota da área 1 precisam de outros roteiros (campanha) ou de testes diferenciais
  sintéticos; nenhuma função entra no produto sem conferência registrada.

### 7.3 Subetapas

| ID | Entrega | Portão |
|---|---|---|
| HH15.9 | Inventário da elevação: mapa função → biblioteca/sistema, grafo de chamadas, executadas por rota, dados globais tocados, ordem por dependência | inventário reproduzível; classificação com evidência |
| HH15.10 | Infraestrutura: `native/game`, registro das funções elevadas pelo slot, modo de conferência na verificação, negativos | uma função elevada conferida em toda chamada da rota; mutação reprova |
| HH15.11 | SDK: runtime C, Shinobi, CRI, Kamui e som substituídos ou descartados | conferência das funções do SDK na rota; zero chamadas do jogo ao SDK gerado |
| HH15.12+ | Sistemas do jogo, um por subetapa (laço principal e tarefas, cenário e carga, jogador, câmera, colisão, inimigos e IA, HUD e menus, eventos e scripts, saves) | conferência por sistema; referências da HH15 iguais |
| HH15.F | Dados nativos, remoção da RAM guest, do `Sh4Context`, do despacho e das fibras; fechamento V3 | critério operacional final; V3 |

### HH15.9 (concluída, 2026-10-30): inventário da elevação

`tools/dev/hh15_elevacao_inventario.py` (seção 7.1): classificação do `hh-hwscan`, grafo de chamadas diretas, perfis de
execução por rota, superfície do SDK chamada pelo jogo e níveis do grafo. A ordem por sistema sai daqui em cada
subetapa; novas rotas entram como novos perfis.

### HH15.10 (concluída, 2026-10-30): infraestrutura da elevação

- `native/game/` (lógica nativa; o primeiro módulo é `angulo.h`) e `transitional/elevacao` com
  `transitional/elevadas_jogo.cpp`: cada elevada entra pelo slot da gerada (o endereço vai para `[native] slots` do
  config e o gerador passa a chamá-la pelo slot; regeneração conferida idêntica antes da mudança). O adaptador da fase
  intermediária reproduz o caminho original na ordem observável (ciclos e back-edges da gerada, escritas antes de cada
  back-edge) e declara as premissas (palavras do literal pool que ele embute), conferidas antes da entrada do jogo e em
  cada chamada da conferência. A instalação recusa elevada sem slot ou que não é função gerada; as elevadas são
  instaladas antes dos mods do usuário (um mod continua valendo).
- `HH_ELEVACAO=gerado|ativo|conferir` (só na verificação; o produto recusa a variável e fica no ativo). Conferir: a
  elevada roda num ensaio (escritas num diário desfeito, back-edges só contados, guarda que encerra o ensaio mesmo com
  erro), a gerada roda de verdade (trajetória de referência) e são comparados custo (ciclos e back-edges), `r8`-`r15`,
  bancos inativos, `pr`, `sr`, `gbr`/`vbr`/`ssr`/`spc`/`sgr`/`dbr`, `mach`/`macl`, `fpscr`, `fr12`-`fr15`, `xf`,
  retorno e o valor final de cada escrita da elevada; nas 256 primeiras chamadas de cada elevada e depois em uma a cada
  1 024, a RAM inteira menos a pilha morta abaixo do `r15` de entrada. Chamada em que a gerada foi atendida só conta; se
  só a elevada teria sido atendida, é divergência.
- Primeiras elevadas: `8C06F62C` (normaliza um ângulo para [-π, π], 112 mil chamadas na rota) e `8C073980` (lê um
  global, 72 chamadores). Rota da área 1 no modo conferir: 333 228 chamadas, 333 227 conferidas (837 com a RAM inteira),
  1 com atendimento, hashes iguais à referência; produto e rota no modo ativo: 94 hashes iguais.
- Testes `tests/unit/test_hh15_elevacao.cpp` (slot, modo ativo com o custo, premissas, e o comparador acusando pelo
  motivo certo escrita a mais, outro valor, ciclos, back-edges, retorno, `macl`, atendimento só da elevada, premissa
  alterada no meio; erro no meio do ensaio o encerra). Passo `elevacao` da regressão integral: a rota no modo conferir,
  com conferência em cada elevada e os hashes da referência.
- Revisão Codex (`reviews/codex-hh15_10.log`): 6 achados; aceitos a ordem dos polls e das escritas, a divergência
  tomada por atendimento, as constantes do pool (premissas), o ensaio sem guarda, parte dos pontos cegos (`mach`/`macl`,
  `xf`, `rbank`, `vbr`/`ssr`/`spc`/`sgr`/`dbr`) e os testes/portão por elevada; rejeitados `t` e `fpul` (voláteis na
  convenção) e a exclusão da memória abaixo do `r15` de entrada (morta depois do retorno pela ABI). Segunda passada
  (`reviews/codex-hh15_10b.log`): aceito, a instalação passava por cima dos mods do usuário (corrigido: antes deles).
  Regressão integral 15/15 (`analysis/hh15e/regressao_10.log`), ctest 93/93.

### HH15.11 (em andamento): SDK

#### HH15.11a (concluído, 2026-10-30): biblioteca do compilador e do runtime C

- `transitional/elevadas_runtime_c.cpp`: `strlen` (8C1247E8), `strcmp` (8C124068), `memset` (8C124640), a cópia byte a
  byte (8C123FE0) e as duas entradas da cópia por palavras pela tabela de saltos (8C123E14 e 8C123E20) do compilador, e
  as divisões do compilador (quociente e resto, com e sem sinal: 8C123B38, 8C123BEC, 8C123C94, 8C123D5C). Identificação
  pelo desmonte e pela conferência contra a gerada em cada chamada. Contratos: C (`strcmp`), completo (todos os
  registradores e o T como a gerada deixa: os auxiliares com convenção própria) e auxiliar (o completo menos os bits M,
  Q e T do SR, rascunho das sequências de `div1`, que toda divisão recomeça). Premissas: a tabela de saltos das cópias e
  as palavras do código de erro das divisões (divisor zero grava o código na variável de erro).
- As divisões usam fórmulas fechadas (resultado, `r1` e o desvio de correção do resto com sinal), deduzidas por um
  modelo das instruções do trecho feito só como ferramenta de análise, fora do produto, e conferidas em 400 mil
  entradas por função (inclusive `INT_MIN / -1`); a elevada não imita `div1`.
- `tools/dev/hh15_custo.py`: o esqueleto de custo de uma função gerada (rótulos, `HH_CYCLES`, back-edges, desvios).
- Rota da área 1 no modo conferir: 12 elevadas, cerca de 3 milhões de chamadas conferidas por rota, 0 divergências,
  hashes iguais; produto: 94 hashes iguais.
- **Referências de RAM sem a pilha morta.** A primeira regressão do lote reprovou as referências pré-corte da HH15 só
  no hash da RAM (quadros, ordem, relógio, áudio e resumo idênticos): 12 a 17 bytes diferentes, todos na pilha
  principal abaixo do SP. Uma elevada não reproduz o lixo que a gerada deixa abaixo do SP, e um tratador de
  interrupção que atende dentro dela empilha registradores de rascunho diferentes; copiar esse lixo exigiria imitar a
  gerada instrução a instrução. O `HH_RAM_DUMP` passou a gravar a pilha de cada thread (topo, SP, o R15 mais fundo na
  entrada de função, as trocas de pilha), e `tools/dev/hh15_referencias.py` zera a pilha morta antes do hash (do mais
  fundo menos 256 bytes até o SP, sem invadir a pilha viva de outra thread, no máximo 64 KiB; só a troca identificada,
  a da pilha do boot para a do jogo, é aceita; outra descontinuidade, o R15 descer mais de 64 KiB numa chamada ou subir
  mais de 4 KiB acima do mais raso já visto, reprova a medição; o maior quadro medido na rota é 0x7840, um buffer
  local). As referências foram
  recapturadas com as geradas (`analysis/hh15/ref_mascara`): quadros, relógio, áudio, resumo e o hash da RAM inteira
  idênticos às pré-corte nos três casos; o `ordem.tsv` pré-corte mantido (a lista fechada de SPCs se aplica sobre ele).
  A máscara cobre cerca de 12 KB de 16 MiB. A conferência das elevadas ignora só os 256 bytes do quadro abaixo do r15 de
  entrada (a cópia de lixo de pilha nas elevadas, tentada antes, foi removida).
- Revisão Codex: do lote (`reviews/codex-hh15_11a.log`), sem achados; da mudança das referências
  (`reviews/codex-hh15_11a2.log`), aceito: a troca de pilha inferida por salto de 64 KiB podia mascarar dados entre
  pilhas próximas (corrigido: troca detectada por descida acima de 64 KiB ou subida acima do mais raso, só a
  identificada é aceita; a primeira versão, com salto de 4 KiB em qualquer sentido, acusava retornos e quadros grandes
  e foi medida e corrigida antes do commit). Regressão integral 15/15
  (`analysis/hh15e/regressao_11a2.log`), ctest 93/93.
