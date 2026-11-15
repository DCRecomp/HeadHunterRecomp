# Plano das etapas restantes (HH15 parte 2 → HH18), revisto com o Blue Stinger de 2026-11-12

**Situação (2026-11-14):** planejamento. Nada abaixo foi implementado nesta revisão. O estado autoritativo continua em
`STATUS.md`; as decisões marcadas **D-P** estão pendentes com o mantenedor (seção 9) e não bloqueiam a HH15.11.

Fonte da comparação: `C:\Source\recomp-BStinger` (MIT, somente leitura), commit `fbc6761` (2026-11-12): `CLAUDE.md`,
`STATUS.md` (roadmap N0–N10), `docs/N8-PLANO.md` (9.1), `docs/N8-8-FECHAMENTO.md`, `docs/N9-PLANO.md` (seções 5, 13–16),
`docs/N9-L-PLANO.md`, `docs/N9-L-LICENCAS.md`, `docs/CAMPANHA.md`, `docs/N9-6-TESTES-FINAIS.md`, `docs/TELEMETRIA.md`,
`docs/PUBLICACAO-GITHUB.md`.

## 1. Onde cada projeto está

| | Blue Stinger (2026-11-12) | Head Hunter (2026-11-14) |
|---|---|---|
| Sem simulação de hardware no produto | N8.8, fechamento técnico com ressalvas (2026-11-02) | HH15 parte 1, concluída com V3 14/14 (2026-10-30) |
| Substrato AOT (gerado, RAM guest, `Sh4Context`, fibras) | fica até o N10 (D-N8-1); N10 não iniciado | sai na HH15 (D-HH15-1 = B); 33 funções elevadas, HH15.11b em curso, parado desde 2026-10-31 |
| Campanha completa | obrigatória, **transferida ao gate pós-N9**: uma gravação do mantenedor valida as duas edições; monitor `campanha.py` pronto e testado (C1–C10) | HH16, prevista como roteiros do agente; **não existe** gravação, estado por passo nem monitor |
| Remasterização | N9 em curso (N9.0–N9.2, N9.5–N9.7 feitos; N9.3 cancelado; N9.12 criado) | HH19 só planejada |
| Linux | N9-L (L0–L5), v0.8.0 publicada; pendências P-L1 (desempenho) e P-L2 (instalador) | só a premissa A2 do `ARCHITECTURE.md`; nenhuma etapa |
| Publicação | cada etapa aprovada vira release (mantenedor, 2026-11-08) | v0.11.0 publicada na DCRecomp; `dist/` por etapa |

O HH está **à frente** do BS na remoção do AOT (o BS nunca executou o N10). Para essa parte não há precedente; para
campanha, remasterização, Linux, licenças e publicação há, e o plano abaixo os incorpora.

## 2. Problemas que o Blue Stinger já teve e a regra adotada aqui

