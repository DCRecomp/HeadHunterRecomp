# HH08 — RELATÓRIO: Arquivos nativos e os dois discos numa instalação só

Data: 2026-10-25. Validação mínima: **V2**. Plano: `docs/planos/HH08-PLANO.md`. Equivalente ao
N1 do Blue Stinger, mais a junção dos dois discos, que o BS (um disco só) não tem.

**Revisão de escopo (mantenedor, 2026-10-25):** "O nativo não pode existir troca de disco, deve
juntar os dois ficando uma coisa já". A primeira entrega (commit `bab49ef`) tinha troca de disco:
tampa lógica, F8 e novo processo. Ela foi substituída por uma **instalação única com os dois
discos juntos**: o jogo segue do disco 1 para o 2 sozinho, sem pedir nada ao jogador. Este
relatório descreve o estado final.

## 1. Objetivo e escopo realizado

Critério de saída (ROADMAP): as rotas dos dois discos funcionam sem acesso a setores nem à
imagem; disco errado ou corrompido falha com mensagem segura. Com a revisão de escopo, o
conteúdo dos dois discos fica numa instalação só, e o jogo nunca pede troca.

| ID | Entrega | Resultado |
|---|---|---|
| HH08.1 | Trace das chamadas gdFs e contratos | `HH_CALL_TRACE` ligado ao host; 208 605 chamadas no roteiro. Contratos do gdFs 1.09 confirmados por disassembly (campos da GDFS) e trace. Achada a camada de driver do 1.09 (tabela 0x8C185DAC). |
| HH08.2 | `native/platform/data_files` | Portado do N1 do BS; 5 testes (`hh08_files_tests`). |
| HH08.3 | Adaptador gdFs, `[native] slots` e `--arquivos` | 18 slots (16 funções gdFs com código gerado, o estado do drive e o gdFsLoadDir); GDFS legado repassado; guardas nas internas; 5 testes (`hh08_gdfs_tests`). |
| HH08.4 | Portões P1/P2 | P1: 4 838 blocos idênticos ao legado (arquivo, deslocamento, bytes, SHA-256). P2: 0 setores de dado pelo GD-ROM HLE. Os 94 hashes de quadro são iguais aos do legado e da referência do HH06. O negativo de mutação é acusado num bloco só. |
| HH08.5 | Os dois discos numa instalação | O Builder prepara e confere os dois CDIs e os junta numa `data\` (`hh-extract merge_runtime_discs`). O adaptador mostra o disco em que o jogo está pela variável do próprio jogo. Sem tampa, F8, `--disco` nem novo processo. Roteiro completo na instalação junta com hashes iguais à referência. |
| HH08.6 | Instalação errada ou corrompida | Pasta só com o disco 2, disco 2 junto de outra versão, versão falsificada, dados ausentes, variante ou `DISK2.ID` ausentes, arquivo alterado, truncado ou ilegível, e mod inválido param com mensagem (rc 1). |
| HH08.7 | Fronteira e produto | 10 entradas da fronteira com provedor nativo (legado 286 → 276, catraca); o produto continua sem CDI (`hh07_produto_sem_cdi`). |
| HH08.8 | Relatório, revisões Codex e commit | Este documento; seção 9. |

O provedor nativo de arquivos é o **padrão**. `--arquivos legado` continua para referência e
rollback; ele só tem o disco 1, porque o GD-ROM HLE monta os metadados desse disco.

Recompilar, nunca emular: o adaptador implementa a API de arquivos sobre arquivos do Windows.
Nenhum setor, FAD ou estado de drive novo é simulado; o drive que o jogo consulta está sempre
pronto.

## 2. Arquivos alterados

- **`native/`**:
  - `platform/data_files.{h,cpp}`, portados do N1 do BS (MIT);
  - `platform/files.h`, com `canonical_path`;
  - `CMakeLists.txt`, com `hh_native_files`.
- **`transitional/`**:
  - `gdfs_adapter.{h,cpp}` (novo; derivado do N1 do BS e adaptado ao 1.09): visão dos discos
    juntos, dirrec e drive sempre pronto;
  - `CMakeLists.txt`.
- **`app/`**:
  - `hh_host.{h,cpp}`: provedor nativo, versão do boot, lista conferida, mods e `HH_CALL_TRACE`;
  - `hh_discs.{h,cpp}` (novo): `resolve_install`;
  - `hh_verified_files.{h,cpp}` (novo): presença, tamanho e SHA-256 contra a lista;
  - `hh_run.cpp`: `--arquivos` e `--mods`;
  - `hh_window.cpp`: as mesmas opções; exige a instalação com os dois discos;
  - `CMakeLists.txt`.
- **`builder/builder_main.cpp`**:
  - `--disc2` obrigatório;
  - os dois discos preparados, conferidos e juntos;
  - instalação sem marcador recusada.
- **`tools/hh-extract/`**:
  - `src/runtime_data_merge.cpp` (novo) e `include/hh_extract/runtime_data.hpp`: `merge_runtime_discs`;
  - `src/main.cpp`: `--juntar-discos`;
  - `src/runtime_data_cdi.cpp`: comentário do manifesto ("Head Hunter");
  - `CMakeLists.txt`.
- **`config/`**:
  - `headhunter.toml`, com `[native] slots` (18);
  - `fronteiras.toml` (10 entradas nativas);
  - `metricas_base.toml`.
- **Testes**:
  - `tests/unit/test_hh08_{files,gdfs}.cpp`;
  - `tests/diff/hh08_{arquivos,discos}.py`;
  - `tests/hh08/check_builder_unmarked.cmake`;
  - `tests/CMakeLists.txt`.
- **Regressão**: `tools/dev/regressao.py`, com a etapa `portoes_hh08`, `HH_DATA_DIR2`, `HH_DISC2` e o Builder com os dois discos.
- **Documentos**: `docs/planos/HH08-PLANO.md`, `docs/planos/HH09-PLANO.md` (planejado, aguarda aprovação), este relatório e `STATUS.md`.

## 3. Evidências e decisões

### 3.1 Contratos do gdFs 1.09 (HH08.1)

Detalhes em `docs/planos/HH08-PLANO.md`, "HH08.1 — resultados". Em resumo:
- as funções que recebem GDFS são chamadas diretas, com os mesmos campos do 1.05: +8 FAD, +12
  bytes, +16 setores, +20 posição, +24 transferidos, +76 aberto, +78 modo, +80 estado e +82
  erro;
- as funções por nome e as de drive passam por stubs que leem `*0x8C188D08`, a tabela
  0x8C185DAC instalada por 0x8C15CFDC;
- o adaptador substitui os stubs que o jogo chama: Open 0x8C15D09A, ChangeDir 0x8C15C7A0,
  LoadDir 0x8C15D042 e o estado do drive 0x8C15D150;
- a tabela e o driver continuam legados: inicialização, TOC e o GDFS interno 0x8C28AB80.

### 3.2 Leituras assíncronas

O servidor do gdFs 1.09 fica atrás da tabela do driver. O comportamento de referência é o do
legado, cujo GD-ROM HLE transfere de imediato: o P1 mostra os blocos entregues mesmo quando o
jogo cancela o pedido ao pular o filme.

O adaptador:
- escreve os dados no buffer dentro do próprio `gdFsReqRd32`;
- publica "ocupado" (2, 0 bytes) até a observação seguinte do arquivo ou o próximo pedido;
- Stop e Close encerram o pedido.

Nenhum caminho depende de consulta para receber dados.

### 3.3 Os dois discos juntos (HH08.5)

O que difere entre os discos (manifestos de `hh-extract --runtime`):
- disco 1: 212 arquivos; disco 2: 160; 69 nomes comuns; união de 303;
- dos comuns, só dois diferem: `1ST_READ.BIN` e `MOVIES/SCENE0.4XM`;
- os executáveis diferem só na dword 0x8C185E28, o FAD do descritor ISO, lido apenas pela
  inicialização legada do driver (0x8C15B870–0x8C15B884, que confere "CD001");
- `DISC/DISK1.ID` e `DISC/DISK2.ID` têm nomes diferentes.

Junção (`merge_runtime_discs`, Builder):
- a árvore do disco 1 recebe os 91 arquivos só do disco 2;
- os 67 iguais ficam uma vez;
- a variante do disco 2 de `SCENE0.4XM` vai para `disc\_DISCO2\`;
- o executável é o do disco 1;
- `arquivos.manifest` lista tamanho e SHA-256 dos 304 arquivos.

Como o jogo sabe o disco, conferido no disassembly; o Codex confirmou os cinco literais da variável:
- 0x8C09EBC4 tenta `DISC\disk%d.id` com N de 1 a 3 e grava o primeiro N encontrado no global
  0x8C15174C, devolvido pelo getter 0x8C09EC68;
- também escrevem nesse global: o comando de script 0x8C02A974 (grava 2), 0x8C086C64 no menu
  0x8C0866A4 (grava 2) e 0x8C072EDE (grava 1);
- o disco vai para o save (0x8C09C5EC, +72) e o menu de carregar compara com o atual (0x8C0EEC34).

Decisão: o adaptador mostra o disco em que o jogo está, pela própria variável dele.
- Com 0 ou 1, o jogo vê `DISK1.ID` e os arquivos comuns.
- Com 2, vê `DISK2.ID` (o `DISK1.ID` some) e a variante `_DISCO2`, e a checagem do jogo
  encontra o disco 2.
- A pasta `_DISCO2` nunca aparece para o jogo.
- O drive (0x8C15D150) está sempre pronto: o caminho de "tampa aberta" (0x8C04E754 →
  0x8C04E470 → BIOS) nunca é acionado.

Medido na instalação junta, no roteiro `newgame_area1`:
- o jogo abre `disk1.id` (GDFS nativo) e não procura o 2;
- os 94 hashes são iguais à referência;
- 0 setores de dado pelo HLE.

A troca para o disco 2 pela variável está coberta pelo unitário
`hh08_gdfs_merged_discs_follow_game_variable`.

### 3.4 Instalação errada ou corrompida (HH08.6)

| Caso | Onde é pego | Mensagem |
|---|---|---|
| Pasta só com o disco 2 | `resolve_install` | "… 'hedhunter_v1006_b', esperado o disco 1 … com o disco 2 junto" |
| Disco 2 junto de outra versão | `resolve_install` (`arquivos.manifest`) | "o disco 2 junto … e de outra versao" |
| Versão falsificada no `disc.manifest` | boot (SHA-256 do executável) | "… sao da versao hedhunter_v1006_b … (dados de outro disco)" |
| Dados ausentes | `resolve_install` | "dados do jogo ausentes …" |
| Variante ou `DISK2.ID` ausente | `VerifiedFileSystem` (presença no boot) | "arquivo do jogo ausente ou ilegivel: …" |
| Arquivo alterado (1 byte) | `VerifiedFileSystem` (SHA-256 na primeira abertura) | "arquivo do jogo alterado ou corrompido: AUDIO64.DRV" |
| Arquivo truncado | verificação dos dados de execução no boot | "… Builder" |
| Arquivo ilegível (trava exclusiva) | presença no boot / adaptador (`Error::Io`) | "… ilegivel …" |
| Mod que não substitui nada | `DataFileSystem` | "NADA.BIN: nao substitui nenhum recurso" |
| Instalação sem o disco 2 | `HeadHunter.exe` | "a instalacao … nao tem o disco 2 …" |
| CDI do disco errado no Builder | Builder (versão do disco) | "o CDI de --disc2 … e a versao …, esperada …" |

Todos terminam com rc 1 e "PARADA", sem travar e sem ler setor.

### 3.5 Mods

`mods\assets` fica ao lado do executável (ou `--mods`), com a mesma árvore de `data\disc`. O mod
maior que o original é aplicado sem conferência de hash e registrado no log.

### 3.6 Fronteira e catraca (HH08.7)

Com `[native] slots`, `hh-hwscan --fronteira-gerar` marca nativas 10 entradas da fronteira:
0x8C15C7A0, C7B8, C7F0, C83E, C8E2, CA48, CD68, D042, D09A e D150.

Efeitos:
- `fronteira.provedor.legado` 286 → 276 e `nativo` 0 → 10;
- `legado.subsistema.tempo` 42 → 38;
- 0 violações;
- a catraca passou antes de a base ser atualizada.

`legado.subsistema.arquivos` = 1 é a inicialização do sistema 0x8C15FA28 (gdFsInit), que fica
legada, como no BS.

## 4. Comandos de build/teste e resultados

```text
cmake --preset vs && cmake --build --preset vs-release -- -m
ctest --preset vs-release
hh-extract --juntar-discos <dados do disco 1> <dados do disco 2> [--links]
python -I tests/diff/hh08_arquivos.py build/app/Release/hh-run.exe .                      (P1/P2)
python -I tests/diff/hh08_discos.py build/app/Release/hh-run.exe build/tools/hh-extract/Release/hh-extract.exe .
hh-hwscan ... --fronteira config/fronteiras.toml --metricas-base config/metricas_base.toml -> rc 0
python -I tools/dev/regressao.py (build, ctest, portoes_hh05, portoes_hh07, portoes_hh08, roteiro_area1, builder)
```

### 4.1 Regressão integral

Executada no código final, com os discos juntos e as correções das duas revisões (log em
`analysis/hh08/regressao_unico_final.log`):

```text
[ok] build (7 s)
[ok] ctest (9 s)            30/30 (os portões com dados privados rodam nas etapas seguintes)
[ok] portoes_hh05 (5 s)     hh_boot_regression, hh_first_frame (provedor nativo padrão)
[ok] portoes_hh07 (239 s)   hh07_fechamento (fronteira com 10 entradas nativas), hh07_ordem
[ok] portoes_hh08 (275 s)   hh08_arquivos (P1/P2 + mutação), hh08_discos (discos juntos, negativos, mods)
[ok] roteiro_area1 (185 s)  hashes iguais a snapshots/hh06/area1_hashes.txt
[ok] builder (228 s)        --disc + --disc2 conferidos e juntos numa data; HeadHunter.exe instalado reproduz a referência
regressao completa — fim rc=0
```

## 5. Comparação com referência/oráculo

- Legado × nativo no roteiro `newgame_area1`, 4 700 VBlanks: mesma sequência de 4 838 blocos e
  94 hashes de quadro iguais. A instalação junta dá os mesmos 94 hashes da referência do HH06.
- Não há deslocamento de tempo (ao contrário do +2 VBlanks do BS), porque o GD-ROM HLE do HH já
  entrega os dados na hora.
- Blue Stinger N1: mesmo desenho (adaptador, handles `0x7E…`, legado repassado, guardas,
  slots). As diferenças são a camada de driver do 1.09, a junção dos discos com a visão pela
  variável do jogo e a verificação contra a lista do Builder.

## 6. Riscos, limitações e trabalho deixado para depois

- **Passagem real do disco 1 para o 2: não verificada dinamicamente.** Não há roteiro
  determinístico até o fim do disco 1, nem saves para carregar um jogo do disco 2.
  - Prova estática: os escritores da variável (0x8C02A974, 0x8C086C64) e a checagem do
    `DISKn.ID`.
  - Prova unitária: a visão segue a variável.
  - Fica com o HH11 (saves e continuidade) e o HH16 (campanha). Lá entram o carregamento de um
    save do disco 2 numa inicialização limpa (o menu 0x8C0EEC34 compara o disco do save) e a
    ausência de qualquer espera por tampa.
  - Risco conhecido: se o jogo, depois de mudar para o disco 2, esperar o jogador abrir a tampa
    antes de reconferir, o drive "sempre pronto" o deixaria esperando. A correção nativa seria
    no ponto em que ele espera, achado no HH11/HH16.
- O modo legado só tem o disco 1; o conteúdo do disco 2 existe só no provedor nativo.
- A integridade do conteúdo é conferida na primeira abertura; uma alteração com o jogo rodando
  não é vista. Presença e tamanho são conferidos no boot.
- Os caminhos assíncronos fora do roteiro (ADXF/LSC/Audio64) não foram exercitados. O dirrec
  tem teste unitário, mas nenhuma abertura por dirrec aparece no roteiro.
- A inicialização do gdFs, o driver e o GDFS interno continuam no GD-ROM HLE do disco 1: 6
  setores "outro" no modo nativo, contra 158 no legado.

## 7. Inventário de dados privados

Ficaram fora do Git:
- `analysis/hh08/`: traces, logs de arquivos, regressões e saídas do hwscan;
- `reviews/codex-hh08*`, `reviews/hh08*-prompt.md`;
- `game-data/` (inclusive `game-data/runtime/unico`, a junção de desenvolvimento por hardlinks),
  `generated/`, `build*/`, `dist/`, `ISO/`.

Os testes diferenciais apagam as próprias pastas temporárias (hardlinks, cópias e logs com nomes
de arquivo e hashes). Versionados só endereços, contagens e fixtures sintéticas.

## 8. Checklist do critério de saída

- [x] Provedor nativo da API de arquivos sobre `data/` instalada (padrão; legado selecionável).
- [x] Nomes, offsets, tamanhos e bytes equivalentes ao legado (P1) e 0 setores de dado pelo HLE (P2).
- [x] Os dois discos numa instalação, sem troca de disco (revisão de escopo do mantenedor); o
  jogo vê o disco em que está; a passagem real fica para verificação no HH11/HH16 (seção 6).
- [x] Mods de arquivos maiores que o original.
- [x] CDI fora do executável do jogo (mapa do linker).
- [x] Instalação errada ou corrompida falha com mensagem segura (seção 3.4).
- [x] Regressão integral (seção 4.1) e revisões Codex com os achados aceitos corrigidos (seção 9).

## 9. Revisões independentes e próximos passos

### 9.1 Primeira revisão Codex (entrega com troca de disco)

Saída em `reviews/codex-hh08.log`. Foram 8 achados: 5 aceitos e 3 aceitos em parte. Os que
continuam valendo depois da revisão de escopo:

| Achado | Decisão | Correção / motivo | Teste |
|---|---|---|---|
| [ALTA] ReqRd32 só transferia na consulta | Aceito em parte | Os dados vão para o buffer no próprio ReqRd32 e os getters não leem nada. Stop não deixar de entregar foi rejeitado: o legado (HLE imediato) entrega, e o P1 mostra. | `hh08_gdfs_adapter_contracts`; P1 |
| [ALTA] Identidade pela linha `version` do manifesto | Aceito | Versão do boot pelo SHA-256 do executável | `hh08_discos`: versão falsificada |
| [MEDIA] Open com dirrec recusado | Aceito, depois ampliado (9.2) | — | `hh08_gdfs_dirrec_opens_native` |
| [MEDIA] Falha de E/S tratada como inexistente; integridade conferida uma vez | Aceito em parte | `Error::Io` vira parada. Reconferir a cada abertura foi rejeitado pelo custo, com presença e tamanho no boot. | `hh08_discos`: ilegível |
| [MEDIA] Negativo do P1 por outro motivo | Aceito | rc 0, 94 quadros, mesmo número de blocos e um bloco diferente | `hh08_arquivos` |
| [ALTA] Builder adotava pasta sem marcador | Aceito | Recusa e não toca | `hh08_builder_sem_marcador` |

Os achados sobre a troca de disco (validação do prompt de troca; o novo processo perdendo
`--mods`) perderam o objeto: a troca saiu do produto.

### 9.2 Segunda revisão Codex (discos juntos)

Saída em `reviews/codex-hh08b.log`. O revisor confirmou:
- os cinco literais da variável 0x8C15174C e seus escritores;
- os números da união (303 nomes, 2 diferentes, sem colisão com `_DISCO2`);
- o executável do disco 1 preservado;
- o FAD 0x8C185E28 usado só na inicialização legada;
- a conferência do hash do boot;
- os negativos.

| Achado | Decisão | Correção | Teste |
|---|---|---|---|
| [MEDIA] Abertura com dirrec continuava no legado (só disco 1) | Aceito | gdFsLoadDir (0x8C15D042) com slot: o original preenche o registro e o adaptador guarda o diretório; o Open com esse dirrec abre pelo provedor nativo | `hh08_gdfs_dirrec_opens_native` |
| [MEDIA] Instalação incompleta ou variante ausente falhava aberta | Aceito | `HeadHunter.exe` exige `arquivos.manifest`; presença e tamanho de todos os arquivos conferidos no boot | `hh08_discos`: variante ausente, `DISK2.ID` ausente |
| [MEDIA] Passagem real e save do disco 2 sem cobertura | Aceito como limitação | Registrado na seção 6, com o escritor adicional 0x8C086C64 | — (HH11/HH16) |
| [MEDIA] `--links` digitado errado movia os arquivos | Aceito | Opção validada e documentada na ajuda | — (CLI de desenvolvimento) |

### 9.3 Próximos passos

HH08 marcada `CONCLUIDA`. O HH09 está `PLANEJADA` (`docs/planos/HH09-PLANO.md`): por instrução
do mantenedor (2026-10-25), a execução aguarda a aprovação dele.
