# HH10 — RELATÓRIO: Controles nativos

Data: 2026-10-25. Validação mínima: **V2**. Plano: `docs/planos/HH10-PLANO.md`. Equivalente ao N3 do
Blue Stinger (N3-0 a N3-3), adaptado ao SDK de periféricos `pd Ver 1.50` do Head Hunter.

## 1. Objetivo e escopo realizado

Critério de saída (ROADMAP): matriz de comandos e rotas de gameplay equivalentes; o nativo vira padrão
automaticamente quando todos os portões objetivos passarem.

| ID | Entrega | Resultado |
|---|---|---|
| HH10.0 | Contrato do `pd` 1.50 medido no legado | Sonda `HH_PAD_CONTRATO` (`transitional/pad_probe`): 2 261 passadas do servidor, 54 264 unidades, **0 divergências** contra `pad_contract` (`analysis/hh10/contrato.tsv`, privado) |
| HH10.1 | Matriz do decodificador 0x8C15D900 | `hh10_pad_matrix`: 53 520 casos contra o `f_8C15D900` gerado (gatilhos com histerese × 5 limiares × 4 travas × 256 valores × 2 modos; botões e pares; eixos com as 64 combinações de support × 256 valores × 2 modos), **0 diferenças**; mutações (`limiar_menos1`, A↔B) detectadas |
| HH10.2 | Núcleo nativo de entrada | `native/platform/{input.h,input_map.cpp,input_system.cpp}` portados do N3 do BS (teclado, SDL3 Gamepad, XInput de reserva, `controles.toml`, perfis por GUID, zona morta radial, hot-plug). Sem a câmera livre do BS (N7c) |
| HH10.3 | Provedor nativo (`--controles legado\|nativo`) | `transitional/pad_adapter`: slots do servidor 0x8C15DB3A, do getter 0x8C15DC40 e do disparo do DMA 0x8C17F0AA. `hh-run`, `HeadHunter.exe` (janela: teclado + SDL3 Gamepad) e `--headless` |
| HH10.4 | Fechamento | Nativo: 0 entradas no servidor e no decodificador gerados, 0 GET_CONDITION no Maple; o adaptador atende cada passada; varredura estática do gerado: só `HHG_SLOT` |
| HH10.5 | Produto | `controles.toml` criado ao lado do `HeadHunter.exe` com o texto padrão; arquivo com erro → padrão com aviso; ESC reservado para fechar |
| Promoção | `ControlsProvider::Native` é o padrão | Todos os portões objetivos C1–C9 passaram (seção 3.3) |

## 2. Arquivos alterados

- Novos (portados do BS, MIT, com cabeçalho de origem):
  - `native/platform/input_map.cpp`, `native/platform/input_system.cpp`;
  - `transitional/{input_bridge,pad_contract,pad_adapter,pad_probe}.{h,cpp}`;
  - `tests/unit/test_hh10_input.cpp`, `tests/unit/test_hh10_pad_adapter.cpp`.
- Novos (HH): `tests/unit/test_hh10_pad_matrix.cpp`, `tests/diff/hh10_controles.py`.
- Alterados:
  - `native/platform/input.h` (contrato do N3: `ControllerState`, `ActionMap`, `InputSystem`) e `native/CMakeLists.txt` (`hh_native_input`);
  - `transitional/CMakeLists.txt`;
  - `app/hh_host.{h,cpp}`: `--controles`, nativo padrão, `HH_PAD_TRACE` com o conteúdo de info/name, checagem da área nativa;
  - `app/hh_run.cpp`: `--controles`, roteiro pelo `InputSystem`;
  - `app/hh_window.cpp`: entrada nativa (teclado + SDL3 Gamepad), `controles.toml`, `--controles`, `--tempo` no uso;
  - `runtime/hle/bios.h`: área nativa do BIOS HLE movida para 0x8C006000 (seção 3.5);
  - `runtime/pvr/holly_intc.cpp` e `runtime/maple/maple.cpp`: transição injetada dos roteiros `indice transicao` (seção 3.4);
  - `config/headhunter.toml` (`[native] slots`), `config/fronteiras.toml`, `config/metricas_base.toml`;
  - `tests/CMakeLists.txt`, `tests/hh07/native_api_check.cpp`, `tools/dev/regressao.py` (`portoes_hh10`);
  - `third_party/README.md` (registro dos portes de `native/` e `transitional/`).

