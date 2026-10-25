# HH10 — Plano detalhado: controles nativos

Validação mínima: **V2**. Critério de saída (ROADMAP): matriz de comandos e rotas de gameplay
equivalente; o nativo vira padrão automaticamente quando todos os portões objetivos passarem.

Equivale ao N3 do Blue Stinger (MIT; `docs/N3-PLANO.md`, `N3-0..N3-3-RELATORIO.md`). Regra
permanente: recompilar, nunca emular. O provedor nativo substitui a API de controles do SDK
(`pd`), como o adaptador gdFs do HH08 substituiu a de arquivos. Não simula Maple, DMA nem
respostas de dispositivo. VMU, `bu` e `pdLcd` ficam para o HH11.

Pré-requisito: HH09 `CONCLUIDA` (2026-10-25). **Estado: EM_ANDAMENTO** (`STATUS.md`).

## Fatos de entrada (preparação, 2026-10-25)

Disassembly: `build\translator\Release\hh-disasm.exe game-data\hh01\disc1_ref\1ST_READ.BIN
0x8C010000 <endereço> <tamanho>`. Tabelas: `analysis/hh07/disc1/{sdk_modulos,sdk_funcs,
callgraph,hw_manifest,strings}.tsv` (privadas, ignoradas). Varredura de literais alinhados: um
Python com `-I` sobre o `1ST_READ.BIN` que só imprime endereços.

### Módulos ligados (`sdk_modulos.tsv`, banners)

`pd Ver 1.50` (banner 0x8C185F1D; o BS tinha o 1.07a), `pdVib Ver 2.01` (0x8C1873ED),
`kd Ver 1.48` (0x8C1873C1), `pdLcd Ver 1.04` (0x8C185E65), `bu Ver 1.47` (0x8C186439),
`syHw 1.07`. A atribuição `modulo:kd` no `sdk_funcs.tsv` vem do banner mais próximo e **não**
identifica funções (ex.: 0x8C17F0AA aparece como `kd,syMmu`).

### Registradores do Maple (`hw_manifest.tsv`, origem `sonda`, rota `newgame_area1`)

| Função | Acesso | Vezes | Papel (análogo do BS) |
|---|---|---|---|
| 0x8C15FAC8 | escreve 6C04, 6C10, 6C14, 6C18, 6C80, 6C8C, 6CE8 | 1 | init do sistema (`syHw`; BS 8C08BD74) |
| 0x8C17EC80 | escreve 6C10, 6C14, 6C80, 6C8C | 1 | init do Maple (BS 8C09727C); chamada por 0x8C16EBB0 |
| 0x8C17F0AA | escreve MDSTAR e MDST; lê MDST | 4 645 | disparo do DMA (BS 8C0975C0) |
| 0x8C17F27A | lê MDST | 4 645 | coleta/fim do DMA (BS 8C0976DE) |
| 0x8C180890 | constante 0xA05F6C00 | — | base dos registradores |

4 645 = número de VBlanks (INTEVT 0x360) do HH09. `callgraph.tsv`: o tratador de VBlank do SDK
0x8C16E908 chama 0x8C17F0AA em 0x8C16ED7C e 0x8C16EDE8, e 0x8C16EF18 (coleta, que chama
0x8C17F27A e os acessores 0x8C16F138/0x8C16F14A/0x8C16F166) em 0x8C16ED76 e 0x8C16EE22.

### SDK `pd` 1.50 (disassembly + grafo + literais)

| Endereço | Papel | Evidências |
|---|---|---|
| 0x8C15DB3A | **servidor** (24 unidades) | guarda `[0x8C185F4C]` ≠ 0; laço `r13` = 0..23 (`mov #24`); unidade = 0x8C311DAC + 52·i (`mul.l`); grava `info` em +48 com o retorno de 0x8C16F14A; `old` (+44) = `on` (+8) **lido da RAM**; `off` (+12) = `~on`; decodifica por 0x8C15D900 ou limpa por 0x8C15D8C2 |
| 0x8C15DC40 | `pdGetPeripheral(porta)` = 0x8C311DAC + 52·porta | código (6 instruções); literal em 0x8C15DC54 |
| 0x8C15D744 | init do `pd` | limpa as unidades (0x8C15D8C2) e chama 0x8C16EBB0 → 0x8C17EC80 (init Maple); chamado pelo boot 0x8C0CDF20 |
| 0x8C15DC64 | encerramento | limpa as 24 unidades e zera a tabela 0x8C15DD94; chamado por 0x8C0CE0A4 |
| 0x8C15D900 | decodificador do controle | lê os bytes da condição; compara um byte com o limiar apontado pelo literal 0x8C15D968 (*hipótese*: bit digital do gatilho) |
| 0x8C15DCAC | `tabela[porta]` em 0x8C15DD94 | *hipótese*: estado por unidade (BS 8C2D8E50) |
| 0x8C16F138, 0x8C16F14A | acessores da camada `per` | *hipótese* por analogia com 8C08F4B6/CC (uma evidência) |

