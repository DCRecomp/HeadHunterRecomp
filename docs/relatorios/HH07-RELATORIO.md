# HH07 — RELATÓRIO: Fundação do port nativo e fronteiras mensuradas

Data: 2026-10-25. Validação mínima: **V2**. Plano: `docs/planos/HH07-PLANO.md`. Equivalente ao
N0 do Blue Stinger. Plano do port nativo: `docs/PLANO-PORT-NATIVO.md`.

## 1. Objetivo e escopo realizado

Critério de saída (ROADMAP): o comportamento permanece igual, o custo é medido e toda
dependência legada tem dono/fronteira ou bloqueio explícito.

| ID | Entrega | Resultado |
|---|---|---|
| HH07.1 | `hh-hwscan` portado | Saídas idênticas nos dois discos (sdk_funcs, fronteira, fechamento, manifesto, grafo, métricas) |
| HH07.2 | `sdkid` com as famílias do HH; nome só com duas evidências | Corrigida a extração de dono e a nomeação; 13 funções nomeadas, nomes únicos; camadas reclassificadas com conferência por strings |
| HH07.3 | Fronteira declarada, fechamento, dominância, estático + sonda | 286 entradas; 0 violações; 2 dependências de hardware do jogo declaradas com dono HH14; casos negativos falham |
| HH07.4 | Catraca `config/metricas_base.toml` | Teste de catraca (unitário e sobre o executável real) |
| HH07.5 | Alvos `native`/`legacy`/`transitional`; interfaces tipadas; "native limpo" | `hh_native_api`, `hh_legacy`, `hh_transitional`; testes com caso negativo |
| HH07.6 | Handles com geração, ABI guest, registro de fronteiras, slots e custo | Testes portados; `HH_MODS`; hashes iguais com e sem slots; custo dentro do ruído |
| HH07.7 | Inventário de threads/reentradas/eventos e teste de ordem | Trace idêntico em duas execuções (4 700 VBlanks) |
| HH07.8 | Produto sem leitura de CDI | Separação `runtime_data_cdi.cpp`; teste sobre o mapa do linker, com caso negativo |
| HH07.9 | Relatório + revisão Codex | Este documento; revisão na seção 9 |

Nenhum subsistema mudou de provedor: tudo continua `legado`. Recompilar, nunca emular: nada
desta etapa executa código do guest fora do C++ gerado.

## 2. Arquivos alterados

- **`translator/hwscan/`**:
  - `hwscan.cpp`: base pura de segmento não é endereço;
  - `sdkid.{h,cpp}`: dono de mensagem e regra de nomeação;
  - `closure.{h,cpp}`, `boundary_io.{h,cpp}`, `metrics.cpp`, `main.cpp`: `[[dependencia_hw]]`, obsoletas e catraca;
  - `pipeline.{h,cpp}`: `constantes_nao_hw`.
- **`translator/app/config.{h,cpp}`**: `[hwscan] constantes_nao_hw`.
- **`config/`**:
  - `headhunter.toml`: 4 constantes não-hardware com evidência;
  - `fronteiras.toml` (novo; 286 entradas e 2 `[[dependencia_hw]]`);
  - `metricas_base.toml` (novo).
- **`native/`**: `CMakeLists.txt` (`hh_native_api`); `audio/audio.h`, `platform/{files,input,saves,time}.h`, `render/render.h`, `video/video.h` (portados do N0 do BS, MIT, comentários do HH); `core/result.h`.
- **`legacy/CMakeLists.txt`** (`hh_legacy`) e **`transitional/CMakeLists.txt`** (`hh_transitional`).
- **`tools/hh-extract/`**: `src/runtime_data_cdi.cpp` e `src/runtime_data_detail.hpp` (novos), `src/runtime_data.cpp` e `CMakeLists.txt` (separação da parte que lê o CDI).
- **`app/CMakeLists.txt`**: opção `HH_MODS` e `/MAP` do `HeadHunter.exe`.
- **`CMakeLists.txt`**: `BUILD_TESTS` antes dos subdiretórios.
- **`.gitignore`**: `build-*/`.
- **Testes**:
  - `tests/unit/test_hh07_hwscan.cpp` (18 testes; 16 do N0 do BS + 2 novos, com acréscimos);
  - `tests/unit/test_hh07_transitional.cpp`;
  - `tests/hh07/{native_api_check.cpp, check_native_clean.cmake, check_product_map.cmake}`;
  - `tests/fixtures/hh07/` (casos negativos);
  - `tests/diff/hh07_fechamento.py`, `tests/diff/hh07_ordem.py`;
  - `tests/CMakeLists.txt`.
