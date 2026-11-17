# HH16 — Campanha completa por gravação única: plano

**Situação (revista em 2026-11-15):** HH16.0 e HH16.1 implementadas e testadas sem janela (commit `bfe3231` e
seguintes); a HH16.P espera só a sessão piloto do mantenedor e os testes de GPU. Placar por portão na seção 9.
**Decisão do mantenedor (2026-11-14):** "Vou gravar a campanha inteira para testar o jogo completo. Para não precisar
zerar mais de uma vez." Com isso, D-P1 = A e D-P2 = sim (`docs/planos/PLANO-ETAPAS-RESTANTES.md`, seção 9).

**Passo = leitura do controle.** O HH não tem passo de jogo como o BS (não há espera própria do VBlank,
`kVblankWaitPc = 0`, HH09/HH10.6). Neste plano, "passo" é a n-ésima leitura do controle do jogo, o índice dos roteiros e
da gravação; "passo lógico" de um fim de filme ou fala é a leitura (e o VBlank) em que o jogo o observa.

Modelo: gate de campanha do Blue Stinger (`docs/CAMPANHA.md`, `docs/N9-PLANO.md` 14, `tools/dev/campanha.py`,
`tools/dev/gravacoes.py`, MIT, somente leitura).

## 1. Objetivo e princípio

O mantenedor joga Head Hunter **uma vez**, do NEW GAME com cartão vazio até os créditos, em quantas sessões quiser. Essa
gravação passa a ser **a rota de referência permanente do projeto**:

1. prova que o jogo inteiro funciona (HH16);
2. dá a cobertura que falta para elevar e conferir as funções fora da área 1 (HH15.12+ e HH15.F);
3. valida o produto sem AOT, as duas edições da HH19, o Linux (HH20), os mods e o pacote final (HH17, HH18).

Por isso o princípio do plano: **tudo o que pode invalidar a gravação é resolvido e testado antes de ela começar**, e tudo o
que vem depois tem de preservar os contratos da seção 4. A gravação nunca é refeita; um programa novo que não reproduz a
gravação está errado, não a gravação.

## 2. O que já existe e o que falta (medido em 2026-11-14)

| Peça | Situação |
|---|---|
| Roteiro de entrada por leitura do controle (`HH_INPUT_SCRIPT`, índice = n-ésima leitura da porta 0) | existe, com `--rtc` fixo; **só na verificação** (o produto recusa a variável, `runtime/debug/validacao_vars.h`) |
| Tempo lógico na janela (HH15.1) | existe; base do determinismo entre janela e headless |
| Saves nativos com cartão temporário no headless (HH11, HH15.5) | existe |
| Referências da HH15 (quadros, ordem, relógios, RAM sem pilhas, áudio) | existem para a rota da área 1 |
| Perfil de funções executadas (`HH_FN_PERFIL`) | existe |
| Gravação da entrada efetiva durante o jogo | **feita** (2026-11-14): `HeadHunter --gravar` (`ReadRecorder`, só na janela, flush por linha, nunca sobrescreve) |
| Estado lógico por leitura e reprodução que o confere | **feito**: `--estado`/`--estado-cada` (FNV-1a de `0x8C147B20`–`0x8C32EF60` a cada K leituras); `--reproduzir`; `tests/diff/hh16_reproducao.py` 10/10 |
| Monitor de campanha (sessões, selos, vereditos, recuperação de queda) | **feito** (2026-11-15): `tools/dev/campanha.py`; `tests/campanha` com programa falso 24/24 e com o jogo de verdade (`hh16_campanha_real`) |
| Passagem do disco 1 ao 2 dentro do jogo, na instalação única (HH08) | **corrigida** (2026-11-14): o fim do disco 1 seguia para "INSERT DISC 2"; slot do save `0x8C09C34C` continua no disco 2; provada com a área final posta no lugar da primeira (mod local); **o caminho real só a campanha prova** |
| Identificação dos filmes dos créditos | **feita**: `SCENE84` (fim natural) seguido de `AHFDATA/GAMEEND.AHF` (comando 229 "GameEnd"), medidos no produto |
| Carregar um save do disco 2 pelo título | **corrigido** (2026-11-14): slot do getter do disco `0x8C09EC68` no menu de LOAD (`HH16-PREPARACAO.md` 7.1) |

