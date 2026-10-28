# Comparação com o Blue Stinger Recompiled — 2026-10-24

Fonte: `C:\Source\recomp-BStinger` (MIT, somente leitura): `ARCHITECTURE.md`,
`CLAUDE.md`, `docs/PLANO-PORT-NATIVO.md`, `STATUS.md`, `CMakeLists.txt`,
`CMakePresets.json` e árvore de diretórios. Objetivo: confirmar que o plano do
Head Hunter segue a mesma estrutura, as mesmas regras e o mesmo produto, que é
**código C++ nativo gerado a partir do SH-4 e depois elevado a sistemas nativos**.

## 1. Conclusão

O plano HH00–HH18 tem a mesma espinha do Blue Stinger: extrator → decodificador
→ análise/codegen (uma função C++ por função SH-4) → runtime híbrido → Preview e
Builder → port nativo em três camadas (interface tipada, adaptador transitório,
provedor `legado|nativo`) → remoção do legado → mods/projeto editável. A
estrutura de diretórios é a mesma.

As diferenças eram de detalhe e foram incorporadas (§4): decisões técnicas da
tradução não registradas, reuso explícito do runtime MIT, regra de revisão
independente, presets CMake e lições de ordem/tempo do port nativo.

## 2. Mapeamento de etapas

| Head Hunter | Blue Stinger | Observação |
|---|---|---|
| HH00 | M0 | Esqueleto, LEGAL, ARCHITECTURE |
| HH01, HH02 | M1 | Forense + extrator; HH separa forense e extração |
| HH03 | M2 | Decodificador + semântica exaustivos |
| HH04 | M3 | Descoberta de funções + C++ que compila |
| HH05 | M4 | Boot até o primeiro frame, oráculo Flycast, trace diferencial |
| HH06 | Preview + Builder (D7) + "NEW GAME até a 1ª área" | O BS **não** completou M5–M8 em LLE: passou ao port nativo com a Preview jogável. O HH segue o mesmo caminho |
| HH07 | N0 | Fundação, alvos `native/legacy/transitional`, métricas |
| HH08 | N1 | Arquivos (o BS tirou o CDI do executável já no M4; o HH adota isso desde o HH05) |
| HH09 | N2a + N2b | Relógio lógico, fila de eventos, tarefas |
| HH10 | N3 | Controles |
| HH11 | N4 | Saves |
| HH12 | N5 | Vídeo |
| HH13 | N7a + N7b | IR de render + Vulkan (antes do áudio: decisão do mantenedor 2026-10-25) |
| HH14 | N6 | Áudio |
| HH15 | N2c + N8 | Fim do tempo guest e remoção do legado |
| HH16 | (dentro do N8/N10) | Campanha completa; o HH a torna etapa explícita |
| HH17 | N10 | Gameplay elevado, mods |
| HH18 | N10 / revisão final | Recompiler/Developer, pacote |
| HH19 | N9 | Remasterização (incluída pelo mantenedor em 2026-10-28; número livre seguinte, executada entre o HH16 e o HH17) |

## 3. Regras equivalentes

| Regra BS (`CLAUDE.md`) | HH | Estado |
|---|---|---|
| Nunca versionar imagem, BIOS, `game-data/`, `generated/`, `analysis/`, `reviews/` | `LEGAL.md`, `.gitignore` | igual |
| MIT; GPL só como referência/oráculo externo | `LEGAL.md`, ADR-002 | igual |
| `STATUS.md` com data e método de cada verificação | `AGENTS.md` | igual |
| Commits só a pedido | `AGENTS.md` | igual |
| Decisões grandes de arquitetura: opções + escolha do mantenedor | `DECISAO_NECESSARIA` | igual (HH é mais autônomo no resto) |
| Validação níveis 1/2/3, nunca enfraquecer testes | V0–V3 | igual |
| Identificação de função SDK com duas evidências | `AGENTS.md` | igual |
| Revisão independente com Codex (somente leitura) em sub-marcos | — | **incorporada** em `AGENTS.md` |
| C++20 para tudo o que vai ao usuário; MSVC sempre compila, clang-cl alvo principal | A1 | **alinhada** (presets) |

## 4. Diferenças encontradas e o que foi feito

| # | Diferença | Ação |
|---|---|---|
| C1 | `ARCHITECTURE.md` do HH não registrava as decisões técnicas da tradução (delay slots, pools, jump tables, FPU PR/SZ/FR, arredondamento RM=01 e proibição de libm na thread da CPU, FSCA por tabela, poll de interrupções em back-edges, `rte` entre threads, setjmp/longjmp, código relocado, despacho com miss registrado, overrides) | Nova seção "Tradução e runtime híbrido" com essas decisões como padrão herdado |
| C2 | Reuso do runtime MIT do BS (`runtime/cpu`, `memory`, `hle`, `gdrom`, `pvr`, `maple`, `boot`, `debug`, ~8.700 linhas, genérico de Dreamcast) não estava explícito para o HH05 | `ROADMAP.md` (tabela de reuso) e HH05 ampliados |
| C3 | BS tem `CMakePresets.json` (Ninja + MSVC/clang-cl); HH usava só o gerador VS | `CMakePresets.json` criado |
| C4 | toml++ (MIT) é a dependência de configuração do tradutor no BS | Previsto no HH04 e no `third_party/README.md` quando entrar |
| C5 | Lições do port do BS: o estado do driver de som é entrada da lógica do jogo (áudio nativo muda o comportamento observado); vídeo/áudio só podem usar a interface de tempo (`TimeProvider`), nunca ciclos; manter render em software até o IR custou desempenho (~17 quadros/s) | Registradas no `ROADMAP.md` (HH09, HH12–HH14) |
| C6 | BS adotou `data/` em vez do CDI no executável já no M4 | HH05 passa a exigir isso desde o primeiro boot |
| C7 | Decisões de backend do BS: Vulkan (D9), XAudio2 sem ARM7 (D10), Media Foundation para vídeo convertido (D11), SDL3 3.2.x estático | `ARCHITECTURE.md`: adotadas como padrão, revisáveis por evidência de formato (o HH usa 4XM, não Sofdec, e `AUDIO64.DRV`, não Manatee) |

## 5. O que muda por ser outro jogo

- Um executável para dois discos; troca de disco é requisito desde o HH01.
- Middleware diferente: vídeo 4X Movie (`.4XM`) em vez de Sofdec; driver
  `AUDIO64.DRV` em vez de Manatee; formatos de dados `.AHF`/`.FPB`/`.FOB`
  próprios. A investigação dos formatos está em andamento (HH01 complementar)
  e o resultado vai para `analysis/formatos/`.
- Não se sabe ainda se o HH usa Ninja/Kamui como o BS: será medido no HH04 por
  assinaturas (`dc-re-ghidra/sdk-symbols.txt`) + grafo de chamadas.
