# HH09 — RELATÓRIO: Tempo, eventos e tarefas nativos (fase híbrida)

Data: 2026-10-25. Validação mínima: **V2**. Plano: `docs/planos/HH09-PLANO.md` (aprovado pelo
mantenedor em 2026-10-25). Equivalente ao N2a + N2b do Blue Stinger.

## 1. Objetivo e escopo realizado

Critério de saída (ROADMAP): roteiros determinísticos não divergem nas tolerâncias justificadas
por evidência; nenhum busy-wait novo.

| ID | Entrega | Resultado |
|---|---|---|
| HH09.0 | Referências e catraca | Trace de ordem (`HH_EVENT_TRACE`), relógios por VBlank (`HH_CLOCK_LOG`), leituras de arquivo e quadros como artefatos comparados. O classificador da catraca foi corrigido: `tempo.consumidores.ciclos` 2 → 0, com teste negativo. |
| HH09.1 | Inventário de esperas | Build `HH_WRITE_WATCH` + `HH_WW_VOLTAS` no roteiro: 2 laços com espera ativa no jogo e 5 no boot, todos classificados por disassembly e pelo modo sombra (seção 3.2). |
| HH09.2 | `native/platform/{time.h,clock,event_queue,progress}` | Portados do BS (MIT); `hh_native_time`; `native` continua limpo. |
| HH09.3 | `transitional/time_adapter` + `--tempo` | Estável: A1, A3 e A5 idênticos ao legado. Virou o **padrão**. |
| HH09.4 | Contrato das tarefas | Documentado (seção 3.4). Nada substituído: as tarefas cooperativas do jogo continuam no código gerado (D4 do BS). |
| HH09.5 | Modo sombra | Cada espera liberada pelo evento esperado. Maior janela sem espera: 31 182 729 (limite 64 000 000); 0 faltas. |
| HH09.6 | `--tempo experimental` | O tempo só anda nas 7 esperas declaradas. O roteiro chega à primeira área; é determinístico e 20% mais rápido. A equivalência B1–B6/B11 com tolerâncias provadas fica para o HH15, como o plano permite (seção 6). |
| HH09.7 | Catálogo para o HH15 | Seção 3.5. |
| HH09.8 | Interface para HH12–HH14 | `native/` sem `time_now`, `kCompatTicks` nem `->cycles`; teste estático no `hh07_native_limpo`. |
| HH09.9 | Promoção, regressão, relatório, Codex e commit | Seções 4 e 9. |

Recompilar, nunca emular: o adaptador decide quando os eventos de hardware vencem pelo relógio
lógico e entrega nos mesmos pontos de sempre. Nada de interpretador e nenhuma simulação nova de
hardware.

## 2. Arquivos alterados

- **`native/platform/`**: `time.h` (substitui o do N0), `clock.{h,cpp}`, `event_queue.{h,cpp}`,
  `progress.{h,cpp}`, portados do BS. `native/CMakeLists.txt` ganha `hh_native_time`.
- **`transitional/time_adapter.{h,cpp}`**, portado do BS:
  - sem o detector de ociosidade da thread 3 do CRI;
  - tabela de esperas do HH;
  - espera do render 0x8C16B7CE.

  `transitional/CMakeLists.txt` atualizado.
- **`app/hh_host.{h,cpp}`**: `TimeProvider` (padrão nativo), instalação antes do boot e resumo.
  **`app/hh_run.cpp`** e **`app/hh_window.cpp`**: `--tempo`.
- **`translator/hwscan/metrics.{h,cpp}`** e **`main.cpp`**: `runtime_cycle_metrics` (poll central
  em `sh4_service.cpp`; stub do HH04 contado como "teste").
- **`config/`**:
  - `headhunter.toml`: `[native] esperas` (7);
  - `metricas_base.toml`: `tempo.consumidores.ciclos` = 0.
- **Testes**:
  - `tests/unit/test_hh09_tempo.cpp` (19, portados do BS);
  - `hh09_cycle_ratchet_classifier` em `test_hh07_hwscan.cpp`;
  - `tests/diff/hh09_tempo.py`;
  - `tests/hh07/check_native_clean.cmake` (tokens do HH09.8);
  - `tests/hh07/native_api_check.cpp` (base de tempo nova);
  - `tests/CMakeLists.txt`.
