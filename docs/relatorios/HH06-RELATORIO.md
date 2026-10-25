# HH06 — RELATÓRIO: Primeira fatia jogável, Preview e Builder inicial

Data: 2026-10-25 · Nível de validação: V2 · Estado: **CONCLUIDA**

## 1. Objetivo e escopo realizado

Levar o jogo recompilado do título até a primeira área controlável do disco 1 com roteiro
determinístico, em headless e em janela, e entregar o Preview local e o Builder inicial que
converte a cópia do usuário.

- Roteiro `tools/dist/roteiros/newgame_area1.txt`: título → START → "memory card; desativar
  save?" → OK → NEW GAME → TRAINING MISSION → SKIP → primeira área (corredor com gás, Jack
  Wade, HUD) → movimento pelo analógico.
- Núcleo comum `app/hh_host` (boot, dados, entrada, VBlank, parada) usado por `hh-run` e
  pelo Preview `HeadHunter` (janela SDL3, teclado/XInput, roteiro = modo espectador).
- Builder inicial `hh-builder` (C++): CDI → dados de execução conferidos → C++ → CMake →
  instalação montada, verificada e só então trocada.
- Regressão retomável inicial `tools/dev/regressao.py`.
- Analisador: switch por `braf` em tabela de stubs `bra; nop`.

## 2. Arquivos alterados

| Arquivo | Mudança |
|---|---|
| `cmake/HHProductGuard.cmake`, `tests/cmake_guard/CMakeLists.txt` | Guarda adiada para o fim da configuração; caso negativo de dependência tardia |
| `runtime/cpu/{sh4_runtime.h,sh4_service.cpp}`, `tests/hh06_service_hook_tests.cpp` | Gancho do poll central (parada sem VBlank) |
| `app/hh_host.{h,cpp}` | Novo núcleo de execução |
| `app/hh_run.cpp` | Reescrito sobre o `hh_host` (mesma saída e códigos; hashes idênticos aos de antes) |
| `app/hh_window.cpp`, `app/CMakeLists.txt` | Preview `HeadHunter` (SDL3 3.2.30 estático, zlib), `hh_assert_product_clean(HeadHunter)` |
| `builder/builder_main.cpp`, `builder/CMakeLists.txt` | `hh-builder` |
| `translator/analysis/analysis.cpp` | `braStubTable` (tabela de stubs declarada no config) |
| `config/headhunter.toml` | `[[jump_tables]]` 0x8C010AD8 (8 stubs) |
| `tools/dist/roteiros/newgame_area1.txt` | Roteiro |
| `tools/dev/regressao.py` | Regressão retomável |
| `tests/hh06_analysis_tests.cpp`, `tests/CMakeLists.txt` | Testes da tabela de stubs |

## 3. Evidências e decisões

### 3.1 Caminho até a primeira área (HH06.1, HH06.2)
Índice do roteiro = leitura do controle; com `--rtc 1700000000` há uma leitura por VBlank
(leitura ≈ VBlank − 56). O direcional funciona nos menus; na área, o movimento é pelo
analógico (`ANALOG_UP` etc.): sem ele o personagem não se move; com ele, 96% dos pixels mudam
e a câmera mostra Jack Wade de frente para um canto.

Despacho em execução (método R2): o caminho NEW GAME → SKIP parou em `braf r0` em 0x8C010AD8
com destinos 0x8C010ADC..0x8C010AF8. O laço automático os aceitou como 8 funções; revisão
manual: é um switch — `braf r0; nop` seguido de 8 entradas `bra caso; nop`, com r0 = 0 e só
`or #16/#8/#4` condicionais (0x8C010A9A, 0x8C010AAA, 0x8C010ABA, 0x8C010ACA) → r0 ∈ {0..28},
8 entradas. As 8 "funções" foram retiradas e o analisador ganhou o padrão (`braStubTable`,
só com tabela declarada no config; entradas são rótulos da própria função, não dado; entrada
que não é `bra; nop` invalida a tabela com aviso). Testes `hh06_braf_stub_table_*` (positivo
e negativo). Análise: 4.699 funções, 28 tabelas, 0 fins mortos, 0 órfãs, idêntica nos dois
discos.

Determinismo: duas execuções do roteiro (4.700 VBlanks, amostra a cada 50) dão os mesmos 94
hashes de quadro.