Literais alinhados para 0x8C311DAC: só 0x8C15D7F8, 0x8C15D94C, 0x8C15DC54 e 0x8C15DD8C, todos
no SDK. O jogo nunca usa a base do `PDS` diretamente.

**Diferença do BS:** lá o servidor rodava pelo servidor por quadro do SDK (8C173044). Aqui o
servidor e o getter são chamados **pelo jogo**: 0x8C0915F8 chama 0x8C15DB3A e depois
`pdGetPeripheral` e grava o ponteiro em `0x8C247384 + 12·porta`. O chamador por quadro é
0x8C08D770, com 4 chamadores: 0x8C04CEAC, 0x8C04DA98, 0x8C08DCE8 e 0x8C09E896.

### Camada de entrada do jogo (*hipóteses*, a confirmar no HH10.0)

- Tabela de portas em 0x8C247380 (4 × 12 bytes: +0 = `tabela[porta]`, +4 = `PDS*`, +8). Há 14
  literais para 0x8C247380–0x8C247388, todos em 0x8C0914A4–0x8C091D04. Os consumidores parecem
  concentrados num avaliador 0x8C091640, um `switch` por códigos 100–107, 120… No BS eram 28
  funções lendo o `PDS*` (8C24CCB0).
- 0x8C0913D4 (chamada por 0x8C08D358) percorre as portas 0–3 e procura `support & 0x000F060E
  == 0x000F060E` e `name` (+0x24) igual à string "Dreamcast Fishing Controller" (0x8C13BBB0,
  comparação por 0x8C1247C0). Devolve a porta ou −1. 0x8C0914D2 refaz o teste do nome. O leiaute
  `support` +4 e `name` +0x24 confirma o `PDS_PERIPHERAL` do Shinobi, e o contrato de
  `info`/`name` tem consumidor real no jogo.
- Os fatos do BS sobre pós-processamento (zona morta 20, limiar 48, soft reset A+B+X+Y+START,
  palavra de ações) **não** valem aqui sem medição.

### O que o runtime já tem (porte do HH05)

| Peça BS | No HH | Situação |
|---|---|---|
| `runtime/input` (`InputSource`, roteiro, `KeyEventInput`, XInput, combinação, `LiveScriptInput`, índices leitura/passo/transição) | igual (`input.h`, 127 linhas) | portado. `input_set_step_counter` **não tem chamador** (`rg input_set_step_counter app runtime transitional`), então ainda não existe roteiro "indice passo" |
| `runtime/maple` com os ganchos do N3-0/N3-2 (`maple_set_controller_present`, `MapleCounters`, `HH_PAD_LEITURAS`, `HH_PAD_MUTACAO=troca_ab\|amostra_vblank_in`, `HH_MAPLE_FUNCAO`, `HH_MAPLE_PORTS`) | `maple.cpp:57–288` | portado, sem uso pelo host |
| `native/platform/input.h` | versão do N0 (45 linhas, `IInput` mínima) | falta a versão final do N3-3 (196 linhas: `InputSystem`, `ActionMap`, perfis) |
| `native/platform/input_{map,system}.cpp`; `transitional/{pad_adapter,pad_contract,pad_probe,input_bridge}.*` | — | **não portados** |
| `--controles legado\|nativo` | — | `hh-run`/`HeadHunter.exe` não têm a opção. A janela (`app/hh_window.cpp`) usa SDL3 para eventos e teclado, e XInput para o controle |
| Roteiro | `tools/dist/roteiros/newgame_area1.txt` (22 linhas, índice = leitura, usa `ANALOG_UP/LEFT`) | referência `snapshots/hh06/area1_hashes.txt` (ignorada) |

## Reuso do Blue Stinger (MIT, atribuição por arquivo)