- **Regressão**: `tools/dev/regressao.py`, com a etapa `portoes_hh09`.
- **Documentos**: este relatório, `docs/planos/HH09-PLANO.md` e `STATUS.md`.

## 3. Evidências e decisões

### 3.1 O que já existia e o que foi portado

A interface de tempo do runtime (`runtime/cpu/time_hooks.h`, `time_legacy.cpp`) é idêntica à
final do BS, e os módulos já liam o tempo por `time_now`. Por isso o `time_adapter` do BS
compilou sem mudança de interface.

Do BS não vieram:
- a ociosidade da thread 3 do CRI (`cri_idle`): no HH não há thread do SDK trocada por `rte` na
  rota medida (R = 0 no inventário do HH07);
- as esperas e endereços dele.

### 3.2 Inventário de esperas (HH09.1)

Build de investigação `build-ww` (`/DHH_WRITE_WATCH=1`), `HH_WW_VOLTAS` no roteiro
`newgame_area1`. O classificador de laços do BS rodou sobre o perfil, em leitura.

Dois laços giram sem progresso:

| Back-edge | Laço | Evidência estática | Evidência dinâmica (modo sombra) | Classe |
|---|---|---|---|---|
| 0x8C08ED26 | f_8C08ED16: espera `*0x8C14DAE4 == 0` | O callback 0x8C0CDD74 decrementa o contador e é chamado 1 vez por VBlank (`HH_CALL_TRACE`: 1 145 VBlanks, nenhum com 2) | Liberada por VBlankIn em 3/3 episódios | Evento: pausa de N quadros (chamada por 0x8C04CA44, 0x8C0954B4 e 0x8C09E904) |
| 0x8C16B7CE | Kamui f_8C16B6FE (chamada por 0x8C159FC0): espera bits de 0x8C31FA04; cada volta chama o despachante 0x8C169260(7) | — | Liberada por RenderDone em 2 222/2 222 | Evento: fim do render (`kRenderWaitPc`) |

Esperas do boot, achadas pelo tempo experimental (falta de progresso com o PC):

| Back-edge | Função | Condição | Classe |
|---|---|---|---|
| 0x8C16A5E4, 0x8C16A608 | f_8C16A5A0 (← 0x8C16A640 ← 0x8C159ACE, Kamui) | SPG_STATUS (leitor 0x8C1793EA, registro 0x10C), máscara 0x3FF (linha); timeout pelo TMU (TCNT0, 0x3D09) | LinhaSpg |
| 0x8C169704, 0x8C169728 | f_8C1696C0 (← 0x8C159C20) | SPG_STATUS, bit 0x2000 (vsync); timeout pelo TMU | LinhaSpg |
| 0x8C11FC3A | f_8C11ECA4, filme 4X (perto de `\MOVIES`) | O objeto do filme diz se o próximo quadro venceu (método virtual; tempo pelo TMU por callback, f_8C12788E) | Evento |

A espera do VBlank por volta do laço principal (`kVblankWaitPc` do BS) não existe como laço
separado no HH: o quadro espera o fim do render (0x8C16B7CE). Os "passos de jogo" (roteiros por
passo) ficam desligados até uma etapa precisar deles.

### 3.3 Portões

| # | Critério | Resultado |
|---|---|---|
| A1 | Ordem legado × estável | `HH_EVENT_TRACE` idêntico byte a byte (7 282 350 bytes) |
| A2 | Instante efetivo (`HH_TIME_TRACE`) | 18 544 eventos; todo VBlank devido na linha do SPG; 0 entregas antes do devido; maior atraso 2 675 contra limite de 4 000 (2 quanta de poll; o BS mediu 2 120) |
| A3 | Quadro | 94 hashes iguais ao legado e à referência do HH06 |
| A5 | Relógios consultáveis | `HH_CLOCK_LOG` idêntico (TMU, SPG, MDST, RTC, ISTNRM, áudio) |
| A8 | Catraca | `tempo.consumidores.ciclos` = 0 (era 2, artefato), varrendo `runtime/` e `transitional/` (o `hardware_wait` entra numa lista fechada de progresso); `fronteira.provedor.legado` não subiu |
| Negativos | Mutações com rc 0 | `atrasar_render` → o A1 acusa; `relogio_degraus` → o A5 acusa |
| S1–S4 | Modo sombra | Trace igual ao estável sem esperas; cada espera liberada pelo evento esperado; 0 faltas de progresso |
| B7 | Vivacidade (experimental) | 4 700 VBlanks sem falta de progresso nem fila vazia |
| B8 | Determinismo (experimental) | 94 hashes iguais em duas execuções |
| B9 | Desempenho / busy-wait | Experimental 108–109 s contra 135 s do estável no roteiro: a espera não gira no host |

