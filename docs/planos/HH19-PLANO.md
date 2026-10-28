# HH19 — Head Hunter Remastered: planejamento

**Situação: incluída nos planos pelo mantenedor em 2026-10-28. NÃO INICIADA.** Equivalente ao N9 do Blue Stinger
(`C:\Source\recomp-BStinger\docs\N9-PLANO.md`, referência MIT). Este documento registra só o escopo, a arquitetura e os
critérios. Nada da HH19 é implementado antes do HH15 (executável sem legado) e do HH16 (campanha completa) concluídos
e da autorização explícita do mantenedor para começar.

Numeração: HH19 é o próximo número livre; as etapas existentes não foram renumeradas porque o código, os testes e os
relatórios as citam. A ordem de execução é **HH16 → HH19 → HH17 → HH18**, como o N9 do BS fica entre o N8 (sem legado)
e o N10 (mods e elevação do gameplay).

## 1. Visão

Do mesmo código, duas edições instaláveis: **Head Hunter Original**, fiel ao Dreamcast, e **Head Hunter Remastered**,
um jogo moderno de PC. Base que já existe:

| A Remastered oferece | Base no HH |
|---|---|
| Instalação moderna | Builder/Recompiler (HH18 antecipado), instalação única dos dois discos (HH08) |
| Menus e configurações dentro do jogo | a construir (o BS tem o menu interno do N7c) |
| Controles modernos | HH10: SDL3 Gamepad, `controles.toml` |
| Saves sem a interface de VMU | HH11: saves nativos, importar/exportar VMS |
| Legendas ligadas por padrão | HH12: legendas do jogo nos filmes; legendas no gameplay a construir |
| Widescreen, iluminação, texturas HD, resolução interna | HH13: Vulkan sobre a IR, substituição de texturas e shaders portados do BS |
| Filmes e áudio remasterizáveis | HH12 (`mods\video`) e HH14 (`mods\assets\STREAMS`, `hh-builder --audio-mod`) |
| Acessibilidade e otimização | HH09/HH15 (tempo lógico, sem legado) |

### 1.1 Objetivos

1. Duas edições do **mesmo núcleo C++**, sem projeto separado, fork ou branch por edição.
2. O Builder escolhe o perfil de edição e o grava na instalação.
3. Experiência de PC completa na Remastered.
4. A Original preservada e validada contra as referências existentes (estado por passo, quadros do modo fiel, Flycast).
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
- legendas no gameplay (o HH tem dublagem por cena; as legendas do jogo só existem nos filmes).

## 4. Critério de saída

As duas edições instaladas pelo Builder passam as validações próprias; a Original continua equivalente às referências;
nenhuma camada legada removida no HH15 volta.
