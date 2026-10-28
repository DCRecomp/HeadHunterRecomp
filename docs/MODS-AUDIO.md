# Mods de áudio e dublagem (HH14)

O áudio do Head Hunter Recompiled vem dos próprios arquivos do jogo, lidos pelo provedor nativo de arquivos (HH08) e
tocados pelo motor nativo de som (HH14). Um mod de áudio é um arquivo do disco substituído em `mods\assets`, na mesma
árvore de `data\disc` (qualquer tamanho), como os demais mods de recursos.

## O que dá para substituir

| Conteúdo | Arquivos | Formato que o jogo espera |
|---|---|---|
| Falas das cenas e dos objetos (dublagem) | `STREAMS\*.ADP` que são WAV (por exemplo `SCENE20A_RL.ADP`, `ANGELAS_BOOK.ADP`) | WAV de ADPCM de 4 bits da AICA (o jogo transmite os bytes direto para a RAM de som) |
| Músicas | `STREAMS\*GML.ADP` | CRI ADX (decodificado pelo código do jogo) |
| Efeitos e ambientes | amostras dentro de `AHFDATA\*.AHF` (`SAMPLES\...\*.ADP`) | pacote AHFF (o pacote inteiro é o arquivo do mod) |

Os filmes têm mods próprios (`mods\video`, `docs/MODS-VIDEO.md`).

## Dublagem: convertendo um WAV

O jogo não lê WAV PCM comum nos streams de fala: o Builder converte.

```text
hh-builder --audio-mod minha_fala.wav SCENE20A_RL.ADP
```

- entrada: WAV PCM de 16 ou 8 bits, mono ou estéreo, na taxa que você quiser (o jogo lê a taxa do cabeçalho; os
  originais usam 32 000 Hz e, alguns, 22 050 Hz);
- saída: WAV de ADPCM da AICA no formato exato dos streams do jogo (mesmo cabeçalho, estéreo com os bytes de cada canal
  alternados);
- copie o resultado para `mods\assets\STREAMS\` com o nome do original.

A duração pode ser diferente da original: o jogo segue o tamanho do arquivo. O ADPCM de 4 bits tem a qualidade do
original (relação sinal/ruído típica acima de 15 dB).

## Diagnóstico

`HH_AUDIO_LOG=<n>` registra os `n` primeiros disparos de som (porta, endereço, formato, laço, tom, volume, pan, envio ao
reverb) e, no fim, as portas com mais energia; `HH_AUDIO_LOG_PORTA=<n>` registra os comandos que o jogo manda ao driver
para uma porta; `HH_AUDIO_WAV=<arquivo>` grava tudo o que o motor produziu. São ferramentas de desenvolvimento: as saídas
contêm o som do jogo e não devem ser publicadas.
