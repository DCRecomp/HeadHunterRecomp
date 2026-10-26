# HH11 — RELATÓRIO: Saves nativos e continuidade entre discos

Data: 2026-10-25. Validação mínima: **V2**. Plano: `docs/planos/HH11-PLANO.md`. Equivalente ao N4 do
Blue Stinger, adaptado à biblioteca de backup `bu Ver 1.47` do Head Hunter.

## 1. Objetivo e escopo realizado

Critério de saída (ROADMAP): saves sobrevivem a reinstalação/upgrade; casos de falha não destroem o último
save válido.

| ID | Entrega | Resultado |
|---|---|---|
| HH11.0 | Contrato da `bu` 1.47 | 14 entradas chamadas pelo jogo identificadas com duas evidências (pareamento com a `bu` do BS + corpo dos alvos/ABI do chamador). Oráculo do jogo original no Flycast com VMU (c1, c2, c3): protocolo, códigos, leiaute do VMS, `file_info` e a data |
| HH11.1 | `native/platform/saves` | Store portado do N4: envelope versionado `HHSV` com CRC-32, escrita atômica (temporário + releitura + troca com `.bak`), recuperação na abertura, falhas injetáveis; VMS com o leiaute do HH |
| HH11.2 | `transitional/bu_adapter` + `bu_machine` | 14 slots (API pública, servidor periódico e `last_error`); `--saves legado\|nativo` em `hh-run` e `HeadHunter.exe` |
| HH11.3 | Cartão presente no nativo | O jogo vê um cartão de 200 blocos sem Maple de armazenamento; o roteiro chega à primeira área com o cartão (captura conferida) |
| HH11.4 | Formato do Dreamcast | `--saves-exportar <pasta>` / `--saves-importar <arquivo.vms>`: o arquivo do save é o arquivo da VMU, sem transformação; o arquivo real gravado pelo jogo no Flycast é importado; VMS inválido é recusado |
| HH11.5 | Continuidade entre discos | Instalação única (HH08): um store, o mesmo executável e a mesma `bu` nos dados dos dois discos. A passagem real do disco 1 para o 2 com save fica para a campanha (HH16), como decidido pelo mantenedor |
| HH11.6 | `pdLcd` | Não chamado na rota medida; fica no código original (sem LCD no produto) |
| Produto | Saves nativos na janela | Padrão `Documentos\DC-Recomp\Head Hunter\saves`; `--saves-pasta`, `--saves-portatil`; a reinstalação mantém `saves\` e `controles.toml` |

## 2. Arquivos alterados

- Novos (portados do BS, MIT, com cabeçalho de origem):
  - `native/platform/saves_format.cpp`, `native/platform/saves_store.cpp` (e `saves.h` substituído);
  - `transitional/bu_machine.{h,cpp}`, `transitional/bu_adapter.{h,cpp}`;
  - `tests/unit/test_hh11_saves.cpp`, `tests/unit/test_hh11_bu_machine.cpp`;
  - `tools/dev/hh11_vmu.py` (de `n4_vmu.py`).
- Novo (HH): `tests/diff/hh11_saves.py`.
- Alterados:
  - `app/hh_host.{h,cpp}` (`SavesProvider`, store, mutações `HH_BU_MUTACAO`, contadores no fechamento);
  - `app/hh_run.cpp` (`--saves`, `--saves-pasta`, cartão temporário);
  - `app/hh_window.cpp` (saves nativos, pasta Documentos, portátil, exportar/importar), `app/CMakeLists.txt`;
  - `builder/builder_main.cpp` (dados do usuário na reinstalação);
  - `transitional/pad_adapter.cpp` (fechamento com as funções da `bu`);
  - `native/CMakeLists.txt`, `transitional/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/hh07/native_api_check.cpp`;
  - `config/headhunter.toml` (14 slots), `config/fronteiras.toml` (13 entradas nativas, subsistema `saves`),
    `config/metricas_base.toml`;
  - `tools/dev/flycast_oracle.py` (`bu-trace` com os endereços do HH, `--status`, despejo de `file_info`),
    `tools/dev/regressao.py` (`portoes_hh11`, reinstalação no `builder`);
  - `docs/planos/HH11-PLANO.md`.

## 3. Evidências e decisões

### 3.1 Contrato da `bu` 1.47 (`1ST_READ.BIN` SHA-256 `aa585325…`)

A `bu` do HH é a do BS deslocada de 0xD3D58 (`buInit` 0x8C08BDEC → 0x8C15FB44), recompilada com outra
ordem de instruções. Pareamento por multiconjunto de instruções mascaradas (os imediatos mantidos) e, nos
ambíguos, pelo corpo dos alvos dos `jmp`:

| HH | Papel | Evidência |
|---|---|---|
| 0x8C15FB44 / 0x8C15FC2A | init / exit | 0.56 e 0.62; lado a lado no disassembly |
| 0x8C15FDCC / 0x8C15FDD2 | registro dos callbacks | 1.00 |
| 0x8C160754 | mount | 1.00 |
| 0x8C15FF54 / 0x8C16037C | disk_info / exists | wrappers idênticos; alvos 0x8C1721CA / 0x8C172EEE batem com os do BS (0.69 e 0.86 contra 0.14) |
| 0x8C1603AA | file_info | alvo 0x8C171CD8 igual instrução a instrução ao do BS (0x8C091494); o jogo passa 3 argumentos |
| 0x8C1603DC | status | 1.00 |
| 0x8C160558 / 0x8C160614 | save (op 0x0C) / load (op 0x0E) | 1.00 |
| 0x8C160500 | op 0x0B | 0.98; o HH o chama (o BS não) |
| 0x8C160774 | last_error | `bra 0x8C15FE66`, e 0x8C15FE66 é o `last_error` do BS (0x8C08C112) linha a linha |
| 0x8C160B30 | CRC-16-CCITT (0x1021) | estrutura do CRC VMS do BS; rotina pura, fica recompilada |

Diferenças do HH no init/exit:
- o servidor periódico 0x8C15FC9C é registrado por 0x8C16F7CA(`*0x8C186498` = 0x14, servidor) numa tabela de 32
  callbacks da camada `per`, não no agendador do jogo;
- o callback do 4º argumento é chamado sem argumentos;
- flag de init 0x8C188D20.

### 3.2 Oráculo do jogo original (Flycast com VMU)

`tools/dev/flycast_oracle.py bu-trace` com os endereços do HH, VMU vazia criada por `hh11_vmu.py nova`, e o
mesmo roteiro de START/A no título. Resultados em `analysis/hh11-oraculo/` (privado).

| Captura | VMU | Resultado |
|---|---|---|
| c0 | vazia (`9db88f82…`), sem entrada | init(0x7A, 0xFF, 0, 0x8C117572); inserção op 0x12: progresso 1, 2; conclusão 15 |
| c1 | vazia | 8 mounts (op 1, progresso 1..0x10 de 0x11), 5 `exists` = FF05, 2 SAVEs de `H_HUNTER.SYS` (6 blocos, op 0x0C, progresso 1..14 de 14, conclusão 0); VMU de saída `5f1da5c8…` |
| c2 | a do c1 | `exists(H_HUNTER.SYS)` = 0; op 0x0B lê o arquivo inteiro em 0x8C2F43B0 (progresso 1..6 de 6, conclusão 0); `file_info` = 0; SAVE; 23 mounts, 20 `exists` (8 nomes `H_HUNTER.001..008` = FF05) |
| c3 | a do c2 | sequência de chamadas igual à do c2 (só o rótulo de 0x8C160774 mudou) e o mesmo arquivo gravado (SHA-256 `a8c3267a…` em c1, c2 e c3); as imagens inteiras das VMUs diferem (data no diretório); despejo do `file_info` |
| c4 | a do c1 | despejo do destino do op 0x0B na conclusão: os 3 072 primeiros bytes são exatamente o `H_HUNTER.SYS` da VMU (revisão Codex: o instrumento agora prova o "arquivo inteiro no destino") |

Sem a parada no `status`: o jogo o consulta num laço apertado, e com uma parada por consulta o Flycast não
anda (primeira tentativa do c1: 2 767 consultas em 10 min, sem progresso). Por isso `--status` ficou opcional.

### 3.3 O que o oráculo fixou e o nativo reproduz

- **VMS do HH:** o campo 0x48 é o total coberto pelo CRC (cabeçalho 128 + 1 ícone 512 + eyecatch tipo 3
  2048 + 60 de dados = 2748). O resto do arquivo é o que estava no buffer do jogo. O `vms_check` do BS recusava
  essa imagem ("dados além do fim"); agora aceita os dois leiautes, decididos pelo CRC (unitário
  `saves_vms_headhunter_layout`, com negativos).
- **op 0x0B:** leitura do arquivo inteiro no destino. Estruturalmente é o pedido do SAVE sem os argumentos de
  tempo; o oráculo mostra a leitura e a conclusão 0. O jogo confere o CRC do que leu.
- **file_info (18 bytes):** +0 tamanho em bytes, +4 blocos, +6 arquivo de jogo (tipo 0xCC), +7 proteção de
  cópia, +8 deslocamento do cabeçalho, +0xA a data. Confirmado nas gravações de 0x8C171D96..0x8C171DB6 e no
  despejo do c3.
- **Data do arquivo:** o 1º argumento de pilha do SAVE aponta para a data do relógio do jogo (ano u16, mês, dia,
  hora, minuto, segundo, dia da semana); com `--rtc 1700000000`, 2023-11-14 22:13:43, terça. O store grava essa
  data, convertida sem fuso (volta exata). O `file_info` a devolve. Assim não depende do relógio do host, e o
  nativo fica determinístico.

### 3.4 Portões (`tests/diff/hh11_saves.py`)

| # | Critério | Resultado |
|---|---|---|
| S0 | Estático: chamadas do jogo à `bu` só pelo slot | 17 por slot, 0 diretas, nenhuma sem slot |
| S1 | Sessão 1 (cartão vazio) = c1 | chamadas iguais (mount 8, exists 5, save 2); SAVE ok; nada sem suporte; **imagem igual à da VMU do oráculo nos 2 748 bytes cobertos pelo CRC**; data = relógio do jogo (1700000048) |
| S2 | Sessão 2 (cartão da sessão 1) = c2 | chamadas iguais (mount 23, exists 20, file_info 1, leitura 1, save 1); LOAD e SAVE ok; nada sem suporte; o que o LOAD deixou na RAM do jogo (relido do guest) = o arquivo do cartão (FNV-1a `9afbacaf95653fcc`, 3 072 bytes) |
| S3 | Determinismo | sessão 2 repetida: quadros e arquivo idênticos |
| S4 | Reinstalação | `saves\` e `controles.toml` passam para a nova instalação (regressão, passo `builder`); a pasta padrão fica fora da instalação |
| S5 | Fechamento | no nativo, 0 entradas nas 19 funções da `bu` geradas (API, servidor e internas); no legado, o init original roda |
| S6 | Legado inalterado | `--saves legado` é o padrão do `hh-run` e do `--headless`: os portões anteriores e o roteiro de referência continuam valendo |

Negativos acusados (rc 0): `sem_data` → S1 (data), `save_corrompe` → S1 (imagem), `load_falha` → S2 (chamadas),
`load_altera` → S2 (bytes entregues, com as contagens iguais).

Falhas seguras (S3 do plano) pelos unitários do store: escrita interrompida em cada ponto mantém o save
anterior; destino inválido volta pelo `.bak`; os dois inválidos viram `.corrompido` (nunca apagados);
somente leitura; versão futura intocada; nomes inválidos; VMS com CRC errado recusado.

### 3.5 Captura

Com o cartão (sessão 2), o roteiro chega à primeira área controlável, com Jack Wade e o HUD no VBlank 4200
(`analysis/hh11/cap/`, privado).

## 4. Comandos de build/teste e resultados

```text
cmake --build build --config Release
build/tests/Release/hh11_saves_tests.exe                       23 testes, 0 falhas
python -I tools/dev/hh11_vmu.py teste                          autoteste: ok
python -I tools/dev/flycast_oracle.py bu-trace analysis/hh11-oraculo/cN.tsv --vmu ... --vmu-saida ... --dump ...
        --roteiro analysis/hh11-oraculo/c1_roteiro.txt --passos 2600
