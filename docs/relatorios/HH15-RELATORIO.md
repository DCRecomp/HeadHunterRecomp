# HH15 — Relatório: fim do tempo guest e remoção do legado

Data: 2026-10-30. Validação mínima do ROADMAP: **V3**. Plano e evidências: `docs/planos/HH15-PLANO.md`. Referência
MIT: N2c e N8 do Blue Stinger (`C:\Source\recomp-BStinger`), cujo N8.8 está suspenso; o fechamento daqui não tem
precedente direto e foi desenhado na HH15.8.

## 1. Objetivo e escopo realizado

Critério de saída do ROADMAP: V3 passa antes e depois da remoção; o build do produto falha se qualquer biblioteca legada
for religada. Escopo executado: tudo o que não depende da decisão D-HH15-1 (seção 9), isto é, o fim do tempo guest e a
remoção de toda a simulação de hardware e dos provedores legados do `HeadHunter.exe`. O substrato AOT (código gerado,
RAM guest, `Sh4Context`, despacho e fibras) continua, como no BS (D-N8-1), até a decisão.

- **HH15.0** referências pré-corte (`tools/dev/hh15_referencias.py`, casos estável, experimental e com saves,
  reproduzidas 6/6) e inventário por sonda e mapa do linker.
- **HH15.1** tempo lógico sem ciclos (cadência B, `runtime/memory/sys_logical` para TMU, CCN não gráfico, porta e
  INTC), padrão da janela, portão `tests/diff/hh15_tempo.py`.
- **HH15.2** runtime por papel (manifesto `runtime_papeis.tsv`), validação só no `hh-run` (stubs no produto, recusa das
  variáveis de teste com saída 2, classificação fechada em `runtime/debug/validacao_vars.h`), catraca do mapa
  `hh15_produto_papeis`.
- **HH15.3** serviço nativo das syscalls (`runtime/system/syscall_service`); BIOS HLE, flash e drive fora do produto.
- **HH15.4** barramento do Maple sem dispositivos (`runtime/system/maple_bus`); modelo do Maple fora do produto.
- **HH15.5** produto só com filme, áudio e saves nativos; referência própria do produto (94 hashes).
- **HH15.6** área de som nativa (`runtime/system/sound_bus`, só o RTC); modelo da AICA fora do produto.
- **HH15.7** render: software sobre a IR (`native/render/soft`), ListSink, estado do quadro, memória das texturas e
  imagem exibida nativos (`native/render/kamui`), DMA do canal 2 e system block (`dma_bus`, `sb_bus`), banco de status
  de interrupção (`event_bus`), SPG (`spg_bus`), pedido de render e VBlank (`render_bus`, `video_bus`), desenho do
  produto separado da captura (`transitional/render_devices`), modelos gráficos e captura fora do produto
  (`pvr_ausente`, `render_capture_ausente`).
- **HH15.8** fechamento: prova de imports e strings (`tests/hh15/check_fechamento.py`), sonda pós-corte, regressão V3,
  Builder e Preview, revisões Codex, decisão D-HH15-1.

## 2. Arquivos alterados

125 arquivos entre `0f88ba7~1` e `c559859` (+9 516/−1 523), mais os da HH15.8. Novos, por área:
- `runtime/system/`: `syscall_service`, `maple_bus`, `sound_bus`, `dma_bus`, `sb_bus`, `event_bus`, `spg_bus`,
  `render_bus`, `video_bus` (serviços nativos) e os substitutos do produto `hle_ausente`, `maple_ausente`,
  `aica_ausente`, `sb_ausente`, `dmac_ausente`, `pvr_ausente`;
- `runtime/cpu/irq_bridge.cpp`, `runtime/memory/sys_logical`, `runtime/debug/validacao*`;
- `native/render/soft/` (`soft_raster`, `soft_device`) e `native/render/kamui/` (`list_sink`, `frame_state`,
  `texture_heap`, `texture_source`, `display_service`, `event_status`, `spg_timing`);
