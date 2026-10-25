# HH05 — Plano detalhado: runtime híbrido, boot e primeiro frame

Validação mínima: **V2**. Critério de saída (ROADMAP): trace e framebuffer do
marco concordam com o oráculo (Flycast externo) dentro de tolerâncias
documentadas, e casos negativos provam o comparador. Boot até o primeiro frame
natural, sem forçar PC/estado, lendo `data/` (nunca o CDI).

## Estratégia

Equivalente ao M4 do Blue Stinger. O runtime MIT dele é genérico de
Dreamcast/Katana (CPU, memória, Holly/PVR/TA, GD-ROM HLE, BIOS HLE, Maple,
registradores AICA) e é portado com atribuição, como no HH04. Partes
específicas do Blue Stinger ficam de fora: `debug/cri_diag.*` (middleware CRI
do BS) e os endereços de laço de espera em `memory.cpp`.

| Componente BS | Uso no HH05 |
|---|---|
| `tools/extractor/runtime_data.*` | Porte sobre o `hh_extract_core`: `hh-extract --runtime` e `--verify-data` (pendência do HH02) |
| `runtime/{cpu,memory,pvr,gdrom,hle,maple,aica,input,boot,debug}` | Porte; `boot/native_boot` adaptado ao IP.BIN/versões do HH |
| `tools/dcmap` | Porte (mapa de regiões do Dreamcast, usado pela sonda de hardware) |
| `app/game_host.*` | Porte do núcleo sem janela (modo headless primeiro) |
| `tools/dev/flycast_oracle.py`, `tests/diff`, `tools/diff` | Oráculo externo e trace diferencial |

## Subetapas

| ID | Entrega | Evidência de saída |
|---|---|---|
| HH05.1 ✔ | `runtime_data` + `hh-extract --runtime <cdi> <dir>` e `--verify-data` | Todos os setores de dados dos dois discos reconstruídos de `data/` idênticos ao CDI |
| HH05.2 ✔ | Runtime portado compila (`hh_runtime`), unitários portados passam | ctest |
| HH05.3 ✔ | Boot nativo a partir de `data/` (IP.BIN e `1ST_READ.BIN` com hash aceito), estado de entrada do BIOS, HLE das syscalls | Boot headless executa código gerado; toda parada tem causa registrada |
| HH05.4 ✔ | Rotina de saída do jogo (0x8C15E588 → 0x8C008300/`[0x8C0000E0]`) como encerramento nativo | Teste dirigido |
| HH05.5 ✔ | Oráculo Flycast externo: estado na entrada (0x8C010000) e trace de chamadas comparável | Primeiras N chamadas iguais; caso negativo |
| HH05.6 ✔ | Primeiro frame natural (VBlank, FB_R_*) em headless; captura do framebuffer | Comparação com o framebuffer do Flycast dentro da tolerância |
| HH05.7 ✔ | Funções descobertas pelo despacho em execução entram no config (método R2) | Zero miss no trecho até o primeiro frame |
| HH05.8 ✔ | Relatório `HH05-RELATORIO.md` + revisão Codex | — |

## Fatos de entrada (HH01–HH04)

- VBR = r15 = 0x8C00F400, SR = 0x700000F0, FPSCR = 0x00040001 definidos pelo
  bootstrap do IP.BIN, que salta para `[0xAC00E004]`.
- Syscalls do BIOS por vetores em 0x8C0000B0/B8/BC/E0 (pools do jogo).
- Identidade do disco por `DISC/DISKn.ID` e dword em 0x8C185E28.
