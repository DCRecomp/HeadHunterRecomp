# HH05 — RELATÓRIO: Runtime híbrido, boot e primeiro frame

Data: 2026-10-25 · Nível de validação: V2 · Estado: **CONCLUIDA**

## 1. Objetivo e escopo realizado

Fazer o C++ gerado no HH04 executar: boot nativo a partir de `data/` (sem BIOS e
sem CDI), hardware temporário suficiente, oráculo Flycast externo e comparação
de trace e framebuffer até o primeiro frame natural.

- Dados de execução: `hh-extract --runtime/--verify-data` (porte do
  `runtime_data` MIT sobre o `hh_extract_core`).
- Runtime híbrido portado do Blue Stinger (MIT): memória, Holly/PVR/TA,
  GD-ROM e BIOS por HLE, Maple, registradores AICA, threads do guest,
  boot nativo e diagnósticos (`hh_runtime`).
- Hosts de desenvolvimento: `hh-run` (headless) e `hh-diff` (diferencial
  contra snapshots do Flycast); código gerado compilado uma vez
  (`hh_generated`).
- Oráculo: `tools/dev/flycast_oracle.py` (porte MIT) contra o Flycast externo
  compilado com servidor GDB (`C:\Source\flycast-oracle`).
- Funções e bibliotecas descobertas em execução (método R2) declaradas no
  config com evidência.

## 2. Arquivos alterados

| Arquivo | Mudança |
|---|---|
| `tools/hh-extract/{include/hh_extract/runtime_data.hpp,src/runtime_data.cpp,src/main.cpp,CMakeLists.txt}` | `--runtime`, `--verify-data` (porte adaptado); formato `hh-runtime-data` |
| `runtime/{memory,pvr,gdrom,hle,maple,input,aica,boot,debug}/*`, `runtime/cpu/{sh4_intc,sh4_threads,time_legacy}.*`, `runtime/cpu/time_hooks.h` | Portados |
| `runtime/cpu/sh4_service.cpp` | Pontos de contato reais (poll, esperas, sleep), separados do stub do HH04 |
| `runtime/gdrom/gdrom.cpp` | `GETSCD` (0x22) |
| `runtime/hle/bios.{h,cpp}` | `MISC` 1/3 e bootstrap do IP.BIN como pedido de saída (`bios_set_exit_hook`) |
| `runtime/boot/native_boot.cpp`, `runtime/gdrom/data_source.cpp`, `runtime/debug/file_log.cpp` | Adaptados à biblioteca de disco do HH |
| `tools/dcmap/*`, `tools/CMakeLists.txt` | Mapa de regiões (porte) |
| `app/hh_run.cpp`, `app/CMakeLists.txt` | `hh-run`, `hh_generated`, `hh-diff` |
| `tools/hh-diff/main.cpp` | Porte do `bs-diff`, com `HH_DATA_DIR`/`HH_NATIVE_BOOT` = pasta de dados |
| `tools/dev/{flycast_oracle.py,hh05_misses.py,hh05_trace_classify.py,hh05_fb.py}` | Oráculo (porte) e ferramentas novas |
| `translator/codegen/codegen.cpp` | Correção do rótulo `hh_reenter_` (bug do meu porte) |
| `config/headhunter.toml` | `[[functions]]` 0x8C16AA00; `[threads]` setjmp/longjmp |
| `tests/unit/test_longjmp.cpp`, `tests/hh05_bios_exit_tests.cpp`, `tests/CMakeLists.txt` | Testes |
| `tests/diff/{hh_boot_regression.py,hh_first_frame.py,hh_boot_accepted.tsv}` | Portões do HH05 contra o oráculo (dados privados; SKIP sem eles) |
| `tools/dev/hh05_boot_check.py` | Regras do portão do trace |

## 3. Evidências e decisões

### 3.1 Dados de execução sem CDI (HH05.1)
`hh-extract --runtime` gera, por disco, `disc/` (arquivos do ISO), `disc_meta.*` (setores de
sistema) e `disc.manifest`; `verify_runtime_data` reconstrói **todos** os setores das faixas
de dados e compara com o CDI. Disco 1: 212 arquivos, 711.899.911 bytes, 42 setores de sistema;
disco 2: 160 arquivos, 712.129.640 bytes, 39. Negativo: 1 byte alterado em `DISC/DISK1.ID` →
"setor 230772 diferente da imagem", rc=1.

