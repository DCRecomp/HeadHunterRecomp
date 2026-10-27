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

## Decisão do mantenedor (2026-10-27): opção B

Os provedores nativos da camada Kamui ficam no HH13 (não no HH15): o caminho nativo deixa de passar pelo TA, pelos
registradores do PVR/Holly e pela VRAM guest. HH13.6 vira a próxima subetapa, com o inventário abaixo como ponto de
partida e o render atual (IR do TA, R1–R5) como referência de equivalência.

## HH13.6 — inventário da camada Kamui (2026-10-27)

Ferramentas: `HH_FN_PERFIL=<tsv>;<ini>-<fim>` (contagem por função e ponto de retorno; `hh_host`),
`tools/dev/hh13_kamui_api.py` (API do SDK chamada pelo jogo), `--watch` com `HH_WATCH_MAX` (argumentos), a sonda de
hardware do HH07 (`analysis/hh07/sonda_area1.tsv`). Roteiro da área 1.

**API gráfica usada pelo jogo** (78 funções do SDK chamadas por código do jogo; as de render, com a identificação por
argumentos contra as assinaturas do Kamui2, a confirmar com a segunda evidência):

| Função | Chamadas | Hipótese |
|---|---|---|
| 0x8C159820 | 334 181 | cópia para endereço fixo (FIFO do TA / caminho de texturas): `r4` destino fixo, `r5` origem, `r6` bytes, em pares de 64 bits |
| 0x8C15A860 | 334 482 | função vazia (`rts`) |
| 0x8C15982A | 136 656 | entrada no meio de 0x8C159820, usada pelo caminho por store queue (0x8C0C4CE0, HH07) |
| 0x8C159FC0, 0x8C159D80, 0x8C159E20, 0x8C15A1E0, 0x8C15A220, 0x8C15A3C0 | 2 253–2 254 (1 por quadro) | ciclo de cena/passe/render/flip; 0x8C20CD20 é o descritor do vertex buffer, 0x8C31F088 o estado global do Kamui |
| 0x8C15ACA0 `(desc, w, h, tipo)` | 248 | kmCreateTextureSurface |
| 0x8C15AD60 `(desc)` | 46 | liberação de textura (o filme libera as duas) |
| 0x8C15AE00 `(desc, origem, deslocamento, 0x2000)` | 768 | carga de textura em partes (quadros do filme) |
| 0x8C15A780 `(ctx, flags)` | 6 | geração dos contextos de strip |
| 0x8C15A900, 0x8C15A880 `(vb, …)` | 310, 1 240 | início de strip / bloco de vértices (escrevem QACR e store queue) |

**Funis de hardware** (sonda do HH07): toda escrita de registrador do PVR passa por **0x8C1793E0** `(off, val)`:
`*(0xA05F8000 + off) = val` (350 401 escritas); leitura por **0x8C1793EA**; o DMA dos buffers da RAM para o TA por
**0x8C17C600** (DMAC do SH-4 + `SB_C2DST`/`SB_C2DLEN`/`SB_C2DST` do Holly); escrita na VRAM/FIFO por 0x8C159820 e
zeragem por 0x8C159874; store queue com QACR para o TA por 0x8C159E20/0x8C16BAA0/0x8C15A880/0x8C15A900. A geometria do
motor do jogo (0x8C010744, 0x8C011C18, 0x8C010924/946, 0x8C010A98) vai por store queue para **buffers na RAM**
(0x0C400000–0x0C610000; QACR por 0x8C090974) e só chega ao TA pelo DMA. O tratador de interrupções do Holly
(0x8C168160, pelo despachante 0x8C180CFC) também atende o VBlank e o resto: a entrega das interrupções é do HH15
("remover ciclos, IRQ e polling guest"); no HH13 o fim do render continua chegando ao jogo pelo caminho de eventos do
HH09.

**Desenho do provedor** (verificável a cada passo contra o caminho atual):
1. **Fluxo de parâmetros:** os funis que alimentam o TA (DMA, cópia para a FIFO, store queue do Kamui) entregam os
   mesmos bytes a um consumidor nativo, a partir dos argumentos e da RAM do jogo. Prova: o fluxo do consumidor é igual,
   byte a byte e por quadro, ao que o TA do runtime recebe.
2. **Parser nativo** dos parâmetros do TA (`native/render`, só formato) → IR. Prova: a IR é igual à do
   `render_capture` (R1) render a render.
3. **Estado do quadro** a partir das escritas do funil de registradores (fundo, fog, clip, ISP, início do render, endereços
   de quadro) → `FrameParams`. Prova: igual aos `FrameParams` do `render_capture`.
4. **Texturas** a partir das cargas (FIFO de texturas, cargas em partes, zeragem) → armazenamento nativo pelo endereço que
   o Kamui alocou. Prova: texel a texel igual à conversão a partir da VRAM.
5. **Troca:** os funis viram slots nativos (o TA, os registradores do PVR e a VRAM guest deixam de ser tocados); o render
   vai ao Vulkan; o fim do render vira evento do provedor de tempo. Prova: R1–R5 com o caminho novo e a sonda de hardware
   sem acessos a TA, PVR e VRAM no modo nativo.

### Passo 1 — modo sombra (2026-10-27): concluído

`transitional/kamui_device` (`HH_KAMUI=sombra`), slots dos cinco funis em `config/headhunter.toml`. Diagnósticos:
`HH_TA_FONTES=<tsv>` (bytes por destino, caminho e função; por registrador do PVR), `HH_KAMUI_LOG`, `HH_KAMUI_ORDEM`.