## 3. Evidências e decisões

### 3.1 Contrato do `pd` 1.50 (disassembly do `1ST_READ.BIN`, SHA-256 `aa585325…`)

- Tabela PDS 0x8C311DAC, 52 bytes por unidade, 24 unidades. Tabela de estado 0x8C31228C; `status_code` 0x8C15DB06.
- Modo 0x8C3122EC: 0 inverte os bits.
- Limiares: alto 0x8C3122F0, baixo 0x8C3122F1.
- Travas dos gatilhos por unidade: L em 0x8C3122F2+i (bit 17), R em 0x8C31230A+i (bit 16).
- Decodificador 0x8C15D900:
  - valor ≥ alto (`cmp/hs` em 0x8C15D926): limpa o bit e trava;
  - valor ≤ baixo (`cmp/hi` em 0x8C15D982): seta o bit e destrava;
  - entre os dois: mantém o estado anterior;
  - eixos só com os bits de support 0x40000/0x80000/0x100000/0x200000.
- Servidor 0x8C15DB3A (chamado pelo jogo em 0x8C0915F8, uma vez por passo): `old` = `on` lido da RAM (0x8C15DBAA–AE); `off/press/release` em 0x8C15DBF0–0x8C15DC20.
- Id/support 0x8C15DAB6: dados da função em big-endian, convertidos por 0x8C15DA94 se a configuração 5 da camada `per` for 1. Na rota, support = 0x000F06FE.
- Ausente: 0x8C15D8C2 limpa id/support/eixos; nome em 0x8C185F50 "(no device)".
- Getter 0x8C15DC40: `mul.l` por 52, `sts macl,r0` e soma da base.

Duas evidências independentes por função: disassembly + matriz contra o gerado (HH10.1) + sonda de
contrato sobre o legado (HH10.0). A revisão Codex conferiu os mesmos endereços (seção 9).

### 3.2 Regra de coleta (medida)

No BS a amostra de um disparo chegava ao servidor depois (coleta após o servidor ou no disparo
seguinte). No HH o traço do PDS e os quadros só batem com o legado coletando **no próprio disparo**,
nos tempos estável e experimental.

- A primeira versão nativa divergiu na amostra 21: o aperto chegava 2 VBlanks depois.
- Ordem estática do SDK (revisão Codex): coleta anterior (0x8C16ED76) → novo disparo (0x8C16ED7C).
- No runtime híbrido, o Maple legado responde na escrita do MDST, e a resposta chega à passada do servidor do mesmo passo. É a semântica do oráculo.
- O adaptador implementa exatamente essa regra. A lógica de coleta tardia do BS foi removida (era código morto).

É uma equivalência com o runtime híbrido, não um modelo da duração física do DMA. O HH15 troca isso
pela fronteira de quadro nativa.

### 3.3 Portões (`tests/diff/hh10_controles.py`, rota `newgame_area1`, 4 700 VBlanks)