| # | Problema no BS (evidência) | Regra no HH | Onde entra |
|---|---|---|---|
| L1 | Campanha completa inviável como roteiro do agente; virou gravação única do mantenedor, reutilizada nas duas edições (`N8-PLANO` 9.1, `N9-PLANO` 14) | campanha = gravação humana por passo + reprodução headless que prova; o agente constrói e testa o monitor antes | HH16 (seção 4) |
| L2 | Apresentação mudando o estado do jogo invalidaria a gravação (`N9-PLANO` 14.2) | contratos congelados: entrada efetiva, passos, estado lógico, saves, VBlanks, fim de fala e de filme; portão curto (C8) em cada subetapa | HH16.0 em diante |
| L3 | N7 ficou semanas em "95 %" e N8 fechou "com ressalvas" | toda pendência tem dono, critério e etapa; nada fica em porcentagem aberta sem registro em `STATUS.md` | todas |
| L4 | H9 (áudio) intermitente: suspenso, nunca corrigido (`N8-8` 5.1) | falha intermitente tem reprodutor com N repetições e taxa antes/depois (modelo `n9_vulkan_perda.py`: 11/30 → 0/30); suspender não aprova | todas |
| L5 | Executor da regressão com limite (3 h) menor que a etapa (6,1 h): reprovação falsa (`N8-8` 5.4) | antes de cada V3, conferir duração medida × limite de cada passo do `tools/dev/regressao.py`; nunca cortar casos | HH15.F, HH16, HH19, HH18 |
| L6 | V3 aprovada "por composição" de execuções, por decisão do mantenedor (`N8-8` 1) | V3 é uma bateria contínua e retomável; composição só com decisão registrada | todas as V3 |
| L7 | `fronteiras_us.toml` parado desde o N2a, desatualizado em 262 entradas (`N8-8` 5.5) | `config/fronteiras.toml` e inventários de elevação atualizados a cada subetapa; catraca acusa divergência | HH15.11+ |
| L8 | Protótipo de outro agente integrado na árvore principal sem compilar; cabeçalho particular quebrou o build limpo (`N9-PLANO` N9.12) | protótipo entra como passo A0 com commit próprio e portões; build limpo sem arquivos particulares é passo da regressão | HH19 |
| L9 | Perda do dispositivo Vulkan na APU AMD sob carga de CPU; cache de descritores não descartado ao recriar imagens | matriz com APU + CPU carregada; falha injetada e real; recuperação do dispositivo coberta por teste | HH19 (otimização) |
| L10 | Estimativa de VRAM por constante (96 B/px) errada (~268 B/px medidos) | orçamento de memória medido por recurso, não por constante | HH19 |
| L11 | Saves de PC (N9.3) cancelados para preservar a fidelidade | saves do jogo original intocados nas duas edições (ver D-P3) | HH19 |
| L12 | Melhorias gráficas ligadas por padrão geraram instabilidade; virou "presets seguros" | toda melhoria da Remastered começa desligada ou no nível seguro; ligar é escolha do jogador | HH19 |
| L13 | Instabilidade acumulada nas subetapas exigiu uma etapa própria (N9.12) | HH19.E estabilização antes do gate final de campanha | HH19 |
| L14 | Linux: fibras, write-watch, Media Foundation, DirectWrite, XAudio2; caminhos com `\` e maiúsculas; `std::execution::par` sem TBB sequencial em silêncio; fidelidade entre compiladores | código novo sem Win32 fora de backends; caminhos portáveis; opções `-fno-strict-aliasing -fwrapv -ffp-contract=off`; referências rodando no Linux como prova | regra já hoje; HH20 |
| L15 | Política de licenças só formalizada em 2026-11-03 e codecs com patente (H.264/AAC) | regra permanente de dependências (`AGENTS.md`); inventário de licenças antes de qualquer pacote | HH18, HH20 |
| L16 | Arquivos temporários fora da pasta de trabalho (regra urgente de 2026-11-08) | nada fora do workspace; `tmp/` ignorado; suítes recebem pasta de saída | regra já hoje |
| L17 | Histórico Git com material que não podia ser publicado, reescrito antes da publicação (`PUBLICACAO-GITHUB.md`) | varredura de publicação (histórico e pacote) antes de cada release | cada release |
| L18 | Telemetria só depois, opt-in e sem rede (`TELEMETRIA.md`) | telemetria de desempenho opt-in, local, sem rede, desde a otimização | HH19 |
| L19 | Instalador Linux por `instalar.sh` reprovado pelo mantenedor (P-L2) e desempenho Linux sem medida (P-L1) | instalador gráfico nas duas plataformas; diagnóstico medido antes de otimizar | HH20, HH18 |

## 3. HH15 (restante): elevação e remoção do substrato AOT

### 3.1 Problema de cobertura descoberto nesta revisão

O critério da HH15.F é tirar a RAM guest, o `Sh4Context`, o despacho e as fibras, e o método (plano da HH15, 7.2) exige
que **nenhuma função entre no produto sem conferência registrada**. Hoje a única rota é `newgame_area1_nativo`: 1 428 das
3 521 funções do jogo e 433 das 1 178 do SDK. As outras ~2 800 só são exercitadas por campanha ou por testes sintéticos.
Como a campanha (HH16) estava planejada **depois** da HH15 e **sobre o produto nativo**, a HH15.F depende de uma rota que
só existiria depois dela. É um ciclo. Opções na D-P1.

### 3.2 Subetapas

| ID | Entrega | Portão |
|---|---|---|
| HH15.11b | fechar a biblioteca matemática; a sonda dos back-edges (trabalho em curso na árvore, não commitado) | conferência em todas as chamadas; V2 |
| HH15.11c | Shinobi (390 funções classificadas; 53 chamadas diretamente pelo jogo) | idem; superfície chamada pelo jogo zerada para a família |
| HH15.11d | CRI (272; 13 diretas), Kamui (58; 9), som (33; 9), BIOS (18; 2) — quase tudo já atendido por serviço nativo: descartar o que o serviço cobre, elevar o resto | zero chamadas do jogo ao SDK gerado na rota |
| HH15.11e | as 737 funções sem família e as 151 desconhecidas: classificar com duas evidências | inventário sem "sem família" nas executadas |
| HH15.12+ | sistemas do jogo, um por subetapa (laço e tarefas, cenário e carga, jogador, câmera, colisão, inimigos e IA, HUD e menus, eventos e scripts, saves) | conferência por sistema, referências da HH15 iguais, **e** reprodução das sessões gravadas disponíveis (HH16.0) |
| HH15.F | dados nativos; remover RAM guest, `Sh4Context`, despacho e fibras; catraca proíbe `hh_generated` e `hh_runtime_cpu` | V3 contínua; **cobertura conferida na campanha gravada** (D-P1) |

Regras adicionais desta revisão:
- `config/fronteiras.toml` e o inventário (`hh15_elevacao_inventario.py`) atualizados por subetapa (L7).
- Ritmo: 33 elevadas em dois dias, com contrato completo por função. Para ~1 800 funções executadas, a HH15.11e passa a
  gerar o esqueleto do adaptador a partir da análise (pool, back-edges, custo) e o trabalho manual fica na lógica; a
  conferência não muda.
- Ao remover as fibras (HH15.F), o principal bloqueador do Linux no BS (B1) desaparece; registrar isso na HH20.

## 4. HH16: campanha completa (reestruturada no modelo do BS)

O critério antigo ("duas execuções limpas completam a campanha") supunha roteiros do agente do início aos créditos, que o
BS descartou. Novo desenho:

| ID | Entrega | Portão |
|---|---|---|
| **HH16.0** | infraestrutura (antecipável, independe de decisão): `HH_GRAVAR` (entrada efetiva por passo de jogo, mesmo formato do `HH_INPUT_SCRIPT`), `HH_STATE_PASSO` (estado lógico por passo, máscara de pilhas da HH15), reprodução headless conferindo estado, VBlanks, LOAD/SAVE e saves; gameplay assistido para o agente (pausa entre VBlanks, capturas, roteiro gravado) | gravar → reproduzir igual em 3/3; divergência injetada acusada no passo certo |
| **HH16.1** | monitor de campanha (`tools/dev/campanha.py`, modelo `docs/CAMPANHA.md` do BS): sessões, cartão próprio, selos SHA-256, programa fixado, vereditos, `reproduzir-em`, recuperação de queda; detecção dos créditos do HH; a passagem do disco 1 ao 2 dentro da mesma instalação (sem troca, HH08) | suíte `test_campanha` C1–C10 equivalente, com o jogo de verdade |
| **HH16.2** | gravação da campanha pelo mantenedor (humano; o agente não joga a campanha) | `CAMPANHA REPRODUZIDA ATE OS CREDITOS` |
| **HH16.3** | matriz de cobertura (capítulos, áreas, chefes, filmes, arquivos lidos, funções executadas por sessão), triagem P0/P1 e correção | matriz completa; nenhum P0/P1 aberto |
| **HH16.G** | gate final, depois da HH19: `reproduzir-em` nas duas edições, no produto sem AOT e no Linux (HH20) | sem divergência; exceções só listadas e aprovadas |

Contratos congelados desde a HH16.0 (L2): a entrada gravada é a efetiva (mapa de controles, mouse e menus antes dela); a
contagem de passos e o estado lógico não mudam por apresentação; saves do jogo original; sequência de VBlanks e esperas;
fim de fala e de filme no mesmo passo lógico, mesmo com mod ou versão remasterizada. Portão curto (sessão gravada de teste)
em cada subetapa da HH15.12+, HH19 e HH20.

## 5. HH20 (nova): Linux nativo (equivalente ao N9-L do BS)

Número livre seguinte; não renumera nada. Posição proposta: depois da HH16.3 e antes da HH19.1 (instalador), como no BS
(o N9-L ficou antes do N9.1 e do N9.2 porque instalador e interface precisam nascer portáveis). Depende da D-P4.

| ID | Entrega | Portão |
|---|---|---|
| L0 | presets e CMake por plataforma; opções obrigatórias (L14); build limpo sem arquivos particulares | Windows idêntico; ctest |
| L1 | núcleo sem janela: escrita atômica POSIX (`saves_store`), plataforma POSIX, sem fibras (pós-HH15.F) | boot até o gameplay por roteiro |
| L2 | **prova de fidelidade**: referências da HH15 e sessões gravadas no executável Linux | iguais; senão parar e apresentar |
| L3 | janela: SDL3, Vulkan, saída de áudio pelo SDL (no lugar do XAudio2), entrada | sessão jogável em Linux real |
| L4 | filmes e texto: `mf_movie` (H.264/AAC do Media Foundation) e `subtitles` (DirectWrite) sem equivalente; estratégia de codec sem patente no pacote (D-P5) e FreeType com fonte livre | filmes, legendas e menu; mods com maiúsculas diferentes |
| L5 | Builder Linux e compilador portátil (no BS: Zig 0.13 = Clang/LLD 18 com glibc 2.31) | instalação limpa; dados iguais aos do Windows por SHA-256 |
| L6 | SteamOS/Bazzite (prioritário no BS): RADV, gamescope, controles, atalho; desempenho medido (P-L1) | sessão de 30 min sem queda, números registrados |
| L7 | regressão Windows e revisão Codex | sem regressão |
| L8 | publicação dos dois pacotes, após o inventário de licenças | varredura limpa, `SHA256SUMS.txt` |

Arquivos do HH que hoje dependem de Win32 (medido em 2026-11-14 por `rg`): `runtime/cpu/sh4_threads.cpp` (fibras),
`native/video/mf_movie.cpp`, `native/video/subtitles.cpp`, `native/platform/saves_store.cpp`,
`native/audio/audio_output.cpp`. Validação real exige máquina Linux/Deck do mantenedor (no BS, B7: WSL não basta).

## 6. HH19: remasterização (ajustes ao plano existente)

O `docs/planos/HH19-PLANO.md` continua valendo; acrescentar ao iniciar (sem antecipar, regra do mantenedor):
- **Ordem do BS revista:** HH19.0 perfis de edição → HH19.1 instalador (Win32 + Direct2D; Linux SDL3 + FreeType) →
  HH19.2 interface e menu de configurações (seção 5 do plano) → gráficos primeiro (widescreen e HUD, luz e materiais,
  texturas HD e AA) → controles de PC → filmes, áudio e acessibilidade → refinamentos → otimização (matriz NVIDIA, AMD,
  Intel e APU com CPU carregada; VRAM medida; telemetria opt-in) → **HH19.E estabilização** → HH16.G.
- **Saves:** remover "saves sem a interface de VMU" como sistema novo; manter o do jogo, como o BS fez ao cancelar o N9.3
  (no BS ficou só a lista do LOAD sem VMU e o salvar desativado na pausa) — D-P3.
- **Presets seguros** (L12), **marca cena/HUD pela IR nativa** antes do widescreen, **câmera** a estudar (no BS a Livre é
  obrigatória; no HH a câmera não foi estudada).
- **Filmes remasterizados com IA** (N9.13 do BS, método aprovado no E017: original ampliado pelo Real-ESRGAN no centro e
  laterais pelo Wan2.1-VACE plano a plano): `docs/planos/HH19-PLANO.md` seção 6. No HH só 5 dos 53 filmes são 4:3 e
  precisam das laterais; os outros 48 já são 16:9 ou mais largos e só passam pela ampliação.
- Do BS v0.9.7-1: legendas incluídas no pacote, "Adicionar à Steam" desmarcado por padrão, Remastered no máximo seguro ao
  instalar.

## 7. HH17 e HH18

- **HH17** (sem precedente no BS, cujo N10 não começou): nomes e estruturas com duas evidências sobre o que a HH15 elevou;
  API versionada de mods sobre os contratos já usados (texturas por conteúdo, filmes e falas por ID, shaders, legendas);
  pacotes oficiais com prioridade abaixo dos mods do usuário; projeto exportável.
- **HH18**: Recompiler/Developer finais nas duas plataformas; inventário de licenças no modelo `N9-L-LICENCAS.md` (FFmpeg
  externo, codecs, fontes, SDL3, Vulkan, compilador portátil); SBOM; varredura do histórico e dos pacotes (L17); teste em
  Windows limpo e Linux limpo; aceite do mantenedor.

## 8. Ordem proposta

```
HH15.11b → HH16.0 → HH16.1 → HH16.P → HH16.2 (mantenedor; HH15.11c–e em paralelo)
        → HH16.3 → HH15.12+ → HH15.F → HH20 → [autorização] HH19 → HH19.E → HH16.G → HH17 → HH18
