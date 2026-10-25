# Instruções do projeto Head Hunter Recompiled

Este arquivo é obrigatório para qualquer agente que trabalhe neste workspace.
O objetivo, a arquitetura e as etapas estão em `ARCHITECTURE.md` e `ROADMAP.md`.
O estado autoritativo está em `STATUS.md`.

## Protocolo de início

1. Leia integralmente `AGENTS.md`, `ARCHITECTURE.md`, `ROADMAP.md`, `LEGAL.md` e `STATUS.md`.
2. Consulte `C:\Source\recomp-BStinger` somente como projeto de referência MIT. Não altere essa pasta.
3. Trate `dream-recomp/` e o Flycast como ferramentas ou oráculos externos GPL-2.0. Não copie nem ligue código deles ao produto MIT.
4. Localize em `STATUS.md` a primeira etapa que não esteja `CONCLUIDA`.
5. Retome essa etapa e continue sequencialmente pelas seguintes enquanto não houver uma decisão humana real pendente.

## Autonomia contínua e decisões humanas

- Execute continuamente investigação, implementação, testes e documentação. Não pare por arquivo alterado, subetapa, teste concluído ou relatório produzido.
- Ao terminar uma etapa, gere o relatório, marque-a `CONCLUIDA`, marque a próxima `EM_ANDAMENTO` e continue automaticamente.
- Uma etapa terminada é um checkpoint informativo, não um pedido de aprovação.
- Não peça confirmação para leitura/análise, ferramentas já instaladas, mudanças reversíveis, correções de bugs, criação de testes, escolhas internas cobertas pela arquitetura ou promoção que atingiu todos os portões objetivos.
- Antes de concluir algo sobre o binário, faça investigação profunda e reproduzível. Uma afirmação sem comando, artefato, hash/endereço e confirmação independente é hipótese, não evidência.
- Se um teste ou evidência contradizer o relatório atual, reabra a etapa, corrija o relatório e continue; não peça ao mantenedor para validar trabalho que a máquina pode validar.
- Nomes como `hh-extract`, `hh-recomp`, `HeadHunter.exe` e outros binários do roadmap são **entregáveis a implementar**, não pré-requisitos que o usuário deve instalar. Se ainda não existem, investigue a referência MIT, projete, implemente, compile e teste na etapa correspondente.
- Não peça ao usuário para instalar ou localizar uma ferramenta que o próprio roadmap manda construir. Antes de declarar bloqueio, procure com `rg`/`rg --files`, examine CMake e referências, verifique ferramentas já disponíveis e implemente o componente ausente quando ele estiver no escopo.
- Não invente comandos, flags, formatos ou saídas futuras. Confirme `--help`, fonte ou documentação antes de registrar um comando como válido.
- O alvo é **Head Hunter para Sega Dreamcast** e as imagens válidas estão somente em `ISO/HeadHunter (Disc 1).cdi` e `ISO/HeadHunter (Disc 2).cdi`. Não invente pastas C64, `/mnt/data`, nomes de release ou outros caminhos.
- Nunca despeje um CDI/binário inteiro no terminal ou no contexto. Use parsers limitados por offset/tamanho, hashes em streaming, `read_bytes` com intervalos pequenos e artefatos locais ignorados para resultados extensos.
- Pesquisas no workspace devem excluir `dream-recomp/build*`, `.git`, binários e saídas geradas, salvo quando um arquivo específico for necessário. Prefira `rg --files` e `rg` com globs explícitos; não faça busca recursiva irrestrita em árvores de build.
- Para CDI Dreamcast, use primeiro o executável MIT já compilado: `C:\Source\recomp-BStinger\build\msvc-release\tools\extractor\bs-extract.exe --info "ISO\HeadHunter (Disc 1).cdi"` (e repita para o Disc 2). O fonte está em `C:\Source\recomp-BStinger\tools\extractor`. `dcdisc` suporta GDI/CHD, não CDI; não insista nele para CDI.
- No shell do OpenCode já se está dentro de PowerShell: execute cmdlets diretamente, sem `powershell -Command`, pois o shell externo expande `$variaveis` e quebra comandos aninhados. Para localizar texto/arquivos use `rg` e `rg --files`; não invoque uma ferramenta chamada `search`, que não está disponível. Para leitura binária limitada prefira `Get-Content -AsByteStream -TotalCount` ou Python com `seek()`/`read(tamanho)`; nunca `Path.read_bytes()` em CDI.
- Pare somente quando houver uma **DECISAO_NECESSARIA**: duas ou mais alternativas materialmente diferentes, não resolvidas pelos documentos, que afetem arquitetura pública, licença, fidelidade do produto, dependência permanente, perda de dados, escopo/versão suportada ou distribuição.
- Antes de pedir uma decisão, conclua toda investigação e todos os testes que não dependam dela. Apresente opções, evidências, recomendação, custos, riscos e a pergunta exata. Marque a etapa `BLOQUEADA_DECISAO`.
- Falha técnica, trabalho demorado, teste quebrado ou incerteza investigável não são decisões humanas: diagnostique, corrija e prossiga.
- Não encerre um turno apenas narrando a próxima ação. Execute a ação com as ferramentas disponíveis. Se o host encerrar o turno, o executor autônomo enviará uma continuação; retome imediatamente do estado em disco.
- Se o mantenedor solicitar revisão intermediária, forneça-a sem transformar isso em novo portão permanente.
- Não faça commits, tags, pushes nem abra PRs sem pedido explícito do mantenedor.

## Regras permanentes

