# Head Hunter Native PC: fronteiras e plano de migração

Plano do port nativo (HH07–HH15), equivalente ao `docs/PLANO-PORT-NATIVO.md` do Blue Stinger
(MIT), refeito para o Head Hunter com fatos medidos no HH07
(`docs/relatorios/HH07-RELATORIO.md`). A regra permanente continua: **recompilar, nunca
emular** (`AGENTS.md`). Nenhum interpretador de SH-4/ARM7 entra no executável; o runtime
híbrido é temporário e encolhe até sair no HH15.

## 1. Estado medido (HH07, 2026-10-25)

Medido com `hh-hwscan` (disco 1; o disco 2 dá saídas idênticas) e a sonda do roteiro
`newgame_area1`. Valores com catraca ficam em `config/metricas_base.toml`.

| Métrica | Valor |
|---|---|
| Funções | 4 699: jogo 3 032, SDK 1 515, camada desconhecida 151, conflito 1 |
| Referências a hardware | 623 (218 acessos, 405 constantes); no código do jogo: 8 estáticas, todas em `0x8C090974` |
| Fronteira declarada (`config/fronteiras.toml`) | 286 entradas (285 diretas, 1 indireta), todas com provedor `legado` |
| Fechamento | 0 violações de SDK e 0 de hardware; 672 alvos indiretos não resolvidos (catraca) |
| Dependências de hardware declaradas do jogo | 2: `0x8C090974` e `0x8C0C4CE0`, o caminho de vértices por store queue para buffers na RAM (dono HH14) |
| Código gerado | 13 arquivos, 1 098 862 linhas, 22 690 `HH_CALL`, 1 107 despachos por endereço |
| Runtime: consumidores funcionais do contador de ciclos | 2 (`sh4_hw_m3.cpp`, `sh4_service.cpp`) |

Bibliotecas embutidas: Shinobi 1.68, Kamui2, CRI ADX, Audio64 10.0.f.2 e 4X Movie (sem
banner). Ninja não está presente.

## 2. Arquitetura-alvo

```
native/         sistemas definitivos (interfaces no HH07; implementações HH08-HH14)
  core/         Handle<Tag>, Result<T>
  platform/     arquivos (HH08), tempo e eventos (HH09), controles (HH10), saves (HH11)
  video/        filmes nativos (HH12)
  audio/        motor de som que substitui o AUDIO64.DRV (HH13)
  render/       IR de comandos e dispositivo Vulkan (HH14)
transitional/   handles com geração, ABI guest, registro de fronteiras (some no HH15)
legacy/         nome do runtime híbrido (runtime/): sai subsistema a subsistema
generated/      (privado) funções ainda não elevadas
```

Regras verificadas por teste:
- `native/` não cita `Sh4Context`, `sh4_`, `runtime/`, `dcmap` nem endereços do guest
  (`hh07_native_limpo`, com caso negativo `hh07_native_limpo_negativo`). Os cabeçalhos
  compilam sozinhos (`hh07_native_api`).
- O `HeadHunter.exe` não liga a leitura de CDI (`hh07_produto_sem_cdi`, sobre o mapa do
  linker, com caso negativo). A preparação dos dados a partir do CDI ficou em
  `tools/hh-extract/src/runtime_data_cdi.cpp`, usado só pelo Builder e pelo `hh-extract`.
- O executor de passo único, o tradutor e o stub sem hardware não entram no produto
  (`hh_assert_product_clean`, HH04/HH06).

### Fronteiras em três camadas

1. **Interface nativa tipada** (`native/`): sem endereços guest nem `Sh4Context`.
2. **Adaptador temporário** `Sh4Context ↔ interface` (`transitional/guest_abi.h`):
   - argumentos em r4-r7 e depois na pilha; floats em fr4-fr11; retorno em r0/fr0;
   - `GuestMemory` só acessa a RAM principal e recusa hardware, P4, store queues e VRAM.
3. **Provedor selecionável** (`transitional/boundary_registry.h`), legado ou nativo por
   entrada da fronteira, pelo mecanismo dos mods (`runtime/mods`):
   - as fronteiras migráveis ficam em `[native] slots` e são chamadas sempre pelo slot;
   - `HH_MODS=ON` coloca slot em todas as funções.

