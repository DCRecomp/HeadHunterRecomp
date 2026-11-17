# HH16: preparação feita sem carga na máquina (2026-11-14)

> Registro histórico da preparação, na ordem em que foi feita. As seções 3 e 4 descrevem o primeiro rascunho ("não
> compilado"); depois disso tudo foi compilado e testado. O estado atual dos portões fica em `docs/planos/HH16-PLANO.md`
> seção 9, e os testes pendentes em `docs/planos/TESTES-FUTUROS.md`.

Trabalho leve feito enquanto a bateria final do N9.8 do Blue Stinger rodava (janelas e medidas de desempenho; o plano
dela pede nada pesado junto). Só leitura de arquivos, scripts Python pequenos com `seek()/read()` sobre o executável e os
AHF, e edição de código **sem compilar**. Nada foi compilado, testado em execução ou rodado contra o jogo.

Ferramentas usadas (locais, fora do repositório, na pasta de rascunho da sessão): `xref.py` (literais do pool → função
dona), `tabela.py` (limites e índices da tabela de comandos do script), `asb_dis.py` (desmontagem do bytecode `.ASB`
dentro dos AHF), `asb_cab.py` (cabeçalhos). Saída extensa em `analysis/hh16/` (ignorado).

## 1. Passagem do disco 1 para o 2 (P1)

### 1.1 Fatos (executável do disco 1, `game-data/hh02/disc1/1ST_READ.BIN`, e C++ gerado `generated/disc1`)

| Fato | Evidência |
|---|---|
| O script do jogo é uma VM de pilha com instruções de 8 bytes (`u32` código, `u32` operando), interpretada por `0x8C0DC278` | desmontagem; `switch` por tabela em `0x8C0DC2F0`, 37 códigos |
| O código 7 chama um comando nativo pelo operando, na tabela `0x8C147D58` (619 entradas, até `0x8C148700`) | handler em `0x8C0DC3C6`: `mov.l @(r0,r2)` com a base `0x8C147D58` |
| Comando **561** = `0x8C029F40` → `0x8C09EBC4`: tenta `DISC\disk%d.id` com N de 1 a 3 e grava o primeiro encontrado no global do disco `0x8C15174C` | desmontagem (`sprintf` `0x8C124748`, abertura `0x8C0A1FF8`) e HH08 |
| Comando **596** = `0x8C02A974`: grava 2 em `0x8C15174C`, zera estados, copia uma posição de 12 bytes e um float, e chama o carregador `0x8C083534` | desmontagem |
| Comando **438** = `0x8C028332` → PlayScene `0x8C09E904` (o filme pelo nome desempilhado) | desmontagem; HH12 |
| Nos 217 scripts `.ASB` distintos dos dois discos, só **duas** chamadas reais (código 7) aos comandos de disco: `HEADHUNTER.ASB` chama o 561 (boot) e `S65_MOVIE.ASB` chama o 596 | `asb_dis.py` (as outras ocorrências do valor 596 nos dados não são precedidas do código 7) |
| `S65_MOVIE.ASB` toca os filmes `SCENE43` e `SCENE44` (os últimos do disco 1 junto com o `SCENE45`) e depois chama o 596 | desmontagem do script; lista de `MOVIES` do disco 1 |
| O menu de carregar (`0x8C0EEA20`) compara o disco gravado no save (+72, `0x8C09C5EC`) com o atual (`0x8C09EC68`); se forem diferentes, **mostra uma mensagem e não carrega** | desmontagem em `0x8C0EEC2E`–`0x8C0EECE6` |

### 1.2 Conclusões

- **A passagem dentro da sessão deve funcionar** na instalação única: o 596 grava 2 na variável, o adaptador do HH08
  passa a mostrar `DISK2.ID` e a variante `_DISCO2`, e os arquivos do disco 2 já estão presentes, então o caminho de
  "disco errado/tampa aberta" (`0x8C04E754`, chamado pela abertura de arquivo e pelo open do filme) não é acionado.
  *Hipótese a confirmar por teste dirigido.*
- **Risco alto para a gravação (P0 da HH16.P): carregar um save do disco 2 a partir do título.** No boot, o 561 encontra
  `disk1.id` (a variável começa em 0/1, e o adaptador mostra o disco 1), então a variável fica em 1. Um save feito depois
  da passagem tem disco 2; o menu de carregar recusa com a mensagem de troca de disco, e no produto não existe troca. Na
  campanha gravada, toda sessão depois da passagem começa com LOAD de um save do disco 2: **a campanha pararia aí.**
  No console, o jogador trocava o disco; falta achar o caminho que o jogo usa depois da troca (o menu `0x8C0866A4` grava
  2 na variável quando a flag `0x8C14D7AC` está ligada; a flag é ligada em `0x8C072EE4`, num fluxo de início de jogo,
  ainda sem semântica comprovada).
- **Correção esperada** (dentro da regra "sem troca de disco", memória do projeto e HH08): o produto se comporta como se
  o jogador tivesse trocado o disco na hora, ou seja, o disco do save escolhido vira o disco atual antes da comparação.
  A forma exata (no adaptador de arquivos ou no ponto de comparação) sai da investigação com teste negativo que falha
  antes da correção.

### 1.3 Testes dirigidos a fazer (com a máquina livre)

1. Estado forçado (só verificação) no comando 596 a partir de um save do fim do disco 1 ou de um roteiro: atravessar a
   passagem e chegar ao primeiro controle do disco 2, conferindo `DISK2.ID` aberto e o `SCENE0` da variante.
2. Num ponto de save do disco 2, salvar; fechar; abrir de novo e carregar pelo título: hoje deve **falhar** (negativo); com
   a correção, carregar.
3. Ida e volta: um save do disco 1 carregado com a variável em 2.

## 2. Créditos (P2)

- `S92_MOVIE.ASB` (disco 2) toca `SCENE80B_83` e `SCENE84` e então chama o comando 229 com o nome `GameEnd` (o mesmo
  comando que, nos outros scripts, recebe o nome da área seguinte).
- `SCENE84` é o último filme do disco 2: 3 825 quadros, 30 fps (127,5 s), 480×272 (manifesto do Builder).
- **Critério do veredito** (HH16.1): o `SCENE84` começou e terminou dentro da reprodução e o jogo entrou no estado
  `GameEnd`. A forma de observar `GameEnd` na reprodução (o comando 229 com esse nome, ou o efeito dele) fica para o
  teste com a máquina livre.
- **Observado (2026-11-14, só CPU):** o comando 229 é `0x8C025F66` (tabela `0x8C147D58`): tira o nome da pilha do
  script (`0x8C0DC162`) e chama `0x8C095444(nome)`; "GameEnd" carrega `AHFDATA/GAMEEND.AHF`. No produto sem janela, com
  a área `S92_MOVIE` posta no lugar da primeira por um mod privado (`tmp/p2`, entradas renomeadas como na seção 7.7) e
  START só no SCENE80B_83: `SCENE84: fim natural no VBlank 12002`, `GAMEEND.AHF` aberto no V12003 e
  `STREAMS/CREDITSGM.ADP` (música dos créditos) no V12006. O monitor (`tools/dev/campanha.py`) exige os dois sinais.

## 3. Gravação no produto (HH16.0, código escrito, não compilado)

**Achado de desenho:** no HH os passos de jogo estão desligados (`transitional/time_adapter.cpp`, `kVblankWaitPc = 0`: o
jogo não tem espera própria do VBlank), e os roteiros do HH são indexados pela leitura do controle. O `SessionRecorder`
herdado do BS congela por passo e não serve ao HH. Feito:

- `transitional/input_bridge.{h,cpp}`: `ReadRecorder`, filtro do `InputSystem` indexado pela amostra
  (`SampleContext::sample`), passando o estado pelos bytes do controle, gravando só mudanças, com erro explícito se a
  leitura voltar.
- `app/hh_window.cpp`: `--gravar <arquivo>` (só na janela, controles nativos, sem `HH_INPUT_SCRIPT`; nunca sobrescreve;
  flush por linha; comentário de início e de fim) e `--reproduzir <arquivo>` (janela ou `--headless`). As duas forçam o
  tempo lógico (`--tempo experimental`), salvo `--tempo`, porque o `--headless` do produto fica no tempo estável e a
  janela no lógico.
- `tests/unit/test_hh10_input.cpp`: `input_native_read_recorder` (só mudanças, índice da amostra, ida e volta pelo
  roteiro com os bytes do gatilho e do analógico, negativo da leitura que volta).

Pendente: compilar, ctest, e o portão da HH16.0 (gravar na janela e reproduzir headless com os mesmos hashes de quadro).

## 4. Monitor de campanha (HH16.1, rascunho)

`tools/dev/campanha.py` (sintaxe conferida, não executado): `nova`, `jogar`, `status`, `verificar`, `reproduzir-em`,
`lista`; campanha em `campanhas/<id>` (acrescentado ao `.gitignore`, com `/tmp/`), programa copiado e selado, dados
fixados pelo SHA-256 do `arquivos.manifest`, RTC por sessão, saves de início e de fim, hashes de quadro do `--log` a cada
60 VBlanks como estado observável até existir o estado por passo. Pendente: cadeia entre sessões, créditos, janela,
recuperação de queda, suíte `tests/campanha`.

## 5. Revisão do trabalho pendente da HH15.11b (sonda dos back-edges)

Revisão estática do diff não commitado, contra a desmontagem (com os delay slots decodificados):

| Elevada | Valores no back-edge ou no fim | Conferência |
|---|---|---|
| `strlen` (`8C1247E8`) | `r3` = byte com sinal, `r4` = depois do byte, `r5` = contagem, `T` = 0 | o slot do `bra` é `mov #0,r5`; o laço `add #1,r5 / mov.b @r4+,r3 / tst / bf` dá exatamente esses valores; **ok** |
| `memset` (`8C124640`) | `r0` = destino + i, `r7` = i, `T` = 0; `r3` = 0 | `mov r7,r3` com `r7` = 0; o slot do `bf/s` é `add #1,r0`; **ok** |
| seno interno (`8C126744`) | `r13` = 1, `r14` = a classe e depois 1/0 pelo sinal; restaurados no fim | slots `mov #1,r13`, `mov r13,r14` (negativo) e `mov r6,r14` (positivo, `r6` = 0); **ok** |
| arco-tangente (`8C126124`) | `r13` = classe & 8 | `mov #8,r13` e o slot `and r4,r13`; **ok** |
| `sh4_poll_slow` | a sonda só observa quando instalada; nula no produto | só `transitional/elevacao` a instala (verificação); **ok** |

Nenhum problema encontrado. Falta compilar, rodar os unitários e a conferência na rota.

Achado lateral (regra de temporários de 2026-11-14): o `--headless` do produto cria o cartão temporário em
`%TEMP%` (`app/hh_window.cpp`, `TempSaves`), fora da pasta de trabalho. Corrigir junto da HH16.0 (pasta de saída
passada pela suíte).

## 6. Inventário dos filmes para a HH19 seção 6

`analysis/hh16/filmes_inventario.tsv` (ignorado), a partir de `game-data/runtime/unico/video/manifest.toml`:

| Grupo | Filmes | Quadros | Duração |
|---|---|---|---|
| mais largo que 16:9 (480×204) | 37 | 97 395 | 54,1 min |
| ~16:9 (480×270/272) | 11 | 46 205 | 25,7 min |
| 4:3 (480×360) | 5 | **4 410** | **2,5 min** |

Os cinco 4:3 (os únicos que precisam das laterais geradas): `SCENE0` (as duas variantes de disco), `SCENE0L`,
`SCENE18`, `SCENE54_55`. A geração das laterais no método do BS cobre 4 410 quadros; o resto só passa pela ampliação.

## 7. Segunda rodada leve (2026-11-14, mesma restrição de carga)

### 7.1 Como o console carregava um save do disco 2 e a correção desenhada

- `0x8C04E754` consulta o estado do drive (`0x8C15D150`): tampa aberta (6) ou sem disco (7) liga `0x8C151D68`; se a
  troca não estiver permitida (`0x8C09C43C` devolve 0), vai para `0x8C04E470` (volta ao BIOS, HH08). Ou seja, abrir a
  tampa no meio do jogo reinicia o console.
- O disco atual só é descoberto no **boot**, pelo comando 561 do `HEADHUNTER.ASB`. **Conclusão:** no Dreamcast, para
  carregar um save do disco 2, o jogador **ligava o console com o disco 2**; a mensagem do menu de carregar só pede isso.
- Na instalação única o boot sempre encontra o disco 1, então um save do disco 2 nunca carrega pelo título. Isso
  confirma o risco P0 da seção 1.2.
- **Correção desenhada** (equivale a "o jogador ligou com o disco do save", sem troca de disco, regra do HH08): na
  chamada do getter do disco feita pelo menu de carregar (`0x8C09EC68`, retorno esperado `0x8C0EEC34`), quando o disco do
  save (o valor em `r8`, lido de `0x8C24A560 + índice × 76`) for 1 ou 2 e diferente do atual, o adaptador grava o disco
  do save em `0x8C15174C` e devolve esse valor; a comparação passa e o carregamento segue com os arquivos daquele disco
  (o adaptador do HH08 já segue a variável). Mecanismo: slot nativo do getter filtrado pelo endereço de retorno, o mesmo
  padrão da câmera livre do BS (N7c); os outros 6 chamadores do getter ficam intocados. Fica para depois da bateria do
  BS, porque exige o slot no `config/headhunter.toml`, regenerar e compilar.
- Testes: (1) negativo antes da correção: save do disco 2 carregado pelo título é recusado; (2) com a correção, carrega,
  `DISK2.ID` e a variante `_DISCO2` aparecem para o jogo, e a sessão segue; (3) save do disco 1 com a variável em 2;
  (4) os outros chamadores do getter devolvem o valor de sempre; (5) a rota da área 1 com os 94 hashes iguais.
- **Implementada (2026-11-14, só CPU):** `fDiscGetter` em `transitional/gdfs_adapter.cpp` (campos `discGetter` e
  `loadMenuDiscRet` em `Addresses`, contador `Stats::loadDiscSwitches`), slot `0x8C09EC68` em `config/headhunter.toml`,
  `generated/disc1` e `disc2` regenerados. O slot cobra os 4 ciclos da gerada e deixa `r2` como ela. Diff do gerado do
  disco 1: só as 7 chamadas do getter passam a `HHG_SLOT` e a tabela de slots vai de 87 para 88 (o gerado do disco 2
  estava atrasado desde antes da HH15.1 e recebeu também os slots e a espera `0x8C118346` que já estavam na config).
- Testes unitários `hh16_load_save_other_disc` e `hh16_load_save_other_disc_single_disc` (`tests/unit/test_hh08_gdfs.cpp`):
  itens (1), (3) e (4) acima, mais disco inválido, mesmo disco, variável 0, instalação de um disco só e o custo de 4
  ciclos. Sem o slot registrado, os dois falham (28 verificações); com ele, 7 de 7 passam.
- **(2) aprovado no produto (2026-11-14, sem janela, só CPU):** com o cartão gravado no experimento da seção 7.7 (save
  "Jack Wade, Disc 2, Aquadome"), `hh-run --data game-data/runtime/unico` com todos os provedores nativos, roteiro
  local START, START, A (cartão), DOWN, A (LOAD GAME), A, A: o log mostra `LOAD de um save do disco 2 (estava no disco
  1): o jogo passa para o disco 2`; o jogo abre `AHFDATA/SXX_AQ_MAIN.AHF` (V3475) e `HERO.AHF`, toca
  `MOVIES/SCENE46_48.4XM` (filme do disco 2, V3539) e termina com `disco 2`. Sem a correção, o menu recusaria o save.

### 7.7 Fim do disco 1 no produto: a tela "INSERT DISC 2" travava a campanha (P0, achado e corrigido em 2026-11-14)

Experimento local (só CPU; nada versionado): um mod em `tmp/p1/mods` troca `AHFDATA/S05_LAB.AHF` por uma cópia de
`S65_MOVIE.AHF` com as duas entradas renomeadas para `S05_LAB` (mesmo espaço no diretório do AHF); o NEW GAME do roteiro
`newgame_area1_nativo` cai direto na área que fecha o disco 1. Resultados (`hh-run`, provedores nativos, `--capture`):

- SCENE43, SCENE44 e SCENE45 tocam (START pula); o comando 596 (`0x8C02A974`) roda no V4959 (`--watch`), grava 2 na
  variável do disco, liga o "modo fim de disco" `0x8C14D780` e chama `0x8C083534`, que **não é carregador**: abre o menu
  de save com "End of Disc / Please save your game".
- O save feito ali sai como **save do disco 2** (a lista mostra "Disc 2, Aquadome").
- Depois do save, `f_8C0866A4` (`0x8C086C2E`) vê `0x8C14D780` e liga `0x8C14D784`; 20 quadros depois a tela **"INSERT
  DISC 2 — Press the OPEN button..."** é desenhada a cada quadro, para sempre (hash fixo do V5700 ao fim).
- Tampa aberta simulada (diagnóstico temporário, removido) nessa tela: o jogo vai para o BIOS (`0x8C09C43C` devolve 0;
  "o jogo pediu para sair"). Ou seja, no console o fluxo era **salvar, reiniciar pelo disco 2 e fazer LOAD**.
- Sem o modo fim de disco (diagnóstico temporário que zerava `0x8C14D780` logo depois do 596): o save vira comum e, ao
  fechar o menu, a tela fica preta para sempre, sem abrir arquivo nenhum. O jogo **não tem continuação própria** depois
  do 596; a área "45" enfileirada pelo comando 229 só é alcançada pelo LOAD do save.
- O caminho alternativo `0x8C14D7AC` (ligado por `f_8C072420` em `0x8C072EE4`, que grava disco **1**) grava disco 2 e
  chama `0x8C095444("MainMenu")`: volta ao menu principal, ainda dependendo de um LOAD do jogador.
- O LOAD do título (seção 7.1) leva esse save ao disco 2 corretamente.

**Implementado e aprovado no produto (2026-11-14, só CPU).** Evidência que decidiu o mecanismo: despejos de RAM
(`HH_RAM_DUMP`) no LOAD pelo título (V3476) e na tela INSERT DISC 2 (V5300) têm a **mesma imagem de save** em
`0x8C2F1748` (12 288 de 12 288 bytes iguais; área `SXX_AQ_MAIN`, disco 2). O LOAD aplica essa imagem por
`f_8C09C6C2(buffer)` (`--watch`: chamado por `0x8C117B2A`, r4 = `0x8C2F1748`), que chama `0x8C0DCEBE` e `0x8C0DCB04(buffer +
76)`, e esta entra na área por `0x8C095444`; o próprio jogo usa `0x8C0DCB04` no meio da partida (`f_8C07330C`,
`0x8C0736D0`). Correção: slot do save `0x8C09C34C` (`fSave` em `transitional/gdfs_adapter.cpp`, um único chamador): com os
discos juntos, retorno `0x8C086C18`, modo fim de disco ligado, variável em 2 e save gravado, desliga o modo e chama
`0x8C09C6C2(0x8C2F1748)` como o código gerado chamaria; qualquer outra combinação é o save original. Resultado no cenário
local: `fim do disco 1: o jogo segue no disco 2 pelo save gravado (area SXX_AQ_MAIN), sem troca`, `SXX_AQ_MAIN.AHF` aberto
no V5169, SCENE46_48 no V5227 (a mesma sequência do LOAD pelo título) e, depois do filme, a arena "Aquadome Combat" com o
chefe Gladiator jogável (captura no V5800). Testes `hh16_end_of_disc_continues_on_disc_2` (4 negativos: outra chamada,
modo desligado, disco 1, save falho) e `hh16_end_of_disc_single_disc_keeps_original`; sem o slot registrado o primeiro
falha. O texto abaixo registra o raciocínio antes da correção.

**Consequência (antes da correção):** com a regra do mantenedor (2026-10-25: "sem troca de disco... sem ação do jogador, sem tampa, sem
reiniciar"), o produto precisa fazer sozinho o equivalente a "reiniciar pelo disco 2 e carregar o save de fim de disco".
Sem a correção, a campanha pararia na tela "INSERT DISC 2". Caminhos considerados (o 1 foi implementado, sem passar
pelo menu principal):

1. **Preferido:** na tela "INSERT DISC 2" (com os discos juntos), o produto conduz o fluxo do próprio jogo: volta ao
   menu principal pelo caminho do jogo e carrega o save de fim de disco pela máquina de estados do LOAD
   (`0x8C0EEA20`: estado `0x8C2DFD58`, `0x8C09C2EA`, `0x8C09C3B8(slot + 1)`), sem tela intermediária. Exige mapear como o
   menu principal inicia o jogo depois do `0x8C09C3B8`.
2. Paliativo para a HH16.P se o 1 demorar: ir ao menu principal (caminho `0x8C14D7AC`) e o jogador fazer LOAD, uma ação
   a mais por campanha. **Fere a regra do mantenedor**; só com autorização dele.

Comandos e artefatos: `tmp/p1/` (ignorado): `rota_p2.txt`, `p2.log`, `cap2/`, `saves4/` (cartão com o save do disco 2),
`tampa.log`, `p3.log`, `load.log`, `capL2/`. Ferramentas de análise na pasta temporária da sessão (`ahf_renomeia.py`,
`xref_val.py`, `ppm2png.py`).

### 7.2 Estado por leitura do controle (desenho para a HH16.0)

Sem passos de jogo, o estado é tomado **a cada K leituras do controle** (padrão 60, cerca de 1 s), no `latch` da porta
A, e em cada LOAD/SAVE concluído:
- **região de variáveis do jogo** dada pelo código de partida (`0x8C1583A0` → `0x8C1583E0`): `.data` a partir de
  `0x8C147B20`, `.bss` de `0x8C189340` a `0x8C311740` (zerada por palavra) e de `0x8C311740` a `0x8C32EF60` (zerada por
  byte); cerca de 2 MB, contra 128 KB no BS. Hash rápido (FNV-1a de 64 bits, como os quadros) dessa região **menos as
  pilhas das threads** (a máscara das referências da HH15, `analysis/hh15/ref_pilhas`), com o índice da leitura e o
  VBlank;
- nos LOAD/SAVE e no fim da sessão, a RAM inteira menos as pilhas, como as referências da HH15;
- o heap fora dessa região (objetos das áreas) fica de fora da amostra periódica, como no BS, e entra só no hash
  completo dos LOAD/SAVE;
- depois da HH15.F o exportador projeta as estruturas nativas nesse mesmo layout (HH16-PLANO, seção 4.2).

### 7.3 Auditoria de portabilidade para a HH20 (só busca)

| Medida | Resultado |
|---|---|
| Arquivos com cabeçalhos do Windows (`windows.h`, COM, MF, XAudio2, D2D, DWrite, WIC, XInput, `process.h`) | 19: `app/hh_run.cpp`, `app/hh_window.cpp`; `builder/` (8, inclusive `child_process`, `toolchain`, as janelas); `native/audio/audio_output.cpp`, `native/core/com_scope.h`, `native/platform/input_system.cpp`, `native/platform/saves_store.cpp`, `native/video/mf_movie.cpp`, `native/video/subtitles.cpp`; `runtime/cpu/sh4_threads.cpp` (fibras), `runtime/input/input.cpp` |
| `wchar_t`/`std::wstring`, `_wfopen`, `_fseeki64`, `_getpid` | 27 arquivos, quase todos no Builder e na plataforma |
| Fibras | só `runtime/cpu/sh4_threads.cpp` (sai na HH15.F) |
| `std::execution::par` | `native/render/soft/soft_raster.cpp` (sem TBB no Linux roda em sequência em silêncio; o BS trocou por um pool próprio) |
| Rastreamento de escrita (`GetWriteWatch`) | nenhum (o BS dependia dele; o HH não) |
| Assembly MASM | nenhum |
| Literais com `\` | 21, quase todos em mensagens ao usuário; o núcleo usa `fs::path` |
| Caixa dos nomes | o provedor de arquivos indexa por nome canônico em maiúsculas (`native/platform/data_files.cpp`, `canonical_path`): já ignora a caixa |
| CMake sem condição de plataforma | `/utf-8` (raiz), `/bigobj` (gerado), `shell32 ole32` (app), `xinput` (runtime e native), `mfplat mfreadwrite mfuuid ole32 d2d1 dwrite windowscodecs` (vídeo), `xaudio2 ole32` (áudio) |

Comparado com o BS (13 arquivos do jogo com dependência real), o HH tem um bloqueador a menos (sem write-watch) e o das
fibras some com a HH15.F; os de filmes (Media Foundation), texto (DirectWrite) e áudio (XAudio2) são os mesmos.

### 7.4 `config/fronteiras.toml` desatualizado (lição L7)

- Sem mudança desde o HH11 (2026-10-25); 286 entradas: 261 `provedor = "legado"`, 25 `"nativo"`.
- 27 entradas marcadas `legado` já são slots nativos em `config/headhunter.toml` (`[native] slots`, 95 endereços); nenhuma
  marcada `nativo` está fora dos slots.
- Depois da HH15 parte 1 o produto não tem provedor legado; o rótulo perdeu o sentido. Proposta: `provedor = "nativo"`
  para os slots e `"gerado"` para as funções do SDK que ainda rodam como código gerado (o alvo da HH15.11), atualizado por
  `hh-hwscan --fronteira-gerar` com o teste de fechamento acusando divergência entre o rótulo e os slots. Exige
  compilar; fica para depois da bateria do BS.
- **Feito (2026-11-14, só os rótulos):** `hh-hwscan ... --fronteira config/fronteiras.toml --fronteira-gerar
  tmp/fronteira/fronteiras_novo.toml` dá 52 `nativo` (fechamento sem violações). A geração completa também esvaziaria
  ou encolheria as listas `subsistemas` de 40 entradas (saves, áudio, render, tempo: a varredura parece parar nos slots
  nativos), o que é perda de informação e não atualização; por isso só os 27 rótulos `legado` → `nativo` foram
  copiados para `config/fronteiras.toml` (diff de 27 linhas; cópia anterior em `tmp/fronteira/fronteiras_antes.toml`).
  `tests/diff/hh07_fechamento.py`: 0 falhas. O rótulo `"gerado"` (aceito hoje só `legado`/`nativo` em
  `translator/hwscan/boundary_io.cpp`) e o motivo das listas vazias ficam para a HH15.11, junto com a verificação
  automática rótulo × slots.

### 7.5 Temporários dentro do workspace (feito, sem compilar)

- `tests/CMakeLists.txt`: todos os testes recebem `TMP`/`TEMP` em `tmp/ctest` (o `temp_directory_path` do Windows segue);
  cobre os 11 usos (cartões temporários do `--headless` e do `hh-run`, diretórios dos unitários).
- `tools/dev/regressao.py`: estado em `tmp/regressao/estado.json` (o antigo de `%LOCALAPPDATA%` é copiado uma vez, nunca
  apagado) e `TMP`/`TEMP` dos processos filhos em `tmp/regressao/temp`.
- `.gitignore`: `/tmp/` e `/campanhas/`.

### 7.6 Guia da gravação e licenças

- `docs/CAMPANHA.md` (guia do mantenedor, marcado como rascunho até a HH16.P) e `tools/dist/Jogar-e-Gravar-Campanha.cmd`.
- `docs/LICENCAS.md`: política e inventário preliminar (SDL3 zlib, toml++ MIT, Vulkan-Headers Apache-2.0, runtime do
  MSVC, APIs do Windows, codecs, FFmpeg externo) com as pendências da HH18.

## 8. Dados privados

Nenhum byte do jogo foi copiado para arquivos versionados: este documento cita endereços, índices de comandos, nomes de
arquivos e contagens. Saídas extensas ficaram em `analysis/hh16/` e na pasta de rascunho da sessão, ambos fora do git.