| # | Critério | Resultado |
|---|---|---|
| C1 | `HH_PAD_TRACE` por VBlank (PDS da porta A + leituras), legado × nativo | idêntico em 4 700 linhas, fora os ponteiros info/name (por desenho) |
| C2 | 94 hashes de quadro | iguais nos dois modos e a `snapshots/hh06/area1_hashes.txt` (obrigatória) |
| C3 | Fechamento | nativo: `f_8C15DB3A` = 0, `f_8C15D900` = 0, `maple_portaA_getcond` = 0; `pd_servidor` = entradas do servidor no legado; disparos > 4 000; getter > 0; guarda 0 (`pd_servidores_extras` = 27 é diagnóstico, seção 4.1); estático: 6 chamadas por `HHG_SLOT`, 0 diretas |
| C4 | Ponto de amostragem | coluna de leituras igual por VBlank (última 4 644); negativo `amostra_adiantada` acusado |
| C5 | Conexão | 4 roteiros `indice transicao` (1 600 VBlanks cada): desconectar com botão apertado, ausente no início, reconectar neutro, reconectar com botão. Traço, info/name e quadros = legado, com porta vazia depois do boot e o controle de volta; negativo `presenca_imediata` acusado |
| C6 | Remapeamento | `hh10_pad_adapter_remap_c6`: perfil por GUID com A↔B + botão físico "b" = roteiro lógico com A no PDS; outro GUID diverge. Mais `input_native_profiles_by_guid` |
| C7 | Info/nome | conteúdo apontado por info (120 bytes) e name igual ao legado em todo VBlank (4 593 com o controle identificado); negativo `info_sem_conectado` acusado |
| C8 | Determinismo | experimental: traço, info/name e quadros legado × nativo iguais; duas execuções nativas idênticas; rc 0 e contagens completas exigidas |
| C9 | Catraca | `fronteira.provedor.legado` 276 → 274, `nativo` 10 → 12, `legado.subsistema.sistema` 72 → 71; `fronteira.subsistema.controles` não subiu; `hh07_fechamento` passa |

Negativos com rc 0 e acusados pelo portão certo: `press_on` (C1), `amostra_adiantada` (C1/C4),
`info_sem_conectado` (C7), `presenca_imediata` (C5), `maple` (C3).

Não acusáveis nesta rota, com a razão medida:
- `old_host`: o jogo nunca reescreve o `on` do PDS. O `old` lido da RAM é coberto pelo unitário `hh10_pad_adapter_presence_model_and_server`.
- Amostra nos disparos com retorno < 0: `pd_disparos` = `pd_disparos_efetivos` (1 545 em 1 600 VBlanks). A mutação foi retirada em vez de mantida sem teste.

### 3.4 Transição injetada dos roteiros de conexão

Os roteiros de presença precisam de índice que ande com a porta vazia (`indice transicao`). Medido:
no HH esse índice ficava em 0. Em 1 543 leituras a coluna de transição era 0 em todas.

Causas:
- o runtime não chamava `maple_vblank_in()`;
- o `pd` 1.50 só escreve o MDST, não o lê.

Correção:
- `holly_intc` arma a transição na entrega do VBlank-In;
- o Maple a conta no primeiro acesso ao MDST, leitura ou escrita, antes da amostra nos dois provedores.

Com isso a primeira versão do C5, que só cobria o boot, foi refeita. Agora exige porta vazia depois
do boot e reconexão. Escopo: só testes (roteiros `indice transicao` e o diagnóstico
`amostra_vblank_in`). Nenhum caminho do jogo muda.

### 3.5 Área nativa do BIOS HLE

A `info` sintetizada da porta A precisa de memória guest sem dono. O BS usava 0x8C004000. A revisão
Codex pediu evidência do HH, e a varredura de literais de 32 bits nos dois `1ST_READ.BIN` encontrou 4
na faixa:
- 0x8C1254FC = 0x8C004020: lido por 0x8C1254E0, que compara com "Katana Debug Adapter";
- 0xAC004000 em 0x8C180BB8/C14/C48: chamado pelo tratador de exceção do SDK, atrás de um `bra` em 0x8C180BC8.

É a área do adaptador de depuração. A área foi movida para **0x8C006000**:
- 0 literais de 0x8C004100 a 0x8C007FFF;
- as estruturas do boot terminam em 0x8C0031DC;
- o portão confere, por VBlank e com `HH_PAD_FECHAMENTO`, que o legado nunca altera a faixa (roteiro e os 4 de conexão);
- no nativo a guarda confere antes de cada escrita (`pd_guarda` = 0).

