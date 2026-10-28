# Plano mestre — Head Hunter Recompiled

Este é o roteiro completo, do repositório vazio ao produto nativo final. As etapas organizam evidências e relatórios, mas não interrompem o trabalho: o agente conclui uma, registra o resultado e continua automaticamente. Ele só para em uma decisão humana material definida na política ao final.

## Portões globais

Uma etapa só termina quando:

- todos os entregáveis existem;
- o nível mínimo de validação foi executado sem reduzir cobertura;
- `STATUS.md` e o relatório da etapa refletem fatos e limitações;
- dados privados continuam ignorados;
- a árvore de referência não foi alterada;
- não existem mudanças não explicadas no pacote de revisão.

## Etapas

| Etapa | Resultado revisável | Validação mínima |
|---|---|---|
| HH00 | Governança, repositório e esqueleto equivalentes ao Blue Stinger | V0 |
| HH01 | Inventário forense reproduzível dos dois discos e versões-alvo | V1 |
| HH02 | Extrator CDI multissessão, manifestos e identidade de disco | V2 |
| HH03 | Decodificador e semântica SH-4 completos | V2 |
| HH04 | Analisador, descoberta de funções e codegen C++20 | V2 |
| HH05 | Runtime híbrido inicial: boot, primeiro frame e oráculo | V2 |
| HH06 | Primeira fatia jogável e Preview/Builder inicial | V2 |
| HH07 | Fundação do port nativo e fronteiras mensuradas | V2 |
| HH08 | Arquivos nativos e troca dos dois discos sem CDI em runtime | V2 |
| HH09 | Tempo, eventos e tarefas nativos (fase híbrida) | V2 |
| HH10 | Controles nativos | V2 |
| HH11 | Saves nativos e continuidade entre discos | V2 |
| HH12 | Vídeos nativos e moddáveis | V2 |
| HH13 | Renderização nativa Vulkan (IR + backend + SDK gráfico) | V2 |
| HH14 | Áudio nativo e moddável | V2 |
| HH15 | Fim do tempo guest e remoção completa do legado | V3 |
| HH16 | Campanha completa determinística nos dois discos | V3 |
| HH19 | Remasterização: edições Original e Remastered do mesmo núcleo (executada depois do HH16 e antes do HH17) | V3 |
| HH17 | Gameplay nomeado, projeto editável e mods | V3 |
| HH18 | Recompiler/Developer finais, pacote e revisão de lançamento | V3 |

## HH00 — Fundação e governança

Entregáveis:

- inicializar o repositório Git na raiz sem incorporar `dream-recomp/` nem `ISO/`;
- criar a estrutura definida em `ARCHITECTURE.md`, CMake mínimo e testes smoke;
- importar somente componentes genéricos MIT estritamente necessários do Blue Stinger, com atribuição e registro por arquivo;
- criar `LICENSE`, `third_party/README.md`, ADR de licenciamento e scripts de verificação de dados privados;
- preservar os arquivos operacionais do OpenCode/Ollama fora dos alvos do produto;
- registrar toolchain e versões sem instalar ou atualizar nada sem autorização.

Critério de saída: configure/build/test mínimo passa numa árvore limpa; arquivos privados são recusados pelo Git; nenhuma dependência GPL entra no link.

## HH01 — Forense dos dois discos

Entregáveis:

- analisar os dois CDI sem extrair para caminhos versionados;
- registrar sessões, faixas, setor, LBA, IP.BIN, produto, versão, região, boot filename, tamanho e SHA-256 do executável;
- determinar se os discos usam o mesmo binário, executáveis diferentes, overlays, módulos carregáveis ou Windows CE;
- comparar árvores de arquivos por nome/tamanho/hash e localizar o contrato de troca de disco;
- criar configs preliminares por disco/versão e relatório REA/Ghidra com entry point, arquitetura e mapa inicial;
- definir nomenclatura canônica e matriz de versões aceitas.

Critério de saída: duas ferramentas independentes confirmam metadados críticos; nenhuma hipótese sobre endereços ou formato fica tratada como fato.

## HH02 — Ingestão e extração

Entregáveis:

- `hh-extract --info` somente leitura;
- suporte ao CDI observado, múltiplas sessões/faixas e arquivos que cruzam sessões;
- extração segura, reprodutível e resistente a path traversal;
- hash/manifesto de todos os arquivos, identificação dos dois discos e detecção de rip divergente;
- saída de runtime separada, sem CDI em execução;
- testes sintéticos e testes reais opcionais via variável de ambiente.

