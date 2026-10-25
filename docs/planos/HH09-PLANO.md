# HH09 — Plano detalhado: tempo, eventos e tarefas nativos (fase híbrida)

Validação mínima: **V2**. Critério de saída (ROADMAP): roteiros determinísticos não divergem
nas tolerâncias justificadas por evidência; nenhum busy-wait novo.

Equivale ao N2a + N2b do Blue Stinger (MIT; `docs/N2-PLANO.md`, `N2a-RELATORIO.md`,
`N2b-PLANO.md`, `N2b-RELATORIO.md`, `N2b-ESPERAS.md`, `N2-PLANO.md` §14). O N2c do BS (remover
`HH_POLL`, `HH_CYCLES`, INTC e periféricos do SH-4) é o **HH15**. Regra permanente: recompilar,
nunca emular. O HH09 não acrescenta simulação de hardware: só tira dos ciclos a decisão de
quando o tempo anda. A simulação de registradores do runtime híbrido só encolhe.

Pré-requisito: HH08 `CONCLUIDA` (2026-10-25).

**Estado: EM_ANDAMENTO.** Aprovado pelo mantenedor em 2026-10-25 ("Continua").

## Fatos de entrada (preparação, 2026-10-25)

Disassembly: `build\translator\Release\hh-disasm.exe game-data\hh01\disc1_ref\1ST_READ.BIN
0x8C010000 <endereço> <tamanho>`. Tabelas: `analysis/hh07/disc1/*.tsv` e
`analysis/hh07/ordem_inventario.tsv` (privados, ignorados).

### O que o runtime já tem (porte do HH05)

| Peça (BS) | No HH | Situação |
|---|---|---|
| `runtime/cpu/time_hooks.h` (`TimeProvider`, `time_now`, ganchos `progress`/`wait_point`/`wait_end`/`wait`/`holly_pending`) | igual | portado; só existe o provedor **legado** |
| `runtime/cpu/time_legacy.cpp` | igual | portado (cálculo preguiçoso por ciclos) |
| Módulos lendo o tempo por `time_now` | `aica_regs`, `sh4_intc`, `sh4_threads`, `gdrom`, `maple`, `sh4_tmu`, `holly_intc`, `pvr_regs` | portado (`rg -l "time_now\|time_provider()" runtime`) |
| `HH_WAIT`/`HH_WAIT_END` no gerador, `[native] esperas` | `translator/app/config.cpp:102`, `codegen.cpp:110,147` | portado; **lista vazia** em `config/headhunter.toml` |
| `HH_TMU_DIAG`, `HH_CLOCK_LOG`, `HH_EVENT_TRACE`, `HH_CALL_TRACE`, `HH_WW_VOLTAS` (`write_watch.h`) | `runtime/` | portados |
| `native/platform/time.h` | versão do **N0** (`Instant{frame,subframe}`, `IEventQueue` mínima) | falta a versão do N2a (ticks, `TimeBase`, `post/cancel/reschedule`, `set_kind_priority`, `IProgress`) |
| `native/platform/{clock,event_queue,progress}.*` | — | **não portado** |
| `transitional/time_adapter.*` (estável e experimental, `timed_wait`/`hardware_wait`) | — | **não portado** |
| `transitional/cri_idle.*` (ociosidade da thread 3) | — | provavelmente desnecessário (ver Tarefas) |
| `--tempo legado\|nativo\|experimental` | — | `hh-run`/`HeadHunter.exe` não têm a opção |
| `tools/dist/test_tempo.ps1`, `test_n2b*.ps1`, `tools/dev/n2b_{lacos_quentes,classifica_lacos,esperas_estaticas}.py`, `config/n2_excecoes.toml` | — | não portados |

Quantum de poll: `kServiceQuantum = 2000` ciclos (`runtime/cpu/sh4_runtime.h:58`).

### Ordem de eventos (HH07.7, `newgame_area1`, 4 700 VBlanks)

V = 4 700; I = 39 625 (INTEVT 0x320: 34 980; 0x360: 4 645); N = 8; T = 74 320 (sempre
principal ↔ thread n, 4 645 idas e voltas por thread); J = 74 312 (0x8C1838E0 para as threads
1–8, 0x8C18392A de volta à principal); **R = 0**. Sonda (`hw_manifest.tsv`, origem `sonda`): o
tratador 0x8C168160 lê ISTNRM 34 980 vezes (= nº de 0x320); 0x8C16E908/0x8C16EF18/0x8C15F7F6
rodam 4 645 vezes (= nº de 0x360).

### Tarefas: escalonador cooperativo (disassembly + trace de longjmp)

