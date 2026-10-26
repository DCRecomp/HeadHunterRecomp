# HH12 — Plano detalhado: vídeos nativos e moddáveis

Data: 2026-10-25. Validação mínima: **V2**. Equivalente ao N5 do Blue Stinger
(`C:\Source\recomp-BStinger\docs\N5-PLANO.md`, `N5-RELATORIO.md`, MIT, somente leitura), com uma diferença
de base: o Head Hunter não usa Sofdec, e sim um decodificador **4X Movie** embutido no executável.
Preparado durante a regressão do HH11. Endereços sem duas evidências são *hipótese* até o HH12.0.

## Critério de saída (ROADMAP)

Todos os vídeos dos dois discos cobertos e comparados; o caminho legado não é usado no modo nativo.

## Fatos de entrada (`analysis/formatos/RELATORIO-FORMATOS.md`, verificado em 2026-10-24)

- **53 arquivos `.4XM`**: 25 no disco 1 (2 364,4 s), 28 no disco 2 (2 569,5 s). Só `SCENE0.4XM` existe nos
  dois discos, com conteúdo diferente (512 e 513 quadros). Na instalação única (HH08), as variantes do disco 2
  ficam em `disc/_DISCO2`.
- Contêiner RIFF `4XMV` ("Packed with 4xmovie v. 4.5.0.811"); árvore completa validada em 53/53.
- Vídeo: 30,0 fps (52) / 29,976 (1); 480×208→204 (37), 480×272→270 (10), 480×368→360 (5), 480×272 (1);
  148 010 quadros (1 `ifrm` por arquivo, 142 483 `pfrm`, 120 020 `cfrm`); 5 474 quadros sem chunk de vídeo
  (*hipótese*: repetição).
- Áudio: ADPCM 4X estéreo 32 kHz (41), mono 32 kHz (2), PCM 16 bits mono 22 050 Hz (10).
- Executável: `"4XMV"` (0x8C124F7C, 0x8C12509C, 0x8C127694), `"nameinfovtrk"` 0x8C1271E4, `"nameinfostrk"`
  0x8C127554, `"ifrmpfrmsnd_cfrm"` 0x8C127E7C, `"Movies\"` 0x8C13C4D0 + `"Scene%s.4xm"` 0x8C13C4DC. Nenhuma
  string SFD/Sofdec.
- HH09: a espera de filme 0x8C11FC3A (filme 4X f_8C11ECA4, objeto do filme por método virtual, tempo pelo TMU)
  é declarada só no tempo experimental.
- Decisão A11 (mantenedor, 2026-10-24): o **FFmpeg** (LGPL) pode decodificar o 4X Movie no Builder como processo
  externo, nunca ligado ao produto. A distribuição do binário com o Builder é decidida no HH18.

### Primeira leitura do caminho do filme (2026-10-25, durante a regressão do HH11)

- O único literal de `"Scene%s.4xm"` (0x8C13C4DC) está no pool de **0x8C09E904**. *Hipótese*: `PlayScene(nome)`.
  r4 vai para a pilha; `sprintf` (0x8C124748) monta o nome em 0x8C24D514 com o formato e r4; 0x8C11E30C cria
  o objeto do filme (r4 = 0x8C20CD20, r5 = 0x8C149F64).
  - Em seguida chama, pela tabela de literais: 0x8C09EAC8 e seguintes (abrir, tocar, laço, fechar: a conferir).
  - Antes: 0x8C073980/0x8C073870 (pausa de subsistemas?) e 0x8C15B000/0x8C08ED34/0x8C08ED16 (a espera de N
    quadros do HH09).
  - Candidata a fronteira do HH12: a função de vídeo do jogo, como a 8C08B544 do BS.
- `"Movies\"` (0x8C13C4D0) está no pool de 0x8C09E868, a montagem do caminho.

## Subetapas

| ID | Entrega | Saída |
|---|---|---|
| HH12.0 | Inventário do caminho do filme no jogo: quem abre `Scene%s.4xm`, a máquina do decodificador (0x8C11ECA4 e vizinhança), leitura por gdFs (HH08), saída para o PVR (textura/YUV?), áudio (AICA/Audio64?), skip (START), fim, fade, legendas. Fronteira escolhida na API do jogo do filme (como no N5: a função de vídeo do jogo), não nas rotinas internas | tabela do contrato; fronteira reclassificada (`video`) |
| HH12.1 | Conversão offline no Builder: FFmpeg como processo (decodificador 4X → formato revisado: vídeo + áudio em contêiner que o Media Foundation toque, como no BS); manifesto com hash do original e do convertido; os 53 arquivos | `video.manifest`; teste do Builder |
| HH12.2 | Referência independente da fidelidade: quadros do decodificador do jogo (recompilado, legado) contra o convertido, alinhados por quadro; PSNR e erro máximo por canal com limites fixados; áudio comparado por amostras | `hh12_video` V3 |
| HH12.3 | `native/video` (porte do N5: player com relógio lógico injetado) + `transitional/video_adapter` (slot na API do filme do jogo) + `--video legado\|nativo` | V1/V2/V4 |
| HH12.4 | Skip, aspect ratio (as resoluções exibidas acima), áudio e legendas (se o jogo tiver; os CSV de LOCALE têm legendas?) | V5 |
| HH12.5 | Mods por nome do filme (`mods\video`), catálogo inspecionável | V5 |

## Portões (rascunho, do N5)

| # | Critério |
|---|---|
| V1 | Nenhuma entrada no decodificador 4X recompilado durante os filmes no modo nativo |
| V2 | Contrato da API do filme igual ao original (abrir, tocar, skip, fim, ausente), por cenário |
| V3 | Fidelidade: o convertido contra o decodificador do jogo (legado), por quadro, nos 53 arquivos |
| V4 | Sincronia pelo relógio lógico (HH09), determinística nos modos de tempo |
| V5 | Mods e legendas; erros (ausente, corrompido, sem decodificador) com diagnóstico |
| V6 | O produto toca com o CDI removido; `--video legado` exercitado até a promoção |

## Riscos

- **Fidelidade do FFmpeg no 4X:** o decodificador 4xm do FFmpeg pode diferir do embutido no jogo (versão 4.5 do
  codec). Se o V3 mostrar diferença sistemática, a alternativa é decodificar no Builder com o próprio decodificador
  do jogo recompilado (código gerado, em processo de ferramenta). Seria escolha de arquitetura: vira
  `DECISAO_NECESSARIA` se acontecer.
- Rotas: só `SCENE0` aparece no começo do disco 1 (roteiro do HH06?); cobrir os 53 exige rotas de campanha (HH16)
  ou um acionamento direto do filme pela API (diagnóstico de desenvolvimento, como o H1/H2 do BS).
- O tempo experimental tem uma espera de filme declarada; o estável precisa ficar idêntico ao legado até o fim
  do filme.
