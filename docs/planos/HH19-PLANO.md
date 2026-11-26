# HH19 — Head Hunter Remastered: planejamento

**Situação: incluída nos planos pelo mantenedor em 2026-10-28. NÃO INICIADA.** Equivalente ao N9 do Blue Stinger
(`C:\Source\recomp-BStinger\docs\N9-PLANO.md`, referência MIT). Este documento registra só o escopo, a arquitetura e os
critérios. Nada da HH19 é implementado antes do HH15 (executável sem legado) e do HH16 (campanha completa) concluídos
e da autorização explícita do mantenedor para começar.

Numeração: HH19 é o próximo número livre; as etapas existentes não foram renumeradas porque o código, os testes e os
relatórios as citam. A ordem de execução é **HH16 → HH19 → HH17 → HH18**, como o N9 do BS fica entre o N8 (sem legado)
e o N10 (mods e elevação do gameplay). Revisão de 2026-11-14 (`PLANO-ETAPAS-RESTANTES.md` seção 8): a HH19 começa
depois da HH15.F e da HH20 (Linux, se a D-P4 a incluir), termina com a HH19.E (estabilização) e o gate da campanha
(HH16.G) roda depois dela, nas duas edições.

## 1. Visão

Do mesmo código, duas edições instaláveis: **Head Hunter Original**, fiel ao Dreamcast, e **Head Hunter Remastered**,
um jogo moderno de PC. Base que já existe:

| A Remastered oferece | Base no HH |
|---|---|
| Instalação moderna | Builder/Recompiler (HH18 antecipado), instalação única dos dois discos (HH08) |
| Menus e configurações dentro do jogo | a construir (o BS tem o menu interno do N7c) |
| Controles modernos | HH10: SDL3 Gamepad, `controles.toml` |
| Saves sem a interface de VMU | HH11: saves nativos, importar/exportar VMS. **Em revisão (D-P3, pendente):** a recomendação é manter os saves do jogo, como o BS fez ao cancelar o N9.3, porque a campanha gravada depende deles |
| Legendas ligadas por padrão | HH12: legendas do jogo nos filmes; legendas no gameplay a construir |
| Widescreen, iluminação, texturas HD, resolução interna | HH13: Vulkan sobre a IR, substituição de texturas e shaders portados do BS |
| Filmes e áudio remasterizáveis | HH12 (`mods\video`) e HH14 (`mods\assets\STREAMS`, `hh-builder --audio-mod`) |
| Acessibilidade e otimização | HH09/HH15 (tempo lógico, sem legado) |

### 1.1 Objetivos

1. Duas edições do **mesmo núcleo C++**, sem projeto separado, fork ou branch por edição.
2. O Builder escolhe o perfil de edição e o grava na instalação.
3. Experiência de PC completa na Remastered.
4. A Original preservada e validada contra as referências existentes (estado por leitura do controle e a campanha
   gravada da HH16, quadros do modo fiel, Flycast).
5. Recursos audiovisuais substituíveis sem recompilar o executável.

### 1.2 Fora do escopo

- Ferramentas e APIs de modificação e a elevação do gameplay para C++ editável: HH17.
- Mudar o conteúdo do jogo (história, falas, regras, IA, dificuldade).
- Reintroduzir qualquer camada legada removida no HH15.
- Distribuir recursos comerciais do jogo.

## 2. Princípios (critérios formais, herdados do N9 do BS)

1. Original e Remastered compartilham gameplay, tempo, áudio, saves, controles e Vulkan.
2. O perfil de edição é imutável durante a execução; as preferências do jogador ficam separadas dele.
3. O núcleo de gameplay não conhece a apresentação (menus, HUD moderno, efeitos): expõe eventos e estados.
4. Saves e configurações do jogador sobrevivem a atualizações.
5. Validação separada para as duas edições; a Original continua equivalente às referências.

## 3. Decisões em aberto (antes de iniciar)

Específicas do Head Hunter, a levantar com evidência quando a etapa começar:
- que recursos da Remastered do BS se aplicam ao HH (o BS impõe a câmera livre; a câmera do HH ainda não foi
  estudada);
- widescreen real e HUD;
- legendas no gameplay (o HH tem dublagem por cena; as legendas do jogo só existem nos filmes): passam a ser necessárias
  para os pacotes de idioma (5.6).

## 4. Critério de saída

As duas edições instaladas pelo Builder passam as validações próprias; a Original continua equivalente às referências;
nenhuma camada legada removida no HH15 volta.