Duas evidências independentes (o corpo abaixo e os alvos de `J` do trace, que são os retornos
dos dois `setjmp`):
- **Passada** 0x8C1838F8 (`r4` = estrutura de controle): guarda o controle corrente de
  `[0x8C32EF3C]`, faz `setjmp` em 0x8C183926 (retorno 0x8C18392A), percorre as tarefas
  (vetor em +0x210, passo 0x124, quantidade em +0x208, corrente em +0xD78) pelo estado em
  +12: 1 = dorme até o prazo +20 ≤ contador (então `longjmp` para o buffer +24); 2 e 3 =
  encerramento (0x8C183762). No fim, contador +0x204 += 1. Troca o FPSCR durante a passada.
- **Ceder** 0x8C1838B6(`N`): prazo = contador + N (N < 0: 0x7FFFFFFF), `setjmp` em 0x8C1838DC
  (retorno 0x8C1838E0) e `longjmp` de volta à passada. 0x8C1838F4 = ceder(1); 0x8C1837EE marca
  a tarefa como encerrada e cede.
- Mesmo modelo do "nível 2" do BS (8C0903C0). **Diferença:** o trace não mostra o "nível 1"
  (threads do SDK trocadas por `rte`, R = 0). Não há, nesta rota, o laço sem espera da thread
  3 do CRI que bloqueou o N2b-2 do BS. *Hipótese* até cobrir vídeo 4XM/ADX e outras rotas.
- Camada: `bloco:sdk`, família desconhecida (uma evidência só). Os nomes ficam descritivos.

### Temporizador e RTC

- syTmr (Shinobi), mesmo leiaute do BS 8C15D17C…8C15D1B4: 0x8C15E830 inicia o canal 0
  (TCOR0 = TCNT0 = 0xFFFFFFFF, TCR0 = 2), 0x8C15E856 devolve a contagem decorrida, 0x8C15E860
  faz a diferença, 0x8C15E866 converte em µs. Evidências: disassembly + `hw_manifest.tsv`
  (escritas em 0xFFD80004/08/0C/10).
- Leituras de TCNT0 na sonda: 0x8C15E856 138 918; 0x8C1696C0 134 260; 0x8C16A5A0 36 545;
  0x8C17C600 e 0x8C179520 14 534 cada; 0x8C16C120 4 504. RTC da AICA: 0x8C1706E8, 9 296.
  SPG_STATUS por 0x8C1793EA (leitor genérico de registrador do PVR): 170 803.
- **Diferença do BS** (lá nenhum chamador do TMU era do jogo): há chamadores na camada do
  jogo (`bloco:jogo`, uma evidência):
  - 0x8C07147C/0x8C0714A0: medidor (início; fim acumula, conta amostras e grava uma taxa em
    +20), lido por 0x8C07154C a partir de 0x8C04E090;
  - 0x8C01FFD8: milissegundos decorridos (contagem → µs → divisão por 1 000), chamada por
    0x8C09F72A (grava instantes numa máquina de estados) e por 0x8C09F5D0 (compara o decorrido
    com uma tabela). *Hipótese:* é uma decisão do jogo por tempo;
  - 0x8C0E3DD8, 0x8C0E4006 e o início em 0x8C0CDF20: sem classe.

### Catraca `tempo.consumidores.ciclos` = 2 é artefato

`analysis/hh07/disc1/runtime_ciclos.tsv` aponta `cpu/sh4_service.cpp:36` e
`cpu/sh4_hw_m3.cpp:25`. As duas linhas são `c->next_event = c->cycles + kServiceQuantum`
(progresso). O classificador (`translator/hwscan/main.cpp:257`) só isenta essa expressão em
`cpu/sh4_runtime.cpp`, o lugar do poll central no BS; no HH ele fica em `sh4_service.cpp`, e
`sh4_hw_m3.cpp` é o stub de testes do HH04. Os consumidores funcionais reais são **0**.
`fronteira.subsistema.tempo` = 38, `hw.subsistema.tempo` = 136 (`config/metricas_base.toml`).

### Candidatos a espera (hipóteses, a medir no HH09.1)

- As ~36 leituras de SPG_STATUS por VBlank (170 803 / 4 700) apontam para um laço que consulta
  o feixe. O HH05 viu o boot preso num laço de espera do VBlank em volta de 0x8C1793EA.
- O laço principal que espera o VBlank antes da passada das tarefas ainda não foi localizado.
- Timeouts pelo TMU (como 8C173044 no BS): procurar entre os chamadores de 0x8C15E856.

## Reuso do Blue Stinger (MIT, com atribuição por arquivo)

