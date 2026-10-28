# HH14 — Plano detalhado: áudio nativo e moddável

Data: 2026-10-27. Validação mínima: **V2**. Equivalente ao N6 do Blue Stinger (`C:\Source\recomp-BStinger\docs\
N6-PLANO.md`; MIT, somente leitura). Estado: **fases A–F feitas** (resultados nas seções "Fases D–F"; relatório `docs/relatorios/HH14-RELATORIO.md`).

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

**Opcodes** (`tools/dev/hh14_ac_opcodes.py`; segunda evidência de cada nome: o opcode que a função envia existe na
tabela de despacho do driver, `tools/dev/hh14_drv.py` com Capstone 5.0.7, despacho em 0x30B4 do `AUDIO64.DRV`, fila
de comandos em 0xC800 da RAM de som, 16 bytes por entrada, +0 opcode u16, +2 porta):

| Opcode | Função | Opcode | Função |
|---|---|---|---|
| 0x81 | acSystemSetMasterVolume | 0x96 | acDigiSetVolume |
| 0x8A | acSystemSetStereoOrMono | 0x97 | acDigiSetPan |
| 0x90 | acDigiOpen | 0x98 | acDigiSetCurrentPitch |
| 0x91 | acDigiClose | 0x99 | acDigiSetQSoundAngle |
| 0x92 | acDigiRequestEvent | 0x9A / 0x9B | acDspInstallProgram / InstallOutputMixer |
| 0x93 | acDigiPlay | 0x95 | acDspSetMixerChannel |
| 0x94 | acDigiStop | 0x9C, 0x9D | acDigiMultiPlay (0x9D também em acDigiMultiStop) |
| 0xAC | acDigiSetSampleRate | 0xAD | acDigiPlayWithParameters |

Sem comando ao driver: `acDigiSetLoopPoints` e `acDigiGetCurrentReadPosition` (memória compartilhada pelo G2),
`acTransfer`/`acSetTransferMode` (G2/DMA para a RAM de som), `acSystemInit`/`InstallDriver`, `acInt*`. O driver trata
ainda 0x30–0x32, 0x35, 0x36, 0x3E, 0x80, 0x82–0x85, 0x89, 0x8F, 0x9E, 0x9F, 0xAE, 0xAF (MIDI e outros, sem uso visto
no roteiro: a confirmar nos dois discos).

**O que o driver faz** (handlers lidos no código ARM7): **porta = canal da AICA** (0–63). O driver mantém uma sombra
de 18 palavras por porta (0x117C + porta × 0x48, iniciada de um modelo em 0x1134) no formato dos registradores do
canal, e os comandos só a alteram e a copiam para o canal (base `*(0xAA8)` + 0x80 × porta):
- 0x90 Open: SA (endereço na RAM de som, bits altos em +0 bits 6–0, baixos em +4), PCMS (formato em +0 bits 8–7:
  16 bits, 8 bits, ADPCM, ADPCM longo), LSA = 0, LEA = tamanho − 1 ajustado pelo formato, OCT/FNS (+0x18) já
  calculado no SH-4 (0x8C120FBC a partir da taxa);
- 0x91 Close: a sombra volta ao modelo;
- 0x92 RequestEvent: guarda um valor por porta (0xD34 + porta × 4) e liga o bit da porta numa máscara de 64;
- 0x93 Play: LPCTL (laço, +0 bit 9) e, opcionalmente, LSA/LEA do pacote; marca a porta ativa, copia a sombra para o
  canal e faz o key-on.
- 0x94 Stop: tira KYONB e LPCTL (+0 bits 14 e 9; key-off) e limpa o bit de evento da porta;
- 0x95 SetMixerChannel: ISEL e IMXL (envio ao DSP, +0x20 bits 3–0 e 7–4) e DISDL (nível direto, +0x24 bits 11–8);
- 0x96 SetVolume: TL (+0x28 bits 15–8);
- 0x97 SetPan: DIPAN (+0x24 bits 4–0; o pan com sinal do pacote convertido para a codificação da AICA);
- 0x98 / 0xAC SetPitch / SetSampleRate: OCT/FNS (handler comum em 0x3F84/0x3F94; detalhes na fase D).
Modelo do canal (0x1134): AR = RR = 31 e D1R = D2R = 0 (liga e desliga sem rampa), filtro aberto (FLV 0x1FFF), LFO
em repouso (0x4210), DISDL 15 (0 dB), DIPAN 0x10, TL 0. A voz nativa é, portanto, um sampler: memória de som, formato,
laço, pitch (OCT/FNS), volume (TL + DISDL), pan e envio ao DSP; o envelope completo do BS (`envelope.h`) só entra se um
comando mudar o envelope (não visto nos opcodes usados).

