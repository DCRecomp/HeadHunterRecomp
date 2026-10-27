# HH13 — Plano detalhado: renderização nativa Vulkan

Data: 2026-10-26. Validação mínima: **V2**. Equivalente ao N7 do Blue Stinger (`C:\Source\recomp-BStinger\docs\
N7-PLANO.md`, seções 1–14 e 25; MIT, somente leitura). Ordem trocada com o áudio (HH14) pelo mantenedor em
2026-10-25: o jogo precisa ganhar desempenho antes de o áudio nativo poder ser testado em tempo real.

## Critério de saída (ROADMAP)

Matriz visual dentro das tolerâncias documentadas e desempenho mínimo comprovado; nenhum PVR2/Holly/VRAM guest no
caminho nativo.

## Ponto de partida (verificado em 2026-10-26)

- `runtime/pvr` do HH é o do BS (diferenças só nos nomes das variáveis de diagnóstico): TA, decodificador das listas,
  rasterizador de referência em software (`pvr_render`), texturas (`pvr_texture`) e o ponto de captura
  `PvrCaptureSink` / `pvr_render_set_capture` / `pvr_render_triangles` já existem.
- `native/render/render.h` do HH é a versão inicial do contrato; o do BS evoluiu no N7 (clip do usuário, fog completo,
  regras do quadro, `ListType` dos volumes, bits do TSP guardados para o backend).
- HH07: o jogo nunca toca o TA; as duas dependências de hardware do jogo (0x8C090974, 0x8C0C4CE0) são store queues
  para buffers na RAM, depois enviados pelo Kamui (dono HH13). O jogo usa Kamui (0x8C159xxx–0x8C16Bxxx; espera do
  render 0x8C16B7CE, HH09).
- HH12: o filme nativo já chega ao host como imagem ARGB (`compose_frame`).

## Fronteira

Como no BS, em duas camadas:
1. **Saída do TA (N7a do BS):** a lista que o decodificador do TA produz vira a IR (`CommandList`); o rasterizador em
   software continua como referência e recuo. Não muda a lógica do jogo, do Kamui nem do tempo.
2. **Backend Vulkan (N7b do BS)** atrás de `IRenderDevice`: a IR desenhada na GPU; `--render software|nativo`.

A troca das entradas do Kamui por provedores nativos (assets pelo Builder, sem TA nem VRAM guest) é a parte do
critério de saída que o BS não fez no N7 (ficou no N8). Fica medida e planejada aqui (HH13.6) e é executada no HH13
se couber; o que ficar é dono do HH15 (remoção do legado), registrado no relatório.

## Subetapas

| ID | Entrega | Saída |
|---|---|---|
| HH13.0 | Inventário: Kamui usado pelo jogo (entradas, contagens, rota da área 1), formatos de textura vistos, listas, volumes, fog, framebuffer direto, resolução e entrelaçamento (640 × 476 do HH12) | tabela no plano |
| HH13.1 | `native/render` do BS (render.h atual, render_ir, texture_convert, frame_prep, texture_replacement, fidelity_scenes) + testes sem GPU | `hh13_render_tests` |
| HH13.2 | `transitional/render_capture` (IR a partir do TA) em modo conferir: a IR descreve os mesmos triângulos, os FrameParams reproduzem os registradores, a imagem só da IR é idêntica, as texturas nativas iguais texel a texel ao legado | `HH_RENDER_IR=conferir` com 0 divergências na rota da área 1 |
| HH13.3 | Backend Vulkan do BS (`native/render/vulkan`, SPIR-V versionado, cabeçalhos Khronos Apache-2.0) + `--render nativo` (imagem do Vulkan no lugar da do software; recuo para o software com o motivo) + modo comparar | PSNR por quadro Vulkan × software |
| HH13.4 | Equivalência: rota da área 1 e telas (logos, título, menus, HUD, carga, filme) Vulkan × software; Vulkan × Flycast rodando livre (como o V3 do HH12) | tabela de PSNR por cena |
| HH13.5 | Desempenho: quadros por segundo de host, software × Vulkan, na rota; promoção a padrão na janela (headless continua software: roteiro de referência) | medição + promoção |
| HH13.6 | Fronteira Kamui: inventário e, onde couber, provedores nativos; o resto com dono HH15 | tabela |