## 5. Menu de configurações (pedido do mantenedor, 2026-11-01)

**Situação: só planejado. Não antecipar:** segue a ordem da etapa (HH15 e HH16 concluídos e autorização para começar).
Pedido: "assim como o BSRecomp", um menu para escolher gráficos, controles e idioma, nas duas edições, com a
possibilidade de adicionar novos idiomas com dublagem e legendas, ou só legendas. Nada abaixo está implementado.

### 5.1 O que reaproveitar do Blue Stinger (MIT)

| Peça do BS | Papel | Adaptação no HH |
|---|---|---|
| `app/settings_menu.{h,cpp}` | modelo: estado, navegação, confirmação de 15 s, restaurar padrões, sem desenho | páginas Gráficos, Controles, Idioma e Áudio; legendas e câmera ficam de fora até serem decididas (seção 3) |
| `app/menu_overlay.{h,cpp}` | desenho em Direct2D/DirectWrite, numa camada ARGB pré-multiplicada, com teste de mouse | portável; ligar `d2d1` e `dwrite` só no alvo `HeadHunter` |
| `InGameMenu` em `app/main_sdl.cpp` | ligação com SDL e jogo: abre com ESC ou Back/View, pausa entre VBlanks, soltar teclas sempre chega ao jogo | extrair para arquivo próprio; o `hh_window.cpp` não cresce com essa lógica |
| `native/platform/graphics_settings`, `user_settings` | `graficos.toml`, `audio.toml`; capacidades, `clamp_to`, gravação atômica | só as opções com efeito real no HH |
| testes `test_settings_menu`, `test_menu_overlay`, `test_graphics_settings` | modelo, desenho e persistência | portar junto, com casos negativos |

Regra herdada: **nenhuma opção de enfeite**. O que ainda não funciona aparece desativado.

### 5.2 Diferenças do HH que o plano precisa resolver

- **Apresentação.** O HH copia o quadro ARGB para uma textura do `SDL_Renderer` (`app/hh_window.cpp`); o BS apresenta direto
  no Vulkan. A camada do menu entra como uma segunda textura com blend pré-multiplicado, o que basta para o menu. A
  resolução interna é outro assunto: `set_scale`, `hires_for` e `read_hires` existem em `transitional/render_devices.h`,
  mas a janela só recebe a imagem de 640x480. Sem apresentação direta (porta de `window_present.cpp`, com o `present_*` que
  o `vk_device.h` já tem) ou leitura da imagem maior, resolução e escala ficam desativadas.
- **ESC.** Hoje fecha o jogo. Passa a abrir o menu, que precisa de um item "Sair do jogo".
- **Pausa.** O `hh_host` não tem pausa exposta como o `GameHost::setPaused` do BS. Criar entre VBlanks, sem tocar no tempo
  lógico; provar que pausar e retomar não muda o estado do jogo.
- **Roteiros e `--headless`.** Menu desligado (no BS, `menu.disable()` no modo espectador), para os hashes não mudarem.

### 5.3 Opções previstas

| Página | Opção | Estado previsto |
|---|---|---|
| Gráficos | Renderizador (Vulkan ou software) | real: `opt.render`; a troca em execução não existe, avaliar se vale ao reiniciar |
| Gráficos | Modo de tela (janela, tela cheia, sem bordas), tamanho da janela | real pelo SDL, com a confirmação de 15 s |
| Gráficos | V-Sync | real: `SDL_SetRenderVSync` |
| Gráficos | Filtro da imagem na janela | real: modo de escala da textura |
| Gráficos | Resolução interna, escala | depende de 5.2 |
| Gráficos | Proporção 16:9 | desativada: depende da decisão de widescreen e HUD (seção 3) |
| Gráficos | Filtragem de texturas, efeitos, shaders | avaliar item a item contra `post_shader` e o Vulkan |
| Controles | Ver o mapa atual | real: `ActionMap` |
| Controles | Remapear no menu | **novo: o BS só mostra o mapa** ("em breve"). Capturar a próxima tecla ou botão, recusar conflitos, gravar `controles.toml` preservando comentários e perfis, e aplicar sem reiniciar (`InputSystem` hoje recebe o mapa só na criação) |
| Idioma | Textos e legendas | os idiomas do jogo (5.4) e os dos pacotes instalados (5.6) |
| Idioma | Dublagem | a original ou a de um pacote; independente dos textos (5.6) |
| Idioma | Idioma do menu | pt-BR e en-US, como no Builder |
| Áudio | Volumes | não pedido; há `set_bus_volume` no mixer, mas o som do jogo passa pelo motor de canais: confirmar antes de oferecer |

