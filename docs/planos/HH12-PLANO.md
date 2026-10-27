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

### Contrato da API do filme (HH12.0, 2026-10-26)

Comando: `python -I tools/dev/hh_dis.py generated/disc1 game-data/hh02/disc1/1ST_READ.BIN <endereco>` (desmontagem
do C++ gerado com os literais resolvidos) e um varredor de chamadores sobre `generated/disc1/funcs_*.cpp`.

**PlayScene 0x8C09E904** (chamadores: 0x8C0F0AF4 e 0x8C028332, únicos) é o único usuário do objeto do filme:

| Passo | Endereço | Evidência (disassembly) | Papel |
|---|---|---|---|
| guarda estado, pausa | 0x8C073980, 0x8C073870(4), 0x8C15B000(1), 0x8C08ED34/0x8C08ED16 | salva 0x8C149F5C/0x8C14A220 na pilha e restaura no fim | sai do jogo para o filme; espera de N quadros (HH09) |
| nome | 0x8C124748 = sprintf(0x8C24D514, "Scene%s.4xm", nome) | literal 0x8C13C4DC | caminho do filme |
| preparação | 0x8C09BAEC, 0x8C02E084, 0x8C09FFD4, 0x8C016960(0), 0x8C0ED0FC(0x200000), 0x8C0E3DD8 | — | modo de vídeo, memória (hipótese) |
| parâmetros | 0x8C24D7A8..B8 = {0, 0.0f, 0, 1, mov.w} | escritas antes da criação | estrutura passada ao open (r6) |
| **create** | 0x8C11E30C(r4=0x8C20CD20, r5=0x8C149F64) -> obj em 0x8C24D7C0 | 0x8C016A18 = alocação; obj+4=r4, obj+0=r5, +8/+28/2 campos = 0 | objeto do filme |
| **open** | 0x8C11E9D2(obj, caminho, 0x8C24D7A8) | obj+8 = 0x8C11FFA0() (objeto de stream); até 10 x 0x8C15C7A0("\MOVIES") (troca de pasta do gdFs) com 0x8C04E754 e atraso; 0x8C11FCC6(stream, caminho, ...) != 0 -> retorna 1; senão 0x8C11E580(obj) (cabeçalho) | abre o .4XM pelo stream |
| áudio | 0x8C11EBAC(obj) = obj+28; se != 0: 0x8C11EBE0(obj, 0) -> 0x8C11FE5A(obj+8, 0, 2) e obj[32+16*0+12] = 0 | 6 e 21 instruções | trilha de áudio 0 |
| **play** | *0x8C189340 = 1; 0x8C11ECA4(obj, cb = 0x8C09E896); *0x8C189340 = 0 | 1 705 instruções; escala min(640/w, 480/h), centraliza (0x8C30F724/0x8C30F728); decodificador 0x8C120610..0x8C123D5C; Kamui 0x8C159820..0x8C15A7E0; espera 0x8C11FC3A (HH09) | laço do filme |
| callback | 0x8C09E896(obj) | lê o controle (0x8C08D770 sobre *0x8C20CD44), 0x8C08D890(pad, 6) != 0 -> grava 0 num campo do obj (mov.w) | **skip** pelo controle |
| **close** | 0x8C11EB32(obj, 0x8C24D774) | free(*0x8C30F720); 0x8C11FCF6/0x8C11FF4C/0x8C11FFA6 sobre obj+8; 0x8C11E482; 0x8C15AD60 (Kamui: libera 2 texturas do obj); obj+8 = 0 | fecha |
| **destroy** | 0x8C11E360(obj) | 3 x 0x8C016B4A (free) de dois campos e do obj | libera |
| volta | 0x8C0169C8, 0x8C0ED1D0, 0x8C0A007A, 0x8C09BBFC, 0x8C15B000(2), 0x8C073870(estado salvo), 0x8C016DF8 | — | volta ao jogo |

Fronteira escolhida (como o N5 do BS, que manteve a função de vídeo do jogo e trocou só as entradas do middleware):
**PlayScene continua original**; o provedor nativo atende **open, áudio (0x8C11EBE0), play e close** do objeto do filme
(create e destroy continuam originais: só alocam e liberam; o open nativo deixa os campos liberados no destroy em 0).
O open nativo continua trocando a pasta do gdFs para `\MOVIES` pelo jogo (efeito observável preservado). O play nativo
chama o callback do jogo a cada quadro e termina no fim natural ou quando o callback zera o campo de continuar; o
tempo anda pela espera de N quadros do jogo (0x8C08ED16, HH09). O quadro do filme é apresentado pelo host no lugar do
quadro do PVR (como no BS).

### Referência do V3 no Flycast (2026-10-26, em aberto)

- `flycast_oracle.py snap 0x8C11E482 <base> --hit k --vram --pvr` (a função vazia que o laço chama uma vez por
  quadro, depois de desenhar), k = 31, 121, 301, 501 (e 301 com `--no-mask-irq`); `tools/dev/hh12_v3.py` compara
  com o quadro do FFmpeg composto como o provedor nativo.
