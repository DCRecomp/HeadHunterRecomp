# Head Hunter Recompiled

Recompilação estática AOT de **Head Hunter (Sega Dreamcast)** para Windows x64
em C++20, gerada localmente a partir dos dois discos fornecidos pelo próprio
usuário. O repositório não contém o jogo nem dados dele (`LEGAL.md`).

## Documentos

- `STATUS.md` — estado autoritativo das etapas HH00–HH18.
- `ROADMAP.md` — plano mestre e critérios de saída.
- `ARCHITECTURE.md` — arquitetura-alvo.
- `AGENTS.md` — regras para agentes que trabalham no repositório.
- `LEGAL.md`, `docs/ADRs/` — licença e política de dados.
- `docs/planos/`, `docs/relatorios/` — planos detalhados e relatórios por etapa.

## Build e testes

```powershell
cmake --preset vs                 # ou msvc-release/clang-release no Developer Prompt (Ninja)
cmake --build --preset vs-release
ctest --preset vs-release
```

## Estrutura

`app/` host · `builder/` Recompiler/Developer · `config/` metadados por disco ·
`translator/` decodificador SH-4, análise e codegen · `runtime/`, `legacy/`,
`transitional/` camadas temporárias · `native/` sistemas definitivos ·
`tools/` extração e geradores · `tests/` testes.

## Trabalho autônomo

`TRABALHAR-AUTONOMO.bat` inicia o executor do OpenCode, que segue `AGENTS.md`
até o HH18 ou até uma `DECISAO_NECESSARIA`; `PARAR-TRABALHO-AUTONOMO.bat`
pede a parada depois do turno atual.

Licença: MIT.