- **Recompilar, nunca emular** (mantenedor, 2026-10-25): o código do jogo só executa como C++
  gerado do SH-4. É proibido introduzir interpretador ou emulador de CPU (SH-4 ou ARM7) no
  executável do jogo, inclusive no runtime híbrido, e proibido resolver uma divergência
  emulando hardware a mais quando o caminho do roadmap é substituir por sistema nativo (ex.:
  o driver `AUDIO64.DRV` do ARM7 é reconstruído no HH13, não executado). O executor de passo
  único do HH03 é só oráculo de testes (`HH_TOOL_ONLY`). A simulação temporária de
  registradores em `runtime/` existe apenas na fase híbrida, não ganha escopo novo e é
  removida no HH15; o Flycast é só processo externo de comparação.

- Nunca versione imagens de disco, BIOS/flash, SDK Sega, executáveis ou assets extraídos, saves, dumps de RAM/VRAM, capturas com conteúdo do jogo, código gerado do binário ou relatórios que incorporem bytes proprietários.
- Antes e depois de cada etapa, execute `git status --short` e `git check-ignore` sobre amostras dos caminhos privados.
- Não enfraqueça, remova ou pule testes para obter aprovação.
- Não invente símbolos, contratos ou comportamento. Toda afirmação de engenharia reversa precisa de evidência reproduzível.
- Identificação de função exige duas evidências independentes entre strings/tabelas, assinatura mascarada, grafo de chamadas, ABI/argumentos, desassembly/pseudocódigo e teste de contrato.
- Revisão independente (regra do Blue Stinger): ao concluir um sub-marco relevante (analisador, gerador, subsistemas do runtime, cada provedor nativo), rode o Codex CLI em modo somente leitura (`codex exec --sandbox read-only --ephemeral`) sobre o diff, salve o resultado em `reviews/` (ignorado), aceite só achados comprovados por bytes/disassembly/manual/teste e registre aceitos e rejeitados no `STATUS.md`. Se o Codex não estiver disponível ou autenticado, registre e siga.
- Use REA/Ghidra e `dc-re-ghidra` para análise estática; use o Flycast apenas como processo-oráculo externo. Registre comandos, hashes, endereços e resultados, nunca conteúdo proprietário.
- Uma mudança de comportamento precisa de teste negativo que falhe antes da correção, quando tecnicamente possível.
- C++20 e CMake, com headers (como no Blue Stinger); módulos C++20 nomeados só quando um ADR justificar. APIs de plataforma ficam atrás de interfaces; gameplay e contratos compartilhados não recebem dependência Win32 nova.
- Preserve a Preview jogável durante a migração. Cada subsistema começa com provedores `legado` e `nativo`; o nativo só vira padrão após equivalência comprovada.
- Atualize `STATUS.md` em cada etapa com data, comando, resultado e limitações. Documentos descrevem fatos verificados, não intenção como se já estivesse pronta.
- Preserve alterações preexistentes do usuário. Não use `git reset --hard`, `git clean`, `checkout --` ou remoções recursivas.

## Estrutura-alvo

- `app/`: host e executável final.
- `builder/`: Recompiler, Developer e pipeline de instalação.
- `config/`: metadados, hashes, endereços e fronteiras por versão/disco.
- `docs/`: planos, ADRs e relatórios aprovados.
- `legacy/`: compatibilidade temporária, ausente do produto final.
- `native/`: sistemas definitivos de plataforma, render, áudio, vídeo, SDK e gameplay.
- `runtime/`: runtime recompilado/híbrido que deve encolher até o HH14.
- `tests/`: unitários, integração, diferenciais e campanha.
- `cmake/`: módulos CMake (guarda do produto).
- `tools/`: extração, análise, geração, oráculos e distribuição.
- `transitional/`: adaptadores guest/nativo temporários.
- `translator/`: decodificador, análise e geração AOT.
- `analysis/`, `game-data/`, `generated/`, `reviews/`, `snapshots/`, `build/` e `dist/`: saídas locais ignoradas.

## Saída obrigatória de cada etapa

Crie `docs/relatorios/HHxx-RELATORIO.md` contendo:

1. objetivo e escopo realizado;
2. arquivos alterados;
3. evidências e decisões;
4. comandos de build/teste e resultados completos;
5. comparação com referência/oráculo quando aplicável;
6. riscos, limitações e trabalho deliberadamente deixado para depois;
7. inventário de dados privados e confirmação de que não foram versionados;
8. checklist dos critérios de saída da etapa;
9. próximos passos executados automaticamente ou, somente se aplicável, a decisão humana pendente.

Depois atualize `STATUS.md` para `CONCLUIDA` e continue a próxima etapa. Só interrompa com este formato quando houver decisão real:

```text
DECISAO_NECESSARIA — HHxx
Pergunta: <uma pergunta objetiva>
Recomendação: <opção recomendada e motivo>
Opções: <alternativas e impactos comprovados>
Trabalho já concluído: <investigações e testes executados antes da pausa>
```

## Validação por nível

- **V0 — documentação/estrutura:** links, esquemas, licenças, arquivos ignorados e configuração.
- **V1 — desenvolvimento:** build incremental, unitários e testes diretamente afetados.
- **V2 — fechamento de subsistema:** V1 + integração dirigida + comparação com referência/oráculo + casos negativos.
- **V3 — marco crítico:** regressão integral retomável + matriz de campanha aplicável + inspeção do produto/distribuição.

O `ROADMAP.md` determina o nível mínimo de cada etapa. Um portão não atendido mantém a etapa `EM_ANDAMENTO` ou `BLOQUEADA`; nunca a conclui por aproximação.