## Portões

| # | Critério |
|---|---|
| R1 | IR válida e igual ao que o rasterizador desenhou (triângulos, estado, quadro, imagem e texturas): 0 divergências |
| R2 | Vulkan × software: PSNR ≥ 40 dB por quadro (o do plano do BS) nas rotas; exceções explicadas por regra de GPU |
| R3 | Ciclo de vida: 0 objetos Vulkan vivos no fim; perda do dispositivo e falta de Vulkan com recuo para o software |
| R4 | Determinismo: duas execuções com o mesmo resultado (captura do alvo offscreen) |
| R5 | Desempenho: o Vulkan mais rápido que o software na rota (medido; o BS saiu de ~17 quadros/s) |
| R6 | Flycast: cenas pareadas dentro do que o software já dá contra o Flycast |
| R7 | Regressão integral; headless e hh-run no software (roteiro de referência) |

## Resultados (2026-10-26)

- **HH13.1:** `native/render` e `native/render/vulkan` do BS (SPIR-V versionado; Vulkan-Headers 1.3.282 Apache-2.0 em
  `third_party/vulkan`); `hh13_render_tests` 36/36 nesta máquina (NVIDIA GeForce RTX 5080), inclusive os casos com GPU.
- **HH13.2 (R1):** `HH_RENDER_IR=conferir` no roteiro da área 1 (4 700 VBlanks): 2 250 renders conferidos, 1 502 983
  desenhos, 4 957 942 triângulos, 39 422 texturas, 34 047 volumes modificadores (sombras), fog por tabela em
  1 291 630 desenhos: IR inválida 0, triângulos 0, imagens 0, texturas 0, quadros 0 divergentes.
  Correção própria: a leitura das texturas lineares com stride menor que a largura (a do filme do jogo, TCW 0E0C2960,
  1024 × 512) passava do fim do buffer (`stride × altura`); `linear_texture_texels`, e a regra linear do
  `texture_source` restrita aos formatos de 16 bits sem mipmap (paleta: o seletor; revisão Codex). Testes
  `hh13_capture_tests` (com mutações conferidas).
- **HH13.3/4 (R2, R3):** `HH_RENDER_IR=comparar` no mesmo roteiro: 2 250 renders, maior diferença 1 nível, nenhum quadro
  abaixo de 40 dB, 0 falhas; recuo para o software e recriação do dispositivo cobertos (`hh13_capture_tests`, portados do
  BS). Os quadros do `--render nativo` (hash a cada 50 VBlanks) são **iguais** aos do software nos 94 pontos: o nativo
  reproduz o roteiro de referência.
- **HH13.5 (R5):** headless sem pacing, roteiro da área 1: software 200–210 s, Vulkan 73–92 s (2,3–2,7×); com o tempo
  experimental, 23,9 s. Na janela (pacing de 59,94 Hz): Vulkan 80,4 s para 78,4 s de tempo real (com a abertura),
  software 84,5 s. GPU por quadro: média 1,15 ms, pior 4,8 ms. Promoção: `--render nativo` padrão na janela; headless e
  `hh-run` no software.
- **R6 (Flycast):** o nativo é igual ao software (≤ 1 nível); a fidelidade do software contra o Flycast é a do HH05
  (quadros do boot conferidos). Uma cena de jogo pareada contra o Flycast rodando livre fica como verificação adicional.
- **HH13.6 (fronteira Kamui):** `config/fronteiras.toml`: 33 entradas do jogo com subsistema render (8 Kamui, 11
  Shinobi, 14 de família desconhecida), 102 pontos de chamada; o jogo não usa Ninja. O caminho nativo ainda lê as listas
  do TA e as texturas da VRAM guest (como o N7 do BS, que deixou isso para o N8): ver o relatório.

## Riscos

- Autosort por pixel das translúcidas (o BS precisou de listas por pixel); volumes modificadores (stencil); fog por
  tabela (o BS deixou sem prova contra o Flycast).
- `DEVICE_LOST` intermitente que o BS viu em AMD (aqui NVIDIA; recuo seguro obrigatório).
- Sem o Vulkan SDK no build: os SPIR-V vêm versionados do BS (gerados por `gera_spirv.ps1`).