### 3.2 Execução do jogo recompilado (HH05.2–HH05.4, HH05.7)
`hh-run --data game-data\runtime\disc1 --vblanks 10800 --rtc 1700000000`: 3 minutos de jogo sem
parada; logos SEGA, CRI ADX, 4X Movie e NOISEBOX e tela-título "PRESS START BUTTON".
Descobertas em execução (método R2) declaradas no config com evidência: callback 0x8C16AA00
(`jsr @r2` em 0x8C17C208) e a biblioteca de threads cooperativa (`[threads]` setjmp
0x8C131F3C / longjmp 0x8C131E9C: leiaute simétrico do buffer; threads novas criadas pela
tabela {pilha, entrada} em 0x8C1710D4). Nenhum miss de despacho depois disso.
- GD-ROM: comando 0x22 `GETSCD` (formato 1, 14 bytes), pedido periodicamente pelo driver de GD
  do jogo (0x8C16E2E8). Bytes capturados no oráculo logo após o 1º pedido (`mem 0x8C16E312
  0x8C289B40 16 --depois 0x8C16E302`): `00 15 00 0E 41 02 01 00 36 1E 00 03 BB DB` = estado de
  áudio 0x15, tamanho 14, controle 4/ADR 1, faixa 2, índice 1, FAD relativo 0x361E e absoluto
  0x3BBDB — a posição da cabeça, o FAD logo depois da última leitura (0x3BBA9 + 0x32).
  Implementado esse contrato; formato, tamanho ou posição não observados são fatais; teste
  `hh05_getscd_matches_oracle_contract` reproduz os 14 bytes. (A 1ª versão zerava o canal Q;
  apontado na revisão Codex.)
- Saída do jogo (HH05.4): `MISC` r4 = 1/3 e salto ao bootstrap do IP.BIN (0x8C008300) viram
  pedido de encerramento pelo gancho do host; sem gancho ou com outro código, fatal
  (`hh05_game_exit_requests_end_the_program`).
- Bug do meu porte corrigido: rótulo `bs_reenter_` dentro de string no codegen não foi
  renomeado; com `[threads]` o C++ gerado não compilava
  (`codegen_setjmp_return_is_reentry_label` cobre).

### 3.3 Trace do boot contra o Flycast (HH05.5)
Oráculo: `flycast_oracle.py trace 0x8C010000 <entradas> --max-calls 3000` (breakpoint em cada
entrada de função; Flycast externo com servidor GDB). Recompilado: `hh-diff trace` a partir do
estado de entrada capturado no Flycast (`snap --pvr --vram`).
- Primeira tentativa (todas as funções): após a chamada #62 as 3.000 chamadas ficam num laço
  de espera do vblank (helper 0x8C1793EA lê `SPG_STATUS`); a cópia em bloco 0x8C159820
  domina o trecho seguinte (363 mil em 400 mil chamadas). Os dois helpers folha foram
  excluídos dos dois lados (`HH_TRACE_SKIP`, novo no `hh-diff`; lista de entradas filtrada
  no oráculo).
- Resultado: **sequência de funções idêntica nas primeiras 558 chamadas** (boot, leituras do
  GD-ROM e a primeira interrupção no mesmo índice). Na #558 o Flycast recebe outra
  interrupção (VBR+0x600) e o recompilado sai do tratador por `jmp @VBR` (VBR+0): momento de
  entrega de interrupção, aproximado no runtime híbrido (poll em back-edges, risco R1 do
  Blue Stinger).
- Diferenças de registrador antes da #558, todas classificadas:

