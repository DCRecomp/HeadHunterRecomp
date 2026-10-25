# HH08 — RELATÓRIO: Arquivos nativos e troca dos dois discos sem CDI em runtime

Data: 2026-10-25. Validação mínima: **V2**. Plano: `docs/planos/HH08-PLANO.md`. Equivalente ao
N1 do Blue Stinger, mais a troca de disco, que o BS não tem.

## 1. Objetivo e escopo realizado

Critério de saída (ROADMAP): as rotas dos dois discos funcionam sem acesso a setores nem à
imagem; disco errado ou corrompido falha com mensagem segura.

| ID | Entrega | Resultado |
|---|---|---|
| HH08.1 | Trace das chamadas gdFs e contratos | `HH_CALL_TRACE` ligado ao host; 208 605 chamadas no roteiro. Contratos do gdFs 1.09 confirmados por disassembly (campos da GDFS) e trace. Achada a camada de driver do 1.09 (tabela 0x8C185DAC). |
| HH08.2 | `native/platform/data_files` | Portado do N1 do BS; 5 testes (`hh08_files_tests`). |
| HH08.3 | Adaptador gdFs, `[native] slots` e `--arquivos` | 17 funções com provedor nativo (as 16 gdFs com código gerado e o estado do drive); GDFS legado repassado; guardas nas internas; estado lógico do drive; 3 testes de contrato (`hh08_gdfs_tests`). |
| HH08.4 | Portões P1/P2 | P1: 4 838 blocos idênticos (arquivo, deslocamento, bytes, SHA-256). P2: 0 setores de dado pelo GD-ROM HLE. Os 94 hashes de quadro são iguais aos do legado e da referência do HH06. O negativo de mutação é acusado num bloco só. |
| HH08.5 | Dois discos e troca | O Builder prepara `data\disc1` e `data\disc2`, cada um conferido contra o próprio CDI e a versão do disco. `--disco`; F8 abre a tampa lógica; o jogo sai pelo próprio caminho e a janela dá o boot do outro disco. O disco 2 roda com o provedor nativo. |
| HH08.6 | Disco errado ou corrompido | Disco trocado (pela versão do executável), disco ausente, arquivo alterado, truncado ou ilegível e mod inválido param com mensagem (rc 1). |
| HH08.7 | Fronteira e produto | 9 entradas da fronteira com provedor nativo (legado 286 → 277, catraca); produto continua sem CDI (`hh07_produto_sem_cdi`). |
| HH08.8 | Relatório, revisão Codex e commit | Este documento; seção 9. |

O provedor nativo de arquivos passou a ser o **padrão** depois do P1/P2. `--arquivos legado`
continua para referência e rollback.

Recompilar, nunca emular: o adaptador implementa a API de arquivos sobre arquivos do Windows.
Nenhum setor, FAD ou estado de drive novo é simulado. O estado do drive que o jogo consulta
passou a ser um estado lógico do host, no lugar do HLE.

## 2. Arquivos alterados

- **`native/`**:
  - `platform/data_files.{h,cpp}`, portados do N1 do BS (MIT);
  - `platform/files.h`, com `canonical_path`;
  - `CMakeLists.txt`, com `hh_native_files`.
- **`transitional/`**:
  - `gdfs_adapter.{h,cpp}` (novo; derivado do N1 do BS e adaptado ao 1.09);
  - `CMakeLists.txt`.
- **`app/`**:
  - `hh_host.{h,cpp}`: provedor nativo, tampa, versão esperada, mods e `HH_CALL_TRACE`;
  - `hh_discs.{h,cpp}` (novo): `data\discN` e versão;
  - `hh_verified_files.{h,cpp}` (novo): tamanho e SHA-256 contra o manifesto;
  - `hh_run.cpp`: `--arquivos`, `--disco`, `--tampa-vblank` e `--mods`;
  - `hh_window.cpp`: as mesmas opções, mais F8, título e novo boot;
  - `CMakeLists.txt`.