### 3.6 Janela

`HeadHunter.exe --controles nativo` (padrão):
- `InputSystem` com o `ActionMap` de `controles.toml` ao lado do executável (criado se faltar);
- eventos SDL3 de teclado e de Gamepad (hot-plug, botões, eixos, gatilhos), com `SDL_INIT_GAMEPAD`;
- perder o foco solta as teclas;
- `backend = "xinput"` liga o XInput de reserva;
- `--headless`, ou com roteiro, usa a fonte de teste.

O `InputSystem` é protegido por mutex: os eventos vêm da thread da janela, a amostra da thread do
jogo. O legado continua igual.

Verificado:
- headless nativo × legado com o roteiro: logs de hash idênticos (600 VBlanks);
- janela real por 300 VBlanks, rc 0, `controles.toml` criado.

## 4. Comandos de build/teste e resultados

```text
cmake --build build --config Release
build/tests/Release/hh10_input_tests.exe                       19 testes, 0 falhas
HH07_BIN=... HH_EXIGIR_DADOS=1 build/tests/Release/hh10_pad_matrix.exe   53 520 casos, 0 diferencas
python -I tests/diff/hh10_controles.py build/app/Release/hh-run.exe . generated/disc1
ctest --preset vs-release -R "^hh10_(input_tests|pad_matrix|controles)$"   (HH_EXIGIR_DADOS=1)
hh-hwscan config/headhunter.toml <1ST_READ.BIN> <saida> --sonda analysis/hh07/sonda_area1.tsv
          --fronteira config/fronteiras.toml --metricas-base config/metricas_base.toml   rc 0
python -I tools/dev/regressao.py (... portoes_hh10 ...)
```

### 4.1 Portão HH10

Execução por `ctest` com `HH_EXIGIR_DADOS=1` (871 s). A primeira rodada acusou o novo C3 do adaptador:
`pd_servidores_extras` = 27. O jogo roda o servidor duas vezes entre dois disparos em 27 passos da rota.

Isso era invariante da regra de coleta do BS (coleta depois do servidor). No HH a coleta é no
disparo: a segunda passada vê a mesma amostra, como o servidor original relendo o mesmo buffer da
camada `per`. O C1 prova o traço idêntico ao legado nesses passos. O contador ficou como diagnóstico,
fora do portão, com a razão registrada no código. Contadores do nativo na rota:

```text
pd_disparos 4645 | pd_disparos_efetivos 4645 | pd_servidor 2261 (= f_8C15DB3A do legado)
pd_getter 2264 | pd_amostras 4644 | pd_leituras 4644 | pd_guarda 0 | pd_servidores_extras 27
```

Resultado da rodada (todas as demais verificações):

```text
[ ok ] C3 estatico: ... generated\disc1: 6 por slot, 0 diretas
[ ok ] C1: traco do PDS igual (fora info/name) (4700 linhas)
[ ok ] C2: 94 hashes iguais legado x nativo
[ ok ] C2: nativo igual a referencia do HH06 (HH_AREA1_HASHES)
[ ok ] C3: fechamento no nativo (servidor 0, decodificador 0, getcond 0)
[ ok ] C3: o legado usa o caminho do SDK
[ ok ] area nativa intocada pelo jogo em todo VBlank (legado, roteiro e conexao) (['0','0','0','0','0'])
[ ok ] C4: leituras por VBlank iguais e andando (ultima 4644)
[ ok ] C7: conteudo de info/name igual ao legado em todo VBlank (4593 VBlanks com o controle identificado)
[ ok ] C8: experimental, traco e quadros iguais legado x nativo (rc 0/0/0)
[ ok ] C8: experimental nativo deterministico
[ ok ] C5 desconecta_apertado (190 VBlanks sem dispositivo depois do boot, volta True)
[ ok ] C5 ausente_inicio (704 VBlanks sem dispositivo depois do boot, volta True)
[ ok ] C5 reconecta_neutro (100 VBlanks sem dispositivo depois do boot, volta True)
[ ok ] C5 reconecta_botao (100 VBlanks sem dispositivo depois do boot, volta True)
[ ok ] negativos press (C1), adiant (C1/C4), infonc (C7), presenca_imediata (C5), maple (C3)
```