python -I tools/dev/hh11_vmu.py valida analysis/hh11-oraculo/c1/vmu/H_HUNTER.SYS --headhunter    VALIDO
python -I tests/diff/hh11_saves.py build/app/Release/hh-run.exe . generated/disc1   (HH_DATA_DIR)   0 falhas
hh-hwscan ... --fronteira config/fronteiras.toml --metricas-base config/metricas_base.toml        rc 0
HeadHunter.exe --saves-pasta <p> --saves-exportar <d> / --saves-importar <arquivo.vms>
```

### 4.1 Portão HH11

```text
[ ok ] S0 estatico: ... 17 por slot, 0 diretas, sem slot: []
[ ok ] oraculo do Flycast presente (c1, c2, VMS do c1)
[ ok ] S1: sessao 1 termina pelo limite com o provedor nativo
[ ok ] S1: chamadas do jogo iguais ao oraculo c1 (cartao vazio) ({'mount': 8, 'exists': 5, 'file_info': 0, 'save': 2, 'load': 0})
[ ok ] S1: SAVE ok e nada sem suporte
[ ok ] S1: imagem gravada = VMU do oraculo no trecho coberto pelo CRC (3072 bytes, coberto 2748)
[ ok ] S1: data do arquivo = relogio do jogo (t 1700000048, faixa [1700000000, 1700000139])
[ ok ] S2: chamadas do jogo iguais ao oraculo c2 ({'mount': 23, 'exists': 20, 'file_info': 1, 'save': 1, 'load': 1})
[ ok ] S2: LOAD do arquivo inteiro ok, SAVE ok e nada sem suporte
[ ok ] S2: o LOAD entregou ao jogo exatamente o arquivo do cartao (RAM 9afbacaf95653fcc (3072 bytes), arquivo 9afbacaf95653fcc)
[ ok ] S3: sessao 2 deterministica (quadros e arquivo)
[ ok ] S5: fechamento no nativo (funcoes da bu geradas nao rodam) (todas 0)
[ ok ] S5: o legado roda o init original (referencia do fechamento) (f_8C15FB44 1)
[ ok ] negativo sem_data / save_corrompe / load_altera / load_falha
0 falhas   (rodada final, depois das correcoes da revisao)
```

### 4.2 Regressão integral

Executada no codigo final, com as correcoes da revisao (log em `analysis/hh11/regressao_final.log`):

```text
[ok] build (8 s)
[ok] ctest (13 s)
[ok] portoes_hh05 (8 s)
[ok] portoes_hh07 (432 s)
[ok] portoes_hh08 (383 s)
[ok] portoes_hh09 (209 s)
[ok] portoes_hh10 (519 s)
[ok] portoes_hh11 (446 s)   hh11_saves_tests + hh11_saves (S0-S5 e negativos, contra o oraculo c1/c2)
[ok] roteiro_area1 (137 s)  hashes iguais a snapshots/hh06/area1_hashes.txt (saves legado no hh-run)
[ok] builder (210 s)        reinstalacao com saves\ e controles.toml e copia antiga com save pendente; HeadHunter.exe reproduz a referencia
regressao completa - exit=0
```

Uma rodada anterior parou no portoes_hh11: a chave do cache da regressao tratava HH11_ORACULO (uma pasta) como
arquivo. Corrigido (`dir_stat` para pastas).

## 5. Comparação com referência/oráculo

O oráculo é o jogo original no Flycast com VMU. O nativo repete as chamadas do jogo e os resultados das duas
situações medidas (cartão vazio e cartão com o arquivo). Grava o mesmo arquivo nos bytes cobertos pelo CRC, e
o arquivo gravado pelo Flycast é importado e lido pelo nativo. O tempo é abstraído: cada operação conclui num
número fixo de passadas do servidor. Contam a ordem, os progressos e os resultados.

## 6. Riscos, limitações e trabalho deixado para depois

- **Cobertura:** só o arquivo de opções (`H_HUNTER.SYS`) foi gravado e lido nas rotas medidas. Os saves de
  partida (`H_HUNTER.001..008`, "SAVED GAME") só aparecem como `exists` = FF05. O SAVE/LOAD de partida em
  gameplay exige uma rota longa até um ponto de save: fica para o HH16 (campanha). O protocolo (op 0x0C/0x0E/0x0B)
  é o mesmo; o tamanho e o conteúdo serão conferidos lá.
- **Passagem real do disco 1 para o 2 com save:** HH16 (decisão do mantenedor: "o teste da troca de disco fica
  para depois").
- Erros do cartão (cheio, não formatado, sem cartão) vêm do N4 do BS e dos unitários. No HH não foram
  capturados no Flycast (a matriz de VMUs preparadas do BS pode ser repetida no HH16).
- `pdLcd` e a vibração não são chamados nas rotas medidas.
- `--headless` usa o legado por padrão (o roteiro de referência foi gravado sem memory card); a janela usa o
  nativo.

## 7. Inventário de dados privados

Ficaram fora do Git:
- `analysis/hh11-oraculo/`: TSV, VMUs, despejos e o VMS do jogo;
- `analysis/hh11/cap/`;
- `generated/`, `game-data/`, `build/`, `dist/`;
- `reviews/codex-hh11.log`, `reviews/hh11-prompt.md`.

Os unitários usam imagens sintéticas. O portão compara com o oráculo privado e apaga a própria pasta
temporária. Versionados só endereços, contagens e tamanhos.

## 8. Checklist do critério de saída

- [x] Store nativo: slots, escrita atômica, integridade, versão do formato (migração versionada pelo envelope).
- [x] Importação/exportação do formato Dreamcast (o arquivo da VMU sem transformação).
- [x] Save/load real do jogo (arquivo de opções) igual ao oráculo; corrupção, falta de espaço e nomes testados.
- [x] Saves sobrevivem à reinstalação (fora da instalação por padrão; portátil carregado pelo Builder).
- [x] Falhas não destroem o último save válido (unitários de falha injetada e recuperação).
- [~] Continuidade entre discos: estrutural (instalação única, um store); a passagem real fica para o HH16.
- [x] Regressão integral e revisão Codex com os achados aceitos corrigidos.

## 9. Revisão independente e próximos passos

### 9.1 Revisão Codex

Saída em `reviews/codex-hh11.log`. O revisor confirmou:
- o init/exit e as gravações em 0x8C313660..0x8C313674 (só lidas pelo init e pelo servidor originais, os dois
  substituídos);
- os 18 bytes do `file_info` (0x8C171D96..0x8C171DB4, data por 0x8C174C16);
- o desvio de `last_error`;
- o portão sem SKIP silencioso;
- nenhum interpretador novo.

| Achado | Decisão | Correção / motivo | Teste |
|---|---|---|---|
| [ALTA] Reinstalação seguinte apagava a cópia de segurança com dados retidos | Aceito | `carryUserData` arquivo por arquivo (move o que falta, descarta o igual, mantém o diferente) também no início, da cópia antiga para a instalação; `removeBackup` recusa apagar cópia com dados do usuário | regressão `builder`: cópia antiga com save pendente volta para a instalação e é removida; `saves\` e `controles.toml` sobrevivem |
| [ALTA] S2 aprovaria LOAD determinístico com bytes errados | Aceito | o adaptador relê do guest o que o LOAD entregou (FNV-1a no fechamento); o portão compara com o arquivo do cartão; mutação `load_altera` (contagens iguais) | `hh11_saves`: S2 bytes entregues + negativo `load_altera` |
| [MEDIA] O instrumento não despejava o destino do op 0x0B | Aceito | `bu-trace` despeja o destino do `op_0b`; captura c4 | c4 (seção 3.2) |
| [MEDIA] Importação aceitava o leiaute do HH de qualquer VMS | Aceito | `vms_layout`; na importação, o leiaute do HH exige a descrição longa `HEADHUNTER`; o padrão continua | `saves_import_requires_headhunter_identity` |
| [BAIXA] Restos do N4/BS na ferramenta e nos testes | Aceito | `hh11_vmu.py` (comandos, `.hhsv`, CRC 0x8C160B30, nomes de teste) e amostras dos unitários | `hh11_vmu.py teste`, `hh11_saves_tests` |

Não verificado pelo revisor e mantido como limitação (seção 6): equivalência visual pós-LOAD com o Flycast (o
portão agora confere os bytes entregues), pasta Windows sem permissão de escrita por ACL (os unitários cobrem
somente leitura e falhas injetadas), `.VMI` (só VMS puro).

### 9.2 Próximos passos

HH11 marcada `CONCLUIDA` e HH12 (vídeos nativos e moddáveis) `EM_ANDAMENTO`.