## 3. Subetapas

### HH16.0 — Gravação, estado por leitura e reprodução

- **Gravação no produto:** opção explícita de linha de comando (não variável de teste), escrita pelo monitor: grava a
  entrada efetiva por leitura do controle (depois do mapa de teclado e gamepad, dos menus do host e de qualquer recurso
  futuro de apresentação), no formato do roteiro atual, mais os eventos de sessão (LOAD/SAVE concluídos, filme
  iniciado e terminado, VBlank). Gravação incremental com `flush` periódico: uma queda não perde o que já foi gravado.
- **Reprodução no produto e na verificação:** a mesma opção em modo de leitura, aceita pelos dois executáveis. A
  classificação de `validacao_vars.h` ganha a categoria "entrada determinística do produto", conferida pela catraca; as
  mutações e sondas continuam recusadas.
- **Relógio:** a data inicial do RTC de cada sessão é gravada e reaplicada; a data do envelope dos saves fica fora da
  comparação (é a hora real).
- **Estado por leitura do controle, em duas camadas** (seção 4.2): observável (permanente) e RAM do jogo sem as pilhas
  (enquanto houver RAM guest).
- **Portões:** gravar na janela (Vulkan, XAudio2, gamepad real ou virtual do SDL) e reproduzir headless (software) com
  o estado idêntico em todos os passos; 3/3 reproduções iguais; ruído de ciclos e carga da CPU sem efeito; divergência
  injetada (um botão, um byte de save, um passo) acusada no passo certo; negativos para gravação truncada.

### HH16.1 — Monitor de campanha

Porte do modelo do BS para o HH, com os ajustes daqui:
- janela simples, `Jogar e gravar (continuar)`, `Verificar`, `Status`; atalho `tools\dist\Jogar-e-Gravar-Campanha.cmd`;
- campanha com cartão próprio (os saves normais do jogador não são tocados), programa fixado na criação (cópia do exe e das
  DLLs, identidade e SHA-256 dos dados), selos SHA-256 de cada arquivo, sessões numeradas, checkpoints manuais (F5) com
  captura;
- primeira sessão do cartão vazio; cada sessão seguinte parte dos saves **reproduzidos** da anterior;
- recuperação de queda do jogo e do monitor; segunda instância recusada; campanha real recusa roteiro de teste;
- vereditos `CAMPANHA REPRODUZIDA ATE OS CREDITOS`, `REPRODUZIDA, MAS INCOMPLETA`, `REPRODUZIDA_ATE_A_QUEDA`, `REPROVADA`,
  `TESTE REPRODUZIDO`;
- `reproduzir-em <instalação|exe>`: a cadeia num outro programa (versão nova, verificação com `HH_ELEVACAO=conferir`,
  edição, Linux), comparando com a referência do programa fixado; nunca aprova nem reprova a campanha;
- **cobertura por sessão:** funções executadas (`HH_FN_PERFIL`), arquivos lidos, filmes, áreas; é a entrada da elevação;
- **verificação paralela:** cada sessão pode ser reproduzida a partir dos saves gravados ao mesmo tempo que as outras; a
  cadeia (saves reproduzidos → sessão seguinte) é conferida depois. A campanha toda leva horas para reproduzir, e a
  conferência das elevadas é mais lenta;
- **formato versionado:** manifesto com versão; leitores futuros aceitam as versões antigas. A gravação nunca é convertida
  no lugar.
- **Portões:** suíte `tests/campanha` com o jogo de verdade, equivalente aos C1–C10 do BS (nova campanha, continuidade com
  SAVE/LOAD reais, repetição, corrupção, divergência, queda do jogo, queda do monitor, segunda instância, créditos,
  `reproduzir-em` com outro programa, roteiro de teste recusado).

### HH16.P — Antes de gravar (portão de prontidão)

Nada aqui é gravação de verdade. Tudo deve passar antes da primeira sessão real:

| # | Verificação | Por quê |
|---|---|---|
| P1 | **Passagem do disco 1 ao 2**: localizar por análise estática a rotina que verifica o disco (`DISC/DISK{N}.ID`) e o fluxo do fim do disco 1; teste dirigido por estado forçado (só verificação) que atravessa a passagem na instalação única e chega ao primeiro controle do disco 2 | é o ponto mais provável de a campanha parar no meio; nunca foi exercitado |
| P2 | **Créditos**: identificar os filmes do fim pelo catálogo e o fluxo depois deles (volta ao título, save de fim) | o veredito depende disso |
| P3 | **Sessão piloto do mantenedor** (~15 min, marcada como teste): janela, gamepad real, um SAVE, fechar, continuar com LOAD, verificar | prova o caminho real (hardware, controle, áudio) antes da campanha |
| P4 | **Robustez de longa duração**: sessão assistida pelo agente de 60 min na janela, sem queda nem vazamento; perda e recuperação do dispositivo Vulkan; troca de dispositivo de áudio | uma queda não invalida a gravação, mas interrompe o mantenedor |
| P5 | **Backups**: cópia de cada sessão concluída para um segundo local escolhido pelo mantenedor, conferida por SHA-256 | a gravação é insubstituível |
| P6 | **Pacote de gravação**: instalação pelo Builder do produto atual, com o atalho do monitor, em `dist/` | o programa fixado é o do Builder, não um build de desenvolvimento |

### HH16.2 — Gravação (mantenedor)

O mantenedor joga normalmente, no ritmo que quiser. O agente não joga a campanha. Durante a gravação o agente continua a
HH15 em outros programas: a campanha usa o programa copiado na criação e não é afetada.

**Defeito achado durante a gravação** (bloqueio, queda, gráfico ou som errado): o mantenedor registra (sessão e passo, o
monitor mostra) e para ou continua; o agente reproduz a sessão até o ponto, corrige e gera um programa novo. A **troca do
programa fixado** só acontece se o novo reproduzir **todas** as sessões já gravadas sem divergência (ou com divergência
medida, explicada e aprovada pelo mantenedor); as sessões seguintes usam o novo programa. Nunca se regrava o que já foi
jogado.

### HH16.3 — Verificação, cobertura e triagem

- `Verificar` com o programa fixado: `CAMPANHA REPRODUZIDA ATE OS CREDITOS`;
- `reproduzir-em` uma instalação recém-gerada pelo Builder (o antigo "partir de instalação limpa");
- matriz de cobertura: áreas, chefes, filmes, arquivos lidos, funções do jogo e do SDK executadas (comparadas ao
  inventário da HH15.9; as nunca executadas viram candidatas a código morto, com evidência);
- triagem P0/P1 de tudo o que o mantenedor relatou e do que a reprodução mostrou.

### HH16.G — Gate final (depois da HH19)

A mesma gravação, por `reproduzir-em`, no produto sem AOT (HH15.F), nas duas edições (HH19), no Linux (HH20) e no pacote
final (HH18). Exceções só listadas, medidas e aprovadas pelo mantenedor.

## 4. Contratos que nenhuma etapa futura pode quebrar

### 4.1 Entrada e sequência

| Contrato | Exigência |
|---|---|
| Entrada | o índice do roteiro continua sendo a n-ésima leitura do controle do jogo; mapa de controles, mouse, menus do host e recursos da Remastered agem **antes** da entrada efetiva gravada |
| Sequência | a ordem e a quantidade de VBlanks, leituras do controle e esperas do jogo não mudam por apresentação, renderizador, resolução, áudio ou plataforma |
| Filmes e falas | terminam no mesmo passo lógico, mesmo com mod, filme remasterizado, outra duração de áudio ou outro decodificador; senão a diferença é medida e aprovada |
| Saves | cartão, arquivos e conteúdo do jogo original; mesmo `.vms` em todas as edições e plataformas (D-P3) |
| Relógio | RTC da sessão reaplicado; nada do jogo lê a hora real fora dele |
| Câmera e apresentação | qualquer câmera nova da Remastered muda só a apresentação; se tocar estado do jogo, a parte tocada é declarada e excluída da comparação entre edições, como o objeto câmera no BS |