A rodada final, com o C3 corrigido, está na regressão integral (4.2, `portoes_hh10`).

### 4.2 Regressão integral

Executada no código final, com as correções da revisão (log em
`analysis/hh10/regressao_final.log`):

```text
[ok] build (11 s)
[ok] ctest (17 s)
[ok] portoes_hh05 (12 s)    (controles nativos padrão)
[ok] portoes_hh07 (508 s)
[ok] portoes_hh08 (380 s)
[ok] portoes_hh09 (206 s)
[ok] portoes_hh10 (629 s)   hh10_input_tests + hh10_pad_matrix + hh10_controles (HH_EXIGIR_DADOS=1)
[ok] roteiro_area1 (157 s)  hashes iguais a snapshots/hh06/area1_hashes.txt
[ok] builder (249 s)        instalação com os dois discos; HeadHunter.exe instalado (controles nativos) reproduz a referência
regressao completa — exit=0
```

Uma rodada anterior falhou no `ctest`: `hh07_native_api` conferia bytes do texto de teste antigo, e o
teste foi ajustado ao texto neutro.

## 5. Comparação com referência/oráculo

O oráculo é o legado (Maple + servidor `pd` gerado) no mesmo runtime:
- traço do PDS, leituras, conteúdo de info/name e quadros idênticos por VBlank, na rota e nos 4 roteiros de conexão;
- quadros iguais à referência do HH06 (que veio do Flycast no HH05/HH06).

O contrato do servidor tem 0 divergências em 54 264 unidades do legado. O decodificador tem 0
diferenças em 53 520 casos contra o gerado.

## 6. Riscos, limitações e trabalho deixado para depois

- Regra de coleta (seção 3.2): equivalência com o runtime híbrido, não modelo físico do DMA. O HH15 a substitui pela fronteira de quadro nativa.
- Cobertura da rota: modo 0, um único support, 3 botões, gatilhos e eixos só nos extremos (contrato.tsv). A matriz cobre todos os valores e os dois modos contra o gerado; rotas longas de gameplay ficam para o HH16.
- HH10.6 (roteiros por passo de jogo): o HH não tem a espera do VBlank do jogo que o BS usa como passo (`kVblankWaitPc` = 0, HH09). Os roteiros portáveis entre modos de tempo continuam pendentes; os de conexão usam `indice transicao`.
- Vibração (`pdVib 2.01` e `kd 1.48` ligados ao executável): não verificado se alguma rota os usa. O legado não mostra unidade de vibração, e o nativo também não. Investigar no HH16 com rotas maiores.
- Detecção do "Dreamcast Fishing Controller" (0x8C0913D4): não verificada isoladamente. Coberta indiretamente por C7 (conteúdo de info/name igual) e C2 (quadros iguais).
- `pdLcd 1.04` (tela da VMU) e a VMU continuam na camada `per` original até o HH11.
- Sem controle físico na máquina: o caminho SDL Gamepad foi exercitado pelos unitários do InputSystem e por uma execução real da janela, não por um controle físico.

## 7. Inventário de dados privados

Ficaram fora do Git:
- `analysis/hh10/`: contrato, traços do PDS, logs;
- `generated/disc1`, `generated/disc2` (`disc2` regenerado com os slots);
- `reviews/codex-hh10.log`, `reviews/hh10-prompt.md`;
- `build/` (inclui o `controles.toml` criado pela execução da janela);
- `game-data/`.

O portão apaga a própria pasta temporária. Versionados só endereços, contagens e o roteiro sintético
de teste (sem bytes do jogo).