### 3.2 Núcleo comum e Preview em janela (HH06.3, HH06.4)
`hh-run` reescrito sobre `hh_host`: mesmos 94 hashes de antes da refatoração. O Preview roda o
jogo numa thread no ritmo do Dreamcast (59,94 Hz) e apresenta o último quadro na frequência do
monitor (área lógica 640×480 com barras). Em modo espectador (`HH_INPUT_SCRIPT`), a janela
grava os hashes (`--log --hash-every 50`): **os 94 hashes são iguais aos do headless**.

Sem interpretador: `hh_assert_product_clean(HeadHunter)` passa — o executável não alcança
`sh4_core` (executor de passo único), o tradutor nem o stub sem hardware (a configuração
falharia; mecanismo provado pelo teste negativo do HH04).

### 3.3 Builder inicial (HH06.5)
`hh-builder --disc "ISO\HeadHunter (Disc 1).cdi" --out dist\preview`: 94 s. Etapas: dados de
execução (212 arquivos, conferidos setor a setor) → recompilação (hash aceito) → CMake +
MSVC → montagem em pasta nova → verificação headless (600 VBlanks, rc 0) → troca. O
`HeadHunter.exe` instalado, sem `--data` (usa `data\` ao lado), em janela e com o roteiro,
reproduz os **94 hashes de referência**.

### 3.4 Regressão retomável (HH06.6)
Etapas: build, ctest, portões do HH05, roteiro da área 1 contra a referência, Builder a
partir do CDI + roteiro no `HeadHunter.exe` instalado. Chave por etapa = SHA-256 de fontes,
toolchain (cache do CMake), executáveis gerados e entradas privadas consumidas (§9); estado
em `%LOCALAPPDATA%\HeadHunterRecomp\regressao`.
- Primeira execução completa (`--novo`): build 6 s, ctest 4 s, portões 6 s, roteiro 129 s,
  Builder + roteiro na instalação nova 239 s → "regressao completa".
- Retomada: sem mudanças, nova execução aprova tudo de imediato; com a referência do roteiro
  propositalmente errada (`--desde roteiro_area1`), falha nessa etapa; a execução seguinte,
  com a referência certa, pula build, ctest e portões e continua do roteiro.
- Depois da revisão (chaves endurecidas): execução completa refeita; a etapa do Builder parou
  numa pasta de trabalho de versão anterior sem marcador (recusa correta); marcada a pasta, a
  execução seguinte **retomou direto na etapa `builder`** e o `HeadHunter.exe` instalado
  (`--headless`, roteiro) reproduziu os 94 hashes da referência.

## 4. Comandos de build/teste e resultados

```powershell
cmake --preset vs -DHH_GENERATED_DIR=C:/Source/recomp-head-hunter/generated/disc1
cmake --build --preset vs-release -- -m        # sem erros nem avisos C4
ctest --preset vs-release                      # 15/15 (2 SKIP sem dados privados)
$env:HH_INPUT_SCRIPT="tools\dist\roteiros\newgame_area1.txt"
build\app\Release\hh-run.exe --data game-data\runtime\disc1 --vblanks 4700 --rtc 1700000000 --every 50
build\builder\Release\hh-builder.exe --disc "ISO\HeadHunter (Disc 1).cdi" --out dist\preview
dist\preview\HeadHunter\HeadHunter.exe --vblanks 4700 --rtc 1700000000 --log <arq> --hash-every 50
python -I tools\dev\regressao.py              # HH_DATA_DIR, HH_SNAPSHOT_DIR, HH_DISC1, HH_AREA1_HASHES
```

| Teste | Resultado |
|---|---|
| ctest (inclui `hh_translator_tests` 36 casos, `hh_runtime_tests` 8, guarda com caso tardio) | 15/15 |
| Roteiro headless, 2 execuções | 94 hashes iguais |
| `hh-run` antes × depois da refatoração | 94 hashes iguais |
| Janela (modo espectador) × headless | 94 hashes iguais |
| Instalação nova do Builder (janela e `--headless`) × referência | 94 hashes iguais |
| Cenários de falha do Builder (pasta estranha, trava, troca interrompida, `%`) | comportamento esperado |
| Regressão completa e retomada | ok (§3.4) |

## 5. Comparação com referência/oráculo
O roteiro do HH06 não tem contraparte no oráculo ainda (o Flycast em execução livre não
recebe o mesmo roteiro); a referência é o próprio recompilado (headless × janela × instalação
nova, hashes iguais). Comparação de gameplay com o oráculo fica para o HH09/HH16.

## 6. Riscos, limitações e trabalho deixado para depois
- Vídeos 4X continuam abandonados (HH05 §3.5) → HH13/HH12; o caminho do roteiro não depende
  deles.
- Sem VMU: o jogo pede para desativar o save (saves nativos no HH11).
- Preview usa o SDL_Renderer (não Vulkan, que é o HH14) e o rasterizador em software do
  runtime híbrido.
- O Builder usa o próprio repositório como kit e o CMake/MSVC desta máquina; kit
  redistribuível, interface gráfica, idiomas e verificação de espaço/permissões: HH18.

## 7. Inventário de dados privados
`dist/` (instalação do Preview com dados do jogo e código gerado), `game-data/`,
`generated/`, `snapshots/hh06/` (hashes de referência), `analysis/hh06/` (capturas, logs) e
`reviews/` ignorados.

## 8. Checklist do critério de saída
- [x] Menu, início de jogo e primeira área controlável do disco 1.
- [x] Roteiro determinístico de entrada e relógio fixo.
- [x] Preview local e Builder inicial que converte a cópia do usuário.
- [x] Funções ausentes do trecho resolvidas (zero miss até a primeira área).
- [x] Regressão retomável inicial — §3.4.
- [x] Roteiro chega à primeira área em janela e headless.
- [x] Build sem interpretador para o trecho coberto.
- [x] Revisão independente — §9.

## 9. Revisão independente e próximos passos
Revisão: `codex exec --sandbox read-only --ephemeral`, saída em
`reviews/codex-hh06-2026-10-25.md` (ignorado). 6 achados, todos aceitos:

| Achado | O que foi feito | Evidência |
|---|---|---|
| [ALTA] Regressão podia aprovar sem revalidar (chave só das fontes; `ctest -R` sem teste passa) | Chave por etapa = fontes + toolchain (cache do CMake) + executáveis gerados + entradas privadas consumidas (manifesto dos dados, snapshots, referência, CDI por tamanho/data/extremos); `--no-tests=error`; portões conferidos pelo nome; executável ausente invalida a etapa | `tools/dev/regressao.py`; regressão completa refeita (§3.4) |
| [ALTA] Troca da instalação sem rollback | Marcador `.hh-builder` em toda pasta criada; nada sem marcador é removido; restauração de troca interrompida no início; rollback se a promoção falhar; cópia de segurança só sai depois da nova no lugar; trava por saída; caminhos com `"`/`%` recusados | Cenários: pasta estranha no staging preservada e recusada; trava recusa 2ª execução; troca interrompida restaurada; `%` recusado; caminho de sucesso sobre instalação existente ok |
| [MÉDIA] Guarda contra interpretador dependia da ordem dos alvos | `hh_assert_product_clean` adiada para o fim da configuração (`cmake_language(DEFER)` via `EVAL CODE`) | Novo teste `hh_product_guard_rejects_late` (dependência definida depois do produto): a guarda antiga **passava**, a nova falha |
| [MÉDIA] Builder e regressão não executavam o `HeadHunter.exe` instalado | `HeadHunter --headless` (mesmo executável, sem janela); o Builder verifica o executável copiado com log de hashes; a regressão roda o roteiro no executável instalado | `verificacao.log` da instalação; etapa `builder` da regressão |
| [MÉDIA] Fechar a janela podia travar sem VBlank | Gancho do poll central (`sh4_set_service_hook`): o host para o jogo também nos pontos de poll | Teste `hh06_service_hook_can_stop_without_vblank` |
| [BAIXA] Falhas de log e de apresentação silenciosas | `--log` que não abre é erro; falhas de textura/apresentação do SDL param com erro | `app/hh_window.cpp` |

Não verificado pelo revisor e resolvido: os registros de determinismo agora ficam em
`snapshots/hh06/evidencias/` (duas execuções headless, depois da refatoração, janela,
instalação do Builder): os 6 arquivos (com a referência) têm o mesmo SHA-256.

Próxima etapa: HH07 (fundação do port nativo), já em preparação
(`docs/planos/HH07-PLANO.md`: `hh-hwscan` portado e primeira medição).