| Componente BS | Uso no HH09 |
|---|---|
| `native/platform/time.h` (N2a/N2b), `clock.*`, `event_queue.*`, `progress.*` | Porte; substitui o `time.h` do N0 e mantém `hh07_native_limpo` |
| `transitional/time_adapter.*` (estável + experimental, `timed_wait`, `hardware_wait`) | Porte; experimental sem `cri_idle` enquanto o HH09.1 não achar thread preemptiva |
| `tools/dev/n2b_{lacos_quentes,classifica_lacos,esperas_estaticas}.py` | Porte como `hh09_*` (inventário de laços) |
| `tools/dist/test_tempo.ps1`, `test_n2b.ps1`, `test_n2b2.ps1` | Refeitos em Python (`tests/diff/hh09_*.py`), como o `hh07_ordem.py` |
| `config/n2_excecoes.toml` | Formato de `config/hh09_excecoes.toml` (lista fechada, escrita antes) |
| Lições: critério A2 pela linha do SPG; mutação válida = rc 0 + divergência no artefato certo; artefato vazio nunca é diferença; VBlanks fundidos; SPG_STATUS comparado só em quantidade; espera de DMA anda o relógio lógico (§14.1); headless fica no estável | Aplicadas nos portões |

## Subetapas

| ID | Entrega | Evidência de saída |
|---|---|---|
| HH09.0 | Referências: TN (ordem normalizada por VBlank: tipo, INTEVT, thread, longjmp, tarefa), CL (leituras de arquivo), CK (`HH_CLOCK_LOG` sem TMU), SL (estado lógico mínimo: modo, sala, contador +0x204), quadro. Classificador da catraca corrigido (poll central em `sh4_service.cpp`; stub do HH04 isento só como teste) | Artefatos idênticos em duas execuções; catraca 2 → 0 com negativo (uma leitura nova de `->cycles` num módulo volta a contar) |
| HH09.1 | Inventário de esperas (`HH_WW_VOLTAS` + scripts portados + varredor estático) e classe de cada consumidor do TMU (medição, timeout, decisão), por `HH_CALL_TRACE` e disassembly | Tabela laço → evento que libera, conferida à mão; chamadores do TMU classificados; R (continuações por `rte`) medido também com 4XM e ADX |
| HH09.2 | `native/platform/{time.h,clock,event_queue,progress}` | Unitários `hh09_*` (ordem total, prioridade, cancelar/reagendar, inscrição durante a entrega, RTC no reset); `hh07_native_limpo` |
| HH09.3 | `transitional/time_adapter` estável + `--tempo legado\|nativo` (`hh-run`, `HeadHunter.exe`) | A1–A8 (portões) em 4 700 VBlanks e numa rota longa; `tempo.ciclos.funcionais` = 0 |
| HH09.4 | Contrato das tarefas: passada, ceder(N), prioridade = ordem do vetor, encerramento, reentrância (passada dentro de passada pelo controle salvo em `[0x8C32EF40]`?), FPSCR | Documento + trace de tarefas por VBlank; a `ITaskScheduler` não troca threads do guest (D4 do BS) |
| HH09.5 | Modo sombra: `[native] esperas` com as esperas do HH09.1, `IProgress`, `HH_PROGRESS_TRACE`; limite da guarda de vivacidade calibrado | S1–S4: trace completo igual byte a byte ao estável; cada espera liberada pelo evento esperado; 0 faltas |
| HH09.6 | `--tempo experimental`: o tempo só anda nas esperas; espera de hardware (DMA, render) pelo relógio lógico; timeouts por `schedule_timer` | B1–B11 com `config/hh09_excecoes.toml` |
| HH09.7 | Catálogo HH15: `HH_POLL`/`HH_CYCLES`/`HH_IRQ_CHECK` no gerado, INTC, TMU/SPG/RTC/ISTNRM como interface do SDK, entrega por IRQ, fibras, `time_now` | Tabela com contagens e dono (HH15 ou etapa de subsistema) no relatório e em `PLANO-PORT-NATIVO.md` |
| HH09.8 | Interface para HH12–HH14: só `IClock`/`IEventQueue` | Teste estático: `native/` e `transitional/` de vídeo/áudio/render sem `cycles`, `kCompatTicks*` ou `time_now` |
| HH09.9 | Promoção, regressão (`regressao.py`, etapa `portoes_hh09`), relatório, Codex, commit | `docs/relatorios/HH09-RELATORIO.md` |

Promoção: o estável vira padrão quando A1–A8 passarem. O experimental vira padrão na janela só
com B1–B11 passando; o headless e as referências ficam no estável (como no BS §14.3). Se o
experimental não fechar, ele fica como opção e o fechamento vai para o HH15, como o ROADMAP
permite.

