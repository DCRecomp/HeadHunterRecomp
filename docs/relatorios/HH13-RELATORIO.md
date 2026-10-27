# HH13 — Relatório: renderização nativa Vulkan e provedor nativo da camada Kamui

Data: 2026-10-27. Validação mínima do ROADMAP: **V2** (cumprida; regressão integral também executada).
Plano e evidências: `docs/planos/HH13-PLANO.md`. Referência MIT: N7 do Blue Stinger (`C:\Source\recomp-BStinger`).

## 1. Objetivo e escopo realizado

Critério de saída do ROADMAP: matriz visual dentro das tolerâncias documentadas e desempenho mínimo comprovado; nenhum
PVR2/Holly/VRAM guest no caminho nativo.

- **IR e backend Vulkan (HH13.1–HH13.5, commit 20c49a8).** `native/render` e `native/render/vulkan` do N7 do BS
  (SPIR-V versionado, Vulkan-Headers 1.3.282 em `third_party/vulkan`), `transitional/render_capture` (a saída do
  decodificador do TA vira a IR), `--render software|nativo`. R1–R5 no roteiro da área 1 (seção 4).
- **Provedor nativo da camada Kamui (HH13.6, decisão B do mantenedor, 2026-10-27).** Todo o caminho de render do jogo
  até o hardware passa por cinco funis do SDK (0x8C1793E0 escrita de registrador do PVR, 0x8C1793EA leitura,
  0x8C17C600 pedido de DMA, 0x8C159820 cópia, 0x8C159874 zeragem). `transitional/kamui_device`:
  - **sombra** (`HH_KAMUI=sombra`): os originais continuam e um estado nativo é conferido com o do runtime em cada
    STARTRENDER (registradores, parâmetros do TA, memória);
  - **nativo** (`HH_KAMUI=nativo`, padrão de `--render nativo`): os funis deixam de tocar o PVR; registradores no banco
    nativo com a semântica de comando do runtime; os bytes que o DMA entrega à área 4 vão ao destino nativo
    (`ta_set_sink`), com as interrupções de fim de lista; cópia e zeragem só na memória nativa; o render de STARTRENDER
    e o quadro exibido leem o estado nativo emprestado (`pvr_regs_set_override`, `vram_set_override`,
    `ta_set_override`). O slot segue o tempo do código gerado do original (ciclos por bloco, atendimento no back-edge).
- **Promoção:** `--render nativo` (Vulkan + Kamui nativo) padrão na janela; `--headless` e `hh-run` no software com o
  caminho legado (roteiro de referência); `HH_KAMUI=legado|sombra|nativo` escolhe explicitamente.

## 2. Arquivos alterados

- HH13.1–5 (20c49a8): `native/render/*`, `native/render/vulkan/*`, `third_party/vulkan`, `third_party/README.md`,
  `transitional/render_capture.*`, `tests/unit/test_hh13_*.cpp`, `tests/diff/hh13_render.py`, `app/hh_host.*`,
  `app/hh_window.cpp`, `app/hh_run.cpp`, CMake, `tools/dev/regressao.py` (`portoes_hh13`).
- HH13.6 passo 1 (18aa90b): `transitional/kamui_device.*` (sombra), diagnósticos `HH_FN_PERFIL`, `HH_WATCH_MAX`,
  `HH_TA_FONTES` (`runtime/pvr/ta.cpp`, `pvr_regs.cpp`, `vram.cpp`, `runtime/memory/sh4_dmac.cpp`, `sh4_sq.cpp`),
  `tools/dev/hh13_kamui_api.py`, `config/headhunter.toml` (5 slots).
- HH13.6 passos 2–5 (este commit): `transitional/kamui_device.*` (modo nativo, ciclos, `HH_KAMUI_CUSTO`),
  `runtime/pvr/pvr_regs.*` (`pvr_start_render`, `pvr_regs_set_override`), `runtime/pvr/vram.*` (`vram_set_override`),
  `runtime/pvr/ta.*` (`ta_set_override`, `ta_set_sink`), `app/hh_host.cpp` (seleção e quadro nativo),
  `tests/unit/test_hh13_kamui.cpp`, `tests/diff/hh13_kamui.py`, `tests/CMakeLists.txt`, `tools/dev/regressao.py`,
  `docs/planos/HH13-PLANO.md`, `STATUS.md`, este relatório.