| Componente BS | Uso no HH10 |
|---|---|
| `native/platform/input.h`, `input_map.cpp`, `input_system.cpp` | Porte (substitui o `input.h` do N0); SDL3 Gamepad + XInput reserva, latch, `ActionMap` TOML, perfis por GUID, hot-plug |
| `transitional/input_bridge.*` | Porte: o caminho legado lê do mesmo latch, e o Maple só traduz |
| `transitional/pad_contract.*` | Refeito para o `pd` 1.50: `decode`/`server_unit` medidos no 0x8C15D900/0x8C15DB3A, e não copiados do 1.07a |
| `transitional/pad_adapter.*` | Porte com outros slots (servidor 0x8C15DB3A, getter 0x8C15DC40, disparo 0x8C17F0AA), área nativa do BIOS HLE e modelo de presença/coleta |
| `transitional/pad_probe.*` (`BS_PAD_TRACE`, `BS_PAD_FECHAMENTO`) | Porte como `HH_PAD_TRACE`/`HH_PAD_FECHAMENTO` |
| `tests/unit/test_input_native.cpp`, `test_pad_contract.cpp`, `test_pad_adapter.cpp` | Porte como `test_hh10_*` |
| `tools/dist/test_controles.ps1`, `test_pd_nativo.ps1` (P1–P9, C1–C9) | Refeitos em Python (`tests/diff/hh10_*.py`), como o `hh09_tempo.py` |
| `docs/CONTROLES.md`, arquivo padrão comentado | Base da documentação do produto |
| Lições: amostra só com retorno ≥ 0 do disparo; `old` da RAM; coleta depende do modo de tempo; info com ponteiro fixo e conteúdo atualizado; negativo exige rc 0 e traço não vazio; ligar uma ação não tira o botão da ação padrão | Aplicadas nos portões |

## Subetapas

| ID | Entrega | Evidência de saída |
|---|---|---|
| HH10.0 | Contrato e instrumentação sem mudança de comportamento. `HH_CALL_TRACE` (build `HH_MODS`, `build-mods/`) sobre o servidor, o getter, o init, o decodificador, os acessores e 0x8C0915F8/0x8C08D770/0x8C091640, para medir contagem por VBlank e quem lê o quê. Leiaute medido do `PDS_PERIPHERAL` 1.50 (dumps com A, UP, ANALOG, LT+RT, solto), variáveis de modo (literal 0x8C15DC60) e de limiar (literal 0x8C15D968), pós-processamento do jogo. Uso de `pdVib`, `kd` e do controle de pesca. Presença roteirizada por leitura no Maple (os 4 casos do N3-0). `HH_PAD_TRACE` | Tabela função → papel com duas evidências; traço idêntico em duas execuções; achados de `pdVib`/`kd` com endereço |
| HH10.1 | Teste de contrato: `decode(condição, support, old, modo)` em C++ puro contra o legado (`ctest` com a imagem). Matriz do N3-0: 256 valores por byte, 64 combinações de `support`, bits sozinhos e em pares, `old`/`on`, dois modos, unidade ausente, fronteiras dos limiares medidos | 0 diferenças na matriz; negativo (limiar ±1) acusado |
| HH10.2 | Núcleo nativo `native/platform/input*` (porte), `input_bridge`. O legado passa a ler o latch | Unitários `hh10_*`; `--controles legado` com hashes iguais a `area1_hashes.txt`; `hh07_native_limpo` |
| HH10.3 | `transitional/pad_adapter` + `--controles legado\|nativo` (`hh-run`, `HeadHunter.exe`). Slots em `[native] slots`. O init 0x8C15D744 continua original, porque a VMU usa a camada `per` até o HH11. Maple sem controle no nativo | C1–C5 e C8 (portões) |
| HH10.4 | Fechamento: no nativo, 0 GET_CONDITION, 0 execuções de 0x8C15DB3A/0x8C15D900, getter atendido pelo slot. Estático: todas as chamadas ao servidor, ao getter e ao disparo no gerado passam por `HH_SLOT` | C3 com contadores; varredura do gerado |
| HH10.5 | Produto: remapeamento TOML na pasta do usuário, perfis por GUID, troca a quente, desconexão, mapa padrão do teclado, configuração amigável (ROADMAP). Vibração ligada só se o HH10.0 achar uso de `pdVib` (ver Questões) | C6, C7; teste manual com teclado e controle virtual do SDL3 |
| HH10.6 | Roteiros "indice passo" (ligar `input_set_step_counter` ao contador de passos do HH09) e novas rotas: menus, exploração com analógico parcial (AX/AY intermediários), gatilhos | Quadros iguais legado × nativo nas rotas novas |
| HH10.7 | Promoção, regressão (`regressao.py`, etapa `portoes_hh10`), relatório, Codex, commit | `docs/relatorios/HH10-RELATORIO.md`; nativo padrão |

## Portões