- **`builder/builder_main.cpp`**:
  - `--disc2`;
  - `data\disc1`/`data\disc2` com a versão conferida;
  - verificação dos dois discos;
  - instalação sem marcador recusada.
- **`tools/hh-extract/src/runtime_data_cdi.cpp`**: comentário do manifesto ("Head Hunter").
- **`config/`**:
  - `headhunter.toml`, com `[native] slots` (17 funções gdFs);
  - `fronteiras.toml` (9 entradas nativas);
  - `metricas_base.toml`.
- **Testes**:
  - `tests/unit/test_hh08_{files,gdfs}.cpp`;
  - `tests/diff/hh08_{arquivos,discos}.py`;
  - `tests/hh08/check_builder_unmarked.cmake`;
  - `tests/CMakeLists.txt`.
- **Regressão**: `tools/dev/regressao.py`, com a etapa `portoes_hh08`, `HH_DATA_DIR2`, `HH_DISC2` e o Builder com dois discos.
- **Documentos**: `docs/planos/HH08-PLANO.md` (resultados do HH08.1), `docs/planos/HH09-PLANO.md` (preparação), este relatório e `STATUS.md`.

## 3. Evidências e decisões

### 3.1 Contratos do gdFs 1.09 (HH08.1)

Detalhes em `docs/planos/HH08-PLANO.md`, "HH08.1 — resultados". Em resumo:
- as funções que recebem GDFS são chamadas diretas, com os mesmos campos do 1.05: +8 FAD, +12
  bytes, +16 setores, +20 posição, +24 transferidos, +76 aberto, +78 modo, +80 estado e +82
  erro;
- as funções por nome e as de drive passam por stubs que leem `*0x8C188D08`, a tabela
  0x8C185DAC instalada por 0x8C15CFDC;
- o adaptador substitui os stubs que o jogo chama (Open 0x8C15D09A, ChangeDir 0x8C15C7A0 e o
  estado do drive 0x8C15D150);
- a tabela e o driver continuam legados: inicialização, TOC e o GDFS interno 0x8C28AB80.

Os chamadores observados no roteiro são:
- a camada de arquivos do jogo, 0x8C0A1FF8 (`.AHF`, `.CSV`, `.AVQ`, `AUDIO64.DRV`, `HHrev.*`
  e `DISC\disk1.id`);
- o player de 4X Movie, 0x8C12521C.

### 3.2 Leituras assíncronas

O servidor do gdFs 1.09 fica atrás da tabela do driver. O comportamento de referência é o do
legado, cujo GD-ROM HLE transfere de imediato: o P1 mostra os blocos entregues mesmo quando o
jogo cancela o pedido ao pular o filme.

O adaptador:
- escreve os dados no buffer dentro do próprio `gdFsReqRd32`;
- publica "ocupado" (2, 0 bytes) até a observação seguinte (GetStat/GetTransStat do arquivo)
  ou o próximo pedido, quando o pedido vira concluído (1);
- Stop e Close encerram o pedido (os dados já estavam entregues).

Nenhum caminho depende de consulta para receber dados. Esta é a revisão do primeiro desenho,
depois do achado do Codex; seção 9.

### 3.3 Troca de disco (HH08.5)

Fatos, conferidos no disassembly e pela execução:
- O jogo identifica o disco pelo arquivo. 0x8C09EBC4 tenta `DISC\disk%d.id` com N de 1 a 3,
  grava o primeiro N encontrado no global 0x8C15174C e o devolve pelo getter 0x8C09EC68. Quem a
  chama é 0x8C029F40, a partir da tabela de comandos 0x8C14861C.
- A dword por disco do executável (0x8C185E28) é o FAD do descritor de volume ISO. A
  inicialização do driver lê esse setor e confere "CD001" (0x8C15B870–0x8C15B884). Ela continua
  legada e só é usada na inicialização.
