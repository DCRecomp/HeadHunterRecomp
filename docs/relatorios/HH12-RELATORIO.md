# HH12 — Relatório: vídeos nativos e moddáveis

Data: 2026-10-26. Validação mínima do ROADMAP: **V2** (cumprida; regressão integral também executada).
Plano e evidências: `docs/planos/HH12-PLANO.md`. Referência MIT: N5 do Blue Stinger (`C:\Source\recomp-BStinger`).

## 1. Objetivo e escopo realizado

Critério de saída do ROADMAP: todos os vídeos dos dois discos cobertos e comparados; o caminho legado não é usado no
modo nativo.

- **Contrato (HH12.0).** PlayScene 0x8C09E904 (chamadores 0x8C0F0AF4 e 0x8C028332, únicos) é o único usuário do objeto
  do filme 4X: create 0x8C11E30C, open 0x8C11E9D2, trilha de áudio 0x8C11EBE0, play 0x8C11ECA4 (callback de skip
  0x8C09E896), close 0x8C11EB32, destroy 0x8C11E360. Layout do objeto conferido no Flycast na entrada do play
  (+0x0C/+0x10 área exibida, +0x14/+0x18 640 × 480, +0x1C trilhas, trilha 0 em +0x20 com bytes/s, +0x33C continuar).
- **Provedor nativo** (`transitional/video_adapter`, `--video nativo`): PlayScene continua original (como a função de
  vídeo do N5); open/áudio/play/close atendidos sobre os MP4 do Builder. O play repete a ordem do laço original: quadro
  na tela, efeitos por quadro (0x8C14A220, 0x8C2388DC, 0x8C016DF8), callback do jogo, espera do instante seguinte no
  ponto declarado 0x8C11FC3A (tempo pelos VBlanks do jogo), fim do filme ou obj+0x33C zerado pelo skip. O host
  apresenta o quadro do filme no lugar do quadro do PVR (área exibida, escala min(640/w, 480/h), centralizado).
- **Áudio** pelo XAudio2 com controle de fluxo (saída nula no headless e se o dispositivo cair).
- **Legendas do jogo**: o laço original desenha por quadro (0x8C0E4006) a lista carregada pelo jogo
  (`Locale\<idioma>\SubTitle\<filme>.txt`, opção ligada por padrão); o nativo lê a lista da RAM (*0x8C151E10) e desenha
  por DirectWrite. Mods SRT/VTT por nome.
- **Builder** (`builder/video`): o FFmpeg (A11) só decodifica, como processo externo; o Media Foundation codifica
  H.264 + AAC 48 kHz; `data\video\manifest.toml` com quadros, tamanho exibido (vtrk), fins, PSNR, SNR, SHA-256 do MP4 e
  da fonte. `hh-builder --filmes <data>` e `--ffmpeg`.
- **Mods** (`mods\video\<NOME>.mp4`, `<NOME>.D2.mp4` só para a variante do disco 2, legendas `<NOME>.<idioma>.srt`).
- **Promoção**: `--video nativo` padrão na janela; `--headless` e `hh-run` ficam no legado (roteiro de referência e
  verificação do Builder), `--video legado` como reserva.

## 2. Arquivos alterados

- Novos: `transitional/video_adapter.{h,cpp}`; `native/video/{movie_decoder.h,mf_movie.*,movie_playback.*,
  movie_catalog.*,subtitles.*}`, `native/audio/audio_output.cpp`, `native/core/com_scope.h` (portados do N5);
  `builder/video/{fourxm_header,fourxm_convert,movie_build}.*`; `tests/unit/test_hh12_video.cpp`,
  `tests/diff/hh12_video.py`; `tools/dev/{hh_dis.py,hh12_v3.py,hh12_v3_livre.py,hh12_v3_blocos.py,
  hh12_v3_bilinear.py}`.
- Alterados: `app/hh_host.*` (provedor, quadro do filme, `HH_VIDEO_DADOS`), `app/hh_window.cpp`, `app/hh_run.cpp`
  (`--video`), `builder/builder_core.*` (passo dos filmes, `ffmpeg`), `builder/main_cli.cpp`, `config/headhunter.toml`
  (4 slots), `native/audio/audio.h` (IAudioOutput completo), CMake (`hh_native_video`, `hh_native_audio`, testes),
  `tools/dev/regressao.py` (`portoes_hh12`), `tools/dist/LEIA-ME.txt`, `docs/planos/HH12-PLANO.md`, `STATUS.md`.

## 3. Evidências e decisões

- Contrato: `tools/dev/hh_dis.py` (desmontagem do C++ gerado com os literais resolvidos), varredura de chamadores no
  gerado, oráculo do Flycast (`flycast_oracle.py mem 0x8C11ECA4 0x8C74E3A0 64`), despejos de RAM (`HH_RAM_DUMP`).
- Legado: o laço do filme fica preso já no open (o stream do arquivo depende da AICA sem o ARM7); o filme some no
  VBlank 312. O nativo é o primeiro caminho que toca os filmes.
- **Decisão do mantenedor (2026-10-26): opção A**, o FFmpeg continua decodificando (seção 5).
- Revisão Codex (`reviews/codex-hh12.log`): 8 achados. Aceitos e corrigidos: legendas na reprodução, SHA-256 do MP4 no
  open e erro do decodificador separado do fim natural, SHA-256 real da fonte no reaproveitamento (com teste negativo),
  cancelamento do FFmpeg (thread vigia, espera limitada), mod por nome contido na pasta (links/junções), controle de
  fluxo do áudio e troca para a saída nula, evidência do endereço 0x8C142E78 no plano, cobertura dos dois discos no
  portão. O achado ALTA (promoção sem o portão contra o decodificador do jogo) foi levado ao mantenedor e resolvido
  pela decisão A.