A primeira mutação tentada, `perder_vblank_out`, não mudou nada: o jogo não usa a interrupção de
VBlank-Out. Ela foi trocada por mutações que atingem o artefato certo (lição do BS: mutação válida
é rc 0 mais divergência no artefato).

### 3.4 Contrato das tarefas (HH09.4)

Escalonador cooperativo do jogo; evidência no plano: disassembly mais os alvos de longjmp do
trace do HH07.
- A passada é 0x8C1838F8: `setjmp` com retorno 0x8C18392A; vetor de tarefas em +0x210 com passo
  0x124, quantidade em +0x208, estado em +12, prazo em +20 e contador em +0x204.
- Ceder(N) é 0x8C1838B6 (prazo = contador + N; `setjmp` com retorno 0x8C1838E0); 0x8C1838F4 é
  ceder(1).
- A ordem é a do vetor, e não há troca de threads do guest.

Fica no código gerado (D4 do BS: a `ITaskScheduler` não troca threads do guest). O tempo
nativo não interfere: a passada anda pelo contador próprio.

### 3.5 Catálogo para o HH15 (HH09.7)

No código gerado: 2 571 `HH_POLL`, 67 093 `HH_CYCLES`, 265 `HH_IRQ_CHECK` e 7 `HH_WAIT`.

No runtime, os módulos que ainda leem `time_now` (pelo provedor), por número de chamadas:

| Módulo | Chamadas |
|---|---|
| `aica_regs` | 8 |
| `gdrom`, `maple`, `pvr_regs`, `sh4_tmu` | 2–3 cada |
| `holly_intc`, `sh4_intc`, `sh4_service` | 1–2 cada |

INTC, TMU, SPG, RTC e ISTNRM continuam como interface do SDK, simulados só na fase híbrida.
Donos: HH15 (fim do tempo guest), HH13 (áudio) e HH14 (render).

## 4. Comandos de build/teste e resultados

```text
cmake --preset vs && cmake --build --preset vs-release -- -m
ctest --preset vs-release
MSYS_NO_PATHCONV=1 cmake -S . -B build-ww ... "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /EHsc /DHH_WRITE_WATCH=1"
HH_WW_VOLTAS=analysis/hh09/voltas_area1.tsv build-ww/app/Release/hh-run.exe --data ... --vblanks 4700
HH_PROGRESS_TRACE=... hh-run --tempo nativo        (modo sombra)
hh-run --tempo experimental                         (B7/B8)
python -I tests/diff/hh09_tempo.py build/app/Release/hh-run.exe .   (HH_DATA_DIR, HH_AREA1_HASHES)
python -I tools/dev/regressao.py (... portoes_hh09 ...)
```

### 4.1 Regressão integral

Executada no código final, com as correções da revisão (log em
`analysis/hh09/regressao_final.log`):

```text
[ok] build (7 s)
[ok] ctest (15 s)
[ok] portoes_hh05 (8 s)     (tempo nativo estável padrão)
[ok] portoes_hh07 (342 s)
[ok] portoes_hh08 (289 s)
[ok] portoes_hh09 (203 s)   hh09_tempo_tests + hh09_tempo (A1/A2/A3/A5, negativos, B7/B8)
[ok] roteiro_area1 (175 s)  hashes iguais a snapshots/hh06/area1_hashes.txt
[ok] builder (266 s)        instalação com os dois discos; HeadHunter.exe reproduz a referência
regressao completa — fim rc=0
```

## 5. Comparação com referência/oráculo

- O estável é idêntico ao legado em ordem de eventos, quadros e relógios. É o mesmo resultado do
  N2a do BS (A1–A8).
- O experimental difere do legado de propósito: o boot não gasta tempo girando, e o primeiro
  quadro não preto vem no VBlank 5 contra 127. O roteiro, por VBlank, chega à primeira área
  controlável com o HUD (captura conferida).

## 6. Riscos, limitações e trabalho deixado para depois

