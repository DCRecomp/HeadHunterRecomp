# Política legal e de dados

Este projeto transforma cópias dos discos de **Head Hunter** fornecidas pelo próprio usuário em um executável nativo. Não contém o jogo e não é afiliado aos detentores dos direitos.

## Nunca versionar

- CDI, GDI, CHD, ISO, BIN/CUE, faixas brutas ou setores dos discos;
- BIOS, flash, SDK, headers, bibliotecas ou código-fonte da Sega;
- executáveis, assets, áudio, vídeo, modelos ou texturas extraídos;
- `icone-local/`: ícone feito da tela de título da cópia do próprio usuário. Vale só para builds locais; um executável que o embuta não se publica (a distribuição publicável leva o desenho original de `app/res/`);
- `game-data/`, `generated/`, `analysis/`, `snapshots/`, `reviews/`, saves e dumps;
- trechos de pseudocódigo ou relatórios que reproduzam quantidade substancial de código proprietário.

Configurações versionadas podem conter somente metadados necessários: produto, versão, região, tamanhos, hashes, endereços, nomes simbólicos inferidos e contratos descritos em linguagem própria.

## Licenças e referências

- O projeto novo será MIT.
- Código genérico do `recomp-BStinger` pode ser reutilizado porque é MIT, mantendo autoria, avisos e histórico de origem.
- `dream-recomp` e Flycast são GPL-2.0 e ficam como processos/ferramentas externos. Não copiar, adaptar ou linkar seu código no produto MIT.
- FFmpeg (LGPL) é permitido só como processo externo do Builder para converter os vídeos 4X Movie (ARCHITECTURE.md A11). Não copiar nem linkar seu código; a redistribuição do binário com o Builder é decidida no HH18.
- Projetos sem licença confirmada servem apenas como documentação comportamental; nenhum código é copiado.
- Toda nova dependência entra por `third_party/` com versão, origem, licença e justificativa registradas.

Qualquer dúvida de compatibilidade de licença bloqueia a incorporação até revisão humana.

