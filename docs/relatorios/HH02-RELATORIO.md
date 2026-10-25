# HH02 — Ingestão e extração (`hh-extract` C++20)

Etapa HH02 conforme `ROADMAP.md` (nível de validação V2). Data: 2026-10-24.
Estado: **CONCLUIDA** — extração completa de ambos os discos conferida por
implementação independente (byte a byte) e reextração determinística.

## 1. Objetivo e escopo realizado

Entregáveis da etapa (todos cumpridos):

1. `hh-extract --info` somente leitura — feito (§4);
2. suporte ao CDI observado (v3/v3.5), múltiplas sessões/faixas e arquivos
   que cruzam sessões (leitura por passada de setor com troca de faixa) —
   feito (§3);
3. extração segura, reprodutível e resistente a path traversal — feita (§3);
4. hash/manifesto de todos os arquivos (sha256+crc32 em streaming), identificação
   dos dois discos por `known_versions` e detecção de rip divergente
   (porta de `--force`) — feitos (§4/§5);
5. saída de runtime separada, sem CDI em execução: a árvore extraída +
   `manifest.json` são o dado consumido pelo runtime nativo (§4) — feito;
6. testes sintéticos (CDI fabricado) e reais opcionais via execução CLI —
   feitos (§6).

O modo `--runtime` do executável de referência (verificação setor a setor de
dados de execução) não é necessário nesta etapa: a conferência equivalente
foi feita externamente pelo dif byte-a-byte da árvore completa (§4). Será
relevado quando o runtime nativo consumir setores de sistema (HH05+).

## 2. Arquivos alterados (caminhos versionáveis)

Novos:

- `tools/hh-extract/CMakeLists.txt`;
- `tools/hh-extract/include/hh_extract/{hash,scramble,cdi,iso9660,ip_bin,
  known_versions}.hpp` e `extract.h`;
- `tools/hh-extract/src/{hash,scramble,cdi,iso9660,ip_bin,known_versions,
  extract,main}.cpp`;
- `tests/hh_extract_tests.cpp`;
- `tests/CMakeLists.txt` (novo alvo `hh_extract_tests`);
- `tools/CMakeLists.txt` (inclusão de `hh-extract`);
- `docs/relatorios/HH02-RELATORIO.md` (este).

Atualizados:

- `STATUS.md` — HH02 → `CONCLUIDA`; HH03 → `EM_ANDAMENTO`.

`analysis/hh02_*.{py,cpp,json,txt}` são caminhos ignorados (probes e difs).

## 3. Decisões e arquitetura da ferramenta

- **Zero código copiado**: os módulos foram implementados do zero (MIT do
  projeto) a partir do formato documentado em comentários e da semântica
  validada empiricamente em HH01 por três fontes (leitor linear Python,
  bs-extract e CDI sintético). A referência MIT serviu só de oráculo de
  comportamento; probes de diagnóstico (compilação temporária fora do
  build, em `analysis/`) compararam saídas, não código.
- **Leitor CDI linear determinístico** (`cdi.cpp`): rodapé
  `[u32 versão][u32 ho]`, `header_pos = size - ho` (v3/v3.5), marca de faixa
  de 20 bytes, encadeamento completo de campos (inclui u32 variante
  0x80000000 condicional DiscJuggler-4, condicional 0xFFFFFFFF/skip 78 e
  trilha de fim de sessão), cursor de dados brutos + `file_offset =
  cursor + pregap * sector`, validação `total ≥ pregap+length` e
  `cursor ≤ header_pos`. Sem offsets absolutos por disco.
- **Dados de usuário sempre por setor** (`readUserData`/`readExtent`):nunca
  leitura contínua através de faixas/pregaps; `userOffset()` por tamanho de
  setor (2048→0, 2336→8, 2352→mode1?16:24); o custo é aceitável e evita
  misalignments do formato.
- **ISO9660** (`iso9660.cpp`): PVD em `start_lba+16` da faixa de boot;
  caminhamento recursivo com profundidade ≤16 e diretório ≤8 MB; registros
  não cruzam setores (`rlen==0` encerra o setor; `o+rlen>2048` é erro);
  nomes sanitizados (remove `;versão` e `.final`, rejeita `.`/`..` e
  caracteres fora de alnum+`_.-` — só o disco pode prover nomes, e
  sanitização por componente impossibilita traversal como `..`, separadores
  ou caminhos absolutos). Comparação de caminho case-insensitive. Paths
  compostos sem barra inicial (convenção do manifest de referência).
- **Desembaralhamento** (`scramble.cpp`): especificação do formato
  (blocos 2 MB→32 B, fatias de 32 B, PRNG `seed=(size&0xFFFF)`,
  `seed=(seed*2109+9273)&0x7FFF`, `rnd=(seed+0xC000)&0xFFFF`,
  permuta com `(rnd*i)>>16`); `descramble`/`scramble` são inversos.