- Antes de cada abertura, 0x8C04E754 lê o estado do drive pelo stub 0x8C15D150 (`tabela[84]`).
  Com 6 (tampa aberta) ou 7 (sem disco) ele arma a flag (0x8C04E7B2–0x8C04E7BE). Se 0x8C09C43C
  (chamada em 0x8C04E7C8) devolver 0, chama 0x8C04E470. Essa função desliga os subsistemas
  (0x8C0CE0A4) e sai para o BIOS (0x8C15E588: vetor 0x8C0000E0 ou bootstrap em 0x8C008300).
- Medido: com a tampa lógica aberta no VBlank 3000 do roteiro (área 1), o jogo sai no VBlank
  3001 com o código 1 (menu do BIOS).

Decisão, coberta pela arquitetura: o provedor nativo devolve o estado lógico do drive (1, ou 6
com a tampa aberta). A janela abre a tampa com F8. Quando o jogo sai depois disso, ela dá o
boot do outro disco num processo novo, como o boot do console, e mantém o `--mods` do usuário.
O host confere a identidade do disco pelo hash do executável de boot (`versionId`), não pela
linha do manifesto.

### 3.4 Disco errado ou corrompido (HH08.6)

| Caso | Onde é pego | Mensagem |
|---|---|---|
| Pasta do disco 2 com o disco 1 | `resolve_disc` (manifesto) e o boot (hash do executável) | "nao tem o disco 2 … (e o disco 1)" / "dados … sao da versao …, esperada … (disco trocado)" |
| Disco ausente | `resolve_disc` | "dados do disco 2 ausentes … informando o CDI desse disco" |
| Arquivo alterado (1 byte) | `VerifiedFileSystem` (SHA-256 na primeira abertura) | "arquivo do jogo alterado ou corrompido: AUDIO64.DRV" |
| Arquivo truncado | verificação dos dados de execução no boot | "… Builder" |
| Arquivo ilegível (trava exclusiva) | adaptador (`Error::Io`) | "arquivo do jogo ilegivel …" |
| Mod que não substitui nada | `DataFileSystem` | "NADA.BIN: nao substitui nenhum recurso" |
| CDI do disco errado no Builder | Builder (versão do disco) | "o CDI de --disc2 … e a versao …, esperada …" |

Todos terminam com rc 1 e "PARADA", sem travar e sem ler setor.

### 3.5 Mods

`mods\assets\discN` fica ao lado do executável (ou `--mods`). O mod maior que o original é
aplicado sem conferência de hash e registrado no log.

### 3.6 Fronteira e catraca (HH08.7)

Com `[native] slots`, `hh-hwscan --fronteira-gerar` marca nativas as 9 entradas da fronteira
que são funções gdFs chamadas pelo jogo: 0x8C15C7A0, C7B8, C7F0, C83E, C8E2, CA48, CD68, D09A e
D150.

Efeitos:
- `fronteira.provedor.legado` 286 → 277 e `nativo` 0 → 9;
- `legado.subsistema.tempo` 42 → 38, porque o alcance de tempo passava pelas funções migradas;
- o fechamento continua com 0 violações;
- a catraca passou antes de a base ser atualizada.

`legado.subsistema.arquivos` = 1 é a inicialização do sistema 0x8C15FA28, que inclui o
gdFsInit. Ela fica legada, como no BS.

## 4. Comandos de build/teste e resultados

```text
cmake --preset vs && cmake --build --preset vs-release -- -m
ctest --preset vs-release                          (ver 4.1)
python -I tests/diff/hh08_arquivos.py build/app/Release/hh-run.exe .   (HH_DATA_DIR, HH_AREA1_HASHES)
python -I tests/diff/hh08_discos.py   build/app/Release/hh-run.exe .   (HH_DATA_DIR, HH_DATA_DIR2)
hh-hwscan config/headhunter.toml <1ST_READ.BIN> analysis/hh08/hwscan --sonda analysis/hh07/sonda_area1.tsv \
    --fronteira config/fronteiras.toml --metricas-base config/metricas_base.toml -> rc 0
python -I tools/dev/regressao.py (build, ctest, portoes_hh05, portoes_hh07, portoes_hh08, roteiro_area1, builder)
```

