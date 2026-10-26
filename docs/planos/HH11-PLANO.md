# HH11 — Plano detalhado: saves nativos e continuidade entre discos

Data: 2026-10-25. Validação mínima: **V2**. Equivalente ao N4 do Blue Stinger
(`C:\Source\recomp-BStinger\docs\N4-PREPARACAO.md` e relatório do N4, MIT, somente leitura).
Preparado durante a regressão do HH10. Endereços marcados *hipótese* precisam de duas evidências
independentes no HH11.0 antes de qualquer slot.

## Situação atual

- O runtime híbrido não tem VMU. No roteiro `newgame_area1` o jogo avisa que não há memory card de
  30 blocos e oferece desativar o save (HH06). Não há caminho legado que salve: o HH11 entrega uma
  capacidade nova, por um provedor nativo, como o N4 do BS.
- O produto junta os dois discos numa instalação (HH08, decisão do mantenedor 2026-10-25): não há
  troca de disco, e a continuidade é o save feito no disco 1 continuar valendo quando o jogo segue
  para os dados do disco 2.

## Fatos de entrada (preparação, 2026-10-25)

### Bibliotecas ligadas (banners em `analysis/hh07/banners.txt`)

| Biblioteca | Banner | Uso no HH11 |
|---|---|---|
| `bu Ver 1.47` | 0x8C186439 | backup (VMU): fronteira principal |
| `pdLcd Ver 1.04` | 0x8C185E65 | tela da VMU: responder "LCD não suportado", desacoplado do save |
| `pdVib 2.01`, `kd 1.48` | 0x8C1873ED, 0x8C1873C1 | fora do escopo (vibração/teclado) |

### Camada de save do jogo (strings e literais no `1ST_READ.BIN`)

| Literal | Texto | Referências |
|---|---|---|
| 0x8C140E24 | `H_HUNTER.SYS` | 0x8C116CA4, 0x8C116E90, 0x8C117A34, 0x8C117C60, 0x8C117E14, 0x8C1187C0 |
| 0x8C140E48 | `HEADHUNTER` (descrição VMS, 32 colunas) | 0x8C117040 |
| 0x8C140E6C | `Headhunter01` | 0x8C117044 |
| 0x8C140E80/EAC | ícones `IconJack.avq`, `IconAmuze.avq` | — |
| 0x8C140ED4 | `SAVED GAME` | — |

*Hipótese*: o módulo de save do jogo ocupa 0x8C116C00–0x8C118800. Um único arquivo `H_HUNTER.SYS`,
porque não aparece a varredura de `.000`–`.005` do BS.

### `bu` 1.47: chamadas do módulo de save do jogo (callgraph do HH09)

O módulo chama 14 entradas do SDK na faixa 0x8C15FB44–0x8C160B30. A fronteira atual as classifica
como `cri`/`desconhecida`, sem subsistema; é o mesmo erro de classificação do BS. Com a `bu` do BS
deslocada de 0xD3D58 (`buInit` 0x8C08BDEC → 0x8C15FB44), o par de callbacks bate exatamente
(BS 0x8C08C074/07A → HH 0x8C15FDCC/FDD2). As entradas seguintes ficam 0x12–0x18 antes do previsto,
o que indica versão próxima, não idêntica.

| HH | Papel (*hipótese*, pela `bu` do BS) | Chamadas |
|---|---|---|
| 0x8C15FB44 | `buInit` | 1 |
| 0x8C15FC2A | `buExit`? | 1 |
| 0x8C15FDCC / 0x8C15FDD2 | registro dos callbacks de conclusão / progresso | 1 / 1 |
| 0x8C15FF54 | informação do disco | 2 |
| 0x8C16037C | existência de arquivo / estado | 3 |
| 0x8C1603AA, 0x8C1603DC | estado da operação / último erro | 1 / 1 |
| 0x8C160500, 0x8C160558 | gravação / remoção | 1 / 1 |
| 0x8C160614 | leitura (`buLoadFile`) | 1 |
| 0x8C160754, 0x8C160774 | montagem | 1 / 1 |
| 0x8C160B30 | imagem VMS (rotina pura?) | 2 |

### HH11.0 iniciado (2026-10-25): pareamento com a `bu` do BS

Script local `bu_match.py` (scratchpad): multiconjunto de instruções mascaradas, com os imediatos
mantidos, contra as funções da `bu` do BS (papéis do N4). Ordem livre, porque o compilador reordenou
as instruções. HH 0x8C160614 confere instrução a instrução com o `load` do BS (op 14, mesma sequência
de chamadas).

| Papel (BS) | HH | Similaridade (1º / 2º) |
|---|---|---|
| init 8C08BDEC | 0x8C15FB44 | 0.56 / 0.24 |
| exit 8C08BEBC | 0x8C15FC2A | 0.62 / 0.33 |
| reg_done / reg_progress | 0x8C15FDCC / 0x8C15FDD2 | 1.00 |
| status 8C08C696 | 0x8C1603DC | 1.00 |
| op0B 8C08C7BE | 0x8C160500 (o HH o chama; no BS só por literal morto) | 0.98 / 0.67 |
| save 8C08C818 | 0x8C160558 | 1.00 / 0.69 |
| load 8C08C8D4 | 0x8C160614 | 1.00 / 0.67 |
| mount 8C08CA34 | 0x8C160754 | 1.00 / 0.46 |
| vms_build 8C08CA5C | 0x8C160774 | 0.59 / 0.10 |

Resolvidos (segunda evidência: corpo do alvo e ABI do chamador do jogo):

