# Dependências e licenças: política e inventário (preliminar)

**Situação (2026-11-14):** inventário **preliminar**, feito só pela leitura das fontes (`CMakeLists.txt`,
`app/`, `native/`, `builder/`, `tools/dist/make_builder.ps1`, `third_party/`), sem compilar nem montar pacote. **Não é
parecer jurídico e não declara conformidade.** Modelo: `docs/N9-L-LICENCAS.md` do Blue Stinger (MIT). É pré-requisito
da HH18 e da publicação de qualquer pacote Linux (HH20).

## 1. Política

Regra permanente do `AGENTS.md` (adotada do BS em 2026-11-14): nenhuma dependência de compilação, execução ou
distribuição pode exigir licença paga, royalties, assinatura, autorização comercial individual ou restrição
incompatível com distribuição pública, gratuita e open source. Permissivas são preferidas; GPL, LGPL e MPL valem com as
obrigações cumpridas, sem ligar código GPL ao produto MIT (`LEGAL.md`). Conferir também patentes (codecs), marcas e
ativos. Nenhuma troca de componente que funciona sem necessidade.

## 2. O que é distribuído

| Entregável | Conteúdo | Distribuído pelo projeto? |
|---|---|---|
| Builder Windows (`dist/HeadHunter-Builder`) | `HeadHunter-Recompiler.exe`, `HeadHunter-Developer.exe`, `hh-builder.exe`, DLLs do runtime do MSVC quando o `dumpbin` as pede, `licencas/`, kit de fontes (projeto, SDL3 em fontes, toml++, cabeçalhos Vulkan) | sim, quando publicado (v0.11.0 publicada) |
| Jogo (`HeadHunter.exe`) | compilado **na máquina do usuário** a partir dos discos dele | não |
| C++ gerado, dados dos discos, filmes convertidos, saves, campanhas gravadas | derivados dos discos do usuário | **nunca** |

## 3. Matriz de componentes

| Componente | Licença | Onde entra | Como entra | Situação |
|---|---|---|---|---|
| Código do projeto | MIT (`LICENSE`) | tudo | fonte | compatível |
| Código portado do Blue Stinger Recompiled | MIT (`third_party/licenses/recomp-BStinger-LICENSE.txt`) | runtime, native, transitional, builder, testes | fonte, com cabeçalho de origem por arquivo | compatível |
| SDL3 3.2.30 | zlib | janela, entrada e controles do jogo | FetchContent; ligado estático no `HeadHunter.exe`; fontes no kit | compatível (licença em `licencas/SDL3-zlib.txt`) |
| toml++ v3.4.0 | MIT | tradutor, Builder, configuração nativa | FetchContent; fontes no kit | compatível |
| Vulkan-Headers 1.3.282 | Apache-2.0 | render Vulkan | `third_party/vulkan`; o carregador do sistema é aberto em execução | compatível (`LICENSE.md` e `Apache-2.0.txt` no pacote) |
| Runtime do MSVC (`msvcp*`, `vcruntime*`, `concrt*`) | termos de redistribuição do Visual Studio | executáveis do Builder | DLLs copiadas quando necessárias | **ajustes**: registrar o fundamento da redistribuição no pacote (como "Distributable Code") |
| APIs do Windows: Media Foundation, XAudio2, Direct2D, DirectWrite, WIC, XInput, COM, shell32, comctl32 | parte do sistema operacional | filmes, áudio, legendas, controles de reserva, pastas, janelas do Builder | ligadas por bibliotecas de importação do SDK do Windows; nada redistribuído | compatível no Windows; **sem equivalente no Linux** (HH20) |
| Codificação H.264 + AAC dos filmes | codecs do Windows (Media Foundation) | Builder, na máquina do usuário | o codificador do sistema; os MP4 ficam só na instalação do usuário | **análise**: patentes cobertas pelo licenciamento do sistema no Windows; no Linux, decidir o formato (D-P5) |
| FFmpeg | LGPL-2.1+ (conforme a compilação) | Builder: só decodifica os 4X Movie | **processo externo** instalado pelo usuário; não vai no pacote, não é ligado | compatível (decisão do mantenedor de 2026-10-24, ARCHITECTURE A11) |
| Fonte das legendas e da interface | fontes do sistema (DirectWrite) | legendas e menus | sistema | compatível no Windows; no Linux precisa de fonte livre no pacote (BS: Noto Sans, OFL) |
| Ghidra, Flycast, dream-recomp, dc-re-ghidra | Apache-2.0 / GPL-2.0 / não confirmada | só análise e oráculo | ferramentas externas, nunca no produto | fora da distribuição |

## 4. Pendências antes da HH18

1. Fundamento da redistribuição das DLLs do MSVC no `LEIA-ME` e em `licencas/`.
2. Aviso de terceiros (`AVISOS-DE-TERCEIROS.txt`) com cada componente da seção 3 que vai no pacote, como no BS v0.8.0.
3. Conferir o texto da licença de cada componente na versão exata usada (SDL3, toml++, Vulkan-Headers) contra o que vai em
   `licencas/`.
4. Linux (HH20): fonte livre, saída de áudio, decodificação dos filmes sem codec patenteado no pacote, compilador
   portátil (no BS: Zig 0.13 = Clang/LLD, Apache-2.0 com exceção LLVM; CMake BSD-3; Ninja Apache-2.0).
5. Filmes remasterizados e texturas HD (HH19 seção 6): pacotes derivados do jogo, distribuição só com decisão do
   mantenedor (D-F3).
6. Varredura do histórico Git e dos pacotes antes de cada publicação (`docs/PUBLICACAO-GITHUB.md` do BS).
