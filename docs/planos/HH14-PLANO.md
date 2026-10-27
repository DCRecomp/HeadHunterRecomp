# HH14 — Plano detalhado: áudio nativo e moddável

Data: 2026-10-27. Validação mínima: **V2**. Equivalente ao N6 do Blue Stinger (`C:\Source\recomp-BStinger\docs\
N6-PLANO.md`; MIT, somente leitura). Estado: **fase A** (inventário e fronteira feitos; contrato das funções `ac*` em curso).

## Critério de saída (ROADMAP)

Matriz de cenas dos dois discos dentro das tolerâncias; nenhum acesso AICA alcançável no modo nativo. O driver
`AUDIO64.DRV` (ARM7) só é analisado estaticamente e nunca executado (regra permanente "recompilar, nunca emular").

## Inventário dos formatos (disco 1, 2026-10-27; só nomes, contagens e cabeçalhos)

Comando: leitura dos 48 primeiros bytes de cada arquivo de `game-data/hh02/disc1/STREAMS` (script no relatório).

| Família | Arquivos | Formato (cabeçalho) | Duração |
|---|---|---|---|
| Músicas `*GML.ADP` | 20 + `RADIOMUSICGML.ADP` | CRI ADX (`80 00`, big-endian): estéreo, 32 000 Hz (o rádio 44 100 Hz) | 46,6 + 0,8 min |
| Falas e efeitos longos `*.ADP` | 52 mono, 18 estéreo, 4 mono 22 050 Hz (`GREYWOLFTAUNT*`) | RIFF/WAVE, format tag 0 = ADPCM de 4 bits da AICA (o mesmo das falas do BS) | 8,5 + 8,5 + 0,2 min |
| Contêineres `AHFDATA/*.AHF` | 81 (250 MB) | `AHFF` versão 2 com caminhos internos (`HEADHUNTER\OBJECT\...`, `...\SCRIPT\...`): pacote geral de recursos, bancos de som a localizar dentro | — |
| DSP de efeitos `REVERB/HHREV.{FPB,FOB}` | 2 | `SFPB`/`SFOB` (formato do DSP da AICA, o mesmo do `BS_SND.MLT` do BS) | — |
| Driver | `AUDIO64.DRV` (18 608 bytes, idêntico nos dois discos, HH01) | código ARM7 | — |

**Conteúdo dos `AHF`** (nomes internos `HEADHUNTER\<dir>\...` nos primeiros 256 KB de cada pacote,
`scratchpad/ahf_paths.py`, a versionar em `tools/dev` com o inventário): `SAMPLES` tem **5 911 entradas `.ADP`**
(amostras de efeitos e instrumentos por área/personagem, ex. `SAMPLES\MISC\...`, `SAMPLES\LEILA\...`) e **78 `.SEQ`**
(sequências); o resto é jogo (texturas PVR, objetos, scripts, colisões, legendas). Os efeitos e as sequências do
Audio64 vêm dos pacotes de cada área, carregados com ela; as músicas longas são os streams ADX de `STREAMS`.

**Amostras e sequências** (tabela do `AHFF` v2: contagem em +8, fim da tabela de deslocamentos em +16; registros a partir
de 0x1C com nome NUL alinhado a 4 e três `u32` — deslocamento absoluto, tamanho, 0; `scratchpad/ahff.py`, alguns
pacotes têm variante a investigar): as `SAMPLES\*.ADP` internas são RIFF/WAVE tag 0 (ADPCM da AICA), mono, 44 100 Hz;
as `.SEQ` são definições pequenas de evento que citam amostras por nome (`gasstation\garage_door_open`), não MIDI.
Disco 2: 61 streams (17 ADX, 53,2 min; 44 WAV ADPCM), 30 iguais por hash aos do disco 1; reverb igual nos dois.
Todos os pacotes lidos (`tools/dev/hh14_inventario.py ahf`): disco 1 com 5 911 amostras e 78 sequências em 81
pacotes, **368 nomes distintos**; disco 2 com 4 469 e 27 em 60 pacotes, 307 nomes. **77 nomes (70 no disco 2) têm
conteúdo diferente entre pacotes**: o ID estável de um mod de efeito não pode ser só o nome (pacote + nome, ou o hash
do conteúdo; a decidir na fase F com a evidência de qual pacote está carregado).