- **Boot check** (`known_versions.cpp`): SHA-256 cru; se desconhecido,
  re-testa após descramble; só os dois `hedhunter_v1006_{a,b}` (matriz HH01)
  estão na tabela; desconhecido → `AVISO` e bloqueio sem `--force`
  ( comportamento igual ao da referência).
- **Hash streaming** (`hash.cpp`): SHA-256 único por arquivo e CRC32
  tabela (polinômio 0xEDB88320, complemento final); memória O(1) por arquivo.
- **Manifest** (`extract.cpp`): `IP.BIN` (32 768 bytes) e `1ST_READ.BIN`
  na raiz de saída; árvore ISO sob caminhos relativos; `manifest.json` com
  `tool/source/tracks/ip/volume/boot/files[]` — mesmo esquema do manifest
  de referência para amarrar verificação cruzada.

## 4. Comandos de build/teste e resultados

```powershell
cmake --build build --config Debug     -> exit 0 (alvos hh_extract_core, hh-extract, hh_extract_tests)
& build\tests\Debug\hh_extract_tests.exe -> ok, exit 0 (39 verificações)
& build\tests\Debug\hh_smoke.exe        -> exit 0
cmake --build build --config Release   -> exit 0
& build\tools\hh-extract\Release\hh-extract.exe --info "ISO\HeadHunter (Disc 1).cdi"  -> exit 0
& build\tools\hh-extract\Release\hh-extract.exe --info "ISO\HeadHunter (Disc 2).cdi"  -> exit 0
& build\tools\hh-extract\Release\hh-extract.exe "ISO\HeadHunter (Disc 1).cdi" game-data\hh02\disc1 -> exit 0 (212 arquivos, 678 MB)
& build\tools\hh-extract\Release\hh-extract.exe "ISO\HeadHunter (Disc 2).cdi" game-data\hh02\disc2 -> exit 0 (160 arquivos, 679 MB)
```

`--info` no disco 1 reproduz HH01: faixas (219295/128784 setores de 2336),
IP.BIN MK-5104150 V1.006 20011116 JUE, título correto, boot 1.545.024 bytes
com SHA-256 `aa585325…b640d7` → `hedhunter_v1006_a`.

### Testes sintéticos cobertos (unitários)

- SHA-256: vetores `abc`, vazio, 1M 'a', limites de padding (55/56/64),
  atualização fragmentada — todos contra valores gerados por implementação
  independente (hashlib do Python 3.11) na sessão (§6);
- CRC32: vetor clássico `123456789` → `CBF43926`;
- scramble/descramble: roundtrip para 8 tamanhos (31 B..5 MB), cobrindo
  blocos de 2 MB, múltiplos blocos e sobra < 32 B;
- IP.BIN: campos; bit0=1 census Windows CE; hardware_id inválido lança;
- CDI fabricado: parser do cabeçalho v3.5 (sessões, marca, nome, variante,
  pregap/length/total, código de setor, userOffset 2336), leitura de
  IP.BIN/PVD/arquivo, ISO (volume id, `find` case-insensitive, ausência),
  e teste negativo de nome hostil (`BAD.NA/ME` → `DiscError`);
- known_versions: hash sintético não reconhece versão nenhuma.

Nota de processo: três bugs foram capturados por essas validações e
corrigidos com teste falhando antes da correção:
1. `Crc32::value()` sem complemento final (vetor CRC falhava);
2. `descramble` perdia a base `pos` do bloco na cópia (hunt por diagnóstico
   diferencial contra a referência MIT em 8 tamanhos — `analysis/
   hh02_scramble_probe.cpp`Aggregate "6144/71680/... DIVERGENCIAS");
3. vetores de teste SHA-256 fabricados errados (`1M 'a'`, digest duplo).
Havia também um bug de encadeamento no builder sintético do CDI (u32 da
variante e trailer de fim de sessão ausentes) — corrigidos com o teste.

### Comparação com a referência (oráculo HH01)

```powershell
python analysis\hh02_compare.py   -> exit 0
```

Resultado (artefato `analysis/hh02_compare.json`):

| Disco | arquivos | difs por bytes/sha/lba/size/crc32 | ip disco | volume |
|---|---:|---:|---:|---|
| disc1 | 212 | **0 / 0 / 0 / 0 / 0** | igual | igual |
| disc2 | 160 | **0 / 0 / 0 / 0 / 0** | igual | igual |