Critério de saída: extração completa de ambos os discos, conferida por implementação independente e reextração determinística.

## HH03 — Núcleo SH-4

Entregáveis:

- tabela declarativa das formas SH-4 usadas pelo SH7091;
- decodificação, disassembly, delay slots, modos de FPU e semântica completa;
- testes exaustivos dos 65.536 opcodes, vetores de borda e comparação independente;
- nenhuma dependência em dados do jogo nos unitários.

Critério de saída: todos os opcodes classificados sem sobreposição; divergências externas revisadas e documentadas.

## HH04 — Análise e tradução AOT

Entregáveis:

- descoberta recursiva, pools de constantes, jump tables, ponteiros, callbacks, relocação e overlays;
- config TOML para seeds, fronteiras, patches e módulos de ambos os discos;
- uma função C++20 por função guest, despacho rastreável e relatórios de cobertura;
- testes sintéticos que geram, compilam e executam C++;
- primeira geração completa que compile e linke.

Critério de saída: zero fluxos mortos/órfãos inexplicados; gaps conhecidos ficam configuráveis e observáveis, nunca silenciosos.

## HH05 — Boot híbrido e primeiro frame

Entregáveis:

- host SDL3, boot HLE, memória/hardware temporários suficientes para execução, portados do runtime MIT do Blue Stinger (`runtime/cpu`, `memory`, `hle`, `gdrom`, `pvr`, `maple`, `boot`, `debug`) com atribuição;
- dados lidos de `data/` gerada pelo `hh-extract` (modo `--runtime`), nunca do CDI;
- modo headless determinístico, trace e snapshots;
- Flycast externo como oráculo diferencial;
- boot até o primeiro frame natural, sem forçar PC/estado;
- captura/replay offline para diagnóstico gráfico.

Critério de saída: trace e framebuffer do marco concordam com tolerâncias documentadas e casos negativos provam o comparador.

## HH06 — Primeira fatia jogável

Entregáveis:

- menu, início de jogo e primeira área controlável do disco 1;
- scripts determinísticos de entrada e relógio fixo;
- Preview local e Builder inicial que converte a cópia do usuário;
- fluxo de sugestão de funções ausentes até eliminar misses da fatia;
- regressão retomável inicial.

Critério de saída: roteiro reproduzível chega à primeira área em modo janela e headless; build sem interpretador para o trecho coberto.

## HH07 — Fundação do port nativo

Entregáveis:

- alvos `native`, `legacy` e `transitional` separados;
- interfaces tipadas, handles com geração e regras de propriedade/reentrância;
- banco de funções identificadas e manifesto de hardware/dominância;
- métricas reproduzíveis de dependência guest e mapa de linker;
- provedores selecionáveis e slots de fronteira sempre ativos.

Critério de saída: comportamento permanece igual, custo medido e toda dependência legada tem dono/fronteira ou bloqueio explícito.

## HH08 — Arquivos e dois discos nativos

Entregáveis:

- provedor nativo da API de arquivos sobre `data/` instalada;
- nomes, offsets, tamanho e bytes equivalentes ao legado;
- troca de disco como estado lógico nativo, com prompt/UI e validação de identidade;
- mods de arquivos maiores que o original;
- CDI removido do executável do jogo.

Critério de saída: rotas dos dois discos funcionam sem acesso a setores ou à imagem; disco errado/corrompido falha com mensagem segura.

## HH09 — Tempo, eventos e tarefas

Entregáveis:

- relógio lógico e fila de eventos nativos;
- tarefas cooperativas, waits, callbacks, prioridades e reentrância documentados;
- modos legado/nativo comparados por ordem de eventos e snapshots lógicos;
- dependências temporárias de IRQ/ciclos catalogadas para HH15;
- como no Blue Stinger (N2a/N2b), o fechamento completo do tempo pode ficar para o HH15; HH12–HH14 usam só a interface de tempo nativa, nunca ciclos, e são validados nos modos de tempo existentes.

Critério de saída: roteiros determinísticos não divergem nas tolerâncias justificadas por evidência; nenhum busy-wait novo.

## HH10 — Controles

Entregáveis:

- entrada SDL3 para teclado e controles, hot-plug e remapeamento persistente;
- provedor nativo da API usada pelo jogo, sem Maple no caminho nativo;
- latência, dead zones, gatilhos, vibração quando suportada e desconexão testados;
- configuração amigável no produto.

Critério de saída: matriz de comandos e rotas de gameplay equivalente; nativo vira padrão automaticamente quando todos os portões objetivos passarem.

## HH11 — Saves e continuidade entre discos