## 3. Evidências e decisões

- **Funis (passo 1):** sonda `HH_TA_FONTES` no roteiro: o fluxo para o TA só chega pelo DMA de 0x8C17C600 (tipos 0, 1
  e 2; vale o destino na área 4), a VRAM pela CPU só pela cópia e pela zeragem, os registradores do PVR só pelos dois
  funis (as outras leituras, 0x8C16EF18/0x8C16E908/0x8C1696C0/0x8C16A5A0, são SPG_STATUS: tempo e Maple). A sombra
  conferiu 2 250 renders, 350 401 escritas, 8 864 155 parâmetros e 562 000 blocos de memória: 0 divergências.
- **Dependências de hardware declaradas do jogo** (`0x8C090974`, `0x8C0C4CE0`, dono HH13): confirmadas como store
  queues para buffers na RAM (0x0C400000–0x0C610000); esses buffers só chegam ao TA pelo DMA de 0x8C17C600, que no modo
  nativo vai ao destino nativo. As store queues para a RAM (QACR, `pref`) ficam no runtime até o HH15.
- **Tempo guest:** o slot nativo reproduz o custo do código gerado substituído (escrita 5 ciclos, leitura 3, laços de
  cópia/zeragem por unidade com os mesmos ciclos e o mesmo ponto de atendimento), conferido com `HH_KAMUI_CUSTO`
  (2 098 360 cópias, 2 zeragens, 350 401 escritas, 202 335 leituras). Precedente no BS: o `sound_adapter` gasta o tempo
  do original por `hardware_wait`. Prova negativa: sem o gasto, 2 260 renders contra 2 250 e hashes divergentes a partir
  do VBlank 150. É dependência híbrida catalogada para o HH15 (fechamento do tempo), não uma espera nova.
- **Divisão HH13/HH15:** o HH13 tira do caminho nativo o TA, os registradores de render do PVR e a VRAM guest; a
  entrega das interrupções (INTC do Holly), o SB do Holly, o DMAC e os registradores de tempo do vídeo (SPG) saem no
  HH15. O código de decodificação/render ainda mora em `runtime/pvr` e é usado com o estado nativo emprestado; movê-lo
  para `native/` é do HH15.
- **Revisão Codex** (`reviews/codex-hh13.log`, `reviews/codex-hh13_6.log`; aceitos e rejeitados também no `STATUS.md`). HH13.1–5: 7 achados,
  todos aceitos e corrigidos (regra de paleta do `texture_source`, determinismo R4 e R2 completos, underflow com
  `h == 0`, testes determinísticos com mutação, SKIP e R5 mais estritos, resumo sempre, atribuição). HH13.6: 4 achados,
  todos aceitos: laços de cópia/zeragem com os pontos de atendimento do original e `pc` preservado em `redirect`;
  leitura gasta os ciclos antes de ler; escrita devolve `r0 = 1`; portões mais fortes (referência do HH06 obrigatória
  na regressão, K4 conferindo a IR com o provedor nativo).

## 4. Comandos e resultados

