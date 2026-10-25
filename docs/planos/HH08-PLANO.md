# HH08 — Plano detalhado: arquivos nativos e troca dos dois discos sem CDI em runtime

Validação mínima: **V2**. Critério de saída (ROADMAP): as rotas dos dois discos funcionam sem
acesso a setores nem à imagem; disco errado ou corrompido falha com mensagem segura.

Equivalente ao N1 do Blue Stinger (`docs/N1-PLANO.md` e `N1-RELATORIO.md` dele, MIT), mais a
troca de disco, que o BS não tem: ele é de um disco só. Regra permanente: recompilar, nunca
emular. O provedor nativo substitui a API de arquivos do SDK e não simula mais GD-ROM.

## Fatos de entrada (preparação, 2026-10-25; `analysis/hh07/disc1/*`)

### Biblioteca de arquivos

- `gdFs Ver 1.09` (o BS usava a 1.05). Os dados do módulo ficam em 0x8C185DA4–0x8C185E61 e o
  código na faixa Shinobi 0x8C15AEA0–0x8C15E55C.
- **Candidatos com duas evidências**: mensagem `a64File*` logo após a chamada, mais
  layout/padrão igual ao do BS.

  | Endereço | Função |
  |---|---|
  | 0x8C15D09A | gdFsOpen |
  | 0x8C15C7B8 | gdFsClose |
  | 0x8C15C7F0 | gdFsGetFileSize |
  | 0x8C15C8E2 | gdFsRead |
  | 0x8C15CD68 | gdFsGetStat |

- **Hipóteses** (só layout ou padrão), a confirmar por trace de argumentos e retorno no HH08.1:

  | Endereço | Função provável |
  |---|---|
  | 0x8C15C7A0 | ChangeDir |
  | 0x8C15C83E | Seek |
  | 0x8C15CA48 | ReqRd32 |
  | 0x8C15C80A | GetFileSctSize |
  | 0x8C15CD80 | GetTransStat |
  | 0x8C15CD96 | Stop |
  | 0x8C15C794, 0x8C15D042 | diretório |
  | 0x8C15D06E, 0x8C15CFF8, 0x8C15D00E, 0x8C15D0F6, 0x8C15D122, 0x8C15D150, 0x8C15D168, 0x8C15B9F2 | desconhecidas |

### Camada de arquivos do jogo (grafo)

| Função | Papel | Chamadores |
|---|---|---|
| 0x8C0A1FF8 | open | 42 |
| 0x8C0A22EC | close | 51 |
| 0x8C0A2388 | read | 632 |
| 0x8C0A25E4 | leitura assíncrona | 12 |
| 0x8C0A27E4 | tamanho | 5 |

- Contêiner `AHFF` em 0x8C019148/0x8C0229C4/0x8C04E7DE.
- Também chamam gdFs diretamente: Audio64 (`a64File*` em 0x8C11FFFE…0x8C120444 e
  0x8C124xxx), CRI ADXF (0x8C161xxx) e LSC (0x8C1760EC).

### Identidade do disco

- `DISC\disk%d.id` em 0x8C09EBC4: sprintf, depois open/close na camada do jogo. O único
  chamador é 0x8C029F40.
- 0x8C04E754 (7 chamadores) chama GetStat e 0x8C15D0F6/D150/D168; parece checagem do
  drive.
- Opções do menu de depuração "CHECKDISC:" e "CHECKDISC REINIT:" em 0x8C051704.
- O `1ST_READ.BIN` difere entre os discos só no u32 do offset 0x00175E28 (RAM 0x8C185E28, dentro
  dos dados do gdFs): 230861 (disco 1) e 251635 (disco 2), igual a `iso_root_lba + 143` (HH01).

### Runtime atual

- Sem troca de disco: `gdrom_set_disc` uma vez (`app/hh_host.cpp:98`).
- `GET_DRV_STAT` sempre em pausa (`runtime/gdrom/gdrom.cpp`).
- O Builder do HH06 só prepara o disco 1.
- Correção de fato antigo: 0x8C16E2E8, que o HH05 chamou "driver de GD do jogo", está no
  bloco do SDK.