**Componentes do BS a portar** (`C:\Source\recomp-BStinger\native\audio`, MIT): `aica_adpcm` (decodificador),
`envelope.h` (envelope da AICA, fase G do N6), `mixer`, `voice_player`, `sound_file`, `sound_catalog` e `aica_dsp`
(programas `SFPB`/`SFOB`, o formato do `HHREV`). O que é do Manatee (`manatee*`, `sosb`, `sd_adapter`) não se aplica.

**Fase C, passo 1 (2026-10-27): núcleo portado.** `native/audio/{aica_adpcm,envelope.h,mixer,sound_file,aica_dsp}` do
BS em `hh_native_audio_core` (sem Win32); FLAC/OGG com erro explícito até a fase F (`sound_mod_codecs.cpp`).
`hh14_audio_tests` 22/22: os 12 testes do mixer/ADPCM/WAV e os 9 do DSP do BS (o caso do programa do jogo refeito para
o `HHREV`: RBL 2, EFSDL 15/15/15/15, EFPAN 1F/0F; resposta ao impulso estéreo que decai) e `hh14_streams_wav_adpcm`
sobre os streams reais. Duas adaptações do leitor de WAV, com o teste falhando antes de cada uma:
- o `fmt` dos WAV do HH declara 18 bytes e ocupa 20 (2 bytes zero antes do `data`): sem o ajuste, "WAV truncado";
- **ADPCM estéreo** (o BS só tinha mono): bytes alternados, pares = esquerda, ímpares = direita, cada canal com o
  próprio estado. Evidência: correlação esquerda/direita de 0,96–0,999 nas falas estéreo das cenas (`SCENE*_RL`) com
  esse leiaute, contra < 0,2 em blocos de 16 a 32 768 bytes, nibbles alternados ou metades
  (`tools/dev/hh14_estereo.py`). A segunda evidência (o código do jogo que separa os canais ao mandar o
  stream para a RAM de som) fica para a fase D.
Resultado: 74 WAV do disco 1 (18 estéreo) e 44 do disco 2 (9 estéreo) lidos sem falha, sinal plausível.

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

## Fases D–F (2026-10-27)

**Provedor nativo** (`transitional/audio_device`, `--audio nativo`; padrão na janela). Os funis do Audio64 até a AICA
(sonda do HH07 e, no nativo, a sonda nos caminhos que só rodam com o som vivo) viram slots: `_acG2SetHostCommand`
0x8C122440, `acG2Write`/`Read`/`Fill`/`ReadLong`/`WriteLong` (0x8C122784, 0x8C122602, 0x8C12250E, 0x8C1226AC,
0x8C12271C), a limpeza do DSP 0x8C12234C, `acDigiGetCurrentReadPosition` 0x8C120610 (lê o ponteiro da tabela direto da
RAM de som) e o início do DMA do G2 0x8C123564 (único escritor de `ADSTAG`…`ADST`). RAM de som e registradores nativos; o
driver reconstruído:
- comandos 0x30 (SetLoopPoints, só a sombra), 0x81, 0x8A, 0x90–0x98, 0x9A–0x9D, 0xAC, 0xAD com a semântica dos handlers;
  MultiPlay/MultiStop: a palavra em +4 cobre as portas 63..32 e a em +8 as 31..0 (0x3CD4 conta de 63 a 0);
- boot do ARM (bit 0 de 0x2C00 de 1 para 0): a versão 0x00030040 em 0xEC/0xB408 e MVOL 15 (0x120–0x154);
- passada do laço principal a cada 32 amostras: posição atual/anterior em 0xB34/0xC34, fim sem laço (porta inativa),
  eventos (alvo em 0xD34, cruzamento com volta), anel de mensagens 0xAEC–0xB2B com o ponteiro em 0xE4, MCIPD bit 5 e o
  nível externo da AICA no INTC do Holly (ISTEXT bit 1; `holly_intc_set_external`);