```text
hh-recomp config\headhunter.toml --game-data game-data\hh02\disc1 --analysis analysis\hh04\disc1 --out generated\disc1
cmake --build build --config Release                                   -> ok
hh13_render_tests -> 36/36 (GPU RTX 5080); hh13_capture_tests -> 16/16; hh13_kamui_tests -> 3/3
tests/diff/hh13_render.py (roteiro newgame_area1, 4 700 VBlanks)       -> 0 falhas
  R1 conferir 1 a cada 5: 450 renders, IR invalida 0, triangulos/imagens/texturas/quadros 0 divergentes
  R2 Vulkan x software: 2 250 de 2 250 renders, maior diferenca 1 nivel, 0 quadros < 40 dB, 0 falhas
  R4 duas execucoes do nativo (Vulkan + Kamui nativo) iguais entre si e ao software legado (94 hashes)
  R5 Vulkan em 2 250 de 2 250 renders; software 165,5 s, nativo 70,5/65,4 s (headless, sem pacing)
HH_KAMUI=sombra HH_KAMUI_CUSTO=<tsv> hh-run ... --render software      -> 0 divergencias; custos por chamada
tests/diff/hh13_kamui.py (HH_EXIGIR_DADOS=1)                           -> OK:
  K1 sombra: 2 250 renders, 350 401 escritas, 8 864 155 parametros, 562 000 blocos: 0 divergencias
  K2 nativo: 94 hashes iguais ao legado e a snapshots/hh06/area1_hashes.txt; renders nativos 2 250 = tempo
  K4 conferir com o estado nativo: 450 renders, 7 861 texturas, 0 divergencias
  K3 HH_TA_FONTES: nada em ta_*/vram_*; pvr_* so 0x0C8-0x0E0 (escritos 1x por 0x8C169FE0), 0x0DC e SPG_STATUS
     lidos (tempo/Maple); kamui_poligonos 301 857 856 bytes, kamui_textura 10 941 248 bytes
prova negativa (sem o gasto de ciclos nos slots)                        -> 2 260 renders, hashes divergem no VBlank 150
python -I tools/dev/regressao.py                                       -> todas ok (analysis/hh13/regressao.log):
  build, ctest, portoes HH05, HH07-HH13 (hh13: 5/5 ctest), roteiro da area 1, Builder
```

## 5. Comparação com a referência/oráculo

- Vulkan × software (R2): maior diferença 1 nível em 2 250 renders, nenhum quadro abaixo de 40 dB.
- Kamui nativo × legado (K2): os 94 hashes do roteiro iguais e iguais à referência do HH06; com o Vulkan (R4), iguais
  aos do software legado.
- Software × Flycast: a fidelidade do rasterizador de referência é a do HH05 (quadros do boot conferidos). Uma cena de
  jogo pareada contra o Flycast rodando livre fica como verificação adicional (R6).

## 6. Riscos, limitações e trabalho deixado para depois

- HH15: INTC do Holly, SB, DMAC, SPG, store queues para a RAM, o código de `runtime/pvr` (usado com estado emprestado)
  e o custo em ciclos dos slots nativos saem com o fechamento do tempo.
- Autosort por pixel das translúcidas, volumes modificadores e fog por tabela: conferidos contra o software (R1/R2),
  não contra o Flycast em cenas de jogo.
- `DEVICE_LOST` (visto pelo BS em AMD): recuo para o software coberto por teste; aqui só NVIDIA (RTX 5080).
- Retorno `r0 = 1` da escrita de registrador: reproduzido, mas nenhum chamador do roteiro o lê (sem prova dinâmica).
- Conversor YUV: o jogo não usa; no modo nativo é erro fatal explícito.

## 7. Dados privados

Logs e TSVs de sonda (`analysis/hh13`), dados de execução (`game-data/`), código gerado (`generated/`), capturas BMP,
revisões (`reviews/`) e instalações (`dist/`, `build/regressao`) ficam em pastas ignoradas (`git check-ignore`
conferido). Nenhum byte do jogo, quadro, textura ou parâmetro do TA foi versionado; os documentos citam só endereços,
contagens e métricas.

## 8. Checklist de saída

- [x] IR do TA validada contra o rasterizador (R1) e Vulkan dentro da tolerância (R2), recuo e recriação (R3).
- [x] Determinismo e equivalência dos hashes (R4); desempenho (R5).
- [x] Funis do SDK gráfico inventariados e cobertos (sombra, 0 divergências).
- [x] Modo nativo sem TA, registradores de render do PVR nem VRAM guest (K3), hashes iguais ao legado e ao HH06 (K2),
      IR conferida com o estado nativo (K4).
- [x] Promoção na janela; headless e `hh-run` no legado; seleção explícita por `HH_KAMUI`.
- [x] Revisão Codex registrada; regressão integral ok.

## 9. Próximos passos

HH14 (áudio nativo) marcado `EM_ANDAMENTO` e iniciado automaticamente.