| Classe | Evidência |
|---|---|
| PR em P1 × P2 (2 chamadas) | mesmo endereço físico; o jogo começa em 0xAC010000 |
| Leitura de tempo em 0x8C1793E0 (r0, r2, r3, r10) | contador TMU; ciclos estimados por bloco |
| Relógio (RTC) da sessão do oráculo (`0xE7E4` × `0xE1E6`) | o valor recompilado é exatamente o RTC de `entry.bss` (0x907BE1E6); com o RTC da sessão do trace todas somem |
| r2 em 0x8C168940 (#239) | registrador morto: sobrescrito em 0x8C16894E antes de ler (lista aceita) |

- Portão `hh_boot_regression` (`tools/dev/hh05_boot_check.py`, endurecido após a revisão
  Codex): 21 colunas hexadecimais em toda linha; trace recompilado com exatamente 3.000
  linhas e `hh-diff` terminando pelo limite (rc 0); sequência idêntica até a **única**
  divergência declarada (`divergencia 558 8C00FA00 8C00F400` em `hh_boot_accepted.tsv`),
  que precisa ocorrer exatamente; só as classes acima; toda entrada aceita precisa ocorrer.
  Negativos automatizados em `hh_gate_comparators` (dados sintéticos, sempre no CTest):
  registrador e função injetados, trace truncado, Flycast curto, coluna faltando, aceita que
  não ocorre, divergência diferente da declarada, PR em outro endereço físico → todos
  rejeitados. Com os dados reais, RTC desalinhado → rejeitado.

### 3.4 Primeiro frame contra o Flycast (HH05.6)
Captura livre do Flycast (`snap-free --seconds 11`, `rend.EmulateFramebuffer = yes`)
decodificada pela mesma regra do `pvr_video` (`tools/dev/hh05_fb.py`), contra o quadro 200 do
`hh-run` (tela estática do logo SEGA, VBlanks 180–240):
- 640×476 nos dois (`FB_R_SIZE` = 0x1413B53F, 238 linhas por campo, entrelaçado): o tamanho é
  configuração do jogo, não erro do runtime.
- 288.006 pixels iguais (94,54%); 16.634 diferentes, **todos por exatamente um passo de 5
  bits** (8: 14.104; 9: 2.530), com distribuição desigual por paridade de pixel.
  `FB_W_CTRL` = 0x08: dither ligado, não modelado pelo rasterizador de referência (mesma
  constatação do Blue Stinger; o modelo do Flycast é GPL e não foi copiado).
- Portão `hh_first_frame` (regras restritas após a revisão Codex): `hh-run` precisa terminar
  pelo limite (rc 0); PPM validado (tamanho exato); ≥ 94% de pixels idênticos e os demais só
  com diferença máxima 8 ou 9 (um passo de 5 bits). Caso negativo embutido: o quadro 400
  (logo CRI ADX) tem 43.457 pixels fora dos deltas aceitos → rejeitado. Casos sintéticos
  (`hh_gate_comparators`): 5% com 1 passo aceito; 7% rejeitado; delta 5 e 255 rejeitados;
  dimensões diferentes e PPM truncado rejeitados.

### 3.5 Divergência conhecida: vídeos 4X Movie abandonados
No Flycast o jogo toca o logo animado AMUZE e o vídeo de abertura (noticiário com legendas);
no recompilado as telas desenhadas pelo jogo aparecem, mas os dois vídeos são abandonados:
`SCENE0.4XM` só 135 de ~1.300 setores lidos (VBlank 282) e `SCENE01_06.4XM` só 134 setores
(VBlanks 767–795); o jogo segue para a tela seguinte.

Investigação (depois da revisão Codex, que pediu eliminar alternativas):
- **Alternativas eliminadas:** perfil de tempo do GD-ROM `reios` (`HH_GDROM_TIMING=reios`) e o
  `GETSCD` corrigido: os vídeos continuam abandonados com os mesmos 135/134 setores. O
  controle reporta estado neutro (não é "pular vídeo").
- **Mecanismo comprovado:** 0x8C122440 é a fila de comandos para o driver de som: 32 slots
  (índice & 31) na RAM da AICA (base 0xA080B200, lida no snapshot do oráculo); para cada
  comando, espera até 20 vezes o slot ser consumido (palavra zerada) e, se não, chama a rotina
  de erro 0x8C1222BC (código 0x12D) e falha. No recompilado (`hh-run --watch`), depois de 33
  chamadas, a partir do VBlank 56, **todo** comando de som falha. No Flycast, no 60º comando,
  os 32 slots estão zerados (`mem 0x8C122440 0xA080B200 128 --hit 60`): quem consome é o
  driver `AUDIO64.DRV` no ARM7, que o runtime híbrido não executa (decisão D10; regra
  "recompilar, nunca emular" do `AGENTS.md`).
- **Ainda inferência:** que a falha dos comandos de som é o que faz o player do 4X abandonar o
  vídeo (forte, coerente no tempo, mas a decisão de abandono não foi isolada).
- **Correção correta:** não é emular o ARM7. Dono: HH13 (reconstrução nativa do protocolo do
  driver e do áudio) e HH12 (vídeo 4X nativo).
- Correção da análise inicial: o laço 0x8C1224EC, que eu tinha associado ao driver, espera o
  FIFO do G2 (`0xA05F688C`), não a fila.

## 4. Comandos de build/teste e resultados

```powershell
cmake --preset vs -DHH_GENERATED_DIR=C:/Source/recomp-head-hunter/generated/disc1
cmake --build --preset vs-release -- -m      # sem erros nem avisos C4
ctest --preset vs-release                    # 14/14 (2 SKIP sem dados privados)
$env:HH_SNAPSHOT_DIR="...\snapshots\hh05"; $env:HH_DATA_DIR="...\game-data\runtime\disc1"
ctest --preset vs-release -R "boot|first"    # 2/2 Passed
```

| Teste | Resultado |
|---|---|
| `hh_smoke`, `hh_extract_tests`, `hh03_*`, `hh_product_guard_*`, `hh_translator_tests` (34), `hh_codegen_exec_*` | ok |
| `hh_generated_link` (agora com o runtime real) | ok |
| `hh_runtime_tests` (longjmp ×5, saída do jogo, contrato do `GETSCD`) | ok |
| `hh_gate_comparators` (16 casos sintéticos positivos/negativos dos dois portões) | ok |
| `hh_boot_regression` | ok com dados (558 chamadas); SKIP sem |
| `hh_first_frame` | ok com dados (94,54%, só deltas 8/9; negativo rejeitado); SKIP sem |

## 5. Comparação com referência/oráculo
Ver §3.3 e §3.4. Estado de entrada no oráculo: PC=0xAC010000, PR=0xAC00E0B2, R15=0x8C00F400
(confirma a análise estática do IP.BIN do HH04). Boot nativo × boot pelo snapshot: mesma
sequência de 3.000 funções.

## 6. Riscos, limitações e trabalho deixado para depois
- Vídeos 4X abandonados (§3.5): fila do driver de som não consumida sem o ARM7 → HH13/HH12.
- Entrega de interrupção e tempo aproximados (ciclos estimados) → HH09/HH15.
- `sleep` no runtime híbrido executa um `sh4_service` e retorna (comportamento do Blue
  Stinger); nenhuma emissão de `sleep` foi observada no código gerado executado.
- Integridade dos dados: o runtime confere o SHA-256 de cada arquivo na primeira abertura e
  dos setores de sistema na abertura (negativo: 1 byte alterado em `STRING_COLLECTION.CSV`,
  mesmo tamanho → "alterado ou corrompido"); `--runtime` recusa pasta de saída existente não
  vazia ou link.
- Dither do PVR não modelado (tolerância de 1 passo) → HH14.
- O oráculo encerrou a conexão na chamada 2.793 do trace filtrado (sem efeito no portão).

## 7. Inventário de dados privados
`game-data/runtime/`, `generated/`, `snapshots/hh05/`, `analysis/hh05/` (capturas de tela e
logs) e `reviews/` ignorados (`git check-ignore`). Versionados só código, ferramentas,
endereços e a lista de diferenças aceitas (metadados).

## 8. Checklist do critério de saída
- [x] Host headless, boot HLE, hardware temporário suficiente (porte MIT).
- [x] Dados de `data/`, nunca o CDI, conferidos setor a setor.
- [x] Flycast externo como oráculo diferencial.
- [x] Boot até o primeiro frame natural, sem forçar PC/estado.
- [x] Trace e framebuffer do marco concordam dentro de tolerâncias documentadas.
- [x] Casos negativos provam os dois comparadores.
- [x] Revisão independente — §9.

## 9. Revisão independente e próximos passos
Revisão: `codex exec --sandbox read-only --ephemeral`, saída em
`reviews/codex-hh05-2026-10-25.md` (ignorado). 8 achados; todos analisados:

| Achado | Decisão | O que foi feito |
|---|---|---|
| [ALTA] Portões podiam passar com execução truncada; `hh-run` dava "limite atingido" após saída do jogo | Aceito | `hh-run` rc 3 na saída do jogo; portões exigem rc 0, término pelo limite e artefatos completos |
| [ALTA] Comparador de trace aceitava TSV truncado/incompleto | Aceito | 21 colunas, 3.000 linhas, entradas aceitas obrigatórias; negativos sintéticos |
| [MÉDIA] Regra "divergência em vetor encerra" ampla demais | Aceito | Só a divergência declarada (#558, 8C00FA00 × 8C00F400). Reconvergência pós-interrupção não comparada (limitação; HH09) |
| [MÉDIA] Tolerância 9 por canal aceitava erro global | Aceito | ≥ 94% exatos, só deltas 8/9, PPM validado; negativos sintéticos |
| [MÉDIA] `GETSCD` com canal Q inventado | Aceito | Contrato capturado no oráculo e implementado (§3.2); teste dos 14 bytes |
| [MÉDIA] Dados alterados com mesmo tamanho aceitos em silêncio | Aceito | Hash por arquivo na primeira abertura + setores de sistema; negativo real |
| [MÉDIA] Causa ARM7 era só hipótese | Aceito | Alternativas eliminadas e mecanismo comprovado no oráculo (§3.5); ligação final continua inferência |
| [BAIXA] Proteção de caminhos só lexical | Aceito | Pasta de saída nova ou vazia, não link |

Observação: o revisor consultou o código-fonte GPL do Flycast para comparar o `GETSCD`. Esse
código não foi aberto nem copiado aqui; o contrato implementado vem dos bytes observados no
oráculo.

Próxima etapa: HH06 (primeira fatia jogável: menu, início do jogo e primeira área; roteiros
determinísticos de entrada; Preview local e Builder inicial).
