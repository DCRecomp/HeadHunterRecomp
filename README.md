# Head Hunter Recompiled

<a href="https://recomplauncher.com/game/headhunterrecomp"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

Head Hunter (Sega Dreamcast) rodando **nativo no Windows**, sem emulador.

O código do jogo é convertido do processador do Dreamcast (SH-4) para C++ e compilado no seu computador a partir dos
**seus próprios discos**. Este repositório **não contém o jogo** nem nenhum arquivo dele (veja `LEGAL.md`).

> **Estado: pré-lançamento (beta de testes).** O jogo abre, roda o menu, os filmes, o som e a primeira área jogável.
> O resto da campanha ainda não foi verificado do início ao fim. É aí que você ajuda.

*English summary at the end.*

---

## Como testar, passo a passo

### 1. O que você precisa

| Item | Obrigatório? | Observação |
|---|---|---|
| Windows 10 ou 11, 64 bits | sim | Linux ainda não (planejado) |
| Os **dois discos** de Head Hunter em `.cdi` | sim | versão **MK-5104150 V1.006**. O Builder confere os discos e recusa outra versão |
| [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) 2022 ou 2026 | sim | na instalação, marque **"Desenvolvimento para desktop com C++"**. O jogo é compilado na sua máquina |
| [FFmpeg](https://ffmpeg.org/download.html) no `PATH` | não | sem ele, o jogo é instalado **sem os filmes** |
| Placa de vídeo com Vulkan | recomendado | sem Vulkan, o jogo desenha por software (mais lento) |
| Espaço em disco | sim | a instalação pronta ocupa cerca de 3 GB, e a compilação usa espaço extra durante o processo |

No Windows "N" (edição europeia sem mídia), instale o *Media Feature Pack*: sem ele os filmes não são convertidos.

### 2. Baixe o Builder

Na página [Releases](https://github.com/DCRecomp/HeadHunterRecomp/releases), baixe o
`HeadHunter-Builder-v<versão>-...zip` mais recente e descompacte numa pasta qualquer.

### 3. Gere o jogo

1. Abra **`HeadHunter-Recompiler.exe`**.
2. Escolha o disco 1, o disco 2 e a pasta onde o jogo vai ser instalado.
3. Espere terminar. O Builder extrai e confere os discos, gera o C++, compila e instala. Na máquina de
   desenvolvimento leva cerca de 7 minutos; numa máquina mais simples pode levar bem mais.

Depois disso o jogo **não precisa mais dos discos**. Os dois discos ficam na mesma instalação: ao terminar o disco 1, o
jogo segue para o disco 2 sem pedir troca.

> Prefere linha de comando? `hh-builder.exe` faz a mesma coisa (rode sem argumentos para ver o uso).
> Usa o [RecompLauncher](https://recomplauncher.com)? Instale pela
> [página do jogo](https://recomplauncher.com/game/headhunterrecomp): o launcher pede os dois discos e faz tudo isso
> sozinho (a partir da v0.12.0, o zip traz o `recomplauncher.json`).

### 4. Jogue

Abra **`HeadHunter.exe`** na pasta de instalação. **ESC** fecha o jogo.

| Dreamcast | Teclado | Controle (posição Xbox) |
|---|---|---|
| Direcional | setas | direcional |
| Analógico | I / J / K / L | analógico esquerdo |
| A / B / X / Y | Z / X / A / S | A / B / X / Y |
| Gatilhos L / R | Q / W | LT / RT |
| Start | Enter | Start |

- **Trocar teclas:** edite `controles.toml` na pasta do jogo. Exemplo:
  ```toml
  [teclado]
  A = ["SPACE", "Z"]   # A no espaço ou no Z
  [controle]
  A = "b"
  ```
- **Saves:** `Documentos\DC-Recomp\Head Hunter\saves`. Eles sobrevivem a reinstalar ou atualizar o jogo.

---

## O que testar

O que mais ajuda agora:

1. **Jogar o mais longe possível**, de preferência até os créditos, e dizer onde parou.
2. **A passagem do disco 1 para o disco 2** (o fim do disco 1, o save e o começo do disco 2).
3. **Carregar um save** do disco 2 pelo menu.
4. Filmes, som, falas e legendas fora de sincronia ou faltando.
5. Gráficos errados (texturas, transparências, objetos sumindo) comparados com o Dreamcast.
6. Controles: teclado, controle de Xbox/PlayStation, conectar e desconectar no meio do jogo.
7. Travamentos, quedas e lentidão, com o modelo da sua placa de vídeo.

### Problemas conhecidos

- **v0.11.0:** o jogo pode ficar parado na tela "INSERT DISC 2" no fim do disco 1, e o LOAD de um save do disco 2 não
  funciona. **Corrigido na v0.12.0**: use a versão mais recente.
- Ainda não há menu de configurações (gráficos, idioma). Os controles se ajustam só pelo `controles.toml`.
- A tela de carregamento antes da cena de abertura não aparece (a carga é instantânea).
- Só Windows por enquanto.

---

## Como relatar um problema

Abra uma [issue](https://github.com/DCRecomp/HeadHunterRecomp/issues) com:

- a versão (o conteúdo do `VERSAO.txt` da pasta do jogo);
- Windows, processador e placa de vídeo;
- **onde** no jogo aconteceu (disco, área, cena) e **o que** você fez antes;
- o que aconteceu e o que deveria acontecer;
- se o **Builder** falhou: o arquivo `builder.log`, que fica na pasta `.hh-build` dentro da pasta de saída.

**Não anexe** os discos `.cdi`, a pasta `data` nem o `HeadHunter.exe` gerado: eles contêm material do jogo e não podem
ser distribuídos.

---

## Estado do projeto

| Parte | Situação |
|---|---|
| Extração dos discos, tradução SH-4 → C++, Builder | pronto |
| Arquivos, tempo, controles, saves, filmes, render Vulkan e áudio nativos | pronto |
| Hardware do Dreamcast fora do executável do jogo | pronto |
| Código do jogo passando para C++ nativo legível (sem a "máquina SH-4" por baixo) | em andamento |
| Campanha completa verificada nos dois discos | em andamento: precisa ser jogada e gravada |
| Linux | planejado |
| Edição *Remastered* (menu de configurações, widescreen, idiomas, texturas HD) | planejado |
| Mods e projeto C++ editável | planejado |
| Versão 1.0 | planejado |

Detalhes: `STATUS.md` (estado oficial) e `ROADMAP.md` (plano completo).

---

## Para desenvolvedores

```powershell
cmake --preset vs                 # ou msvc-release/clang-release no Developer Prompt (Ninja)
cmake --build --preset vs-release
ctest --preset vs-release
```

| Pasta | Conteúdo |
|---|---|
| `app/` | executável do jogo |
| `builder/` | Recompiler, Developer e `hh-builder` |
| `translator/` | decodificador SH-4, análise e gerador de C++ |
| `native/` | sistemas definitivos (render, áudio, entrada, saves, lógica elevada) |
| `runtime/`, `transitional/`, `legacy/` | camadas temporárias da migração |
| `config/` | endereços, fronteiras e metadados por disco |
| `tools/`, `tests/` | ferramentas de análise, distribuição e testes |

Documentos: `ARCHITECTURE.md` (arquitetura), `AGENTS.md` (regras para agentes de IA), `docs/planos/` e
`docs/relatorios/` (planos e relatórios por etapa), `docs/LICENCAS.md` (licenças de terceiros).
Contribuições seguem as regras de `LEGAL.md`: nada de bytes, imagens ou dados do jogo no repositório.

---

## English summary

Head Hunter (Sega Dreamcast) running **natively on Windows**, with no emulator. The SH-4 game code is translated to C++
and compiled on your PC from **your own discs**. This repository contains no game data.

**To test:** you need Windows 10/11 x64, both Head Hunter discs as `.cdi` (MK-5104150 V1.006),
[Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) with *Desktop development with
C++*, and optionally [FFmpeg](https://ffmpeg.org/download.html) on `PATH` (without it, the movies are left out).
Download the Builder zip from [Releases](https://github.com/DCRecomp/HeadHunterRecomp/releases), run
`HeadHunter-Recompiler.exe`, pick both discs and an output folder, then start `HeadHunter.exe`.

Default keys: arrows = D-pad, I/J/K/L = analog stick, Z/X/A/S = A/B/X/Y, Q/W = L/R triggers, Enter = Start, Esc = quit.
Saves go to `Documents\DC-Recomp\Head Hunter\saves`.

Please play as far as you can, especially the disc 1 → disc 2 transition, and report problems in
[Issues](https://github.com/DCRecomp/HeadHunterRecomp/issues) with your `VERSAO.txt`, your hardware, and where in the game
it happened. Never attach disc images or the generated `data` folder.

License: MIT (`LICENSE`).