## Portões

| # | Critério | Oráculo / tolerância |
|---|---|---|
| A1 | Ordem legado × estável | `HH_EVENT_TRACE` byte a byte (entrega nos mesmos polls) |
| A2 | Instante efetivo | VBlank devido **na linha do SPG** (`pvr_spg_crossed`); 0 entregas antes do devido; atraso máximo ≤ limite derivado do quantum e medido (o BS mediu 2 120 contra limite 4 000) |
| A3 | Quadro | hashes do roteiro iguais aos do HH06/HH08 |
| A5 | Relógios consultáveis | CK por VBlank igual; SPG_STATUS só em quantidade |
| A8 | Catraca | `tempo.consumidores.ciclos` = 0 e não sobe; `fronteira.provedor.legado` não sobe |
| B1–B4 | Experimental × referência | TN, IRQ por VBlank, CL, CK iguais; VBlanks fundidos = legado; só exceções da lista fechada (VBlank, artefato, causa provada) |
| B5 | Timeouts | caminho tomado (evento × estouro) igual ao legado |
| B6 | Jogo | SL, quadro e escritas na AICA iguais (áudio: quantidade, se a ordem depender do tempo de execução, como no BS) |
| B7 | Vivacidade | 0 guardas; fila vazia numa espera = erro com PC |
| B8 | Determinismo | duas execuções iguais em todos os artefatos, nos três modos |
| B9 | Desempenho / sem busy-wait | ms por VBlank ≤ estável × 1,05 (3 execuções alternadas); a espera nunca gira no host |
| B11 | TMU | consumidores do jogo do HH09.1: decisão igual ao legado com o TMU no relógio lógico |

## Testes negativos (cada um falha antes da correção ou detecta a mutação)

| Defeito | Detectado por |
|---|---|
| Prioridade invertida, evento perdido/atrasado, cancelado entregue, deriva, relógio em degraus (`HH_TEMPO_MUTACAO`) | unitário + A1/A5 com rc 0 |
| Leitura de ciclos fora do provedor | catraca (HH09.0) |
| Espera declarada ignorada / laço sem espera | S3 / B7 (guarda com PC) |
| IRQ extra ou ausente; VBlank fundido provocado | B2/B4 |
| Render que nunca termina | B5 (estouro no tempo lógico, sem travar) |
| Variável de posição alterada | B6 |
| Espera ativa injetada no host | B9 |
| Consumidor artificial que decide pelo TMU | B11 |
| Mod de função de espera sem `mods::wait` | erro claro nomeando a função |

## Riscos

- **Decisão do jogo por milissegundos do TMU** (0x8C09F5D0, hipótese): com o tempo parado
  durante o trabalho, o decorrido muda. Se a decisão for visível, a equivalência exige
  exceção provada ou um modelo de duração por evento.
- Thread preemptiva fora da rota (vídeo 4XM, ADX, Audio64): reabriria o problema da thread 3
  do BS.
- VBlanks fundidos no boot e lag de carga mudam a fase lógica (BS N2a §4).
- Cobertura: uma rota de 4 700 VBlanks; a guarda de vivacidade é calibrada só nela. Rotas da
  passagem do disco 1 para o 2 (discos juntos, HH08) e da campanha entram quando existirem.
- O render por software domina o custo; B9 é refeito depois do HH14.
- `runtime/mods/mod_api.h` cita 0x8C064A20 (endereço do BS) no exemplo; corrigir no HH09.5.

## Questões abertas

1. Onde o laço principal espera o VBlank, e que bandeira o tratador de 0x360 acende?
2. Quem chama a passada 0x8C1838F8, e se ela roda dentro de um tratador (o que mudaria o
   modelo de entrega)?
3. Quais chamadores do TMU são timeout (ponto de espera com `schedule_timer`) e quais são
   decisão (B11)?
4. INTEVT 0x320 em média 7,4 por VBlank: quais fontes (render, DMA, Maple, GD) e quais viram
   evento da fila?
5. A política para o TMU do BS (relógio lógico, sem avanço por leitura nem tempo real do host)
   vale como precedente. Só vira `DECISAO_NECESSARIA` se o HH09.1 mostrar decisão visível do
   jogo sem alternativa equivalente.

## Regras

- O provedor legado fica disponível (`--tempo legado`) até o HH15.
- Nada de interpretar instruções nem contar ciclos para decidir; esperas são declaradas por
  endereço com evidência.
- Saídas em `analysis/hh09/` e `reviews/` (ignorados); nenhum byte proprietário nos documentos.
