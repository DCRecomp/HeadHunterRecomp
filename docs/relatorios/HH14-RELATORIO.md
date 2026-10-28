# HH14 — Relatório: áudio nativo e moddável

Data: 2026-10-27. Validação mínima do ROADMAP: **V2** (cumprida; regressão integral também executada).
Plano e evidências: `docs/planos/HH14-PLANO.md`. Referência MIT: N6 do Blue Stinger (`C:\Source\recomp-BStinger`).

## 1. Objetivo e escopo realizado

Critério de saída do ROADMAP: matriz de cenas dos dois discos dentro das tolerâncias; nenhum acesso AICA alcançável no modo
nativo. O driver `AUDIO64.DRV` (ARM7) só foi lido estaticamente e nunca é executado.

- **Inventário** (`tools/dev/hh14_inventario.py`): músicas em CRI ADX (21 + 17 streams); falas e efeitos em WAV de
  ADPCM de 4 bits da AICA (streams e amostras dos pacotes AHFF: 368 / 307 nomes, 77 / 70 com conteúdo diferente entre
  pacotes); reverb `HHREV.FPB/FOB` (SFPB/SFOB), igual nos dois discos.
- **Fronteira:** os funis do Audio64 até a AICA (fila de comandos ao driver, acesso G2 à RAM de som, limpeza do DSP,
  leitura de posição, início do DMA do G2). Nomes por mensagens de erro; segunda evidência: o opcode que cada função envia
  existe na tabela de despacho do driver (`tools/dev/hh14_ac_opcodes.py`, `tools/dev/hh14_drv.py` com Capstone 5.0.7
  fora do produto). O motor de som do jogo e o CRI ADX continuam recompilados.
- **Driver reconstruído** (`transitional/audio_device`): porta = canal da AICA; a sombra de cada canal, os comandos usados
  pelo jogo, o boot (versão e MVOL), as tabelas que o jogo lê (posição, eventos, anel de mensagens), a interrupção ao SH-4
  pelo INTC do Holly, o DMA do G2 pela fila do Audio64.
- **Motor nativo** (`native/audio/aica_channels` + núcleo portado do N6 do BS: ADPCM, mixer, WAV, DSP): 64 canais lendo a
  RAM de som enquanto tocam, LEA exclusivo, ganhos/pan da AICA, reverb com o programa do jogo; saída XAudio2 na janela.
- **Mods:** arquivos do disco em `mods\assets` (HH08) e `hh-builder --audio-mod` (WAV PCM → stream ADPCM do jogo) para
  dublagem; `docs/MODS-AUDIO.md`. Diagnóstico por evento: `HH_AUDIO_LOG`, `HH_AUDIO_LOG_PORTA`, `HH_AUDIO_WAV`.
- **Promoção:** `--audio nativo` padrão na janela; `--headless` e `hh-run` no legado (roteiro de referência, gravado com o
  som mudo: sem o ARM7 a fila de comandos enche no VBlank 56).

## 2. Arquivos alterados

- Novos: `transitional/audio_device.{h,cpp}`; `native/audio/{aica_channels,aica_adpcm,aica_dsp,mixer,sound_file}.*`,
  `native/audio/envelope.h`, `native/audio/sound_mod_codecs.cpp`; `tests/unit/test_{audio_native,audio_dsp,hh14_audio,
  hh14_channels}.cpp`, `tests/diff/hh14_audio.py`; `tools/dev/{hh14_inventario,hh14_drv,hh14_ac_opcodes,hh14_estereo}.py`;
  `tools/dist/roteiros/newgame_area1_nativo.txt`; `docs/planos/HH14-PLANO.md`, `docs/MODS-AUDIO.md`, este relatório.
- Alterados: `app/hh_host.*` (`AudioProvider`, instalação, bombeamento no VBlank), `app/hh_run.cpp`, `app/hh_window.cpp`
  (`--audio`), `runtime/pvr/holly_intc.*` (fonte externa da AICA), `transitional/gdfs_adapter.*` (`gdFsSetDir`, log de
  aberturas), `transitional/CMakeLists.txt`, `native/CMakeLists.txt`, `tests/CMakeLists.txt`, `builder/main_cli.cpp`,
  `builder/CMakeLists.txt`, `config/headhunter.toml` (slots), `tools/dev/regressao.py` (`portoes_hh14`), `STATUS.md`.