### 5.4 Idiomas do jogo: cinco, só de texto

Diferente do BS, em que o idioma é só de legendas e interface, aqui há textos do próprio jogo em cinco idiomas. A
dublagem, não: é uma só.

Fatos verificados em 2026-11-01 (comandos com `hh-disasm` e leitura da tabela AHFF; só endereços e contagens):

- pastas `Locale\English`, `German`, `French`, `Spanish`, `Italian` no `1ST_READ.BIN`; entradas nos AHF: 188, 181, 181, 181
  e 93;
- tabela de cinco ponteiros para essas pastas em `0x8C152474`;
- `0x8C152470`, logo antes da tabela: 26 referências no código, 65 leituras e 8 gravações pelo padrão de carga, valor 0 na
  imagem do jogo;
- `0x8C07B670` aloca 16 KB, chama duas rotinas do SDK e grava em `0x8C152470`: 1→0, 2→1, 3→2, 4→3, 5→0, e 0→0;
- `Locale\<idioma>` nos AHF só tem `.TXT`, de 0,1 a 1 MB por idioma: `SubTitle` (88 arquivos), a raiz (87 ou 88) e
  `Leila_Help` (6 ou 12). Inglês, alemão, francês e espanhol estão em 37 pacotes;
- o italiano é parcial: 3 pacotes, 93 arquivos e **nenhuma legenda**;
- os CSV de `HEADHUNTER\LOCALE` trazem os textos em colunas por idioma, numa só linha;
- **nenhum áudio fica em `Locale`**: 126 `STREAMS\*.ADP` sem sufixo de idioma, amostras em `SAMPLES\...` e um áudio por
  filme. Os idiomas do disco não trocam a dublagem.

Hipótese, ainda sem a segunda evidência exigida pelo `AGENTS.md`:

- `0x8C152470` é o índice do idioma, e o valor vem da configuração do console: 16 KB é o tamanho da partição 2 da flash, e
  os códigos 1 a 5 coincidem com a numeração de idioma do Dreamcast. O serviço nativo mantém essa partição em branco, o que
  explicaria o inglês de sempre;
- o código 5 (italiano) cairia em inglês nesse caminho, apesar de existir a pasta italiana; as outras gravações
  (`0x8C029F60`, que recebe o retorno de uma rotina, e `0x8C0EE7B4`, de um campo de `*0x8C151E84`) podem ser outro caminho.

Levantar antes de implementar:

1. comparar no Flycast o console em alemão, francês, espanhol e italiano com o inglês: valor em `0x8C152470`, tela de título
   (há `logo_headhunter_<idioma>`) e falas;
2. o que mais muda por idioma além dos `.TXT`: texturas com texto (`TITLE_LOCALTEXT`, `logo_headhunter_<idioma>`) e quais
   telas usam cada uma;
3. se trocar exige reiniciar: o valor é lido na inicialização, então a expectativa é valer na próxima abertura.

Caminho preferido, se a hipótese se confirmar: o sistema nativo entrega o idioma escolhido pela mesma rota que o jogo já
consulta (bloco de configuração na partição 2), sem remendo no código gerado e sem emulação nova. Com idioma padrão, tudo
continua igual às referências.

### 5.5 Decisões do mantenedor (ao iniciar)

- **Decidido (mantenedor, 2026-11-01):** o menu existe nas duas edições. Vale sobre a seção 1, que o listava como
  recurso da Remastered. A Original mantém os padrões fiéis.
- O idioma do jogo é trocável na Original? Recomendação: sim, porque é uma escolha que o console original já oferecia.
- Italiano: sem legendas no disco e em só 3 pacotes. Recomendação: oferecer só se o levantamento mostrar que funciona, e
  avisar no menu que os filmes ficam sem legenda.
- Pacotes de idioma incluídos no produto: recomendação, nenhum. O projeto entrega o mecanismo e um pacote de exemplo
  sem conteúdo do jogo; tradução e dublagem são distribuídas à parte (`LEGAL.md`).

### 5.6 Pacotes de idioma: dublagem e legendas, ou só legendas