Entregáveis:

- store nativo, slots, escrita atômica, integridade e migração versionada;
- importação/exportação do formato Dreamcast quando legal e tecnicamente aplicável;
- save/load real, corrupção, falta de espaço e nomes/caminhos testados;
- continuidade e validação do save na troca Disco 1 -> Disco 2.

Critério de saída: saves sobrevivem a reinstalação/upgrade; casos de falha não destroem o último save válido.

## HH12 — Vídeo

Entregáveis:

- inventário do middleware/formato de vídeo realmente observado;
- conversão offline no Builder para formato revisado e reprodução por interface nativa; o decodificador 4X Movie pode ser o FFmpeg como processo externo (A11), nunca linkado;
- sincronização, skip, aspect ratio, áudio e legendas;
- substituição por mods e catálogo inspecionável.

Critério de saída: todos os vídeos dos dois discos cobertos e comparados; caminho legado não é usado no modo nativo.

## HH13 — Vulkan nativo

Ordem trocada com o áudio por decisão do mantenedor (2026-10-25): como no Blue Stinger, o jogo precisa
ganhar desempenho (render nativo) antes de o áudio nativo poder ser testado em tempo real.

Entregáveis:

- IR de comandos cobrindo geometria (pode ser antecipado: no Blue Stinger, adiar o IR manteve o render em software, ~17 quadros/s, até o N7), texturas, paletas, clipping, transparência, punch-through, modificadores e framebuffer direto;
- replay offline e backend Vulkan;
- provedores nativos das APIs gráficas/middleware observados, assets convertidos pelo Builder;
- comparação de cenas estáticas, dinâmicas, efeitos, HUD, loading e vídeo.

Critério de saída: matriz visual atende às tolerâncias documentadas e o desempenho mínimo está comprovado; nenhum PVR2/Holly/VRAM guest no caminho nativo.

## HH14 — Áudio

Entregáveis:

- inventário de bancos, sequências, vozes, efeitos e driver observado (`AUDIO64.DRV` só por análise estática);
- o estado do driver de som é entrada da lógica do jogo: medir e documentar as diferenças de comportamento que o áudio nativo introduz (lição do N6 do Blue Stinger);
- motor nativo e saída atrás de interfaces, sem executar ARM7/driver original;
- música, efeitos, falas, mixagem, loops, envelopes e DSP necessários;
- mods de áudio/dublagem e diagnóstico por evento.

Critério de saída: matriz de cenas dos dois discos dentro das tolerâncias; nenhum acesso AICA alcançável no modo nativo.

## HH15 — Remoção do legado

Entregáveis:

- concluir pendências de tarefas/tempo e remover ciclos, IRQ e polling guest;
- remover interpretador, contexto SH-4, RAM guest, dispatch, MMIO e módulos de hardware do produto;
- mapa do linker/imports/strings e instrumentação provando ausência operacional;
- manter ferramentas de análise separadas do produto.

Critério de saída: V3 passa antes e depois da remoção; build do produto falha se qualquer biblioteca legada for religada.

## HH16 — Campanha completa

Entregáveis:

- matriz de cobertura de todos os capítulos, áreas, menus, chefes, vídeos, saves e troca de disco;
- roteiros retomáveis e checkpoints lógicos sem distribuir conteúdo do jogo;
- campanha do início aos créditos em build Release;
- triagem e correção de todos os bloqueadores P0/P1.

Critério de saída: duas execuções limpas completam a campanha; uma delas parte de instalação recém-gerada pelo Builder.

## HH19 — Remasterização (equivalente ao N9 do Blue Stinger)

Incluída pelo mantenedor em 2026-10-28. O número é o próximo livre (as etapas existentes não são renumeradas); a ordem
de execução é depois do HH16 e antes do HH17, como o N9 do BS fica entre o N8 e o N10. Só começa com o HH15 e o HH16
concluídos e autorização explícita do mantenedor. Plano: `docs/planos/HH19-PLANO.md`.

Entregáveis:

- duas edições instaláveis do mesmo núcleo C++ (Original, fiel ao Dreamcast, e Remastered), escolhidas no Builder por
  um perfil de edição gravado na instalação, sem fork nem branch por edição;
- experiência de PC na Remastered: menus e configurações no jogo, saves sem a interface de VMU, controles modernos,
  legendas ligadas, widescreen, gráficos e iluminação, texturas HD, filmes e áudio substituíveis, acessibilidade;
- a Original preservada e validada contra as referências existentes (estado, quadros do modo fiel, Flycast).