## 8. Checklist do critério de saída

- [x] Entrada SDL3 para teclado e controles, hot-plug e remapeamento persistente (`controles.toml`, perfis por GUID).
- [x] Provedor nativo da API usada pelo jogo, sem Maple no caminho nativo (C3).
- [x] Zona morta, gatilhos (histerese), desconexão (C5) e latência (C4: mesmo índice de amostra por VBlank) testados. Vibração: ver seção 6.
- [x] Configuração amigável no produto (`controles.toml` comentado, criado na primeira execução).
- [x] Matriz de comandos (HH10.1) e rota de gameplay equivalentes (C1/C2/C7/C8).
- [x] Nativo promovido a padrão com todos os portões objetivos aprovados.
- [x] Regressão integral e revisão Codex com os achados aceitos corrigidos (seções 4.2 e 9).

## 9. Revisão independente e próximos passos

### 9.1 Revisão Codex

Saída em `reviews/codex-hh10.log`. A revisão foi feita sobre um estado intermediário: o traço já tinha
cinco colunas, e o portão ainda não.

Pontos confirmados pelo revisor:
- histerese, inversão pelo modo e bits de gatilho/eixos do 0x8C15D900;
- `old` do servidor;
- `off/press/release`, estado, ausência e subunidades;
- getter;
- segurança entre threads do caminho da janela;
- causalidade dos negativos `press_on` e `maple`.

| Achado | Decisão | Correção / motivo | Teste |
|---|---|---|---|
| [ALTA] Portão incompatível com o traço de 5 colunas | Aceito (já corrigido no portão final) | `masked()`/`contents()` sobre as 5 colunas | `hh10_controles` |
| [ALTA] C7 não comparava info/name | Aceito | C7 compara as duas colunas em todo VBlank; negativo `info_sem_conectado` | `hh10_controles` C7 |
| [MEDIA] Matriz dos eixos com `v += 3` e só o modo 0 | Aceito | todos os 256 valores × 64 supports × 2 modos (53 520 casos) | `hh10_pad_matrix` |
| [MEDIA] C8 aprovava com rc ≠ 0 ou traço vazio | Aceito | exige rc 0, 4 700 linhas e 94 hashes nas três execuções, além de info/name | `hh10_controles` C8 |
| [MEDIA] Referência HH06 opcional | Aceito | obrigatória no script e em `portoes_hh10` | `hh10_controles` C2 |
| [MEDIA] `generated/disc2` com chamadas diretas | Aceito | `disc2` regenerado (6 `HHG_SLOT`, 0 diretas); o portão varre o gerado compilado (`HH_GENERATED_DIR`) e acusava o `disc2` antigo (6 diretas) | `hh10_controles` C3 estático |
| [MEDIA] Modelo de coleta contradiz a documentação | Aceito | documentação refeita (seção 3.2); lógica tardia do BS removida | C1/C4/C5/C8 |
| [BAIXA] C3 aprovava sem o adaptador ter rodado | Aceito | exige `pd_servidor` = entradas do legado, disparos, getter, guarda 0 | `hh10_controles` C3 |
| [BAIXA] Reserva 0x8C004000 sem evidência do HH | Aceito | a varredura achou 4 literais; área movida para 0x8C006000, com checagem por VBlank no legado | `hh10_controles` (área nativa) |
| [BAIXA] ESC aceito no mapa mas fecha a janela | Aceito | ESC fora das teclas configuráveis | `input_native_action_map_and_toml` |
| [BAIXA] Restos do BS (sonda, câmera N7c, texto de teste) | Aceito | documentação da sonda refeita para o HH; câmera livre removida do núcleo; texto de teste neutro | `hh07_native_api`, `hh10_input_tests` |

### 9.2 Próximos passos

HH10 marcada `CONCLUIDA` e HH11 (saves nativos e continuidade entre discos) `EM_ANDAMENTO`.