## Subetapas

| ID | Entrega | Evidência de saída |
|---|---|---|
| HH08.1 | Trace de chamadas gdFs (`HH_CALL_TRACE`: argumentos/retornos/r15) no roteiro `newgame_area1` e na troca de disco | Contratos e nomes das hipóteses confirmados com duas evidências; tabela de offsets da struct GDFS 1.09 |
| HH08.2 | `native/platform/data_files` (portado do N1 do BS): índice de `data\disc\`, nomes lógicos sem `;1`, sem LBA/FAD; mods em `mods\assets\` de qualquer tamanho | Testes unitários (`canonical_path`, abrir/ler/listar, mods válidos/ilegíveis/LEIA-ME) |
| HH08.3 | `transitional/gdfs_adapter` + `[native] slots` das entradas gdFs; `--arquivos legado|nativo`; ReqRd32 assíncrono pelo servidor gdFs (achado ALTO do Codex no BS) | Contratos do adaptador; seleção de provedor; sem slot = erro claro |
| HH08.4 | Portão P1: mesma sequência global `(arquivo, offset, SHA-256)` no legado (log do GD-ROM HLE) e no nativo (log do adaptador); aliases agrupados por conteúdo | Sequência idêntica no roteiro; 0 setores de dados de arquivo pelo GD-ROM HLE no modo nativo (P2) |
| HH08.5 | Dois discos: Builder prepara `data\disc1` e `data\disc2` (cada um conferido contra o seu CDI); estado lógico de disco no nativo (bandeja/troca); `DISKn.ID` e o dword por disco tratados pelo provedor | Roteiro até a troca: o prompt do jogo aparece, a troca é aceita com o disco certo |
| HH08.6 | Disco errado ou corrompido | Casos negativos: disco 1 no lugar do 2, arquivo alterado e dados ausentes falham com mensagem segura, sem travar nem ler setor |
| HH08.7 | Produto sem caminho de setores para arquivos de jogo (o boot e o init do gdFs ficam no legado até o HH09/HH15, como no BS) | Teste de fechamento (catraca: `fronteira.provedor.legado` cai); mapa do linker sem CDI |
| HH08.8 | Relatório + revisão Codex + commit | — |

## Questões abertas (verificação dinâmica no HH08.1)

1. Nomes das hipóteses (ChangeDir, Seek, ReqRd32, Stop, diretórios, 0x8C15D0F6/D150/D168/D06E).
2. Quem lê 0x8C185E28 (o dword por disco) e se ele decide a checagem do disco ou o FAD raiz.
3. O que 0x8C04E754 faz e qual estado do drive espera durante a troca (bandeja aberta,
   depois `DISKn.ID`).
4. Quando 0x8C09EBC4 roda e com qual N; se compara o conteúdo do `DISKn.ID` ou só a existência.
5. Quais caminhos assíncronos (Audio64, ADXF, LSC, 4XM) precisam de slot.
6. Se o jogo lê campos da struct GDFS diretamente.
7. Se há aliases (mesma LBA) nos discos do HH, que quebrariam um log por LBA.
8. Roteiro determinístico até a troca de disco: o primeiro passo é medir a que VBlank do
   disco 1 o jogo pede o disco 2. Se a campanha até lá for longa demais para um roteiro, usar
   o menu de depuração "CHECKDISC" (0x8C051704) como atalho, com evidência.

## Regras

- O nativo só vira padrão depois do P1/P2. `--arquivos legado` continua disponível.
- Nada de ler o CDI no executável do jogo (`hh07_produto_sem_cdi` continua valendo).
- O formato `data\disc1`/`data\disc2` afeta a instalação. Ele fica coberto pela arquitetura
  (dados por disco preparados pelo Builder) e não é decisão nova; a migração das instalações
  do HH06 é refazer a instalação com o Builder.
