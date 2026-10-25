# HH07 — Plano detalhado: fundação do port nativo e fronteiras mensuradas

Validação mínima: **V2**. Critério de saída (ROADMAP): comportamento permanece igual, custo
medido e toda dependência legada tem dono/fronteira ou bloqueio explícito.

Equivalente ao N0 do Blue Stinger (`docs/N0-RELATORIO.md` e `docs/PLANO-PORT-NATIVO.md` §1–§5
dele). O N0 é sobretudo **análise e medição**; nenhum subsistema muda de provedor aqui.
Regra permanente: recompilar, nunca emular (`AGENTS.md`).

## Reuso do Blue Stinger (MIT)

| Componente BS | Uso no HH07 |
|---|---|
| `translator/hwscan/*` (`bs-hwscan`: varredura estática, `sdkid`, fechamento, métricas, sonda) | Porte como `hh-hwscan`; `sdkid` é genérico (mensagens, banners, blocos, grafo, assinaturas) e recebe as famílias do Head Hunter |
| `tools/dcmap` | Já portado (HH05) |
| `runtime/debug/hw_probe.*`, `event_trace.*` | Já portados (HH05); `HH_HW_PROBE`, `HH_EVENT_TRACE` |
| `transitional/{boundary_registry,handle_table.h,guest_abi.h}` | Porte (modelo de interoperabilidade, handles com geração) |
| `native/` (interfaces tipadas do N0) | Porte só das interfaces independentes de jogo; testes "native limpo" |
| `config/fronteiras_us.toml`, `metricas_base_us.toml` | Formato; conteúdo refeito para o Head Hunter |

## Subetapas

| ID | Entrega | Evidência de saída |
|---|---|---|
| HH07.1 | `hh-hwscan` portado: acessos a hardware com endereço resolvido, literais de registrador, grafo de chamadas, strings | Saídas em `analysis/hh07/` nos dois discos (idênticas) |
| HH07.2 | Identificação do SDK (`sdkid`) com as famílias do HH (Shinobi, Kamui, Ninja se presente, CRI/ADX, 4X Movie, Audio64, BIOS, CRT): camada jogo/SDK com evidências e confiança; nomeação só com duas evidências | `sdk_funcs.tsv`; amostra conferida no disassembly |
| HH07.3 | Fronteira declarada `config/fronteiras.toml`, fechamento e dominância (estático + sonda nos roteiros `newgame_area1`) | 0 violações; caso negativo (fronteira sem uma entrada) falha |
| HH07.4 | Métricas com catraca `config/metricas_base.toml` | Catraca não piora; teste de catraca |
| HH07.5 | Alvos `native`, `legacy`, `transitional` separados; interfaces nativas tipadas sem `Sh4Context`/endereço guest; teste "native limpo" | Teste proíbe `Sh4Context`, `sh4_`, `dcmap` e endereços guest em `native/` |
| HH07.6 | Modelo de interoperabilidade: tabela de handles com geração, ABI guest documentada, registro de fronteiras; slots sempre ativos nas fronteiras migráveis (`[native] slots`) com custo medido | Hashes do roteiro iguais com e sem slots; custo em tempo de quadro |
| HH07.7 | Inventário de threads, reentradas e eventos (`HH_EVENT_TRACE`) e teste de ordem `(quadro, tarefa, callback, evento)` no roteiro | Ordem reproduzível em duas execuções |
| HH07.8 | Produto sem leitura de CDI (mapa do linker do `HeadHunter.exe` sem `CdiImage`) | Teste sobre o mapa do linker |
| HH07.9 | Relatório + revisão Codex | — |

## Bibliotecas do executável (banners, `analysis/hh07/banners.txt`)

| Família | Evidência (banner) |
|---|---|
| Shinobi 1.68 (SEGA) | `gdFs 1.09`, `pd 1.50`, `pdLcd 1.04`, `bu 1.47`, `sy*` (Start, Malloc, Rtc, Tmr, Cache, Chain, Hw, Cfg, Cbl, Bt), `ov` |
| Kamui2 (NEC) | `kamui2 Ver 16,3,4,0` |
| CRI ADX | `ADXT Ver.5.78`, `ADXF`, `ADXSTM` |
| Audio64 10.0.f.2 | `a64File*` sobre `gdFs`, `Audio64::Audio64_QueueLoadEntry`, "Driver file missing 'Audio64.drv'" |
| 4X Movie | sem banner (a identificar por outras evidências) |
| Ninja | não encontrado (diferente do Blue Stinger) |

Consequência para o `sdkid`: famílias Audio64 e 4X no lugar de `som_sd` (Manatee) e, se
confirmado, sem Ninja.

## Fatos de entrada (HH05–HH06)

- Bibliotecas já observadas: driver de GD do jogo (0x8C16E2E8…), fila de comandos de som
  para o driver Audio64 (0x8C122440), CRI ADX (banner na tela de logos), 4X Movie, threads
  cooperativas (setjmp 0x8C131F3C / longjmp 0x8C131E9C), tratadores de exceção em VBR.
- Sonda dinâmica já mostrou acessos a AICA (RAM, RTC, registradores), store queues e G2.

## Primeira medição (preparação, 2026-10-25)

`hh-hwscan` portado e ligado ao build (`hh_hwscan`, `HH_TOOL_ONLY`); saídas idênticas nos dois
discos. Ajustes necessários para o Head Hunter:
- As sementes de "código do jogo" do `sdkid` eram convenções do Blue Stinger (`FT_`, `FI_`...);
  sem semente de jogo, a propagação "chamado pelo SDK é SDK" atravessava a partida
  (`syStart` → `main`) e marcava o executável inteiro como SDK (jogo 0, SDK 4.693). Agora cada
  jogo declara mensagens próprias em `[sdkid] jogo` (aqui: nomes dos fontes do jogo —
  `Script.h`, `Camera.c`, `CollisionTree.c`, `debug.c`, `enemy.c`, `HaltPoint.c`,
  `Pedestrian.c`, `ScriptList.c`, `TransformHero.c` — e os formatos de dados dele).
- Família de som: Audio64 (`a64…`, `Audio64::…`, módulo `Audio64`); rótulo `som_sd` → `som`.

Resultado: 4.699 funções = jogo 2.429, SDK 1.654, indefinidas 615, conflito 1 (partida do
Shinobi chamando o `main`, esperado). Hardware: 648 referências (218 acessos, 430
constantes); 4 referências em funções do jogo, todas **constantes, não acessos**:
0x8C04E984 (`or` com 0xA0000000 converte ponteiro de RAM para P2) e 0x8C0B089E (0xE020E020
passado como argumento). Falsos positivos a tratar no filtro do HH07.1/HH07.3. Fronteira
derivada: 538 entradas (521 diretas, 17 indiretas); fechamento 0 violações de SDK, 744
indiretos não resolvidos (catraca).