- `transitional/render_devices`, `transitional/render_capture_ausente.cpp`;
- testes: `tests/hh15_*_tests.cpp`, `tests/unit/test_hh15_render_devices.cpp`, `tests/hh15/check_papeis.cmake`,
  `check_recusa*.cmake`, `check_variaveis.py`, `check_fechamento.py`, `tests/diff/hh15_tempo*.py`,
  `hh15_excecoes_test.py`; `config/hh15_excecoes_{spc,quadros}.toml`; `tools/dev/hh15_referencias.py`.

## 3. Evidências e decisões

- **Mapa do linker** do `HeadHunter.exe` (catraca `hh15_produto_papeis`, 22 negativos por mutação): do `hh_runtime` só
  os papéis `aot`, `nativo`, `simulacao` (store queue, que é semântica da CPU, roteiros de entrada e o boot nativo),
  `estavel` e `diagnostico`; nenhum objeto de `validacao`, `hle`, `perifericos`, `aica`, `sb`, `intc` e `grafico`
  (`pvr_*`, `ta`, `vram`, `holly_intc`, `holly_sb`, `sh4_dmac`, `maple`, `aica_regs`, `gdrom`, `bios`, `flashrom`,
  `hw_probe`) nem `render_capture`.
- **Imports** (HH15.8): só o CRT do MSVC, Win32, Media Foundation, Direct2D/DirectWrite, `winmm` e `xinput1_4`, nas
  tabelas de imports e de delay-load; Vulkan e XAudio2 carregados em tempo de execução.
- **Strings** (HH15.8): 517 literais exclusivos das fontes fora do produto (por grupo: validação 5, HLE 13, periféricos
  10, AICA 14, SB 3, INTC 4, gráfico 17, captura 10, tradutor e interpretador 441), nenhum no executável. Controle: no
  `hh-run`, que liga os modelos, a mesma prova acusa os oito grupos do runtime.
- **Sonda pós-corte** (`analysis/hh15/sonda_fech_{legado,experimental}.tsv`, `hh-run` com os provedores nativos do
  produto, rota `newgame_area1_nativo`, 4 700 VBlanks): as mesmas regiões das sondas do HH15.0 nos dois tempos; o guest
  só toca o contrato medido, agora atendido por serviços nativos: no PVR só o SPG (`0x005F80DC`, `0x005F810C`), no DMAC só
  o canal 2 (`FFA00020/28/2C/40`), no SB o DMA do canal 2, `SB_FFST` e a inicialização; nenhum acesso à VRAM ou à área 4
  (as store queues só vão para a RAM). No produto, qualquer acesso fora desse contrato é erro explícito dos substitutos,
  e a rota do produto chega aos 94 hashes da referência.
- **Recompilar, nunca emular**: nenhum interpretador no produto (`HH_TOOL_ONLY` em `hh_translator`, `sh4_core`,
  `hh_recomp_lib`, `hh_hwscan` e `hh_runtime_hw_m3`, guarda `cmake/HHProductGuard.cmake` com negativo
  `cmake_guard`; strings do tradutor ausentes).
- **Precedentes do BS** adotados (seção 2 do plano): cadência B, papéis e catraca, serviço nativo na fronteira das
  syscalls, barramento sem dispositivos, área de som nativa, D-K2/D-K3/D-K4 no render.
- **HH15.2–HH15.4**, abertas desde a implementação por aguardarem o fechamento V3, concluídas com os portões: build do
  produto falha ao religar biblioteca legada (guarda do CMake e catraca, com negativos); zero setores e syscalls fora do
  contrato (`[sistema] nativo: ... recusadas 0` no passo `builder`) e bytes lidos iguais (referências 18/18); roteiros e
  saves iguais e zero Maple no produto (o modelo fora do mapa; o guest fala com o barramento nativo sem dispositivos,
  precedente N8.4 do BS).

## 4. Comandos e resultados

- Build: `cmake --preset vs && cmake --build --preset vs-release` — sem erros nem avisos.
- Testes offline: `ctest --preset vs-release --no-tests=error` — 92/92 (73 no início da HH15.7d4).
- Regressão integral: `python -I tools/dev/regressao.py --novo`, 14 passos (build, ctest, portões HH05, HH07–HH15,
  roteiro da área 1, builder):
  - pré-corte: `analysis/hh14/regressao.log` (13/13, fechamento do HH14);
  - em cada subetapa da HH15: `analysis/hh15/regressao*.log`, todas 14/14;
  - pós-corte (HH15.8): `analysis/hh15/regressao_hh15_8.log` — 14/14.
