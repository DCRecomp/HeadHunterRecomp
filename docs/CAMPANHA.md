# Gravar a campanha completa (guia do mantenedor)

> **RASCUNHO (2026-11-14).** Prontos e testados sem janela: o monitor (`tools/dev/campanha.py`, suíte
> `tests/campanha`), `--gravar`/`--reproduzir`/`--estado` do jogo (`tests/diff/hh16_reproducao.py`), a passagem do disco
> 1 para o 2 sem troca, o LOAD de um save do disco 2 e a cópia de segurança automática. **Não comece a campanha de
> verdade** antes do portão de prontidão da HH16.P (`docs/planos/HH16-PLANO.md`): falta a sessão piloto gravada na
> janela e reproduzida sem janela.

Você joga Head Hunter **uma vez**, do NEW GAME até os créditos, em quantas sessões quiser. O jogo grava o que você aperta
no controle; depois, uma reprodução sem janela prova que a campanha vai do começo ao fim e passa a validar todas as
versões seguintes do programa (o produto sem o código gerado, as edições Original e Remastered, o Linux). Você não
precisa zerar de novo.

## Como jogar

1. Abra `dist\HeadHunter-Gravador.exe` e clique em **Iniciar gravação**: o jogo abre gravando. A primeira vez cria a
   campanha (NEW GAME, cartão vazio); as seguintes continuam a mesma. A tela mostra GRAVANDO enquanto o jogo está aberto
   e, quando ele fecha, a sessão gravada, a cópia de segurança e o status da campanha. **Parar gravação** fecha o jogo
   como o X da janela (pergunta antes se você salvou); **Verificar** reproduz as sessões sem janela. A caixa **Campanha
   de teste** é para a sessão piloto: usa só campanhas de teste e nunca mexe na de verdade. Esta tela só fecha depois do
   jogo. (O gravador é compilado com o projeto, alvo `HeadHunter-Gravador`, e copiado para `dist\`; precisa do Python 3.)
   Alternativas sem o gravador: `tools\dist\Jogar-e-Gravar-Campanha.cmd`, ou `python -I tools\dev\campanha.py janela`.
2. Jogue normalmente, com teclado ou controle.
3. **Salve no jogo antes de fechar a janela.** A sessão seguinte continua do último SAVE (use LOAD no título).
4. Feche a janela do jogo para terminar a sessão. **ESC também fecha o jogo na hora, sem perguntar**: a sessão fica
   gravada, mas o que foi jogado depois do último SAVE não passa para a sessão seguinte.

## Dicas

- **Fim do disco 1:** depois dos filmes que encerram o disco, o jogo mostra "End of Disc" e pede para salvar. Salve:
  o jogo segue sozinho para o disco 2 (Aquadome), sem pedir troca de disco nem reiniciar. Salvar também antes dessa
  sequência é uma boa ideia: se a passagem tiver algum problema, a correção retoma desse save.
- Anote a hora dos momentos marcantes (chefes, áreas novas) e de qualquer defeito: ajuda a achar o ponto na gravação.
  (Checkpoints por tecla, como o F5 do BS, ainda não existem.)
- Explore como num jogo normal: menus, opções, armas e áreas opcionais aumentam a cobertura que a conversão do código
  usa.
- Pular filmes com START é permitido. **Deixe terminar o último filme** (os créditos): é ele que marca a campanha como
  completa.
- Se o jogo cair ou travar, abra o gravador (ou o atalho) de novo: o que foi gravado até ali não se perde. Se o próprio
  monitor for derrubado com o jogo aberto, a sessão é recuperada na próxima vez, depois que o jogo fechar.
- Só um monitor por campanha: um segundo atalho aberto ao mesmo tempo é recusado.
- Verificar depois de cada sessão é rápido e mostra cedo qualquer problema: `python -I tools\dev\campanha.py verificar
  --sessao N` (N = a sessão recém-jogada).
- Se você achar um defeito (gráfico, som, travamento), anote a sessão e o momento e me avise. O programa é corrigido sem
  que você precise jogar de novo o que já gravou.

## O que conta como progresso

| Situação | Significado |
|---|---|
| Sessão **gravada** | fechada normalmente; vale a partir do último SAVE |
| Sessão **interrompida** | o jogo caiu; o que foi gravado é conferido até a queda |
| Passos depois do último SAVE | jogados e conferidos, mas a sessão seguinte volta ao SAVE |

## Onde ficam os dados

Em `campanhas\<id>` dentro do repositório (ignorada pelo git): o programa copiado na criação, a entrada de cada sessão,
os saves de início e de fim e os hashes de quadro. Nunca publique essa pasta: ela tem saves e o executável derivado do
seu disco.

**Cópia de segurança automática:** ao fim de cada sessão o monitor copia a campanha para `campanhas\_copias\<id>`, ou
para a pasta dada na criação (`nova --copia <pasta>`, ou a variável `HH_CAMPANHA_COPIA` antes da primeira sessão pelo
atalho). Prefira outro disco: uma cópia no mesmo disco não protege contra falha dele. A cópia é incremental, nunca apaga
nada no destino e confere os selos SHA-256; se falhar, o monitor avisa e a sessão continua gravada. A cópia sozinha é uma
campanha completa (`HH_CAMPANHAS=<pasta> python -I tools\dev\campanha.py verificar`). Para refazer ou conferir a cópia à
mão: `python -I tools\dev\campanha.py copiar [--para <pasta>]`.

## Vereditos (`python -I tools\dev\campanha.py verificar`)

| Veredito | Significado |
|---|---|
| `CAMPANHA REPRODUZIDA ATE OS CREDITOS` | aprovada: todas as sessões reproduzidas, cadeia íntegra, o último filme terminou |
| `REPRODUZIDA, MAS INCOMPLETA (sem os creditos)` | o que existe confere, mas ainda não chegou ao fim |
| `REPROVADA` | divergência ou arquivo alterado; o relatório mostra a primeira diferença |
| `TESTE REPRODUZIDO` | campanha de teste; não vale como a campanha real |

`verificar --paralelo N` reproduz N sessões ao mesmo tempo (cada uma parte dos próprios saves de início; a cadeia é
conferida depois). O `relatorio.json` traz também a cobertura: áreas (`AHFDATA`) e filmes de cada sessão.
