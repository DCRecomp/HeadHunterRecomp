# HH06 — Plano detalhado: primeira fatia jogável, Preview e Builder inicial

Validação mínima: **V2**. Critério de saída (ROADMAP): roteiro reproduzível chega à primeira
área controlável do disco 1 em modo janela e headless; build sem interpretador para o trecho
coberto.

## Caminho do jogo (observado no `hh-run`, disco 1)

Logos (SEGA, CRI ADX, 4X, NOISEBOX) → tela-título → START → aviso "memory card de 30 blocos;
desativar save?" (sem VMU no runtime híbrido; saves nativos no HH11) → OK → menu NEW GAME /
LEILA / OPTIONS → NEW GAME → TRAINING MISSION (PLAY / SKIP) → … → primeira área controlável.

## Subetapas

| ID | Entrega | Evidência de saída |
|---|---|---|
| HH06.1 ✔ | Roteiros determinísticos (`tools/dist/roteiros/`, índice = leitura do controle) e relógio fixo | Mesmo roteiro, mesmos hashes de quadro em duas execuções |
| HH06.2 ✔ | Funções descobertas pelo despacho no caminho (método R2, `tools/dev/hh05_misses.py`) | Zero miss até a primeira área |
| HH06.3 ✔ | Núcleo `app/hh_host` comum a `hh-run` e à janela | `hh-run` com o mesmo resultado de antes |
| HH06.4 ✔ | Preview com janela `HeadHunter` (SDL3 estático, teclado/XInput; roteiro = modo espectador) | Roteiro chega à primeira área em janela |
| HH06.5 ✔ | Builder inicial (CLI): CDI → dados de execução → C++ → compilação → pasta Preview | Instalação nova a partir do CDI roda o roteiro |
| HH06.6 ✔ | Regressão retomável inicial (`tools/dev/regressao.py`, estado por etapa) | Retoma da primeira etapa não aprovada |
| HH06.7 ✔ | Relatório + revisão Codex | — |

## Regras

- Recompilar, nunca emular (`AGENTS.md`): nada de interpretador no executável; o runtime
  híbrido é temporário (HH15).
- O Preview é derivado da cópia do usuário (contém código gerado do executável dele): não é
  distribuído; só o Builder (fonte MIT + kit) será, no HH18.