```

Atualizada em 2026-11-14 depois das decisões D-P1 e D-P2; detalhes em `docs/planos/HH16-PLANO.md`.

A HH16.0 e a HH16.1 podem andar em paralelo com a HH15.11 (são ferramentas de verificação, sem tocar no produto). Se a
D-P1 escolher manter a ordem atual, a linha fica `HH15 inteira → HH16 → HH20 → HH19 → HH17 → HH18`.

## 9. Decisões pendentes (D-P)

| # | Pergunta | Recomendação | Motivo |
|---|---|---|---|
| D-P1 (**decidida 2026-11-14: A**) | Como a HH15.F obtém cobertura das ~2 800 funções fora da área 1? (A) gravação do mantenedor antes da HH15.F, sobre o produto atual, reproduzida em cada programa seguinte; (B) testes diferenciais sintéticos por função, sem campanha; (C) voltar ao modelo do BS: AOT até a HH17 | **A** | a mesma gravação valida HH15.F, HH19 e HH20 (o `reproduzir-em` do BS); B custa milhares de casos e não prova sequência; C desfaz a D-HH15-1 |
| D-P2 (**decidida 2026-11-14: sim**) | A campanha passa a ser gravação humana única (modelo BS) em vez de roteiros do agente? | **sim** | o BS mostrou que roteiro do agente até os créditos não é viável; gravar uma vez evita jogar de novo |
| D-P3 | Na Remastered, os saves continuam os do jogo (sem sistema novo, como o N9.3 cancelado no BS)? | **sim** | fidelidade e contrato da gravação |
| D-P4 | Incluir a HH20 (Linux) e onde | **incluir, antes da HH19.1** | instalador e interface nascem portáveis; após a HH15.F o bloqueador das fibras some |
| D-P5 | Filmes no Linux: o Builder converte 4XM para H.264/AAC pelo Media Foundation do Windows. Manter H.264 (codificador do sistema) ou trocar o formato convertido para codec aberto nas duas plataformas | **decidir na HH20.L4 com medição** | política de licenças e patentes; a decisão D do HH12 (FFmpeg externo) continua |

Nenhuma delas impede a HH15.11b, a HH15.11c–e, a HH16.0 e a HH16.1.