## Fronteira (2026-10-27)

Camadas, de cima para baixo (strings da tabela do HH07 + perfil `HH_FN_PERFIL` no roteiro da área 1,
`analysis/hh14/perfil_a64_resumo.tsv`):

1. **Motor de som do jogo** (`SoundEngine_*`, 0x8C02C20A–0x8C02E2A4; amostras `HeadHunter\Samples\` por
   0x8C02C628 com `Audio64_QueueLoadEntry`, sequências por 0x8C0D4C90 `SoundEngine_SequenceLoad`; streams por
   0x8C02B39E/0x8C02B6A0 `\Streams` sobre o CRI ADXT).
2. **CRI ADXT** (músicas `*GML.ADP`): decodifica no SH-4 (código recompilado) e entrega PCM pelo Audio64.
3. **Audio64, camada `ac*`** (0x8C120534–0x8C1232FE, nomes pelas mensagens de erro de cada função):

| Função | Nome | Chamadas no roteiro (do jogo) |
|---|---|---|
| 0x8C122E78 | acSystemInit (carrega `audio64.drv`/`midida.drv`) | 1 (1) |
| 0x8C123088 | acSystemInstallDriver | 1 |
| 0x8C122974 / 0x8C122888 | acIntInit / acIntSetMessagingMode | 1 / 1 |
| 0x8C123024 | acSystemGetFirstFreeSoundMemory | 11 (11) |
| 0x8C1232FE / 0x8C123234 | acTransfer / acSetTransferMode | 306 (306) / 118 (118) |
| 0x8C122784 / 0x8C12250E / 0x8C122602 | acG2Write / acG2Fill / acG2Read (memória de som) | 335 (28) / 128 (128) / — |
| 0x8C1212FC / 0x8C12177A | acDigiOpen / acDigiClose | 207 (167) / — |
| 0x8C120BCA / 0x8C120DC6 / 0x8C121156 | acDigiPlay / acDigiPlayWithParameters / acDigiMultiPlay | 1 / 35 / 3 |
| 0x8C121A82 / 0x8C121270 | acDigiStop / acDigiMultiStop | 201 / 2 240 |
| 0x8C121916 / 0x8C1219A8 / 0x8C1217D4 | acDigiSetVolume / SetPan / SetCurrentPitch | 186 / 83 / 1 |
| 0x8C121A12 / 0x8C120610 / 0x8C120B38 | acDigiRequestEvent / GetCurrentReadPosition / SetLoopPoints | 1 / — / — |
| 0x8C1215C4 / 0x8C121672 / 0x8C1216F8 | acDspInstallProgram / InstallOutputMixer / SetMixerChannel | 8 / 1 / 13 |
| 0x8C120534 / 0x8C120AE8 | acSystemSetMasterVolume / SetStereoOrMono | — / 1 |
| 0x8C122440 | _acG2SetHostCommand (fila de comandos ao driver, 772) | interno |

**Desenho proposto** (como as 5 entradas do SDK do N6 do BS): os slots nativos ficam na camada `ac*`. O motor do jogo e
o ADXT continuam recompilados (regras do jogo e decodificação no SH-4); o provedor guarda a memória de som numa área
nativa (`acG2*`, `acTransfer`), atende `acDigi*` com vozes do mixer nativo lendo dessa memória (ADPCM da AICA, PCM 8/16,
laço, volume, pan, pitch, posição de leitura e eventos pelo relógio lógico) e o `acDsp*` com o reverb nativo
(`HHREV.FPB/FOB`). Nenhum comando chega ao driver nem à AICA. Mods (por nome de amostra e de stream) entram numa
segunda camada, acima.

**Protocolo com o driver** (`tools/dev/hh_dis.py`): cada `ac*` valida os argumentos (ex. `acDigiOpen(porta < 64,
endereço na memória de som != 0, tamanho <= 0xFFFF amostras, formato 0..3, taxa na pilha)`) e monta um pacote que
`_acG2SetHostCommand` (0x8C122440) põe numa fila circular de 32 entradas na memória de som (índice em 0x8C30F860+20,
base em +16; procura uma entrada livre, palavra 0, até 21 vezes). Não espera resposta; com a fila cheia, avisa
(0x8C1222BC) e descarta, devolvendo falha em `r0`.

**Estado atual (legado):** o ARM7 nunca roda, então a fila enche no VBlank 56 e todo comando seguinte é descartado:
`_acG2SetHostCommand - can't send driver command` 740 vezes no roteiro, mais 2 240 avisos de `acDigiMultiStop` e os
de `acDigiStop`/`SetVolume`/`Open`/`SetPan`/`PlayWithParameters` (3 701 avisos no total). O jogo hoje roda com o som
morto; o provedor nativo aceita os comandos, então o que o jogo lê do driver (retornos, posição de leitura, eventos) muda
e precisa ser medido contra o Flycast (oráculo com o driver real), não contra o legado.

## Fases

| Fase | Entrega | Prova |
|---|---|---|
| A | Contrato de cada `ac*` usado (argumentos, pacote, retorno) por desmontagem + mensagens; o que o driver faz com cada comando, lido no `AUDIO64.DRV` (ARM7, só estático) | duas evidências por função; tabela no plano |
| B | Oráculo no Flycast: sequência de `ac*` (argumentos, VBlank) e áudio de saída em cenas de referência dos dois discos | traço e WAV privados; comandos reproduzíveis |
| C | `native/audio` (mixer, ADPCM da AICA, PCM, laço) portado do BS; memória de som nativa; slots `acG2*`/`acTransfer`/`acSystem*` | unitários; sonda sem acesso à AICA nesses slots |
| D | `acDigi*` (efeitos e streams do ADXT pelo anel na memória de som; posição de leitura e eventos pelo relógio lógico) | traço de comandos e áudio contra o oráculo (B) |
| E | `acDsp*`: reverb nativo a partir de `HHREV.FPB/FOB` | comparação com o oráculo |
| F | Mods (amostra por nome, stream por nome, idioma), diagnóstico por evento, promoção | matriz de cenas dos dois discos; sonda sem AICA no modo nativo |

## Próximos passos de preparação

1. ✔ Disco 2, tabela do `AHFF` e formato das `.ADP` internas; falta o formato das `.SEQ`.
2. ✔ Fronteira (seção acima). Ponto de partida (`analysis/hh07/disc1/fronteira.tsv` e `sdk_faixas.tsv`): o jogo chama ~20 funções em
   0x8C120534–0x8C1232FE (subsistema `audio`; 0x8C1212FC com 13 chamadas, 0x8C122784 com 16, 0x8C121A12 com 8,
   0x8C121A82 com 7), o núcleo `som` do SDK fica em 0x8C12234C–0x8C123088 (30 funções) e `a64File*` em
   0x8C11FFFE–0x8C120444 (HH08); CRI em 0x8C15868E–0x8C176F5E (ADX das músicas). Confirmar a API `Audio64::*`/`a64*` do jogo (HH07: família Audio64 10.0.f.2, `Audio64_QueueLoadEntry`), CRI ADX
   (player das músicas) e o caminho das falas; contagens no roteiro da área 1 (`HH_FN_PERFIL`).
3. Portar de `native/audio` do BS o que for independente do Manatee (mixer, ADPCM da AICA, `sound_file`, catálogo e
   mods); decodificador ADX nativo.