Custo medido no HH07.6: ver o relatório. Os hashes do roteiro são iguais com e sem slots.

### Interoperabilidade da fase híbrida

- Handles de 32 bits com geração (`transitional/handle_table.h`):
  - ponteiros do host nunca vão para a RAM guest;
  - liberar invalida o handle antigo.
- Estruturas que cruzam a fronteira têm layout declarado e versão (`GuestStruct`).
- O nativo chama o guest pelo despacho. A ordem segue a seção 4.

## 3. Identificação e prova de fechamento

- **Identificação de função**: duas evidências independentes (strings/tabelas, assinatura
  mascarada, grafo, ABI, teste de contrato). O `sdkid` registra as evidências em
  `analysis/hh07/disc1/sdk_funcs.tsv`. A semente do lado do jogo vem de `[sdkid] jogo`, com
  os nomes dos fontes do jogo nas mensagens de erro.
  - Um nome só é atribuído com três condições: a mensagem aponta um único nome; nenhuma outra
    função aponta o mesmo nome; e outra evidência (módulo, faixa, assinatura) confirma a
    família. Hoje são 13 funções nomeadas, todas do CRI.
  - "X() failed" é o chamador relatando a falha de X, e "(CONSTANTE)" não nomeia função.
  - Antes dessa correção (HH07.2), a mensagem "kmCreateVertexBuffer() failed!" semeava Kamui
    numa função do jogo. Por contiguidade, isso arrastava 138 funções do jogo para o SDK
    (texturas, fontes, overlays de depuração; entre 0x8C02B39E e 0x8C1173DA) e deixava outras
    465 sem camada. A lista das mudanças, conferida por amostra de strings, fica em
    `analysis/hh07/mudancas_camada.tsv`.
- **Manifesto de hardware com dominância**: `hw_manifest.tsv` e `hw_dominancia.tsv`.
- **Fechamento** (`hh-hwscan --fronteira`, teste `hh07_fechamento`):
  - parte das funções do jogo e anda pelas chamadas diretas e pelas arestas observadas pela
    sonda;
  - violação é uma função do SDK fora da fronteira ou um acesso a hardware alcançado;
  - acessos do próprio jogo ao hardware só são aceitos como `[[dependencia_hw]]`, com
    subsistema, dono (etapa `HHxx`) e evidência. Sem declaração, é violação;
  - declaração não confirmada pela sonda é "obsoleta" e também falha;
  - a contagem tem catraca (`fechamento.dependencias_hw_declaradas`).
- **Catraca** (`config/metricas_base.toml`): chaves de dependência não podem subir. A base
  só é regravada quando um número cai, ou quando uma etapa registra a causa da subida no
  relatório.

### Dependências de hardware do jogo (aceitas, com dono)

`0x8C090974` (camada jogo, `bloco:jogo`, no meio do código de texturas e intermissões do
jogo, 0x8C08ED64–0x8C0923F0):
- escolhe um buffer na tabela 0x8C20CD20 (índice em 0x8C20CD28);
- grava em `0x8C240048` o ponteiro `(end & 0x03FFFFFF) | 0xE0000000`;
- escreve QACR0/QACR1 (0xFF000038/0xFF00003C) com os bits altos do endereço;
- faz `pref` (0x8C090A20).

A sonda viu 668 344 escritas em cada QACR e flushes para a RAM 0x0C400000–0x0C610000. A função
não tem chamador direto: é chamada por ponteiro.

`0x8C0C4CE0` (camada jogo, evidência `bloco:jogo`; está entre funções do jogo em
0x8C0C3FA4–0x8C0D1134):
- copia blocos de 32 bytes (`0x8C15982A`, SDK) para o ponteiro global `0x8C240048`;
- faz `pref` no ponteiro (0x8C0C4EBC/0x8C0C4ED4/0x8C0C4F34/0x8C0C4F4C) e o avança 32 bytes.