- Rota da área 1 (`hh-run`, 94 hashes iguais ao HH06) e produto (`HeadHunter.exe --headless`, 94 hashes iguais a
  `snapshots/hh15/produto_hashes.txt`).
- Fechamento: `python -I tests/hh15/check_fechamento.py build/app/Release/HeadHunter.exe . build/runtime_papeis.tsv`
  — ok; `HH15_FECHAMENTO_MUTAR=autoteste` — ok (o autoteste reprova as mutações sem delay-load, cauda virtual, sem
  comentários de bloco e sem raw string).

## 5. Comparação com a referência/oráculo

As referências da HH15.0 (capturadas antes do corte) seguem iguais no passo `portoes_hh15` (estável idêntico,
experimental fora da lista fechada de exceções provadas). R1–R5 e K1–K4 do HH13 (`portoes_hh13`) e A1–A5 do HH14
(`portoes_hh14`) com o caminho nativo. O Flycast segue como oráculo externo dos portões do HH13.

## 6. Riscos, limitações e trabalho deixado para depois

- Substrato AOT no produto (código gerado, RAM guest, `Sh4Context`, despacho, fibras e o boot nativo): depende da
  D-HH15-1.
- Os serviços nativos ficam na fronteira MMIO do guest (o código gerado ainda acessa os endereços do Holly, do DMAC, do
  SB, do Maple e do SPG); somem com a elevação do Kamui e do gameplay (HH17).
- Limitação registrada: no tempo lógico a tela de carga antes da cena de abertura não aparece (a carga não consome
  tempo).
- O tempo estável continua nos dois executáveis por compatibilidade (precedente N8.2 do BS).

## 7. Dados privados

Saídas em `analysis/hh15/` (logs, sondas, referências), `reviews/` (prompts e respostas do Codex), `snapshots/hh15/`
(hashes do produto), `game-data/`, `build/` e `dist/`, todos ignorados (`git check-ignore`). Nenhuma imagem de disco,
BIOS, asset, save, dump ou código gerado foi versionado; o verificador de strings só lê fontes MIT do projeto e o
executável.

## 8. Checklist de saída

- [x] V3 antes da remoção (`analysis/hh14/regressao.log`) e depois (`analysis/hh15/regressao_hh15_8.log`).
- [x] Build do produto falha se biblioteca legada for religada (guarda do CMake; catraca do mapa com 22 negativos).
- [x] Tempo guest: nenhum ciclo, IRQ ou polling guest no tempo lógico (HH15.1).
- [x] Módulos de hardware fora do produto (HH15.2–HH15.7) e provas de ausência operacional: mapa, imports, strings e
      sonda (HH15.8).
- [x] Ferramentas de análise separadas do produto (`HH_TOOL_ONLY`, `hh-run`).
- [ ] Remover contexto SH-4, RAM guest e despacho: pendente da D-HH15-1.

## 9. Próximos passos

Pendente a decisão D-HH15-1 (a etapa fica `BLOQUEADA_DECISAO`):

```text
DECISAO_NECESSARIA — HH15
Pergunta: o substrato AOT (código gerado, RAM guest, Sh4Context, despacho e fibras) fica no produto até a HH17,
como no Blue Stinger (D-N8-1), ou a HH15 deve removê-lo agora, como o texto do ROADMAP pede?
Recomendação: manter até a HH17 (opção A), corrigindo o ROADMAP e o ARCHITECTURE para dizer que a HH15 remove a
simulação de hardware e os provedores legados, e a HH17 remove o substrato ao elevar o gameplay.
Opções:
  A) manter até a HH17: a HH15 conclui com o que já está feito e provado; a HH16 (campanha) roda sobre o código
     gerado, como planejado; mesmo caminho do BS.
  B) remover agora: exige elevar o jogo inteiro para native/game antes da HH16, ou seja, antecipar a HH17 inteira
     (o maior trabalho do roadmap) e refazer a ordem HH16/HH19/HH17.
Trabalho já concluído: HH15.0–HH15.8 (seções 1–8), regressão V3 pós-corte, provas de mapa, imports, strings e sonda.
```