| # | Critério | Oráculo / tolerância |
|---|---|---|
| C1 | `HH_PAD_TRACE` (campos do `PDS` da porta A e tabela 0x8C247380 por servidor) | legado × nativo byte a byte, em `newgame_area1` e nas rotas do HH10.6 |
| C2 | Quadro, leituras de arquivo (CL do HH09), escritas na AICA | hashes iguais a `snapshots/hh06/area1_hashes.txt` nos dois modos |
| C3 | Fechamento | contadores do Maple e `HH_PAD_FECHAMENTO` = 0 no nativo |
| C4 | Ponto de amostragem | amostra no retorno ≥ 0 de 0x8C17F0AA (mesmo instante do GET_CONDITION legado); coluna `leituras` igual |
| C5 | Conexão | 4 casos (desconectar com botão apertado, ausente no início, reconectar neutro e com botão): traço = oráculo legado com presença roteirizada |
| C6 | Remapeamento | arquivo com A↔B + roteiro físico com B = traço do roteiro lógico com A; perfil de outro GUID diverge |
| C7 | Info/nome | conteúdo apontado por `info`/`name` igual ao legado (inclui o teste do controle de pesca: devolve −1 nos dois modos) |
| C8 | Determinismo | duas execuções iguais; `--tempo nativo` e `experimental`; build com mods |
| C9 | Catraca | `fronteira.subsistema.controles` não sobe; `fronteira.provedor.legado` cai com os slots |

## Testes negativos (cada um detectado com rc 0 e artefato não vazio)

| Defeito (`HH_PAD_MUTACAO`) | Detectado por |
|---|---|
| A↔B trocados (`troca_ab`, já existe) | C1/C2 |
| Amostra na entrega do VBlank-In (`amostra_vblank_in`, já existe) ou 1 leitura adiantada | C4 (com transição injetada entre os dois pontos) |
| Amostra também nos disparos sem DMA (retorno < 0) | C4 (`leituras` à frente) |
| `old` vindo do estado nativo em vez da RAM | C1, quando o jogo reescreve o `PDS` |
| Limiar do gatilho/analógico deslocado em 1 | HH10.1 e C1 |
| `info` sem o bit de conectado; nome trocado | C7 |
| Controle deixado no Maple junto do adaptador | C3 |
| Perfil de outro GUID | C6 |

## Riscos

- **O servidor é chamado pelo jogo**, não pelo VBlank. Se 0x8C08D770 rodar mais de uma vez por
  passo (4 chamadores), o `press`/`release` depende da contagem de chamadas. O adaptador reproduz
  cada chamada e não por quadro; o HH10.0 mede isso.
- Leiaute e modos do `pd` 1.50 diferem do 1.07a: nada do `pad_contract` do BS vale sem a matriz
  do HH10.1.
- Coleta dependente do modo de tempo (BS N3-2 §2.1): medir com `--tempo nativo` e
  `experimental`. O HH15 troca isso pela fronteira de quadro nativa.
- Maple sem controle muda a duração do DMA no tempo estável (bits × ciclos). Se o quadro divergir
  só por isso, a reserva é um controle neutro não consumido até o HH15.
- Área nativa do BIOS HLE para a `info` sintetizada: reservar e provar que o jogo não a usa,
  como no BS (guarda em execução).
- Roteiro único de 4 700 VBlanks, com analógico só nos extremos: cobertura fraca de eixos
  intermediários e gatilhos até o HH10.6.
- Sem controle físico na máquina (como no BS): o teste manual usa o controle virtual do SDL3.

## Questões abertas

1. Quantas vezes por VBlank 0x8C15DB3A roda, e de qual dos 4 chamadores de 0x8C08D770?
2. O que o avaliador 0x8C091640 lê do `PDS` (campos, zona morta, limiares, combinações)?
3. O jogo chama `pdVib` ou `kd` em alguma rota? Se `pdVib` for usado, oferecer vibração exige
   mostrar uma unidade de vibração que o legado (controle sem expansões) não mostra, e isso muda o
   caminho do jogo. Só vira `DECISAO_NECESSARIA` se o HH10.0 provar uso e não houver modo fiel
   equivalente (ex.: vibração só fora do modo fiel).
4. Para que serve a detecção do "Dreamcast Fishing Controller" (0x8C0913D4)? O produto só
   expõe o controle padrão: confirmar que o resultado −1 é o do legado em todas as rotas.
5. O índice "passo" depende do contador do HH09 (`+0x204` da passada?). Falta confirmar que ele
   vale igual nos três modos de tempo.

## Regras

- `--controles legado` fica disponível até o HH15; o Maple continua só para a VMU até o HH11.
- Nenhum interpretador, nenhuma simulação nova de dispositivo; o adaptador escreve o contrato
  `PDS` medido.
- Saídas em `analysis/hh10/` e `reviews/` (ignorados); nenhum byte proprietário nos documentos.
