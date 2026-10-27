# Dependências e ferramentas de terceiros

Toda dependência de terceiros é registrada aqui com origem, licença,
versão e justificativa (`LEGAL.md`).

## Incorporadas ao código

| Pacote | Origem | Licença | Versão | Uso |
|---|---|---|---|---|
| toml++ | https://github.com/marzer/tomlplusplus (FetchContent, não versionado) | MIT | v3.4.0 | Configuração do tradutor (HH04), como no Blue Stinger |
| Vulkan-Headers | `third_party/vulkan` (copiado do Blue Stinger: `vulkan.h`, `vulkan_core.h`, `vk_platform.h`, `vk_video/`) | Apache-2.0 (`third_party/vulkan/LICENSE.md`, `Apache-2.0.txt`) | 1.3.282 (a do SDL3 3.2.30) | Backend Vulkan do HH13; o carregador é aberto em tempo de execução, sem volk nem VMA |
| recomp-BStinger (código portado) | `C:\Source\recomp-BStinger` | MIT (`third_party/licenses/recomp-BStinger-LICENSE.txt`) | 2026-10-24 | `translator/{decoder,analysis,codegen,app}`, `runtime/{cpu,memory,pvr,gdrom,hle,maple,input,aica,boot,debug,mods}`, `tools/hh-extract/*/runtime_data.*`, `tools/hh-diff`, `tools/dcmap`, `tools/dev/flycast_oracle.py`, `native/{core,platform}` (HH07–HH10: arquivos, tempo, entrada), `transitional/{boundary_registry,gdfs_adapter,time_adapter,input_bridge,pad_contract,pad_adapter,pad_probe,bu_machine,bu_adapter}` (HH07–HH11), `native/platform/saves*` e `tools/dev/hh11_vmu.py` (HH11), `native/video/{movie_decoder,mf_movie,movie_playback,movie_catalog,subtitles}`, `native/audio/audio_output.cpp`, `native/core/com_scope.h` e `builder/video/movie_build.*` (HH12), `native/render/*` (inclusive `native/render/vulkan/*`, os shaders GLSL de `native/render/vulkan/shaders` e os SPIR-V gerados deles em `native/render/vulkan/spirv`) e `transitional/render_capture.*` (HH13), `tests/{unit,codegen,fixtures}` (inclusive `test_hh13_render_*`); cada arquivo C++ traz cabeçalho de origem; os shaders e os SPIR-V (sem cabeçalho próprio) têm a origem registrada aqui |

## Ferramentas externas (não versionadas, não ligadas ao produto)

| Ferramenta | Local | Licença | Uso |
|---|---|---|---|
| Ghidra 12.1.3 | `C:\Tools\ghidra_12.1.3_PUBLIC` | Apache-2.0 | Análise estática SH-4; oráculo SLEIGH (`tools/sh4-tablegen`) |
| dc-re-ghidra | `tools/dc-re-ghidra/` (ignorado; cópia em `~/ghidra_scripts`) | não confirmada | Script Ghidra (registradores MMIO, símbolos SDK). Nenhum código copiado |
| dream-recomp | `dream-recomp/` (ignorado) | GPL-2.0 | Oráculo externo apenas |
| Flycast | externo | GPL-2.0 | Oráculo externo apenas (HH05+) |
| FFmpeg | externo (a definir no HH12) | LGPL-2.1+ | Conversão offline dos vídeos 4X Movie no Builder, como processo separado (A11). Nenhum código copiado ou linkado |
| recomp-BStinger | `C:\Source\recomp-BStinger` | MIT | Referência arquitetural; `bs-extract.exe` usado como segunda implementação no HH01/HH02 |