- O fluxo para o TA só chega pelo DMA de 0x8C17C600 (tipos 0, 1 e 2; vale o destino do descritor, na área 4); a VRAM
  pela CPU só por 0x8C159820 (cópia) e 0x8C159874 (zeragem); os registradores do PVR só por 0x8C1793E0/0x8C1793EA (as
  outras leituras, 0x8C16EF18/0x8C16E908, são do Maple: SPG_STATUS, tempo).
- O pedido de tipo 1 escreve TA_LIST_INIT pelo próprio funil antes de transferir: a sombra espelha os dados depois da
  chamada original (antes, perdia o primeiro global de cada render).
- Roteiro da área 1 (`analysis/hh13/kamui/sombra_area1.log`): 2 250 renders; 350 401 escritas de registrador, 8 864 155
  parâmetros e 562 000 blocos de memória conferidos com o runtime antes de cada STARTRENDER, **0 divergências**.

### Passos 2–5 — modo nativo (2026-10-27)

`HH_KAMUI=nativo` (e o padrão de `--render nativo`; o software continua no caminho legado, `HH_KAMUI=legado|sombra|nativo`
escolhe explicitamente). Os funis viram slots nativos:

- registradores no banco nativo (2 048 palavras, iniciado do runtime na instalação), com a semântica de comando do
  runtime (TA_LIST_INIT reinicia os parâmetros e copia NEXT_OPB_INIT/ISP_BASE, TA_LIST_CONT, STARTRENDER, SOFTRESET e os
  somente leitura); só os de tempo do vídeo (SPG, 0x0C8–0x0E0) seguem para o runtime e SPG_STATUS é lido dele (HH15);
- o DMA original continua programando o DMAC e o SB do Holly (HH15), mas os 32 bytes que chegam à área 4 vão ao destino
  nativo (`ta_set_sink`): parâmetros divididos como o TA divide, com as interrupções de fim de lista (o mesmo bit de
  ISTNRM que o TA levantava), e o caminho de textura na memória nativa; YUV é erro fatal (o jogo não usa);
- cópia e zeragem na VRAM só na memória nativa;
- STARTRENDER chama `pvr_start_render` (decodificação, IR, Vulkan ou software, agendamento do fim do render) com o
  estado nativo emprestado (`pvr_regs_set_override`, `vram_set_override`, `ta_set_override`); o host lê o quadro exibido
  do estado nativo (`kamui::video_frame`). Tirar esse código de `runtime/pvr` é do HH15.

Tempo guest: o slot nativo gasta os ciclos que o código gerado do original gastaria (escrita de registrador 5, leitura 3,
cópia e zeragem por bytes e alinhamento — fórmulas dos `HH_CYCLES` de `generated/`, conferidas com a medição
`HH_KAMUI_CUSTO` no roteiro: 2 098 360 cópias, 2 zeragens, 350 401 escritas, 202 335 leituras). Cópia e zeragem
escrevem na mesma unidade do laco original (8/4/2/1 bytes, o mesmo valor literal na zeragem), com os mesmos ciclos por
volta e o atendimento dos eventos no back-edge com o pc de retomada do laço (como `HH_POLL`): uma interrupção ou troca de
thread no meio de uma carga vê o mesmo progresso. A leitura gasta os 3 ciclos antes de ler (SPG_STATUS) e a escrita
devolve `r0 = 1` como o original (revisão Codex). Prova negativa: sem esse gasto o jogo anda mais depressa (2 260 renders contra
2 250) e os hashes divergem a partir do VBlank 150; com ele, os 94 hashes são iguais aos do legado e à referência do HH06.

Portão `tests/diff/hh13_kamui.py` (ctest `hh13_kamui`): K1 sombra sem divergências; K2 nativo com os 94 hashes iguais ao
legado e ao HH06, renders nativos = renders do tempo; K3 sonda `HH_TA_FONTES` sem bytes no TA nem na VRAM do runtime e,
do PVR, só os registradores de tempo do vídeo (0x0C8–0x0E0 escritos uma vez por 0x8C169FE0; SPG_STATUS e 0x0DC lidos
pelo tempo e pelo Maple); K4 `HH_RENDER_IR=conferir` com o provedor nativo (IR do estado nativo igual ao rasterizador,
render a render). A referência do HH06 é obrigatória na regressão. Unitário `hh13_kamui_tests`: ciclos medidos e dos
caminhos do código gerado.

Divisão entre o HH13 e o HH15 (do tratador de interrupções 0x8C168160: lê ISTERR/ISTNRM, reconhece o bit e, no evento 18
— fim do DMA —, o CHCR2 do DMAC do SH-4): o HH13.6 tira do caminho nativo o TA, os registradores **de render** do PVR e
a VRAM guest; a entrega das interrupções (INTC do Holly), os registradores do SB do Holly, o DMAC e os registradores de
tempo do vídeo (SPG, usados pelo tempo e pelos controles) saem no HH15, com as interrupções.

## Riscos

- Autosort por pixel das translúcidas (o BS precisou de listas por pixel); volumes modificadores (stencil); fog por
  tabela (o BS deixou sem prova contra o Flycast).
- `DEVICE_LOST` intermitente que o BS viu em AMD (aqui NVIDIA; recuo seguro obrigatório).
- Sem o Vulkan SDK no build: os SPIR-V vêm versionados do BS (gerados por `gera_spirv.ps1`).
