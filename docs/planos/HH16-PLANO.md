# HH16 — Campanha completa por gravação única: plano

**Situação (2026-11-14):** planejamento; nada implementado. **Decisão do mantenedor (2026-11-14):** "Vou gravar a campanha
inteira para testar o jogo completo. Para não precisar zerar mais de uma vez." Com isso, D-P1 = A e D-P2 = sim
(`docs/planos/PLANO-ETAPAS-RESTANTES.md`, seção 9).

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
| Gravação da entrada efetiva durante o jogo | **falta** |
| Estado lógico por passo e reprodução que o confere | **falta** |
| Monitor de campanha (sessões, selos, vereditos, recuperação de queda) | **falta** |
| Passagem do disco 1 ao 2 dentro do jogo, na instalação única (HH08) | troca pela variável coberta por unitário; **passagem real nunca exercitada** |
| Identificação dos filmes dos créditos | **levantada** (2026-11-14): `SCENE84` seguido de `GameEnd` no script `S92_MOVIE` (`docs/planos/HH16-PREPARACAO.md`) |
| Carregar um save do disco 2 pelo título | **risco P0** (2026-11-14): o menu de carregar recusa save de outro disco, e no boot o disco atual é 1 (`HH16-PREPARACAO.md` seção 1) |

## 3. Subetapas

### HH16.0 — Gravação, estado por passo e reprodução

- **Gravação no produto:** opção explícita de linha de comando (não variável de teste), escrita pelo monitor: grava a
  entrada efetiva por leitura do controle (depois do mapa de teclado e gamepad, dos menus do host e de qualquer recurso
  futuro de apresentação), no formato do roteiro atual, mais os eventos de sessão (LOAD/SAVE concluídos, filme
  iniciado e terminado, VBlank). Gravação incremental com `flush` periódico: uma queda não perde o que já foi gravado.
- **Reprodução no produto e na verificação:** a mesma opção em modo de leitura, aceita pelos dois executáveis. A
  classificação de `validacao_vars.h` ganha a categoria "entrada determinística do produto", conferida pela catraca; as
  mutações e sondas continuam recusadas.
- **Relógio:** a data inicial do RTC de cada sessão é gravada e reaplicada; a data do envelope dos saves fica fora da
  comparação (é a hora real).
- **Estado por passo, em duas camadas** (seção 4.2): observável (permanente) e RAM do jogo sem as pilhas (enquanto houver
  RAM guest).
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
ignorada do workspace (`campanhas/`, a acrescentar ao `.gitignore` e às verificações de `git check-ignore` da regressão), mais a cópia de segurança do P5.
Nenhum script do projeto apaga ou reescreve uma campanha; limpeza só manual, pelo mantenedor.

## 8. Recomendações ao mantenedor para a gravação (versão final no guia da HH16.P)

- Salvar no jogo antes de fechar cada sessão: a sessão seguinte continua do último SAVE.
- Antes do fim do disco 1, salvar: se a passagem falhar, a correção retoma desse ponto.
- Usar F5 nos momentos marcantes (chefes, áreas novas): ajudam a localizar defeitos e a cobertura.
- Explorar como num jogo normal: menus, opções, armas e áreas opcionais aumentam a cobertura que a elevação usa.
- Pular filmes com START é permitido; deixar o filme dos créditos terminar.