- O quadro exibido (`FB_R_SOF1` = 0x16A000, 640 x 476 entrelaçado) está **preto** em todas as capturas, embora o
  `emu.cfg` do oráculo tenha `rend.EmulateFramebuffer = yes` (que no HH05 deu quadros idênticos ao runtime).
- As texturas do filme estão no objeto: duas superfícies Kamui em obj+0x124 e obj+0x144 (1024 x 512, TCW
  0x0600003E: não twiddled, stride, **ARGB1555**; endereços 0x514B00 e 0x614B00). Nesses endereços a VRAM capturada
  não tem imagem; nenhum trecho de 4 pixels do quadro do FFmpeg (RGB565 nem ARGB1555, linear nem twiddled) aparece
  na RAM ou na VRAM, só coincidências em pixels escuros.
- Conclusão provisória: a captura por breakpoint não mostra o quadro do filme do Flycast (hipótese: o quadro só vai
  para a VRAM fora da janela do breakpoint, ou o Flycast atrasa o filme como no N5 do BS). O V3 continua em aberto;
  evidência parcial: o FFmpeg decodifica o SCENE0 com o número de quadros do `vtrk` e o logotipo do quadro 300
  coerente; a composição do jogo (640 x 480, escala da área exibida) vem do objeto do filme (oráculo no play).

### Referência do V3: Flycast rodando livre (2026-10-26)

- A captura por breakpoint deixa o quadro preto; com o Flycast **rodando livre** (`flycast_oracle.py snap-free <base>
  --seconds S`) o framebuffer emulado é o exibido (logotipo SEGA aos 12 s, SCENE0 dos 16 aos 26 s).
- `tools/dev/hh12_v3_livre.py`: o quadro do jogo amostrado no centro de cada pixel do filme (área exibida 480 × 360,
  escala 4/3, centralizado) contra os 512 quadros do FFmpeg. O quadro escolhido anda exatamente 30 por segundo
  (57, 87, 117, 147, 177, 207, 237, 267, 327 para 17…26 s): alinhamento e geometria confirmados.
- Quadros estáticos (logotipo, 207/237/267/327): PSNR 47,9–53,8 dB, erro máximo 9 por canal (a textura do jogo é
  ARGB1555, 5 bits por canal).
- Quadros com movimento (57–177): PSNR 31–40 dB, erro máximo 62–153. Por bloco 8 × 8 (`hh12_v3_blocos.py`, 18 s):
  40% dos blocos batem com o quadro 87 e 54% não batem com nenhum quadro de 84 a 90 (não é mistura de quadros);
  `hh12_v3_bilinear.py` (textura ARGB1555 reamostrada): erro médio 3,4–5,0 níveis, mediana 3, p90 8–11. A olho, o
  conteúdo é o mesmo (os mesmos blocos e bordas) com um padrão fino diferente: **o decodificador 4xm do FFmpeg não é
  bit a bit o do jogo nos quadros P com movimento** (hipótese da causa: arredondamento na compensação de movimento,
  acumulado entre as atualizações intra; há um só `ifrm` por filme). É o risco previsto abaixo (decisão).

### Legendas do jogo e efeitos por quadro do laço (2026-10-26)

- O executável tem `"%s%sSubTitle\%s.txt"` (0x8C13F9D0, literal só em 0x8C0E3964, função 0x8C0E38A4) e os AHF trazem
  `HEADHUNTER/LOCALE/<idioma>/SUBTITLE/<filme>.TXT` (84 nomes, os dos filmes: SCENE01_06, SCENE09_12…), no formato
  `;mm:ss;texto`. O laço original chama 0x8C0E4006 por quadro (0x8C11F18E, entre o upload do quadro e o render), que
  usa *0x8C151E10 (lista carregada) e *0x8C151E14 (opção ligada). RAM do jogo no SCENE01_06 (`HH_RAM_DUMP`, VBlanks
  1600/2000): *0x8C151E14 = 1 e a lista com uma linha por nó {+0x00 texto, +0x0C próximo, +0x10 segundos; 0 =
  continuação}: 60 legendas. O provedor nativo lê essa lista e desenha (DirectWrite) sobre o quadro composto; a
  fonte e a posição exatas do jogo não são reproduzidas (aproximação documentada).
- Também por quadro, o laço original incrementa 0x8C14A220 (restaurado por PlayScene) e 0x8C2388DC (contador de
  quadros desenhados; lido por 0x8C073F28, zerado por 0x8C073EF4, também incrementado pelo laço principal em
  0x8C04F1A0) e chama 0x8C016DF8 (tempos derivados): o play nativo faz o mesmo.
- `0x8C142E78` (texto `"\MOVIES"`, revisão Codex): literal do pool do open 0x8C11E9D2, passado como r4 a 0x8C15C7A0
  (gdFsChangeDir, slot do HH08) nas até 10 tentativas do open; o texto conferido no executável (hh02/disc1, base
  0x8C010000).

**Decisão do mantenedor (2026-10-26): opção A**, os filmes continuam decodificados pelo FFmpeg (a diferença nos
quadros com movimento fica documentada; o decodificador do jogo no Builder não será feito).

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