Objetivo: instalar um idioma novo copiando uma pasta para `mods\`, sem recompilar. Sem pacotes, nada muda.

Base no BS (MIT): `native/localization` (`ModPackages`, `mod.toml`, recuo `pt-BR` → `pt`, caminhos seguros), `SubtitleService` e
`docs/LOCALIZACAO-PLANO.md`, incluindo a regra de sincronia da dublagem. Base no HH: `mods\assets`, `hh-builder --audio-mod`
(`docs/MODS-AUDIO.md`) e o `MovieCatalog`, que já resolve `.srt`/`.vtt` por idioma.

O `mod.toml` declara o que o pacote traz, e o menu mostra só o que existe:

```toml
formato  = 1
id       = "traducao-ptbr"
nome     = "Português (Brasil)"
idiomas  = ["pt-BR"]
conteudo = ["textos", "legendas"]   # "dublagem" é opcional
```

| Tipo de pacote | Traz | Resultado no menu |
|---|---|---|
| Só legendas | legendas de filmes e falas | novo idioma em Textos e legendas; Dublagem continua a original |
| Textos e legendas | também a interface do jogo | o jogo inteiro no idioma, com a dublagem original |
| Dublagem e legendas | também as falas | novo idioma nas duas opções, e o jogador pode combiná-las |
| Só dublagem | as falas | novo idioma em Dublagem |

Textos e dublagem são escolhas independentes, gravadas em `idioma.toml`. Por exemplo: legendas em português com a dublagem
original.

Frentes de trabalho, da menor para a maior:

1. **Legendas dos filmes:** quase prontas, pelo `MovieCatalog`. Falta ligar à escolha do menu e ao pacote.
2. **Legendas das falas no gameplay:** recurso novo. O jogo não as tem no disco: é preciso um evento nativo de início de
   fala por nome de stream, o `SubtitleService` e o desenho na camada do menu, sem tocar no estado do jogo.
3. **Textos do jogo:** o jogo lê `.TXT` e CSV dentro dos AHF e só conhece cinco pastas. Caminho preferido: sobrepor os
   arquivos no provedor nativo, servindo o texto do pacote no lugar do `Locale` ativo, sem remendar código gerado e sem
   exigir um AHF inteiro por pacote, como os mods de hoje. Levantar:
   - formato e códigos especiais dos `.TXT`, para recusar tradução que os quebre;
   - codificação e fonte: os CSV são de 8 bits e as fontes são texturas PVR, então idiomas fora do conjunto delas precisam
     de fonte própria, e os alfabetos não latinos podem ficar restritos a legendas, que usam DirectWrite;
   - como sobrepor uma entrada de AHF, já que o formato AHFF é conhecido, mas o jogo lê o pacote pelo próprio código.
4. **Dublagem das falas:** os `STREAMS` de fala por idioma do pacote, com conversão automática de WAV, FLAC e OGG, sem o
   passo manual do `--audio-mod`. Se a duração diferir, vale a regra do BS: o jogo vê a duração original.
5. **Dublagem dos filmes:** faixa de áudio ou MP4 por idioma, no `MovieCatalog`, com a imagem sincronizada.
6. **Vozes em amostras** (`SAMPLES\HERO`, `SAMPLES\LEILA` e parecidas): levantar quais são fala de verdade. Hoje a troca
   exige o AHF inteiro, então dependem da mesma sobreposição do item 3.

Ferramentas: exportar a lista de falas e textos para o tradutor, sem distribuir conteúdo do jogo, e validar um pacote antes
de instalar. O contrato fica nesta etapa; a API geral de mods e o editor ficam na HH17.

### 5.7 Ordem de trabalho e validação

1. Levantamento do idioma (5.4), que define o escopo da página Idioma.
2. Modelo e persistência, com unitários e negativos (V1).
3. Pausa no `hh_host`, com prova de que o estado não muda.
4. Camada do menu na janela, ligação com teclado, controle e mouse.
5. Opções reais de gráficos; depois, apresentação direta e resolução interna.
6. Remapeamento de controles.
7. Idioma do jogo no sistema nativo.
8. Contrato dos pacotes e legendas dos filmes (5.6, frente 1), com um pacote de exemplo.
9. Legendas no gameplay, depois textos, depois dublagem (5.6, frentes 2 a 6).
10. Fechamento V2: integração, comparação com as referências com o menu fechado, revisão do Codex (regra do `AGENTS.md`).

**RecompLauncher (padrões de `docs/DEVELOPERS.md` do RecompLauncher, conferidos em 2026-11-16; o perfil de instalação já
existe em `tools/dist/recomplauncher.json`, HH18).** O que depende do jogo e entra junto com este menu:
- as opções de gráficos num arquivo TOML (o launcher edita TOML linha a linha, preservando comentários), descritas em
  `config` e `settings` do perfil para aparecerem na página do jogo e no Big Picture; os padrões no primeiro início, nunca
  num arquivo dentro do pacote (a atualização descompacta por cima);
- `RECOMPLAUNCHER_MODE=bigpicture`: abrir em tela cheia (o modo de tela desta seção); `desktop`: em janela;
- `RECOMPLAUNCHER_PAD` (`xbox` ou `playstation`): os glifos dos botões no menu desde o primeiro quadro;
- o jogo inteiro jogável no controle, inclusive o menu e o "sair" (hoje o ESC fecha o jogo na hora, só no teclado);
- um `.log` numa pasta `logs` (ou ao lado do executável), que o launcher anexa ao relatório de queda; acrescentar
  `folders.logs` ao perfil;
- `portable.txt` ao lado do executável equivalente ao modo portátil de hoje (`instalacao.toml`).
- (guia do SDK, `recomplauncher.com/sdk-files/guide.md`, conferido em 2026-11-17) `folders` e `actions` do perfil para
  abrir a pasta de saves e a de logs pela página do jogo, junto com o log acima.

Pendente de decisão do mantenedor (os "Recomp Standards" do mesmo guia): para Dreamcast o padrão aceito é CHD e GDI, e o
selo "verified" (`dumps`) usa os valores do Redump, que cataloga o jogo em GDI. Hoje a versão suportada é a dos dois CDIs
(MK-5104150 V1.006); aceitar GDI/CHD muda o escopo de versão suportada e exige provar que os dados extraídos são os
mesmos, por isso não entra sem decisão.

Critério de saída deste item: cada opção oferecida tem efeito comprovado e sobrevive ao reinício; arquivo inválido volta ao
padrão com aviso; com padrões, roteiros e hashes ficam iguais às referências. Um pacote de exemplo só de legendas e outro
com dublagem funcionam sem recompilar, e pacote inválido é recusado com o motivo, sem impedir o jogo de abrir.

## 6. Filmes remasterizados com IA (registrado em 2026-11-14; equivalente ao N9.13 do BS)

**Situação: só planejado.** Pedido do mantenedor (2026-11-14), apontando que o BS fez e funcionou. Segue a ordem da
etapa: nada começa antes da HH19 autorizada.

### 6.1 O que o BS fez (referência MIT, conferida em 2026-11-14)

- Plano `docs/N9-13-FILMES-IA.md` (branch `n9-8` do BS, ainda fora do `master`): versão **A** (widescreen) e versão **B**
  (modernizada, vídeo para vídeo a 60 fps), fases 13.0–13.4, decisões D13-1 a D13-6, decisão de distribuição na seção 7.
- Método da versão A, **aprovado pelo mantenedor no filme E017** e em produção local desde 2026-11-14
  (`tmp/sessao-20261113/filmes-ia/` do BS: `prova_a.py`, `produz_a.py`, `roda_filmes.py`, `valida_a.py`):
  - o centro 4:3 é **sempre o quadro original ampliado** (Real-ESRGAN x4plus, BSD-3, carregado pelo `spandrel`, MIT); nada é
    gerado no centro;
  - só as laterais do 16:9 são criadas pelo **Wan2.1-VACE-1.3B** (Apache-2.0, `diffusers`), **plano a plano** (corte de cena
    detectado por diferença entre quadros; um pedido nunca atravessa um corte), com o centro como condição;
  - geometria na mesma escala do centro (correção da v1, que esticava as laterais); emenda de 48 px; casamento de cor por
    linha com suavização temporal sem atravessar cortes (correção da v1, o centro parecia mais escuro);
  - **áudio copiado** sem recodificar; mesma contagem de quadros, 30 fps, duração igual;
  - produção retomável por pedaço, parada por falta de RAM, uma execução por vez, arquivo `PARAR`, `progresso.tsv`;
    caches de modelos e temporários dentro da pasta de trabalho, Hugging Face offline e sem telemetria;
  - validação automática por filme: 1920×1080, mesmos quadros e fps, duração com até 1 quadro de diferença, MD5 do áudio
    igual ao original, PSNR ≥ 22 dB do centro reduzido contra o original, laterais não pretas nos quadros não pretos.
- Medida de custo no BS: E017, 238 quadros, 221 s de geração na GPU do mantenedor (NVIDIA; ~27 GB de modelos, caches e
  venv na pasta da sessão).
- Distribuição (decisão do mantenedor no BS, 2026-11-13): os filmes gerados nunca vão no git nem na release; o pacote
  identificado vai para um repositório público fora do GitHub (recomendado Hugging Face Hub), e o instalador baixa sob
  demanda com SHA-256, desmarcado por padrão, sem bloquear a instalação se falhar.

### 6.2 O que muda no Head Hunter (medido no HH12)

O HH tem **53 filmes** (25 + 28, 4 933,9 s, 148 010 quadros, 30 fps), em formatos já largos, o que muda a técnica por
grupo:

| Grupo | Quadro | Proporção | Em 16:9 | Técnica |
|---|---|---|---|---|
| 37 filmes | 480×204 | ~2,35:1 (mais largo que 16:9) | cabe na largura com faixas finas em cima e embaixo | **só ampliação**; a extensão vertical para encher a tela é opcional (D-F2) |
| 11 filmes | 480×270/272 | ~16:9 | tela cheia | **só ampliação** |
| 5 filmes | 480×360 | 4:3 | precisa das laterais | **ampliação + laterais** pelo método do BS |

Consequências: o custo grande do BS (geração das laterais) se aplica a 5 filmes (4 410 quadros, 2,5 min: `SCENE0` nas
duas variantes, `SCENE0L`, `SCENE18`, `SCENE54_55`; `docs/planos/HH16-PREPARACAO.md` seção 6), não a 53; os outros 48 só passam pela
ampliação, muito mais barata. A lista exata por filme sai do `data\video\manifest.toml` (tamanho exibido `vtrk`). As
legendas do jogo são desenhadas pelo jogo sobre o filme (HH12), não estão na imagem: não atrapalham a geração. O
`SCENE0` tem variante por disco (`.D2`), e o contrato de mods já cobre (`<NOME>.D2.mp4`).

### 6.3 Fases (cada uma com autorização, como no BS)

| Fase | Conteúdo | Aceitação |
|---|---|---|
| F0 | inventário por filme (grupo, cortes, movimento de câmera, texto na imagem); licenças conferidas no texto (código e pesos); porte do processo do BS para `tools/filmes-ia/` com a entrada pelos MP4 do Builder; amostra de 10 s de um filme de cada grupo | amostras locais avaliadas pelo mantenedor; tabela de tempo e VRAM |
| F1 | contrato no jogo: versões de filme por pacote, escolha no menu (Filmes: original / remasterizados), o filme 16:9 em tela cheia na Remastered 16:9, recuo por filme, legendas na posição certa; a Original sempre com os filmes do disco | pacote **sintético** (barras e contador): duração e leitura do controle do fim iguais, estado por leitura idêntico, sessão gravada da campanha reproduzida igual |
| F2 | versão A dos 53 (ampliação; laterais nos 5 de 4:3), produção retomável | `valida_a` por filme; revisão visual do mantenedor filme a filme |
| F3 | versão B (modernizada), se o mantenedor quiser, sobre a A | as mesmas medidas, mais consistência temporal; filme reprovado fica na A |
| F4 | bateria e documentação nas duas plataformas | sem falhas; Original igual às referências |

**Contrato inegociável (HH16, seção 4.1):** cada filme remasterizado termina no **mesmo passo lógico**: mesma contagem de
quadros a 30 fps (ou o equivalente exato a 60 fps na B), áudio original. A gravação da campanha valida isso por
`reproduzir-em` com o pacote instalado.

### 6.4 Decisões ao iniciar

| | Pergunta | Recomendação |
|---|---|---|
| D-F1 | Onde roda: ferramenta local (R1) ou etapa opcional do Recompiler (R2) | R1, como o BS; R2 depois da medida |
| D-F2 | Os 37 filmes 2,35:1: manter as faixas (fiel ao enquadramento) ou estender em cima e embaixo para encher o 16:9 | manter as faixas; a extensão vertical só como opção depois de amostra |
| D-F3 | Distribuição: a mesma do BS (pacote identificado fora do GitHub, baixado pelo instalador, desmarcado por padrão) | a mesma, com decisão explícita do mantenedor para o HH |
| D-F4 | Versão B (modernizada) entra no HH | decidir depois de ver a A |