Critério de saída: as duas edições instaladas pelo Builder passam as validações próprias; a Original continua
equivalente às referências; nenhuma camada legada removida no HH15 volta.

## HH17 — Gameplay editável e mods

Entregáveis:

- elevar progressivamente câmera, movimento, combate, IA, HUD, inventário e fluxo do jogo para `native/game`;
- nomes, estruturas e contratos sustentados por evidência;
- API/versionamento de mods, assets substituíveis e exemplos sem conteúdo proprietário;
- projeto C++ exportável, recompilável offline e inspecionável.

Critério de saída: produto continua sem dependência guest; mods demonstrativos e contratos passam na campanha/revisão.

## HH18 — Produto e lançamento

Entregáveis:

- `HeadHunter-Recompiler.exe`: discos -> converter -> compilar -> instalar -> jogar;
- `HeadHunter-Developer.exe`: exportar projeto, compilar e inspecionar recursos/mods;
- localização pt-BR/en-US da interface, instalação portátil, logs e rollback seguro;
- build reproduzível, SBOM/licenças, manual, pacote limpo e teste em Windows limpo;
- revisão jurídica, arquitetural e de segurança final.

Critério de saída: V3 integral, instalação limpa, campanha completa, pacote sem dados proprietários e aceite explícito do mantenedor.

## Reuso da referência MIT e entradas conhecidas por etapa

Mapeamento completo de etapas e regras: `docs/planos/COMPARACAO-BSTINGER.md`.
Antes de implementar, cada etapa lê o componente equivalente em
`C:\Source\recomp-BStinger` (MIT) e o porta com atribuição quando servir.
Planos detalhados ficam em `docs/planos/HHxx-PLANO.md`.

| Etapa | Referência no Blue Stinger | Entradas já provadas (HH01–HH03) |
|---|---|---|
| HH04 | `translator/analysis`, `translator/codegen`, `translator/app`, `config/bluestinger_us.toml` | `1ST_READ.BIN` único, base/entry `0x8C010000`; difere entre discos só no dword `0x00175E28`; sem overlays; plano em `docs/planos/HH04-PLANO.md` |
| HH05 | `runtime/{cpu,memory,hle,gdrom,pvr,maple,aica,boot,debug}`, `app/game_host.*`, `tests/diff`, `tools/dev` (oráculo Flycast) | Katana, sem Windows CE |
| HH06 | `builder/`, `app/` | — |
| HH07 | `transitional/`, `legacy/`, `config/fronteiras_us.toml`, `config/metricas_base_us.toml`, `docs/PLANO-PORT-NATIVO.md` | — |
| HH08 | `native/` (arquivos) | troca de disco por `DISC/DISK{N}.ID` + dword embutido |
| HH12 | `docs/MODS-VIDEO.md` | vídeos `MOVIES/*.4XM` (4X Movie); `SCENE0.4XM` difere por disco |
| HH13 | `docs/DECISAO-VULKAN-LINUX.md`, `docs/MODS-SHADERS.md` | — |
| HH14 | `runtime/aica` (apenas referência de comportamento) | driver `AUDIO64.DRV` idêntico nos dois discos |
| HH18 | `docs/DISTRIBUICAO-COMPILADOR.md`, `docs/RECOMPILER-UI.md` | — |

## Política de decisões e revisão

Relatórios por etapa são checkpoints informativos. O agente não espera `APROVADO HHxx`: se os critérios objetivos passam, marca `CONCLUIDA` e segue.

Uma pausa é permitida somente quando a escolha não estiver resolvida pelo plano e mudar materialmente um destes itens:

1. licença ou incorporação permanente de dependência;
2. arquitetura pública ou formato persistente difícil de migrar;
3. versões/discos suportados;
4. compromisso de fidelidade, tolerância ou comportamento visível;
5. operação destrutiva ou risco a dados do usuário;
6. redução/aumento relevante do escopo final;
7. commit, publicação ou distribuição externa.

Ao pausar, o agente deve usar `BLOQUEADA_DECISAO` e apresentar uma pergunta objetiva com opções, evidências, recomendação e impactos. Não são motivos válidos para pausar: conclusão de etapa, mudança pequena, refatoração, teste falhando, investigação longa, escolha reversível, falta de confiança solucionável por REA/Ghidra/testes ou desejo genérico de revisão.

O mantenedor pode pedir `PAUSAR`, `REVISAR HHxx` ou alterar uma decisão a qualquer momento. Fora disso, o trabalho continua até HH18 ou até uma `DECISAO_NECESSARIA` legítima.

