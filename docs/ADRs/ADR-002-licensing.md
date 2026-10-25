# ADR-002 — Licenciamento

Estado: aceito (revisado em 2026-10-24).

## Decisão

- O projeto é distribuído sob MIT (`LICENSE`).
- Todo arquivo-fonte versionado (C++, CMake de módulo, Python, PowerShell,
  Java) começa com `SPDX-License-Identifier: MIT`, depois do shebang quando
  houver. Documentos, configs TOML e `CMakeLists.txt` herdam a licença da raiz.
- Arquivos gerados que são versionados (`decoder_table.inc`, `op_enum.hpp`)
  recebem o cabeçalho a partir do gerador.
- Código portado de projetos MIT (Blue Stinger) mantém aviso de origem no
  cabeçalho e é registrado em `third_party/README.md`.
- Nenhum código GPL (`dream-recomp`, Flycast) é copiado, adaptado ou ligado.
  Projetos sem licença confirmada não fornecem código.

## Motivo

MIT é permissiva e compatível com o reuso do Blue Stinger. O identificador
SPDX torna a licença verificável por ferramenta sem repetir o texto completo.

## Revisão de 2026-10-24

A versão anterior exigia cabeçalho MIT em todo arquivo, o que não estava
cumprido. Os fontes C++/Python receberam SPDX e a regra foi delimitada a
arquivos-fonte.