## 3. Evidências e decisões

Ver o plano (seções "Fronteira" e "Fases D–F"). Pontos decisivos:
- **LEA exclusivo:** o CRI programa LEA = tamanho do anel e copia o início depois do fim; com LEA inclusivo a música do
  título perdia o estado ADPCM na primeira volta (correlação L/R 0,83 → 0,28; lado esquerdo RMS 9 390 contra 1 532).
  Teste `hh14_channels_adpcm_long_ring` (falha com a mutação inclusiva).
- **Cópia RAM de som → RAM de som:** o jogo transmite amostras longas residentes na RAM de som para um anel
  (`acG2Write(0xA0834040, 0xA0895C48, …)`); lendo a origem na RAM legada (zerada) o ADPCM derivava até saturar (225 252
  amostras saturadas no roteiro; 1 079 depois).
- **MultiPlay:** a palavra em +4 cobre as portas 63..32 (a música estava indo para 46/47 em vez de 14/15).
- **DMA do G2:** concluído pela fila do próprio Audio64 (o tratador do jogo grava 802 e chama o callback); a tentativa de
  concluir no `acTransfer` (804) travava o carregador de amostras — 804 é a falha, 802 o sucesso.
- **`gdFsSetDir`:** o jogo torna `\Streams` o diretório corrente antes de cada stream; sem ele no provedor nativo os
  streams eram procurados em `\MOVIES` e o CRI tocava silêncio.
- **Oráculo:** Flycast com o driver real — sombra das portas da música igual à nativa; saída livre como referência de
  nível (música 1 400–2 400 RMS, sem saturação).
- **Revisão Codex** (`reviews/codex-hh14.log`), 5 achados:
  - aceito: Stop/MultiStop — o handler limpa KYONB/LPCTL no registrador do canal (a sombra fica) e o bit de evento; a
    passada seguinte desativa a porta pelo LPCTL do canal. O nativo olhava o laço da sombra (porta em laço parada ficava
    "ativa"); agora guarda o LPCTL vivo;
  - aceito: a limpeza dos registradores do DSP (0x8C12234C) desliga o programa ativo do motor;
  - aceito: `--audio-mod` e `encode_aica_wav` recusam WAV com mais de 2 canais (erro explícito);
  - aceito em parte: portão só com o disco 1 e sem comparação da saída — a regressão passa a rodar os unitários de áudio
    também com os streams e o reverb do disco 2 (`HH14_STREAMS2`/`HH14_REVERB2`); cenas do disco 2 dependem da campanha
    (HH16, como no HH08); a comparação byte a byte do anel com o oráculo não vale (o nativo enche o anel antes do primeiro
    key-on — leitura de arquivo e transferências na hora —, o Flycast ainda com 1 600 bytes de silêncio de pré-carga:
    tempo, não conteúdo), então a equivalência fica na sombra do driver (A5) e no nível (A4);
  - rejeitado: "LEA do Open = tamanho − 1 perde uma amostra" — é o próprio driver que grava `tamanho − 1`
    (`sub r2, r2, #1` em 0x3AFC) e, com LEA exclusivo, o hardware real também não toca a última amostra: o nativo
    reproduz o original.

## 4. Comandos e resultados