- **Ferramentas**: `tools/dev/hh07_slots_custo.py` (novo); `tools/dev/regressao.py` (etapa `portoes_hh07`).
- **Documentos**: `docs/PLANO-PORT-NATIVO.md` (novo), este relatório, `docs/planos/HH07-PLANO.md`, `STATUS.md`.

## 3. Evidências e decisões

### 3.1 Identificação do SDK (HH07.2): defeito encontrado e corrigido

A amostra conferida no disassembly mostrou nomes errados, herdados da heurística do BS, que
tratava o nome como "dica":

- **Nomes repetidos**:
  - 0x8C176760 e 0x8C17656C eram ambos `mwSndOpenPort`. A 0x8C176760 é o fechamento: a
    mensagem "E1226:acDigiClose fail in mwSndOpenPort" foi copiada pelo CRI;
  - 0x8C1767FA e 0x8C1768B2 eram ambos `mwSndSetNumChan`. As mensagens deles citam
    `mwSndPlay` e `mwSndStop`.
- **Constante tomada como nome**: 0x8C02B39E recebia `ADXT_STAT_PLAYEND` por
  "(ADXT_STAT_PLAYEND)". A função é o overlay de depuração do jogo (`DebugFont`,
  "STREAM %d (%s)").
- **Chamador tomado como dono**: 0x8C090B1C recebia `kmCreateVertexBuffer` por
  "kmCreateVertexBuffer() failed!". Ela é a chamadora que relata a falha, e isso a semeava
  como Kamui.

Correção em `owner_of`:
- "(ID)" só conta se ID tiver minúscula;
- "ID() texto" sem `:` é relato do chamador.

Regra nova de nomeação:
- candidato único nas mensagens da função;
- nome único na imagem;
- outra evidência de tipo diferente (módulo, faixa, assinatura, padrão, BIOS) na mesma família.

Testes:
- os casos novos de `hh07_message_owner_extraction` falham sem a correção (2 falhas, conferido
  com `git stash` só de `sdkid.{h,cpp}`) e passam com ela;
- `hh07_fechamento` confere no executável real que os nomes são únicos e que cada nomeada tem
  evidência além da mensagem.