- **Experimental sem equivalência formal.** B1–B6 (TN, IRQ por VBlank, leituras de arquivo,
  relógios e estado do jogo contra a referência, com lista fechada de exceções) e B11 (decisão
  do jogo pelo TMU) não foram fechados.
  - O experimental fica como opção (`--tempo experimental`), e o padrão é o estável.
  - O fechamento vai para o HH15, como o plano permite.
  - Sem "passos de jogo", um roteiro portável entre modos ainda não existe.
- Consumidores do TMU na camada do jogo (0x8C01FFD8, 0x8C09F5D0): a hipótese de decisão do jogo
  pelo tempo continua não verificada.
- Cobertura: uma rota de 4 700 VBlanks. Rotas novas (filmes completos, ADX, campanha, passagem do
  disco 1 para o 2) podem mostrar esperas não declaradas. O experimental falha alto, com o PC; o
  estável não depende delas.
- A guarda de vivacidade foi calibrada só nessa rota: maior janela de 31 milhões contra limite de
  64 milhões.

## 7. Inventário de dados privados

Ficaram fora do Git:
- `analysis/hh09/`: perfis de voltas, traces de eventos e progresso, capturas e logs;
- `build-ww/` (build de investigação com código gerado);
- `reviews/codex-hh09*`, `reviews/hh09-prompt.md`;
- `game-data/`, `generated/`.

O teste diferencial apaga a própria pasta temporária. Versionados só endereços e contagens.

## 8. Checklist do critério de saída

- [x] Roteiro determinístico sem divergência no estável (A1, A3, A5 idênticos ao legado).
- [x] Nenhum busy-wait novo; as esperas do jogo foram declaradas e o experimental não gira no host.
- [x] Catraca de consumo de ciclos em 0, com teste negativo.
- [x] Base nativa de tempo portada, com testes, e `native/` sem dependência de ciclos.
- [x] Promoção: estável padrão; experimental opcional, com o fechamento no HH15 (seção 6).
- [x] Regressão integral (seção 4.1) e revisão Codex com os achados aceitos corrigidos (seção 9).

## 9. Revisão independente e próximos passos

### 9.1 Revisão Codex

Saída em `reviews/codex-hh09.log`. O revisor confirmou:
- as classificações das esperas: nenhuma das sete é decisão do jogo pelo tempo;
- 0x8C08ED26 e 0x8C16B7CE conferidas no disassembly, com o contador e os bits;
- a tabela de esperas igual ao config;
- a entrega do estável nos mesmos pontos do legado;
- nenhum resto do CRI.

| Achado | Decisão | Correção / motivo | Teste |
|---|---|---|---|
| [MEDIA] Espera LinhaSpg pode passar do timeout do TMU (experimental) | Aceito como limitação | O alvo lógico ainda não inclui o prazo do TMU. Afeta só o experimental (opcional) nos laços do boot com timeout. Vai para o HH15, junto com B1–B6. | — |
| [MEDIA] A2 não verificado antes da promoção | Aceito | `hh09_tempo.py` grava `HH_TIME_TRACE` e confere linha do SPG, entrega ≥ devido e atraso ≤ 4 000. A primeira versão contava os `RenderStart` como precoces; corrigido pelo cabeçalho do trace. | `hh09_tempo`: A2 |
| [MEDIA] Regressão aprovava com o unitário sozinho | Aceito | `portoes_hh09` exige `hh09_tempo_tests` e `hh09_tempo` por nome | regressão |
| [MEDIA] Catraca não varria `transitional/` | Aceito | `runtime_cycle_metrics` varre os dois; lista fechada para o `hardware_wait` | `hh09_cycle_ratchet_classifier` (negativo em `transitional`) |
| [BAIXA] `kVblankWaitPc = 0` deixa contratos e `sem_espera` inoperantes | Aceito | `sem_espera` recusada com mensagem; comentário corrigido (não existe essa espera no HH) | — |
| [BAIXA] Negativos conferem só "alguma divergência" | Rejeitado | O trace de ordem do HH não traz o tipo do evento de hardware (render) nem o instante. Conferir a magnitude exigiria um formato novo. A divergência com rc 0, no artefato do critério, é a regra adotada desde o BS. | — |
| [BAIXA] Endereços do BS em exemplos e testes | Aceito | `mod_api.h` e `test_hh09_tempo.cpp` usam as esperas do HH (0x8C08ED26/20, 0x8C16B7CE) | `hh09_tempo_tests` |

### 9.2 Próximos passos

HH09 marcada `CONCLUIDA` e HH10 (controles nativos) `EM_ANDAMENTO`.