### 4.2 O estado comparado

- **Camada observável (permanente, independe do layout da memória):** entrada consumida por passo, VBlank de cada passo,
  LOAD/SAVE e resultado, bytes dos saves, ordem dos arquivos lidos, comandos de áudio, filme iniciado e terminado e, na
  verificação, o hash dos quadros no renderizador em software do modo fiel.
- **Camada de RAM (enquanto houver RAM guest):** a RAM do jogo sem as pilhas das threads, como nas referências da HH15.
- **Depois da HH15.F** não há RAM guest. O build de verificação mantém um **exportador** que projeta as estruturas nativas
  no layout gravado, para a camada de RAM continuar comparável; a camada observável continua sem mudança. O exportador é
  requisito da HH15.F (não é opcional) e tem teste próprio contra a última versão com RAM guest.
- O programa fixado fica copiado na campanha: qualquer observável novo pode ser gerado depois reproduzindo com ele, sem
  jogar de novo.
- **Quadros: cada renderizador comparado consigo mesmo (2026-11-15).** A janela grava no Vulkan, e o Vulkan não é
  idêntico bit a bit ao software (o HH13 os compara por PSNR, `transitional/render_capture.cpp`). Os quadros gravados na
  janela só informam (`quadro_da_janela_diferente`); a primeira reprodução aprovada (estado por leitura e saves iguais à
  gravação, fim alcançado) fixa a referência em software da sessão (`quadros_referencia.txt`, selada), e toda verificação e
  `reproduzir-em` seguintes têm de ser iguais a ela. Gravar no software foi medido e descartado: 53 VBlanks/s sem
  janela nesta máquina (com a CPU dividida), abaixo dos 60 do jogo.

## 5. Efeitos nas outras etapas

| Etapa | Muda |
|---|---|
| HH15.11 | segue; HH16.0/HH16.1 andam em paralelo (ferramentas de verificação e a opção de gravação) |
| HH15.12+ | cada sistema elevado é conferido com `reproduzir-em` + `HH_ELEVACAO=conferir` nas sessões que o exercitam (o perfil de cobertura diz quais) |
| HH15.F | só com a cobertura da campanha conferida e o exportador da seção 4.2 |
| HH19 | todas as subetapas passam por uma sessão gravada curta (portão C8 do BS); nenhum recurso da Remastered pode quebrar a seção 4 |
| HH20 | prova de fidelidade inclui sessões da campanha |
| HH17/HH18 | mods padrão e pacote final reproduzem a campanha |

## 6. Ordem

```
HH15.11b → HH16.0 → HH16.1 → HH16.P → HH16.2 (mantenedor; HH15.11c–e em paralelo)
        → HH16.3 → HH15.12+ → HH15.F → HH20 → HH19 → HH19.E → HH16.G → HH17 → HH18
```

A HH16.0 começa logo depois de fechar a HH15.11b (a sonda dos back-edges em curso), para a gravação começar o quanto
antes.

## 7. Dados privados

A campanha contém saves, o executável derivado do disco e dados do jogo: nunca versionada nem publicada. Fica numa pasta
ignorada do workspace (`campanhas/`, no `.gitignore` desde 2026-11-14), mais a cópia de segurança do P5
(`campanhas/_copias` ou a pasta escolhida, de preferência em outro disco).
Nenhum script do projeto apaga ou reescreve uma campanha; limpeza só manual, pelo mantenedor.

## 8. Recomendações ao mantenedor para a gravação (versão final no guia da HH16.P)

- Salvar no jogo antes de fechar cada sessão: a sessão seguinte continua do último SAVE.
- Antes do fim do disco 1, salvar: se a passagem falhar, a correção retoma desse ponto.
- Usar F5 nos momentos marcantes (chefes, áreas novas): ajudam a localizar defeitos e a cobertura.
- Explorar como num jogo normal: menus, opções, armas e áreas opcionais aumentam a cobertura que a elevação usa.
- Pular filmes com START é permitido; deixar o filme dos créditos terminar.