`IP.BIN` extraído é byte-idêntico ao da referência (32 768 bytes); boot
`sha256` igual ao manifest de referência em ambos os discos; `scrambled
_on_disc` `false` em ambos, igual à fonte. Campos `boot.verified/
version_id` divergem **a favor** do hh-extract e isso é esperado e
documentado: o binário de referência é uma build anterior à sua fonte (HH01
§4) e não conhece as versões `hedhunter_v1006_{a,b}`; a tabela de minha
ferramenta contém os dois pares com hashes conferidos na fonte MIT. O dif
trata essas chaves como meta-informação de reconhecimento e só sinaliza
quando duas versões reconhecidas divergem.

### Reextração determinística

```powershell
& build\tools\hh-extract\Release\hh-extract.exe "ISO\HeadHunter (Disc 1).cdi" game-data\hh02\disc1_rerun
python (sha256 manifest disc1 == sha256 manifest disc1_rerun)  -> IDENTICO, exit 0
```

## 5. Identificação e detecção de rip divergente

- Cada disco reconhecido pelo hash do boot: `hedhunter_v1006_a` (disco 1) e
  `hedhunter_v1006_b` (disco 2), via `known_versions.hpp` (matriz HH01).
- Hash desconhecido: mensagem `AVISO ... nao corresponde a nenhuma versão
  suportada` + aborto com código 1 sem `--force` (porta de segurança para
  rips não suportados). O mesmo vale para Windows CE (bit0=1 do `peripherals`
  → erro incondicional).

## 6. Riscos, limitações e trabalho deliberadamente deixado

- O modo `--runtime` de verificação setor-a-setor da referência não foi
  implementado (consumidor real aparece em HH05+); a garantia equivalente
  foi obtida por dif de árvore completa.
- `readExtent` lê setores 2 348? Não: buffer de 64 setores por chamada; ainda
  assim cada setor é lido com passada por setor via `ifstream::read` —
  performance suficiente (extração completa < 1 min por disco em Release),
  mas pode otimizar com leitura contínua dentro de uma mesma faixa/pregap
  quando HH05 exigir hot path.
- `parseIpBin` espera ≥ 0xC00 bytes; imagens com faixa de boot menor são
  recusadas (comportamento igual ao da referência).
- Testes sintéticos fabricam CDI/GD-ROM no layout v3.5 praticado (1 sessão),
  sem segunda sessão sintética (√ testes de multissessão dependem dos
  diskes reais; cobertos pelo teste real dos 2 discos, exit 0).

## 7. Inventário de dados privados — nada versionado

Privados criados nesta etapa (todos ignorados, confirmados):

- `game-data/hh02/{disc1,disc2,disc1_rerun}/` (372 arquivos + 3 manifests + 2×IP.BIN + 2×1ST_READ.BIN);
- `analysis/hh02_*.{py,cpp,json,txt}` (script dif, probes, log de referência).

Verificação:

```powershell
git -c safe.directory=C:/Source/recomp-head-hunter check-ignore -v `
  "ISO/HeadHunter (Disc 1).cdi" game-data/hh02/disc1/manifest.json `
  analysis/hh02_compare.json build/CMakeCache.txt   # todos ignorados
git -c safe.directory=C:/Source/recomp-head-hunter status --short  # sem paths privados versionados
```

- [x] `ISO/HeadHunter (Disc 1).cdi` — ignorado;
- [x] `game-data/hh02/disc1/manifest.json` — ignorado;
- [x] `analysis/hh02_compare.py` — ignorado;
- [x] `analysis/hh02_compare.json` — ignorado;
- [x] `build/` — ignorado.

## 8. Checklist do critério de saída

- [x] extração completa de ambos os discos (212 + 160 arquivos);
- [x] conferência por implementação independente: byte a byte contra a
  árvore de referência HH01 + manifest de campos (0 divergências estruturais);
- [x] reextração determinística: manifest SHA-256 idêntico em duas rodadas;
- [x] `--info` somente leitura reproduz dados da HH01;
- [x] suporte a CDI multissessão/faixas e leitura cruzando faixas;
- [x] extração segura (sanitização por componente + rejeição de traversal);
- [x] manifesto com hashes de todos os arquivos (sha256+crc32);
- [x] identificação dos dois discos e porta de rip divergente (`--force`);
- [x] saída de runtime separada (árvore extraída + manifest sem CDI);
- [x] testes sintéticos (CDI fabricado, 39 verificações) e reais via CLI;
- [x] nenhum dado proprietário versionado (§7).

## 9. Próxima etapa

HH03 (`EM_ANDAMENTO`): núcleo SH-4 do SH7091 — tabela declarativa das
formas de instrução, decodificador/semanticas com delay slots, FPU e
modos, testes exaustivos de todos os 65.536 opcodes sem dados do jogo.
