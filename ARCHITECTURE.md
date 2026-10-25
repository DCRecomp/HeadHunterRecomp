# Arquitetura-alvo — Head Hunter Recompiled

Status: planejamento inicial. A referência estrutural é `C:\Source\recomp-BStinger`, sem alterar esse projeto.

## Objetivo final

Produzir uma recompilação estática AOT de **Head Hunter para Dreamcast** em C++20, inicialmente para Windows x64, criada a partir das duas imagens de disco fornecidas legalmente pelo usuário.

O produto final deve:

- executar `HeadHunter.exe` sem BIOS do Dreamcast e sem interpretar instruções SH-4;
- não conter emulação operacional de CPU, RAM guest, MMIO, Holly/PVR2, AICA, Maple, ciclos ou IRQ;
- usar sistemas nativos para arquivos, tarefas/tempo, controles, saves, vídeo, áudio e renderização Vulkan;
- suportar integralmente os dois discos, incluindo troca de disco e continuidade de saves;
- ser jogável do início aos créditos, com equivalência funcional documentada;
- oferecer código C++20 recompilável, sistemas de gameplay nomeados e suporte a mods;
- fornecer `HeadHunter-Recompiler.exe` para jogadores e `HeadHunter-Developer.exe` para desenvolvimento/mods;
- usar as imagens somente na conversão/instalação; o jogo instalado não lê CDI em execução.

## Estratégia de implementação

O caminho preserva a arquitetura comprovada no Blue Stinger:

```text
Discos 1 e 2 do usuário
        |
        v
tools/hh-extract -> game-data/ + manifestos e hashes (privados)
        |
        v
translator/analysis + config/headhunter.toml + config/discs/*.toml
        |
        v
generated/*.cpp (privado, uma função por função SH-4)
        |
        v
runtime híbrido + native/ + transitional/ + legacy/
        |
        v
HeadHunter.exe
        |
        v
migração progressiva até native/ sem legacy/runtime guest
```

### Três camadas durante a migração

1. **Interface nativa tipada:** sem endereços guest e sem `Sh4Context`.
2. **Adaptador transitório:** converte ABI/RAM guest para tipos nativos; é removido quando os chamadores forem elevados.
3. **Provedor selecionável:** `legado` e `nativo` coexistem para testes diferenciais até o portão de promoção.

Handles do host nunca entram na RAM guest. Propriedade, duração, callbacks, reentrância e layouts que cruzam a fronteira devem ser explícitos e versionados.

## Componentes

```text
app/                 host, CLI e HeadHunter.exe
builder/             Recompiler, Developer, exportação e instalação
config/              versões/discos, funções extras, fronteiras e hashes
docs/                ADRs, planos e relatórios
legacy/              simulação temporária; proibida no produto final
native/
  platform/          janela SDL3, entrada, tempo, arquivos e saves
  render/            IR e backend Vulkan
  audio/             motor e saída nativos, atrás de interfaces
  video/             conversão offline e reprodução nativa
  sdk/               substituições tipadas das APIs realmente usadas
  game/              câmera, movimento, combate, IA, HUD, inventário etc.
runtime/              CPU/memória/hardware temporários e contratos comuns
tests/                unitários, integração, diferencial e campanha
tools/                extrator, analisadores, geração, oráculos e distribuição
transitional/         adaptadores temporários guest <-> nativo
translator/           SH-4 decoder, análise e codegen AOT
```

## Decisões iniciais

| ID | Decisão |
|---|---|
| A1 | C++20, CMake e Ninja; MSVC sempre suportado. |
| A2 | Windows x64 é a plataforma inicial; contratos e gameplay permanecem portáveis para Linux x86_64 futuro. |
| A3 | Vulkan atrás de `IRenderDevice`; SDL3 para janela/entrada; backends de áudio e vídeo são escolhidos por evidência de formato e fidelidade. |
| A4 | Licença do projeto novo: MIT, com atribuição ao código MIT reutilizado do Blue Stinger. |
| A5 | `dream-recomp`/Flycast GPL são ferramentas externas; nenhum código GPL é copiado, adaptado ou linkado ao produto MIT. |
| A6 | Versões-alvo são definidas somente após inventário e SHA-256 dos boot binaries dos dois discos. |
| A7 | As duas imagens CDI são suportadas como um único título; troca e identidade do disco são requisitos de arquitetura desde HH01. |
| A8 | Código e dados proprietários gerados permanecem locais e ignorados pelo Git. |
| A9 | O Builder é o fluxo de distribuição: o usuário fornece os discos e recompila/instala na própria máquina. |
| A10 | Nenhum commit é criado sem autorização explícita do mantenedor. |
| A11 | FFmpeg (LGPL) é permitido como **ferramenta externa** de conversão dos vídeos 4X Movie no Builder (decisão do mantenedor, 2026-10-24): executado como processo separado, nunca linkado nem com código copiado para o produto MIT; o jogo instalado toca só o vídeo já convertido. Se e como o binário do FFmpeg acompanha o pacote do Builder é decidido no HH18, com o aviso de licença correspondente. |