### 4.1 Regressão integral

Executada no código final, depois das correções da revisão Codex (log em
`analysis/hh08/regressao_final.log`):

```text
[ok] build (5 s)
[ok] ctest (9 s)            30/30 (os portões com dados privados rodam nas etapas seguintes)
[ok] portoes_hh05 (5 s)     hh_boot_regression, hh_first_frame (agora com o provedor nativo padrão)
[ok] portoes_hh07 (240 s)   hh07_fechamento (fronteira com 9 entradas nativas), hh07_ordem
[ok] portoes_hh08 (227 s)   hh08_arquivos (P1/P2 + mutação), hh08_discos (disco 2, troca, negativos, mods)
[ok] roteiro_area1 (115 s)  hashes iguais a snapshots/hh06/area1_hashes.txt
[ok] builder (184 s)        --disc + --disc2: data\disc1 e data\disc2 conferidos; HeadHunter.exe instalado reproduz a referência
regressao completa — fim rc=0
```

## 5. Comparação com referência/oráculo

- Legado × nativo no roteiro `newgame_area1`, 4 700 VBlanks:
  - mesma sequência de 4 838 blocos;
  - 94 hashes de quadro iguais;
  - hashes iguais à referência do HH06.
- Ao contrário do BS (+2 VBlanks no nativo), não há deslocamento de tempo, porque o GD-ROM HLE
  do HH já entrega os dados na hora.
- Blue Stinger N1: mesmo desenho (adaptador, handles `0x7E…`, legado repassado, guardas,
  slots). As diferenças são a camada de driver do 1.09, o estado lógico do drive e a
  verificação contra o manifesto.

## 6. Riscos, limitações e trabalho deixado para depois

- **Prompt real de troca e continuidade.** Não há roteiro determinístico até o fim do disco 1.
  O teste abre a tampa no meio do jogo e prova o caminho de saída. O disco 2 é testado pelo
  boot isolado. A aceitação do `DISK2.ID` no prompt real e o progresso entre discos dependem de
  saves e ficam com o HH11 (continuidade entre discos) e o HH16 (campanha). A janela alterna
  1↔2; o disco que o jogo pede só é conhecido pela mensagem dele.
- A integridade é conferida na primeira abertura de cada arquivo. Uma alteração do arquivo com o
  jogo rodando não é vista (seção 9).
- Os caminhos assíncronos fora do roteiro (ADXF/LSC/Audio64 com `ReqRd32`) não foram
  exercitados. O registro de diretório `\Streams` (dirrec) vai para o legado.
- O estado 7 (sem disco) só foi conferido no disassembly; o teste usa 6.
- A inicialização do gdFs, o driver e o GDFS interno continuam no GD-ROM HLE: 6 setores
  "outro" no modo nativo, contra 158 no legado.

## 7. Inventário de dados privados

Ficaram fora do Git:
- `analysis/hh08/`: traces, logs de arquivos, regressão e saídas do hwscan;
- `reviews/codex-hh08*.{log,md}` e `reviews/hh08-prompt.md`;
- `game-data/`, `generated/`, `build*/`, `dist/`, `ISO/`.

Os testes diferenciais apagam as próprias pastas temporárias, que guardam logs com nomes de
arquivo e hashes, hardlinks e cópias. Versionados só endereços, contagens e fixtures sintéticas.

## 8. Checklist do critério de saída