## 9. Placar dos portões (2026-11-15)

### HH16.0

| Portão | Situação | Evidência |
|---|---|---|
| Gravar na janela e reproduzir sem janela com o estado idêntico | **pendente** (sessão piloto, P3) | o `--gravar` exige a janela |
| 3/3 reproduções iguais | aprovado | `hh16_reproducao`: duas reproduções e o mesmo roteiro por `HH_INPUT_SCRIPT`, quadros e estados idênticos |
| Ruído de ciclos e carga da CPU sem efeito | parcial | `hh16_campanha_real` reproduz duas sessões ao mesmo tempo (`--paralelo 2`); a medida com a máquina carregada é a P4a |
| Divergência injetada (botão) acusada na leitura certa | aprovado | analógico trocado na leitura 4500: primeiro estado diferente na 4560 (`hh16_reproducao` e `hh16_campanha_real`) |
| Divergência injetada (byte de save, leitura a menos) | parcial | save de início trocado acusado pela cadeia (programa falso); com o jogo real, pendente |
| Gravação truncada | aprovado | linha pela metade na queda: a reprodução usa a gravação até a última linha inteira (programa falso); o roteiro do produto recusa a linha incompleta |
| Quadros da janela (Vulkan) × reprodução (software) | aprovado | não são idênticos bit a bit: a janela informa e a referência em software, fixada na primeira reprodução aprovada, vincula (seção 4.2); programa falso com o "Vulkan" diferente e o negativo da referência |
| Fim da sessão fora da grade dos hashes | aprovado | achado no `hh16_campanha_real` (toda sessão saía REPROVADA): o `--reproduzir` para na leitura seguinte ao comentário de fim; `hh16_reproducao` e o programa falso cobrem |

### HH16.1 (C1–C10 do BS)

| Caso | Programa falso | Jogo de verdade (`hh16_campanha_real`) |
|---|---|---|
| Nova campanha, programa fixado, selos | sim | sim |
| Continuidade com SAVE/LOAD reais | sim | **pendente** (o roteiro do NEW GAME desliga o save; vai na P3) |
| Repetição (reproduzir duas vezes) | sim | sim (`verificar` e `reproduzir-em`) |
| Corrupção / arquivo alterado / cadeia quebrada | sim | — (lógica do monitor) |
| Divergência | sim | sim |
| Queda do jogo | sim (inclusive a linha pela metade) | — |
| Queda do monitor | sim (recuperada depois que o jogo fecha) | — |
| Segunda instância | recusada | — |
| Créditos | sim (SCENE84 + `GAMEEND.AHF`) | sinais medidos no produto com o mod local (seção 2 do `HH16-PREPARACAO.md`) |
| `reproduzir-em` outro programa | sim | sim |
| Roteiro de teste recusado na campanha real | sim; variáveis `HH_` não chegam ao jogo | — |
| Programa sem o contrato da gravação | recusado na criação | — |
| Gravador com tela (`HeadHunter-Gravador`, ctest `hh16_gravador`) | sim (iniciar, continuar, teste × real, Parar, sem Python) | sim, na janela: o Parar fechou o jogo, sessão `gravada` e `TESTE REPRODUZIDO` (2026-11-15) |

Ainda fora: checkpoints por tecla (F5 do BS) e a cobertura de funções por sessão (HH16.3, `TESTES-FUTUROS.md` seção 5).

### HH16.P

| # | Situação |
|---|---|
| P1 passagem do disco | aprovada no produto pelo mod local da área final; o caminho real é a própria campanha |
| P2 créditos | aprovada |
| P3 sessão piloto | **pendente: mantenedor** |
| P4 longa duração na janela | **pendente: GPU livre** |
| P5 cópia de segurança | aprovada (`campanhas/_copias` ou `--copia`) |
| P6 pacote de gravação | aprovado: `dist/HeadHunter-Preview` refeito em 2026-11-15 (`v0.11.0-4-gbfe3231-dirty`) passa na prova do contrato; o `hh16-bfe3231` é recusado. Antes da campanha real, refazer depois do commit, para o programa fixado ter versão limpa |