## Padrões herdados do Blue Stinger

Comparação completa em `docs/planos/COMPARACAO-BSTINGER.md`. Salvo evidência
contrária específica do Head Hunter, valem as decisões do Blue Stinger
(`C:\Source\recomp-BStinger\ARCHITECTURE.md` §4–§6 e
`docs/PLANO-PORT-NATIVO.md`).

### Tradução (HH04)

- Uma função C++ por função SH-4 (`void f_8C0123A0(Ctx*)`); desvios internos
  viram `goto`, chamadas diretas viram chamadas C++, `rts` vira `return`.
- Descoberta recursiva a partir do entry, de `mov.l @(disp,PC)` + `jsr`, de
  ponteiros no pool e em dados (com teste de "cara de função" e fronteira), de
  tabelas de ponteiros e de jump tables `mova`/`mov.w @(R0,Rn)` + `braf`;
  iterada até o ponto fixo. Invariantes a cada execução: zero fins mortos e
  zero instruções órfãs.
- Delay slot emitido antes do desvio, com alvo e condição calculados antes.
- Pool não embutido quando o jogo grava nele (`inline_pool_constants`);
  alvos resolvidos conferidos em execução.
- Chamada indireta por despacho gerado; endereço desconhecido é registrado
  com PC de origem e estado, nunca ignorado.
- FPU: PR/SZ propagados por bloco, variantes com teste em execução quando o
  modo é ambíguo; banco FR por índice. Arredondamento do guest (RM=01)
  aplicado à thread da CPU, portanto **sem libm nessa thread**; `FSCA` por
  tabela; FTZ/DAZ com `FPSCR.DN=1`.
- Código copiado para RAM declarado em `[[relocated]]` e conferido em
  execução; overrides por símbolo `f_<endereço>`.
- Configuração por toml++ (MIT).

### Runtime híbrido (HH05–HH06)

- Interrupções entregues por poll em back-edges, chamadas e retornos;
  `rte` para outro contexto e setjmp/longjmp entre threads do guest tratados
  genericamente, com erro fatal fora dos casos reconhecidos.
- HLE de BIOS e GD-ROM sobre a pasta `data/` instalada desde o primeiro boot;
  o executável nunca abre o CDI.
- Componentes genéricos do runtime MIT do Blue Stinger são portados com
  atribuição em vez de reescritos.

### Backends padrão (revisáveis por evidência)

Vulkan atrás de `IRenderDevice` com IR de comandos; SDL3 estático para
janela/entrada; áudio por `IAudioOutput` (XAudio2 no Windows) com motor nativo
reconstruído, **sem executar o ARM7 nem o `AUDIO64.DRV`**; vídeo convertido
offline pelo Builder e tocado por `IVideoPlayer` (Media Foundation no
Windows), desde que formato e fidelidade sejam validados para o 4X Movie.

## Prova de equivalência

- Decodificador: classificação exaustiva dos 65.536 opcodes e comparação independente.
- Funções: duas evidências independentes antes de nomear ou substituir.
- Execução: unitários sintéticos, traces, contratos normalizados e Flycast como oráculo externo.
- Render: replay offline de IR/capturas, comparação de frames e tolerâncias explícitas.
- Áudio: eventos lógicos e comparação de saída com tolerância documentada.
- Gameplay: snapshots lógicos de sala, posição, vida, inventário, flags e progresso.
- Campanha: matriz determinística cobrindo ambos os discos, menus, vídeos, chefes, saves, troca de disco e créditos.

## Critério operacional final

O binário distribuído não pode conter nem alcançar:

- `Sh4Context`, despacho por endereço guest ou interpretador SH-4;
- RAM/VRAM/AICA guest, MMIO, P4, store queues, Holly, Maple, ciclos ou IRQ;
- execução de firmware, BIOS, SDK Sega ou driver ARM7;
- leitura dos CDI durante o jogo instalado.

Endereços Dreamcast podem permanecer somente em ferramentas, mapas, documentação e oráculos externos.