É uma escrita direta pelas store queues no buffer preparado por `0x8C090974`, que usa o mesmo
global. No roteiro, a sonda viu os flushes irem para a
RAM, em 0x0C5C0000/0x0C5D0000: é um buffer em RAM, não o TA. A hipótese é um buffer de
vértices do Kamui2 enviado depois ao TA, a confirmar no HH14. Dono: **HH14**, que captura
esse caminho na IR de render.

Outras 8 funções do jogo têm o mesmo ponteiro no pool de literais (`0x8C03F256`,
`0x8C048178`, `0x8C095BA8`, `0x8C0C3FA4`, `0x8C0C503C`, `0x8C0C57EC`, `0x8C0C5D5A`,
`0x8C0D0C12`), e há 30 funções do jogo com a base das store queues em literal
(`sq.literal.funcoes.jogo`). A sonda não as viu acessar hardware no roteiro atual. Elas entram na matriz de
cobertura do HH14 e são confirmadas ou descartadas lá.

## 4. Threads, reentradas e eventos (protocolo antes do HH09)

Inventário do roteiro `newgame_area1` (4 700 VBlanks, `tests/diff/hh07_ordem.py`): duas
execuções dão o mesmo trace (sha256 `c2aa0b23…`).

| Evento | Contagem | Observação |
|---|---|---|
| VBlank (`V`) | 4 700 | um por VBlank |
| Interrupção (`I`) | 39 625 | INTEVT 0x320 (34 980) e 0x360 (4 645): IRL do Holly, níveis 6 e 4 |
| Thread nova (`N`) | 8 | ids 1-8, criadas uma vez cada |
| Troca de thread (`T`) | 74 320 | sempre principal ↔ thread n; 4 645 idas e voltas por thread |
| `longjmp` (`J`) | 74 312 | alvos 0x8C1838E0 (para as 8 threads) e 0x8C18392A (volta à principal) |
| Continuação após `rte` (`R`) | 0 | o jogo não retoma fibra por `rte` neste roteiro |

O escalonador cooperativo usa `setjmp` 0x8C131F3C / `longjmp` 0x8C131E9C (`[threads]`). A
ordem `(quadro, thread, interrupção)` é determinística. O HH09 troca as fontes de evento
mantendo essa ordem, e o trace aceito serve de referência.

## 5. Sequência

| Etapa | Subsistema | Fronteira | Observação |
|---|---|---|---|
| HH08 | arquivos, troca de disco | gdFs* (Shinobi), leitura do GD do jogo | `[native] slots` das entradas de arquivo |
| HH09 | tempo, eventos, tarefas | sy* (Tmr, Rtc), VBlank, IRQ do Holly | ordem da seção 4 |
| HH10 | controles | pd* (Shinobi), Maple | — |
| HH11 | saves | bu* (Shinobi), VMU | continuidade entre discos |
| HH12 | vídeo | 4X Movie | FFmpeg só como processo externo (A11) |
| HH13 | áudio | Audio64 (fila de comandos, AUDIO64.DRV) | o ARM7 nunca é executado |
| HH14 | render | Kamui2, store queues, `0x8C090974`/`0x8C0C4CE0` | IR + Vulkan |
| HH15 | fim do tempo guest e do legado | — | `legacy/` e `transitional/` saem |

Cada etapa: provedor nativo atrás de slot; equivalência por contrato contra o legado; casos
negativos; a catraca não piora. O nativo só vira padrão depois de provado.

## 6. Riscos

- Os 672 alvos indiretos não resolvidos são funções do SDK com endereço só em dado. Elas
  dependem da sonda e de análise de points-to; a catraca impede crescimento.
- 151 funções de camada desconhecida, em faixas sem banner nem mensagem própria:
  0x8C00F400–0x8C019BAC (107, o início do executável) e 0x8C11E30C–0x8C120610 (43).
- A camada de 3 032 funções do jogo depende de `bloco:jogo`, ou seja, de contiguidade com
  funções de evidência forte. É uma evidência só. Uma classificação errada muda a fronteira,
  então cada etapa confirma, com duas evidências, as funções que tocar.
- O custo dos slots foi medido com o render por software do runtime híbrido, que domina o
  tempo. A medida é refeita depois do HH14 (como no Blue Stinger).