| HH | Papel | Evidência |
|---|---|---|
| 0x8C15FF54 | disk_info | o alvo do `jmp` (0x8C1721CA) bate com o do BS (0x8C09198A): 0.69 contra 0.14 |
| 0x8C16037C | exists | o alvo 0x8C172EEE bate com o do BS (0x8C0926F6): 0.86 contra 0.14 |
| 0x8C1603AA | informação do arquivo `(drive, nome, info*)` | o alvo 0x8C171CD8 é instrução a instrução o do wrapper BS 0x8C08C664 (0x8C091494): grava blocos e campos em `info`. O jogo (0x8C118296) passa 3 argumentos e devolve `r0 == 0`. O BS não usava essa entrada |
| 0x8C160B30 | CRC-16-CCITT (polinômio 0x1021, máscara 0x8000) | mesma estrutura do CRC VMS do BS (0x8C08CDE2, 0.51). O jogo o chama direto (0x8C1180A6, 0x8C118108). Rotina pura: fica recompilada |
| 0x8C160990 | tamanho da imagem VMS | 1.00 com o do BS (0x8C08CC58); o jogo não o chama direto |

Diferenças do init/exit em relação ao BS (lado a lado no disassembly):
- servidor periódico: **0x8C15FC9C** (o `bra` em 0x8C15FB40). Registrado por 0x8C16F7CA(`*0x8C186498` = 0x14, servidor): uma tabela de 32 callbacks da camada `per` em `*0x8C16F840`, não o agendador do jogo como no BS. O exit chama 0x8C16F7CA(0x14, 0) para desregistrar;
- o init grava a máscara (r5) em 0x8C313674, r6 em 0x8C313670 e zera 0x8C313660–0x8C31366C;
- o init chama 0x8C170F70 e 0x8C1715BA (inicialização interna); o callback do 4º argumento só é chamado se o resultado for 0, e sem argumentos;
- flag de init 0x8C188D20 (= 1 no init, 0 no exit);
- `last_error` (BS 0x8C08C112) não é chamado pelo jogo do HH; a entrada 0x8C160500 (op 0x0B no BS), sim.

Falta para o adaptador: o significado da operação 0x0B no HH (0x8C160500, chamada de 0x8C117E30); os
códigos de progresso e conclusão. No BS vieram de um oráculo com VMU no Flycast; no HH precisam de um
oráculo próprio.

## Subetapas

| ID | Entrega | Saída |
|---|---|---|
| HH11.0 | Contrato da `bu` 1.47: disassembly de cada entrada, argumentos (ABI), códigos de retorno, callbacks e o servidor periódico; papéis confirmados com duas evidências. Varredura de literais mortos, como no BS | tabela de contrato; fronteira reclassificada (`saves`) |
| HH11.1 | `native/platform/saves` (porte do N4 do BS): store com escrita atômica (temp + rename), integridade (hash), versão do formato e migração | unitários: atomicidade, corrupção, falta de espaço, nomes |
| HH11.2 | `transitional/bu_adapter` (+ máquina de estados do N4): slots na API pública da `bu`; operações assíncronas com o mesmo ciclo ocupado → concluído do original, por passo do jogo | `--saves legado\|nativo` |
| HH11.3 | VMU presente no nativo: o jogo vê um cartão (capacidade real da VMU, 200 blocos) sem Maple de armazenamento | roteiro NEW GAME → save → reiniciar → LOAD |
| HH11.4 | Importação/exportação de `.VMS`/`.VMI` (formato Dreamcast), se o contrato provar que o conteúdo é o arquivo do jogo sem transformação | teste de ida e volta |
| HH11.5 | Continuidade entre discos: save feito antes da passagem para o disco 2, carregado depois, na instalação unificada | depende da rota longa (HH16 para a passagem real) |
| HH11.6 | `pdLcd`: respostas "sem LCD" sem acoplar ao save | fechamento |

## Portões (rascunho)

| # | Critério |
|---|---|
| S1 | Contrato: cada entrada da `bu` usada pelo jogo com papel, argumentos e retornos comprovados |
| S2 | Save → reinício → load devolve o mesmo estado do jogo (hash dos bytes do save e quadros depois do load) |
| S3 | Falhas seguras: arquivo corrompido, truncado, sem espaço, pasta sem escrita. O último save válido nunca é destruído |
| S4 | Reinstalação/upgrade: o save fica fora da pasta de instalação e sobrevive a ela |
| S5 | Fechamento: no nativo, 0 comandos de armazenamento no Maple e a `bu` original sem execução nas entradas substituídas |
| S6 | Determinismo e equivalência dos quadros até o primeiro acesso ao save (o legado sem VMU diverge depois, por desenho) |

## Pasta dos saves

Segue o precedente do BS (decisão do mantenedor para o projeto irmão, 2026-10-19):
`Documentos\DC-Recomp\Head Hunter\saves` (FOLDERID_Documents), com `--saves-pasta` e modo portátil.
Se o HH11.0 mostrar algo que torne esse precedente inadequado, o assunto vira `DECISAO_NECESSARIA`.

## Riscos

- Versão da `bu` diferente da do BS: nada do `bu_adapter` do BS vale sem o contrato do HH11.0.
- Sem caminho legado que salve: o oráculo é o contrato do SDK e o ciclo save → load, não um traço
  legado (como no N4).
- A passagem real do disco 1 para o 2 só acontece na campanha (HH16). O HH11 prova o formato e o
  carregamento com os dados unificados.