- [x] Provedor nativo da API de arquivos sobre `data/` instalada (padrão; legado selecionável).
- [x] Nomes, offsets, tamanhos e bytes equivalentes ao legado (P1) e 0 setores de dado pelo HLE (P2).
- [x] Troca de disco como estado lógico nativo, com UI (F8, título) e validação de identidade (versão pelo executável); prompt real no HH11/HH16 (seção 6).
- [x] Mods de arquivos maiores que o original.
- [x] CDI fora do executável do jogo (mapa do linker).
- [x] Disco errado ou corrompido falha com mensagem segura (seção 3.4).
- [x] Regressão integral (seção 4.1) e revisão Codex com os achados aceitos corrigidos (seção 9).

## 9. Revisão independente e próximos passos

### 9.1 Revisão Codex

Comando: `codex exec --sandbox read-only --ephemeral`, com a entrada fechada. Saída em
`reviews/codex-hh08.log` (ignorado).

Confirmado pelo revisor, sem problema:
- os contratos de Close, Seek, Read, ReqRd32, GetStat, GetTransStat e Stop;
- os stubs D09A→+60, D042→+32 e D150→+84;
- a comparação 6/7 em 0x8C04E7B2;
- a separação entre handles legados e nativos;
- `native/` sem Dreamcast;
- a aritmética da fronteira (286 = 277 + 9);
- nenhuma injeção de comando no Builder.

| Achado | Decisão | Correção / motivo | Teste |
|---|---|---|---|
| [ALTA] ReqRd32 só transferia na consulta; Stop/Close entregavam dados | Aceito em parte | Os getters não leem mais nada. Os dados vão para o buffer no próprio ReqRd32 e o estado ocupado é publicado como concluído na observação. Stop não deixar de entregar foi **rejeitado**: o comportamento de referência é o legado, cujo HLE transfere de imediato, e o P1 mostra os blocos do pedido cancelado no log do legado. | `hh08_gdfs_adapter_contracts` (buffer escrito sem consulta; Stop sem pendência); P1 |
| [ALTA] Identidade do disco pela linha `version` do manifesto | Aceito | O host compara o `versionId` do boot (SHA-256 do executável) com o disco pedido | `hh08_discos`: "versão falsificada no manifesto" |
| [ALTA] A troca não valida o prompt real nem o disco pedido | Aceito em parte | O escopo foi registrado com honestidade (seção 6): o prompt real, o `DISK2.ID` e a continuidade dependem de saves (HH11/HH16). A validação de identidade está implementada. | — |
| [MEDIA] Open com dirrec legado recusado | Aceito | `r5 != 0` vai para a função original (GDFS legado) | `hh08_gdfs_adapter_legacy_passthrough_and_guards` |
| [MEDIA] Falha de E/S tratada como inexistente; integridade conferida só uma vez | Aceito em parte | `Error::Io` vira parada com mensagem. Reconferir a cada reabertura foi **rejeitado**: o arquivo seria relido inteiro a cada abertura (os `.AHF` são reabertos várias vezes), e a ameaça coberta é a instalação corrompida, não a alteração com o jogo rodando. Registrado na seção 6. | `hh08_discos`: "arquivo ilegível" |
| [MEDIA] Negativo do P1 podia passar por outro motivo | Aceito | Exige rc 0, os 94 quadros, o mesmo número de blocos e exatamente um bloco diferente | `hh08_arquivos` |
| [ALTA] O Builder adotava e apagava uma pasta sem marcador | Aceito | Uma instalação sem marcador é recusada e não é tocada (era código do HH06) | `hh08_builder_sem_marcador` (antes da correção, a mensagem não seria essa) |
| [MEDIA] O novo boot perdia `--mods` | Aceito | O `--mods` do usuário é repassado | — (o novo boot gráfico não é exercitado em teste) |

### 9.2 Próximos passos (executados automaticamente)

HH08 marcada `CONCLUIDA`. O HH09 ficou `PLANEJADA` (`docs/planos/HH09-PLANO.md`): por instrução
do mantenedor (2026-10-25), a execução aguarda a aprovação dele.
