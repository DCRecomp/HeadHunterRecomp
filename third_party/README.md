# Dependências e ferramentas de terceiros

Toda dependência de terceiros é registrada aqui com origem, licença,
versão e justificativa (`LEGAL.md`).

## Incorporadas ao código

| Pacote | Origem | Licença | Versão | Uso |
|---|---|---|---|---|
| toml++ | https://github.com/marzer/tomlplusplus (FetchContent, não versionado) | MIT | v3.4.0 | Configuração do tradutor (HH04), como no Blue Stinger |
| recomp-BStinger (código portado) | `C:\Source\recomp-BStinger` | MIT (`third_party/licenses/recomp-BStinger-LICENSE.txt`) | 2026-10-24 | `translator/{decoder,analysis,codegen,app}`, `runtime/{cpu,memory,pvr,gdrom,hle,maple,input,aica,boot,debug,mods}`, `tools/hh-extract/*/runtime_data.*`, `tools/hh-diff`, `tools/dcmap`, `tools/dev/flycast_oracle.py`, `tests/{unit,codegen,fixtures}`; cada arquivo traz cabeçalho de origem |

## Ferramentas externas (não versionadas, não ligadas ao produto)

| Ferramenta | Local | Licença | Uso |
|---|---|---|---|
| Ghidra 12.1.3 | `C:\Tools\ghidra_12.1.3_PUBLIC` | Apache-2.0 | Análise estática SH-4; oráculo SLEIGH (`tools/sh4-tablegen`) |
| dc-re-ghidra | `tools/dc-re-ghidra/` (ignorado; cópia em `~/ghidra_scripts`) | não confirmada | Script Ghidra (registradores MMIO, símbolos SDK). Nenhum código copiado |
| dream-recomp | `dream-recomp/` (ignorado) | GPL-2.0 | Oráculo externo apenas |
| Flycast | externo | GPL-2.0 | Oráculo externo apenas (HH05+) |
| FFmpeg | externo (a definir no HH12) | LGPL-2.1+ | Conversão offline dos vídeos 4X Movie no Builder, como processo separado (A11). Nenhum código copiado ou linkado |
| recomp-BStinger | `C:\Source\recomp-BStinger` | MIT | Referência arquitetural; `bs-extract.exe` usado como segunda implementação no HH01/HH02 |