Consequência: a semente Kamui falsa arrastava, por contiguidade, 138 funções do jogo para o
SDK e deixava outras 465 sem camada (`analysis/hh07/mudancas_camada.tsv`, todas por
`bloco`). As strings dessas funções são do jogo:
- `headhunter\texture\intermission\...`, `AHFData\Intermission.AHF`;
- "ERROR: The texture was not a 'PVR' format!", `Fonts\`, "Text stack overflow";
- "GAME MEMORY : %d", "Dreamcast Fishing Controller".

| | antes | depois |
|---|---|---|
| jogo / SDK / desconhecida / conflito | 2 429 / 1 654 / 615 / 1 | 3 032 / 1 515 / 151 / 1 |
| funções nomeadas | 21 (com repetições) | 13 (únicas, todas CRI com `modulo:`) |
| fronteira | 538 | 286 |
| indiretos não resolvidos | 744 | 672 |

### 3.2 Constantes que não são hardware (HH07.1/HH07.3)

- `plausible_constant`: base pura de segmento (0xA0000000, 0xC0000000…) é máscara de
  conversão de ponteiro. Antes, 0xA0000000 passava pela regra da BIOS. O caso novo do teste
  falha sem a correção (conferido com `git stash`) e passa com ela.
- `[hwscan] constantes_nao_hw` (`config/headhunter.toml`): pcs de literais conferidos no
  disassembly como cores ARGB passadas por argumento:
  - 0x8C0B089E e 0x8C0B0996 (0xE0E02020/0xE020E020, função 0x8C0B07B8);
  - 0x8C0CE340 e 0x8C0CE35E (0x90000000 para 0x8C070114, que usa `(r4 & 0xFF000000) >> 24`
    como alfa).

  O pipeline falha se um pc listado deixar de carregar uma constante (lista obsoleta), e a
  métrica `hw.constantes_excluidas_config` deixa a exclusão visível.

### 3.3 Fronteira, fechamento e dependências de hardware do jogo (HH07.3)

`hh-hwscan` com `--sonda analysis/hh07/sonda_area1.tsv` (roteiro `newgame_area1`, `HH_HW_PROBE`):
- 2 acessos do jogo ao hardware, estáticos e da sonda;
- nenhum é falso positivo; os dois são o caminho de vértices por store queue.

| Função | Evidência (estática) | Evidência (sonda) |
|---|---|---|
| 0x8C090974 | Escreve QACR0/QACR1 (pcs 0x8C0909A4/A8/FC, 0x8C090A00); grava em 0x8C240048 o ponteiro `(end & 0x03FFFFFF) \| 0xE0000000` do buffer da tabela 0x8C20CD20; `pref` em 0x8C090A20; sem chamador direto | 668 344 escritas em cada QACR; flushes para a RAM 0x0C400000–0x0C610000 |
| 0x8C0C4CE0 | Copia 32 bytes (0x8C15982A) para `*0x8C240048`; `pref` em 0x8C0C4EBC/ED4/F34/F4C; avança 32 | Flushes para 0x0C5C0000 (15 768) e 0x0C5D0000 (120 888) |

`pref` só dá flush em 0xE0000000–0xE3FFFFFF (`runtime/cpu/sh4_ops.h:306`), e o destino vem
do QACR (`sh4_sq_target`): os dois são flushes reais de store queue para buffers na RAM.

Decisão: registrar como dependências do jogo, sem esconder. A decisão está coberta pelo
critério de saída ("dono/fronteira ou bloqueio explícito"):
- `[[dependencia_hw]]` exige subsistema, dono `HHxx`, evidência e `regioes` (regiões do
  mapa de hardware que a função pode acessar): 0x8C090974 = `store_queue` e `ccn`,
  0x8C0C4CE0 = `store_queue`;
- sem a declaração, é violação; um acesso da função a uma região fora da lista também é
  violação (revisão Codex, seção 9);
- com `--sonda`, cada par (função, região) declarado precisa de acesso visto pela sonda; a
  referência estática não confirma. Par sem acesso dinâmico é "obsoleto" e falha;
- as contagens têm catraca: `fechamento.dependencias_hw_declaradas` = 2 e
  `fechamento.dependencias_hw_regioes` = 3.

Dono: **HH14**, que captura o caminho na IR de render. A hipótese de buffer de vértices do
Kamui2 enviado depois ao TA está registrada como hipótese.

Resultado: 286 entradas (285 diretas, 1 indireta), 3 032 funções alcançadas, **0 violações**
(SDK e hardware), 2 acessos declarados, 672 indiretos não resolvidos (catraca).

Casos negativos (`tests/diff/hh07_fechamento.py`):

| Caso | rc |
|---|---|
| positivo | 0 |
| sem a entrada 0x8C010080 | 3 |
| sem a `[[dependencia_hw]]` 0x8C090974 | 3 |
| sem a `[[dependencia_hw]]` 0x8C0C4CE0 | 3 |
| 0x8C090974 declarada sem a região `ccn` | 3 |
| dependência extra 0x8C0C3C54 (obsoleta) | 3 |
| sonda sem as escritas de 0x8C090974 nos QACR (a estática continua) | 3 |
| catraca de `dependencias_hw_declaradas` abaixo do valor atual | 4 |
| catraca de `dependencias_hw_regioes` abaixo do valor atual | 4 |

Cada caso confere também o motivo: a linha `contorna_fronteira`/`acesso_hw` da função em
`fechamento.tsv`, a mensagem `DEPENDENCIA_HW OBSOLETA` com função e região, ou
`REGRESSAO: <chave>`. O teste exige ainda as 13 funções nomeadas (seção 3.1).

### 3.4 Catraca (HH07.4)

`config/metricas_base.toml` (96 chaves, só contagens). Chaves com catraca:
- `hw.refs.camada.jogo` = 8;
- `fechamento.violacoes.{sdk,hw}` = 0/0;
- `fechamento.dependencias_hw_declaradas` = 2;
- `fechamento.dependencias_hw_regioes` = 3;
- `fechamento.indireto_nao_resolvido` = 672;
- `camada.desconhecida` = 151;
- `pref.nao_resolvido.funcoes.jogo` = 2;
- `sq.literal.funcoes.jogo` = 30;
- `camada.conflito` = 1;
- `fronteira.provedor.legado` = 286;
- `tempo.consumidores.ciclos` = 2.

Inclui as métricas do código gerado (`--gerado`: 13 arquivos, 1 098 862 linhas, 22 690
`HH_CALL`, 1 107 despachos) e do runtime (`--runtime`: 2 consumidores funcionais do contador
de ciclos, `sh4_hw_m3.cpp` e `sh4_service.cpp`).

A subida de `hw.refs.camada.jogo` (0→8), `sq.literal` (19→30) e `pref.nao_resolvido` (1→2) em
relação à primeira medição não é regressão. É a correção da seção 3.1: código do jogo que
estava rotulado como SDK. Fica registrada aqui.

### 3.5 Alvos e interfaces (HH07.5)

- `hh_native_api` (INTERFACE) com as interfaces do N0 do BS (MIT):
  - `files`, `time`, `input`, `saves`, `audio`, `render`, `video`;
  - comentários refeitos para o HH: HH08–HH14, AUDIO64.DRV, 4X Movie com FFmpeg externo,
    Vulkan.
- `hh_legacy` (INTERFACE → `hh_runtime`) e `hh_transitional` (`boundary_registry.cpp`).
- Testes:
  - `hh07_native_api`: cabeçalhos compilam só com `native/` no include path;
  - `hh07_native_limpo`: proíbe `Sh4Context`, `sh4_`, `runtime/`, `dcmap`, `generated/` e
    endereços guest;
  - `hh07_native_limpo_negativo`: fixture com `Sh4Context` precisa falhar com a mensagem
    exata.

### 3.6 Interoperabilidade, slots e custo (HH07.6)

- `hh07_transitional_tests`:
  - handles com geração;
  - `GuestMemory` recusa PVR, P4, store queue, VRAM e acesso desalinhado;
  - convenção de chamada;
  - `GuestStruct` com versão;
  - registro de fronteiras legado/nativo/mod.
- Opção `HH_MODS` (código gerado com slot em todas as funções). Comparação com o mesmo
  roteiro, 4 700 VBlanks e execuções alternadas (`tools/dev/hh07_slots_custo.py`):

| Build | Rodada 1 | Rodada 2 | Mediana |
|---|---|---|---|
| normal | 34,947 ms/VBlank | 33,627 | 34,287 |
| slots (`HH_MODS=ON`) | 33,159 | 33,242 | 33,200 |

  Os 94 hashes de quadro são **idênticos** nas 4 execuções. A diferença (−3 %) fica dentro do
  ruído: outras compilações rodaram em paralelo durante parte da medição, e a primeira
  execução normal foi a mais lenta. Conclusão igual à do N0 do BS: os slots não têm custo
  mensurável. Ressalva: o render por software domina o tempo, e a medida é refeita depois do
  HH14.
- `[native] slots` continua vazio: a primeira fronteira migrável (arquivos) entra no HH08.

### 3.7 Ordem de eventos (HH07.7)

`tests/diff/hh07_ordem.py`, roteiro `newgame_area1`, 4 700 VBlanks, duas execuções:
- trace idêntico, sha256 `c2aa0b2386d6bb39…`;
- V = 4 700; I = 39 625 (INTEVT 0x320: 34 980; 0x360: 4 645);
- N = 8; T = 74 320; J = 74 312; R = 0;
- ciclos não decrescentes.

Inventário em `analysis/hh07/ordem_inventario.tsv` e resumo na seção 4 do
`PLANO-PORT-NATIVO.md`.

### 3.8 Produto sem CDI (HH07.8)

O teste novo sobre o mapa do linker achou `hh_extract_core:cdi.obj` dentro do `HeadHunter.exe`
(a constante `kMark`):
- `runtime_data.cpp` tinha `prepare_runtime_data(CdiImage&)` junto da leitura dos dados;
- o linker resolvia `CdiImage` antes do `/OPT:REF`, e o objeto ficava.

Correção: a preparação e a conferência a partir do CDI foram para `runtime_data_cdi.cpp`, e
os auxiliares comuns para `runtime_data_detail.hpp`.

Teste `hh07_produto_sem_cdi`:
- proíbe `CdiImage`, `cdi.obj`, `extract.obj`, `prepare_runtime_data` e
  `verify_runtime_data`;
- exige `boot_read_data`;
- o negativo (`hh07_produto_sem_cdi_negativo`) usa um mapa sintético com `CdiImage` e
  `cdi.obj`.

## 4. Comandos de build/teste e resultados

```text
cmake -S . -B build && cmake --build build --config Release -- -m        -> sem erros/avisos
build\tests\Release\hh07_hwscan_tests.exe                                -> 18 testes, 0 falhas
ctest -C Release -E "hh_boot_regression|hh_first_frame" (build/)          -> 21/21 (antes do hh07_ordem)
python -I tests/diff/hh07_fechamento.py build/translator/Release/hh-hwscan.exe .
    -> 7 casos ok (positivo, nomes unicos, 4 negativos de fronteira/dependencia, catraca)