## 4. Comandos e resultados

```text
hh-recomp config\headhunter.toml --game-data game-data\hh02\disc1 --analysis analysis\hh04\disc1 --out generated\disc1
cmake --build build --config Release                                   -> ok
ctest --test-dir build -C Release                                      -> 40/40
hh12_video_tests (HH_DATA_DIR=disc1)                                   -> 7/7 (SCENE0: 512 quadros, PSNR med 55,7
                                                                          min 35,8 dB, áudio atraso 0, SNR 41,1 dB)
hh-builder --filmes game-data\runtime\unico                            -> 53 convertidos em 5 min 52 s
tests/diff/hh12_video.py (HH_DATA_DIR=disc1, HH_DATA_DIR2=disc2)       -> 0 falhas:
  disco 1: 25 filmes, quadros == vtrk[4], PSNR med >= 36,92 dB, SNR >= 28,4 dB, atraso 0
  disco 2: 28 filmes, quadros == vtrk[4], PSNR med >= 36,59 dB, SNR >= 29,6 dB, atraso 0
  V0 PlayScene chama o objeto do filme pelo slot
  V1 nativo: 0 entradas no stream e no decodificador 4X; legado: stream criado e aberto
  V2 SCENE0 do VBlank 282 ao fim natural no 1305 (1 023 VBlanks = 512 quadros); START o para (callback do jogo) e
     o SCENE01_06 abre depois; catálogo vazio -> ausente e o jogo segue; MP4 alterado recusado pelo SHA-256; mod
     pelo nome toca pela duração do próprio arquivo; 60 legendas do jogo no SCENE01_06, nenhuma no SCENE0
  V4 duas execuções idênticas; tempo experimental com a mesma duração e os mesmos quadros relativos ao início
python -I tools/dev/regressao.py                                       -> todas ok (analysis/hh12/regressao.log):
  build, ctest, portões HH05, HH07-HH12, roteiro da área 1, Builder (com a conversão dos filmes)
```

## 5. Comparação com a referência/oráculo

- Conversão (MP4 contra o FFmpeg): seção 4.
- **Decodificador do jogo** (Flycast rodando livre, `hh12_v3_livre.py`, 11 instantes do SCENE0): o quadro escolhido
  anda exatamente 30 por segundo (geometria e alinhamento confirmados). Quadros estáticos: PSNR 47,9–53,8 dB, erro
  máximo 9 (quantização da textura ARGB1555 do jogo). Quadros com movimento: mesmo conteúdo, erro médio 3,4–5,0
  níveis, mediana 3, p90 8–11, picos locais (`hh12_v3_bilinear.py`, `hh12_v3_blocos.py`): o 4xm do FFmpeg não é bit a
  bit o decodificador do jogo nos quadros P. Mantido por decisão do mantenedor.

## 6. Riscos, limitações e trabalho deixado para depois

- Diferença de decodificação nos quadros com movimento (seção 5), aceita.
- Fonte e posição das legendas: DirectWrite, não a fonte do jogo (aproximação); o tamanho e a posição foram ajustados
  para a faixa de baixo.
- Amostragem do quadro composto por vizinho mais próximo (o filtro exato do PVR não foi modelado; o HH13 refaz a
  apresentação no Vulkan).
- Volume dos filmes: 1,9 GB em qualidade 100 do H.264 (como o N5); reduzir é ajuste futuro.
- Distribuição do binário do FFmpeg com o Builder: HH18 (sem FFmpeg os filmes ficam "não convertidos" e o jogo os pula).
- A passagem real do disco 1 para o 2 e os filmes do disco 2 em jogo: cobertos pelo catálogo (`_DISCO2`) e pela
  conversão; a rota de campanha é do HH16.

## 7. Dados privados

MP4 convertidos (`game-data/runtime/*/video`, `dist/`, `build/regressao`), capturas e despejos (`analysis/hh12`),
revisão (`reviews/`) e logs ficam em pastas ignoradas (`git check-ignore` conferido em game-data, generated, analysis,
snapshots, reviews, dist, ISO, build). Nenhum byte do jogo, quadro, legenda ou texto do jogo foi versionado; os
documentos citam só endereços, contagens e métricas.

## 8. Checklist de saída

- [x] Inventário do caminho do filme e contrato com duas evidências (desmontagem, grafo de chamadas, oráculo).
- [x] Conversão offline no Builder dos 53 filmes (25 + 28), com manifesto e reaproveitamento seguro.
- [x] Comparação: MP4 contra a fonte (todos) e contra o decodificador do jogo (Flycast, SCENE0); diferença decidida.
- [x] Provedor nativo sem o decodificador 4X, o stream do arquivo nem a AICA (V1).
- [x] Sincronia, skip, aspect ratio, áudio e legendas (V2, V4).
- [x] Mods por nome e catálogo inspecionável.
- [x] Promoção na janela; `--video legado` como reserva; headless no legado.
- [x] Revisão Codex registrada; regressão integral ok.

## 9. Próximos passos

HH13 (renderização nativa Vulkan) marcado `EM_ANDAMENTO` e iniciado automaticamente.
