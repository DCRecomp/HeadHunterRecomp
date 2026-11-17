# Testes futuros: o que falta rodar, quando e com o quê (2026-11-15)

Matriz única dos testes que ainda não rodaram ou que cada etapa futura vai exigir. O estado autoritativo continua no
`STATUS.md`. Os comandos abaixo existem hoje, salvo os marcados **a construir**. Nada aqui afrouxa um portão: cada linha é
um teste que precisa passar, sem SKIP, antes de a etapa fechar.

## 1. Como rodar a suíte hoje

| Rodada | Comando | Pré-requisito |
|---|---|---|
| Só CPU (a GPU ocupada) | `ctest --test-dir build -C Release -j2 -LE gpu` | variáveis de dados abaixo |
| Só os de GPU | `ctest --test-dir build -C Release -L gpu` | GPU livre (o portão de desempenho em 4K mede tempo) |
| Campanha (monitor e reprodução) | `ctest --test-dir build -C Release -L campanha` | `HH_DATA_UNICO` |
| Regressão integral retomável (V3) | `python -I tools/dev/regressao.py` | GPU livre e o Builder; horas |

Variáveis de dados (todas locais e ignoradas): `HH_DATA_DIR=game-data/runtime/disc1`, `HH_DATA_DIR2=.../disc2`,
`HH_DATA_UNICO=game-data/runtime/unico`, `HH_SNAPSHOT_DIR=snapshots/hh05`, `HH_AREA1_HASHES=snapshots/hh06/area1_hashes.txt`,
`HH07_BIN=game-data/hh02/disc1/1ST_READ.BIN`, `HH07_SONDA=analysis/hh07/sonda_area1.tsv`, `HH11_ORACULO=analysis/hh11-oraculo`,
`HH14_STREAMS`/`HH14_REVERB=game-data/hh02/disc1/STREAMS|REVERB` (e `HH14_STREAMS2`/`HH14_REVERB2` do disco 2), `HH14_ORACULO=analysis/hh14/oraculo`, `HH15_REF=analysis/hh15/ref_pilhas` (não a `analysis/hh15/ref`, anterior à máscara das pilhas: reprova a RAM do tempo experimental), `HH15_PRODUTO_HASHES=snapshots/hh15/produto_hashes.txt`, `HH_DISC1`/`HH_DISC2` = as imagens em `ISO/`. Sem elas os testes de
dados dão SKIP (rc 77), o que **não** aprova etapa nenhuma.

Rótulos do ctest (desde 2026-11-15): `gpu` = `hh13_render_tests`, `hh13_capture_tests`, `hh13_render`, `hh15_tempo`;
`campanha` = `hh16_campanha`, `hh16_gravador`, `hh16_reproducao`, `hh16_campanha_real`.

## 2. GPU e regressão integral (GPU liberada em 2026-11-16)

| # | Teste | Por quê | Fecha |
|---|---|---|---|
| G1 | `ctest -L gpu` (os 4) | **aprovado em 2026-11-16: 4/4** (`hh13_render` 902 s, `hh15_tempo` 89 s; `tmp/gpu/ctest-gpu1.log`) | HH15.11b, HH16.0 |
| G2 | `tools/dev/regressao.py` inteira | em andamento (2026-11-16): a 1ª rodada reprovou só pela referência errada da HH15 (`ref` em vez de `ref_pilhas`, erro de configuração); a 2ª, por uma queda única e não reproduzida do `hh-run` no `hh09_tempo` (STATUS); a 3ª roda com a saída inteira de cada comando em `tmp/regressao/saidas/` | HH15.11b |
| G3 | conferir duração medida × limite de cada passo da regressão (lição L5) | **feito em 2026-11-16:** `elevacao` 175 s (limite 3 600 s); achado: o passo `ctest` rodaria os portões da HH16 (~22 min) dentro do limite de 1 h da suíte se o `HH_DATA_UNICO` estivesse definido. Agora a suíte roda sem os dados e há a etapa `portoes_hh16` (3 600 s por teste, 4 h na etapa) | antes da G2 |

## 3. HH16.P: prontidão da gravação (portões em `docs/planos/HH16-PLANO.md` seção 9)