HH_DATA_DIR=game-data/runtime/disc1 python -I tests/diff/hh07_ordem.py build/app/Release/hh-run.exe \
    tools/dist/roteiros/newgame_area1.txt --inventario analysis/hh07/ordem_inventario.tsv
    -> 8 verificacoes ok, 0 falhas (5 min 19 s)
cmake -S . -B build-mods -DHH_GENERATED_DIR=... -DHH_MODS=ON -DBUILD_TESTS=OFF -DHH_BUILD_WINDOW=OFF
HH_DATA_DIR=... python -I tools/dev/hh07_slots_custo.py build/app/Release/hh-run.exe \
    build-mods/app/Release/hh-run.exe tools/dist/roteiros/newgame_area1.txt  -> rc 0, hashes iguais
hh-hwscan config/headhunter.toml <disco1|disco2>/1ST_READ.BIN analysis/hh07/<disco> --sonda ... \
    --fronteira config/fronteiras.toml --metricas-base config/metricas_base.toml
    -> rc 0 nos dois discos; sdk_funcs/fronteira/fechamento/manifesto/grafo/metricas identicos
python -I tools/dev/regressao.py (build, ctest, portoes_hh05, portoes_hh07, roteiro_area1, builder)
    -> ver 4.1
```

### 4.1 Regressão integral

Refeita depois das correções da revisão Codex (log em `analysis/hh07/regressao2.log`; a
execução anterior às correções também passou, `analysis/hh07/regressao.log`):

```text
[ok] build (6 s)
[ok] ctest (10 s)           25/25 (3 pulados por falta de dados, cobertos nas etapas seguintes)
[ok] portoes_hh05 (5 s)     hh_boot_regression, hh_first_frame
[ok] portoes_hh07 (265 s)   hh07_fechamento (8 s), hh07_ordem (257 s)
[ok] roteiro_area1 (127 s)  hashes iguais a snapshots/hh06/area1_hashes.txt
[ok] builder (193 s)        HeadHunter.exe instalado reproduz a referência
regressao completa — fim rc=0
```

## 5. Comparação com referência/oráculo

- Comportamento: os hashes do roteiro `newgame_area1` são iguais à referência do HH06
  (`snapshots/hh06/area1_hashes.txt`) no `hh-run`, no build com slots e no `HeadHunter.exe`
  instalado pelo Builder (regressão).
- Blue Stinger (N0): mesmo método e mesmos formatos (fronteira, catraca, sonda, ordem,
  produto sem CDI). As diferenças estão nas seções 3.1 e 3.3: a regra de nomeação ficou mais
  estrita, e `[[dependencia_hw]]` entrou porque, ao contrário do BS, o Head Hunter acessa
  store queues e QACR a partir do próprio código.

## 6. Riscos, limitações e trabalho deixado para depois

- A camada "jogo" de 3 032 funções depende de `bloco:jogo` (uma evidência). Cada etapa
  confirma as funções que tocar.
- 672 alvos indiretos sem prova (com catraca); 151 funções sem camada.
- Há 30 funções do jogo com a base das store queues em literal, e só 2 foram vistas pela
  sonda: o HH14 precisa de cobertura maior (matriz de campanha).
- A sonda cobre só `newgame_area1`. Roteiros novos podem mostrar acessos novos; o fechamento
  então falha, e isso é desejado.
- O custo dos slots foi medido com o render por software; repetir depois do HH14.
- `[native] slots` está vazio até o HH08.

## 7. Inventário de dados privados

Ficaram fora do Git (ignorados):
- `analysis/hh07/`: saídas do scan, sonda, inventário de ordem, medição de slots, logs;
- `build/`, `build-mods/` (`build-*/` adicionado ao `.gitignore`; contém código gerado);
- `game-data/`, `generated/`, `snapshots/`, `reviews/`, `ISO/`.

Versionados só metadados:
- endereços, contagens e evidências textuais em `config/fronteiras.toml`,
  `config/metricas_base.toml` e `config/headhunter.toml`;
- fixtures sintéticas em `tests/fixtures/hh07/`.

Os testes que leem dados privados apagam as próprias pastas temporárias, porque a saída do
scan contém strings do jogo. Conferência: `git status --short` e `git check-ignore -v` sobre
`build-mods/x`, `analysis/hh07/x`, `game-data/x`, `generated/x` e `snapshots/x`.

## 8. Checklist do critério de saída

- [x] Alvos `native`, `legacy` e `transitional` separados (CMake + testes).
- [x] Interfaces tipadas, handles com geração, ABI guest e regras de reentrância (seções 3.5–3.7).
- [x] Banco de funções identificadas com evidências e nomeação com duas evidências; manifesto
  de hardware e dominância.
- [x] Métricas reproduzíveis de dependência guest (catraca) e mapa de linker (produto sem CDI).
- [x] Provedores selecionáveis (registro) e slots com custo medido; hashes iguais com e sem slots.
- [x] Comportamento igual: roteiro com a referência do HH06; ordem determinística.
- [x] Toda dependência legada tem dono ou fronteira: 286 entradas `legado` e 2 dependências
  de hardware com dono HH14.
- [x] Casos negativos para fronteira, dependência, catraca, native limpo e produto sem CDI.
- [x] Regressão integral (seção 4.1) e revisão Codex com os achados aceitos corrigidos (seção 9).

## 9. Revisão independente e próximos passos

### 9.1 Revisão Codex

`codex exec --sandbox read-only --ephemeral` (gpt-5.6-sol), saída em `reviews/codex-hh07.log`
(ignorado). A primeira execução ficou 7 h parada em "Reading additional input from stdin" e
não revisou nada; a segunda rodou com `< /dev/null`.

Confirmado pelo revisor, sem problema: fatos de 0x8C090974 e 0x8C0C4CE0 (tabela 0x8C20CD20,
ponteiro 0x8C240048, máscara 0x03FFFFFF | 0xE0000000, QACR, `pref`); exclusão de 0x90000000
como cor; separação do CDI e mapa do linker; 13 nomes com duas evidências e únicos;
`native/` sem dependência do Dreamcast no estado atual.

Aceitos e corrigidos:

| Achado | Correção | Teste |
|---|---|---|
| [ALTA] `[[dependencia_hw]]` liberava qualquer acesso da função | Campo `regioes` obrigatório; um acesso fora das regiões declaradas continua violação (`check_closure` com `DeclaredHw`); nova catraca `fechamento.dependencias_hw_regioes` = 3 | Unitário `hh07_declared_hw_dependency_is_counted_not_hidden` (região errada = violação); `hh07_fechamento` "sem a região ccn" → rc 3 |
| [MEDIA] "obsoleta" aceitava referência estática | `unconfirmed_hw_deps`: cada par (função, região) precisa de acesso da sonda (`HwRef::dynamic`) | Unitário `hh07_declared_hw_dependency_needs_probe`; `hh07_fechamento` com a sonda sem os QACR de 0x8C090974 → rc 3 e `OBSOLETA ... regiao ccn`. No código anterior esse caso passaria com rc 0: ele só exigia que a função tivesse alguma referência de hardware, e 0x8C090974 tem as escritas nos QACR também na análise estática (`main.cpp` antes da correção) |
| [MEDIA] portões vacuamente verdadeiros | `hh07_fechamento`: mínimo de 13 nomes, motivo de cada negativo (linha do fechamento, mensagem, chave da catraca); `hh07_ordem`: exige `T >= V` e `J >= 1`; `hh07_slots_custo.py`: exatamente `VBlanks/50` hashes | Os próprios testes |
| [MEDIA] chave da regressão sem `hh-hwscan` e sem os arquivos de dados | `hh-hwscan.exe` em `ARTIFACTS`; `HH_DATA_DIR` entra pela árvore (caminho, tamanho, data de cada arquivo) | Chave recalculada; regressão integral refeita (4.1) |
| [BAIXA] "native limpo" não lia o CMake | O teste lê `native/CMakeLists.txt` e `*.cmake` e proíbe alvos `hh_runtime`/`hh_legacy`/`hh_transitional`/`hh_dcmap`/`hh_generated`/`hh_translator`; o configure confere `INTERFACE_LINK_LIBRARIES` de `hh_native_api` | Novo negativo `hh07_native_limpo_cmake_negativo` (cabeçalho limpo, CMake ligado a `hh_runtime`) |

Rejeitados:

- [ALTA] "3 032 funções do jogo por `bloco:jogo` não sustentam o fechamento". A evidência
  única é real e já está no relatório (seção 6), mas ela não deixa o fechamento passar por
  engano. As 603 mudanças vão de SDK ou desconhecida para jogo. Uma função tratada como jogo
  vira raiz do fechamento: todo acesso a hardware dela é violação, salvo se declarado, e toda
  chamada dela para o SDK precisa estar na fronteira. O sentido perigoso é o contrário: código
  do jogo rotulado SDK e escondido atrás de uma entrada da fronteira. Esse foi o defeito
  corrigido na seção 3.1. O efeito de um rótulo "jogo" errado é a fronteira ficar mais funda
  dentro do SDK, e cada etapa nativa confirma as funções que tocar. Fica a limitação da
  seção 6, sem mudança de código.
- [MEDIA] `owner_of` não aproveita `nome - mensagem` nem `[Classe::nome]`. Não é regressão:
  perder um candidato só deixa a função sem nome, que é o lado conservador. Hoje "X() failed"
  só aparece como mensagem de chamador nas amostras conferidas. O formato `a64File* - ...`
  é útil para o HH08 (gdFs/Audio64) e o HH13, e fica como trabalho dessas etapas, com duas
  evidências.

Não verificado pelo revisor e mantido como limitação: o portão do mapa só existe no MSVC; o
custo dos slots não foi reexecutado por ele (resultado na seção 3.6).

### 9.2 Próximos passos (executados automaticamente)

HH07 marcada `CONCLUIDA` e HH08 `EM_ANDAMENTO` no `STATUS.md`. O HH08 segue
`docs/planos/HH08-PLANO.md`, começando pelo HH08.1 (trace das chamadas gdFs no roteiro
`newgame_area1` para confirmar contratos e nomes). Nenhuma decisão humana pendente.