```text
build/translator/Release/hh-recomp.exe config/headhunter.toml --game-data game-data/hh02/disc1 --analysis analysis/hh04/disc1 --out generated/disc1
cmake --build build --config Release                                   -> ok
hh14_audio_tests (HH14_STREAMS, HH14_REVERB; discos 1 e 2)              -> 29/29
tests/diff/hh14_audio.py (HH_EXIGIR_DADOS=1, HH14_ORACULO)              -> OK (A1-A5)
python tools/dev/flycast_oracle.py mem 0x8C121156 0xA080156C 0x90 --hit 1 --depois 0x8C02B292   -> sombra das portas 14/15
python tools/dev/flycast_oracle.py snap-free ... (SDL_AUDIODRIVER=disk) -> 294 s de saida do Flycast
python -I tools/dev/regressao.py (analysis/hh14/regressao.log)        -> regressao completa, rc 0:
  build ok (6 s); ctest ok (20 s); portoes_hh05 ok (9 s); portoes_hh07 ok (409 s); portoes_hh08 ok (470 s);
  portoes_hh09 ok (215 s); portoes_hh10 ok (772 s); portoes_hh11 ok (449 s); portoes_hh12 ok (118 s);
  portoes_hh13 ok (1 579 s); portoes_hh14 ok (339 s); roteiro_area1 ok (165 s); builder ok (228 s)
```

## 5. Comparação com a referência/oráculo

- Driver: sombra das portas 14/15 da música do título idêntica à do driver real no Flycast (A5).
- Nível: música do título nativa L 1 809 / R 1 678 RMS; Flycast (filme e noticiário) 1 400–2 400 RMS. O gameplay nativo
  (6 922 RMS, música + ambientes + efeitos) não tem trecho correspondente no Flycast sem entrada.
- Formatos: os 74 + 44 WAV dos discos lidos (mono e estéreo) e reconvertidos com o mesmo cabeçalho.

## 6. Riscos, limitações e trabalho deixado para depois

- **Disco 2 em jogo e matriz de cenas:** a instalação junta os dois discos; cenas do disco 2 exigem avançar a campanha
  (HH16). Coberto no HH14: os formatos do disco 2 e o mesmo driver/motor; a matriz de cenas completa fica com a campanha.
- **Gameplay contra o Flycast:** sem roteiro reproduzível no Flycast até a área (como no N6 do BS); nível de gameplay só
  pela ausência de saturação e pela soma dos canais.
- Envelope, filtro e LFO da AICA não modelados (o driver os deixa em repouso; nenhum comando usado os muda).
- Relógio da AICA (RTC, 0x8C1706E8) continua no runtime: é tempo (catálogo do HH09, HH15).
- Comandos do driver não usados no roteiro (MIDI 0x31–0x3E, 0x80, 0x82–0x85, 0x89, 0x8F, 0x99, 0x9E, 0x9F, 0xAE, 0xAF)
  contam como desconhecidos (o portão A1 exige zero).
- O DMA nativo termina na hora (o anel enche antes do primeiro key-on, no Flycast mais tarde).
- FLAC/OGG para mods: erro explícito (os streams do jogo são ADPCM; o conversor aceita WAV).

## 7. Dados privados

WAVs gerados, despejos de RAM de som, capturas do Flycast (áudio, sombra, anel), perfis e logs ficam em `analysis/hh14`
(ignorado); revisões em `reviews/`. Nenhum byte de áudio, amostra, stream ou código do driver foi versionado; os documentos
citam só endereços, contagens, cabeçalhos genéricos e métricas.

## 8. Checklist de saída

- [x] Inventário de bancos, sequências, vozes, efeitos e driver (análise estática).
- [x] Diferença de comportamento medida: o legado roda com o som morto; o nativo responde como o driver (comandos,
      posições, eventos, interrupção) e o jogo passa a usar streams, eventos e DMA.
- [x] Motor e saída atrás de interfaces, sem ARM7.
- [x] Música, efeitos, falas, mixagem, laços e DSP de reverb.
- [x] Mods de áudio/dublagem e diagnóstico por evento.
- [x] Nenhum acesso à AICA no modo nativo (só o RTC, tempo/HH15).
- [~] Matriz de cenas dos dois discos: disco 1 (logos, título, menus, cena, área) medido; disco 2 pelos formatos (cenas
      com a campanha, HH16).
- [x] Revisão Codex registrada; regressão integral ok.

## 9. Próximos passos

HH15 (fim do tempo guest e remoção do legado) marcado `EM_ANDAMENTO`.