- DMA do G2: o pedido da cabeça da fila do Audio64 (`S+0xAC`/`S+0xB4`) copiado na RAM nativa e o fim do DMA no ISTNRM
  bit 15; o tratador do jogo (0x8C1234A2) retira o pedido, grava 802 e chama o callback; ocupado até o reconhecimento.
Cópias com o outro lado na própria RAM de som (o jogo transmite amostras longas residentes nela para um anel) usam a RAM
nativa.

**Motor** (`native/audio/aica_channels`): 64 canais que leem a RAM de som enquanto tocam (PCM 16/8, ADPCM com o estado
do LSA no formato 2 e contínuo no 3), laço com **LEA exclusivo** (o canal toca LSA..LEA−1; evidência: o CRI programa
LEA = tamanho do anel e copia o início depois do fim; com LEA inclusivo o ADPCM longo perde o estado na primeira volta e
o estéreo da música do título se descorrelaciona, 0,83 → 0,28, com o lado esquerdo 10× mais alto), pitch OCT/FNS em
16.16, ganhos TL + DISDL e pan DIPAN (curvas do N6 do BS), envio ISEL/IMXL ao DSP (`aica_dsp`, os programas `HHREV` que
o jogo instala por 0x9A/0x9B), MVOL/MONO. Avança pelo relógio lógico (44 100 amostras por segundo de tempo do jogo).

**Correções fora do áudio expostas pelo som vivo:** `gdFsSetDir` (0x8C15D06E) no provedor nativo de arquivos (o jogo
torna `\Streams` o diretório corrente antes de cada stream; sem ele os streams eram procurados em `\MOVIES`).

**Oráculo (fase B, Flycast com o driver real):** no primeiro MultiPlay da música do título, a sombra do driver para as
portas 14/15 (SA, PCMS, LSA, LEA, OCT, FNS, TL, DISDL, DIPAN, ISEL, IMXL) é igual à do driver nativo; o anel tem dados do
CRI (no nativo, mais cedo: o DMA nativo termina na hora). A saída livre do Flycast (driver "disk" do SDL, 294 s sem
entrada: logos, filme de abertura, noticiário) dá referência de nível: música/filme 1 400–2 400 RMS sem saturação.

**Mods (fase F):** arquivos do disco em `modsssets` (HH08); `hh-builder --audio-mod` converte WAV PCM para o formato
dos streams de fala (`encode_aica_wav`: cabeçalho idêntico ao original, testado nos dois discos). `docs/MODS-AUDIO.md`.

**Portão** `tests/diff/hh14_audio.py` (roteiro `newgame_area1_nativo`, 5 600 VBlanks): A1 driver sem comandos
desconhecidos, 53 key-on, 111 eventos, streams em STREAMS; A2 da AICA só o RTC (HH15), nenhum DMA do G2 pelo Audio64;
A3 WAV idêntico em duas execuções; A4 título L 1 809 / R 1 678 RMS, gameplay 6 922, saturação 0,013 %; A5 sombra igual à
do oráculo.

## Próximos passos de preparação

1. ✔ Disco 2, tabela do `AHFF` e formato das `.ADP` internas; falta o formato das `.SEQ`.
2. ✔ Fronteira (seção acima). Ponto de partida (`analysis/hh07/disc1/fronteira.tsv` e `sdk_faixas.tsv`): o jogo chama ~20 funções em
   0x8C120534–0x8C1232FE (subsistema `audio`; 0x8C1212FC com 13 chamadas, 0x8C122784 com 16, 0x8C121A12 com 8,
   0x8C121A82 com 7), o núcleo `som` do SDK fica em 0x8C12234C–0x8C123088 (30 funções) e `a64File*` em
   0x8C11FFFE–0x8C120444 (HH08); CRI em 0x8C15868E–0x8C176F5E (ADX das músicas). Confirmar a API `Audio64::*`/`a64*` do jogo (HH07: família Audio64 10.0.f.2, `Audio64_QueueLoadEntry`), CRI ADX
   (player das músicas) e o caminho das falas; contagens no roteiro da área 1 (`HH_FN_PERFIL`).
3. Portar de `native/audio` do BS o que for independente do Manatee (mixer, ADPCM da AICA, `sound_file`, catálogo e
   mods); decodificador ADX nativo.