| # | Teste | Quem | Comando / roteiro | Critério |
|---|---|---|---|---|
| P3 | **Sessão piloto** (~15 min, campanha `--teste`) | mantenedor | `dist\HeadHunter-Gravador.exe` com a caixa **Campanha de teste**: Iniciar gravação; NEW GAME, um SAVE, fechar o jogo; Iniciar de novo com LOAD; Verificar (ou `campanha.py jogar --criar --teste` e `verificar`) | `TESTE REPRODUZIDO`; LOAD real na segunda sessão; gamepad e teclado; áudio |
| P3a | Portão da HH16.0 na janela | agente, depois da P3 | a própria P3 | gravação na janela (Vulkan, XAudio2, controle real) = reprodução sem janela (software) em todas as leituras |
| P4 | Longa duração na janela | agente (assistido) | 60 min de janela com roteiro, Vulkan | sem queda nem crescimento de memória; perda e recuperação do dispositivo Vulkan; troca do dispositivo de áudio |
| P4a | Carga de CPU na reprodução | agente | `verificar --paralelo 4` numa campanha de teste com a máquina carregada | mesmo veredito que `--paralelo 1` |

Já aprovados sem janela: P1 (passagem de disco, pelo mod local da área final), P2 (créditos), P5 (cópia de segurança),
P6 (`dist/HeadHunter-Preview` de 2026-11-15; o `hh16-bfe3231` é recusado pela prova do contrato), e os C1–C10 do monitor (seção 9 do plano da HH16).

## 4. Durante a gravação (HH16.2)

| Teste | Quando | Comando |
|---|---|---|
| Verificar a sessão recém-gravada | depois de cada sessão (minutos, não horas) | `python -I tools\dev\campanha.py verificar --sessao N` |
| Verificação completa da cadeia | a cada poucas sessões | `verificar --paralelo 2` |
| Cópia conferida | automática ao fim de cada sessão; à mão | `copiar` |
| Defeito relatado pelo mantenedor | ao ser relatado | reproduzir a sessão até o ponto; corrigir; `reproduzir-em` todas as sessões com o programa novo antes de trocar o programa fixado |

## 5. HH16.3: cobertura e triagem

| Teste | Situação |
|---|---|
| `verificar` com o programa fixado: `CAMPANHA REPRODUZIDA ATE OS CREDITOS` | depende da HH16.2 |
| `reproduzir-em` numa instalação recém-gerada pelo Builder | comando existe |
| Matriz de áreas, filmes e streams por sessão | o `relatorio.json` já traz (`cobertura`) |
| Funções executadas por sessão (entrada da elevação) | **a construir**: o `hh-run` (verificação) não tem `--reproduzir` nem `--estado`; o produto recusa `HH_FN_PERFIL`. Caminho: `--reproduzir`/`--estado` no `hh-run`, ou o perfil no build de verificação do `HeadHunter` |
| Passagem real do disco 1 ao 2 | a própria campanha (o mod local da área final só provou o mecanismo) |

## 6. HH15 parte 2 (depois da gravação)

| Etapa | Teste obrigatório | Pré-requisito |
|---|---|---|
| HH15.11c–e, 12+ | rota da área 1 com `HH_ELEVACAO=conferir` (hoje) **e** `reproduzir-em` das sessões que exercitam o sistema, no modo conferir | o `--reproduzir` no executável de verificação (seção 5) |
| HH15.12+ | as 18 referências da HH15 iguais | existe (`tools/dev/hh15_referencias.py`) |
| HH15.F | V3 contínua; a campanha inteira reproduzida no produto sem AOT; o **exportador** do estado (HH16-PLANO 4.2) com teste contra a última versão com RAM guest | exportador **a construir**; o `--estado` de hoje lê a RAM guest |

## 7. HH20 (Linux, se a D-P4 a incluir)

L2 (prova de fidelidade): as 18 referências e as sessões gravadas no executável Linux iguais às do Windows; L6: 30 min
em SteamOS/Bazzite. Exige máquina Linux real do mantenedor (no BS, o WSL não bastou).

## 8. HH19 (remasterização)

Em cada subetapa: uma sessão gravada curta reproduzida igual (portão C8 do BS) nas duas edições; a Original igual às
referências; F1 dos filmes: pacote sintético com o fim no mesmo VBlank e a mesma leitura do controle; o menu fechado não
muda hash nenhum.

## 9. HH18

Instalação limpa em Windows (e Linux); campanha reproduzida pelo pacote final; varredura do histórico e dos pacotes;
inventário de licenças.
