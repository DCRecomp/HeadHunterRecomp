# HH01 — Inventário forense dos dois discos e versões-alvo

Etapa HH01 conforme `ROADMAP.md` (nível de validação V1). Data: 2026-10-24.
Estado: **CONCLUIDA** — critério de saída atendido: duas ferramentas
independentes confirmam todos os metadados críticos; nenhuma afirmação de
endereço ou formato é registrada sem evidência reproduzível.

## 1. Objetivo e escopo realizado

Entregáveis da etapa (todos cumpridos):

1. análise dos dois CDI sem extrair para caminhos versionados;
2. registro de sessões, faixas, setor, LBA, IP.BIN, produto, versão, região,
   boot filename, tamanho e SHA-256 do executável (§4);
3. determinação de binário único vs. executáveis diferentes, overlays,
   módulos carregáveis e Windows CE (§5);
4. comparação das árvores por nome/tamanho/hash e localização do contrato de
   troca de disco (§6);
5. configs preliminares por disco/versão, nomenclatura canônica e matriz de
   versões aceitas (§7);
6. relatório REA/Ghidra com entry point, arquitetura e mapa inicial (§8).

O executável `hh-extract` em C++ é entregável da HH02 e não foi iniciado aqui.

## 2. Arquivos alterados (caminhos versionáveis)

- `tools/forensics/hh_discinfo.py` — segunda implementação independente,
  reescrita como leitor linear determinístico do cabeçalho CDI; CLI
  `--info`/`--tree`/`--extract`/`--json`.
- `config/discs/hedhunter_v1006_a.toml` e `hedhunter_v1006_b.toml` — configs
  preliminares por disco (novos).
- `docs/relatorios/HH01-RELATORIO.md` — este arquivo.
- `STATUS.md` — HH01 → `CONCLUIDA`; HH02 → `EM_ANDAMENTO`.

Correção executada durante a HH01 mas pertencente ao porta V0 de HH00
(registrada por decisão de manter o relatório primário de HH00 intacto):

- `CMakeLists.txt` raiz e `tests/smoke.cpp`: o porta V0 reportado em HH00
  estava quebrado; com as correções o configure/build roda e `hh_smoke`
  termina com código 0 (§9).

## 3. Decisões tomadas

1. **Nomenclatura canônica**: `hedhunter_v1006_a` (disc 1) e
   `hedhunter_v1006_b` (disc 2). Fonte: `known_versions.h` da árvore de
   referência MIT (`C:\Source\recomp-BStinger\tools\extractor`), que já
   lista o par; os dois hashes de boot conferem em duas fontes (§4).
2. **Katana, não Windows CE**: bit 0 do campo `peripherals` do IP.BIN
   (string "0799A10", bit0 = 0). Confirmado por duas ferramentas
   independentes.
3. **Parser CDI linear**: o cabeçalho v3.5 é percorrido com o esquema
   linear (rodapé `[u32 versão][u32 ho]`, `header_pos = size - ho`, marca
   de faixa de 20 bytes, encadeamento único de campos). Offsets por disco
   variam com o comprimento do nome registrado e a variante DiscJuggler
   (u32 0x80000000 condicional); por isso nenhuma posição absoluta é usada.
4. **Âncora de consistência** (validada nos 4 registros faixa): 
   `total == length + pregap` com `pregap = 150` (2 s do padrão CD).

## 4. Metadados críticos — duas fontes independentes

Comandos:

```powershell
C:\Source\recomp-BStinger\build\msvc-release\tools\extractor\bs-extract.exe --info 'ISO\HeadHunter (Disc 1).cdi'   # exit 0
C:\Source\recomp-BStinger\build\msvc-release\tools\extractor\bs-extract.exe --info 'ISO\HeadHunter (Disc 2).cdi'   # exit 0
python tools\forensics\hh_discinfo.py 'ISO/HeadHunter (Disc 1).cdi' --info   # exit 0
python tools\forensics\hh_discinfo.py 'ISO/HeadHunter (Disc 2).cdi' --info   # exit 0
```

| Campo | Disc 1 | Disc 2 |
|---|---:|---:|
| Formato | CDI v3.5 | CDI v3.5 |
| Sessões | 2 | 2 |
| Faixa 1 (sessão 1) | mode2, LBA 0, length 219295, total 219445, setor 2336 | mode2, LBA 0, length 240069, total 240219, setor 2336 |
| Faixa 2 (sessão 2, boot) | mode2, LBA 230695, length 128784, total 128934 | mode2, LBA 251469, length 108094, total 108244 |
| pregap | 150 | 150 |
| IP.BIN | MK-5104150, V1.006, 20011116, JUE, boot 1ST_READ.BIN | idem |
| IP.BIN título | `HEADHUNTER - SEGA - AMUZE - 2001` | idem |
| peripherals | `0799A10` (bit0 = 0) | idem |
| Windows CE | não (Katana) | não (Katana) |
| Executável | 1.545.024 bytes | 1.545.024 bytes |
| SHA-256 do boot | `aa5853253b49…d4b640d7` | `2d1ead048b28…dd578b0a` |

Observação sobre o binário `bs-extract.exe` em uso: é uma compilação mais
antiga que a fonte corrente da árvore MIT — mostra um aviso de hash porque
sua lista de versão embutida ainda não conhece o par hedhunter, enquanto
`known_versions.h` (fonte da árvore) já o lista. O aviso não invalida
qualquer resultado: a nomenclatura foi conferida na fonte, não no binário.

## 5. Executáveis, overlays, módulos e Katana/CE

Comando e artefato: `python analysis\hh01\diff_executables.py` →
`analysis/hh01/executable_diff.json`.

- Os dois `1ST_READ.BIN` têm o mesmo tamanho (1.545.024 bytes) e divergem
  em **exatamente 1 região de 2 bytes** no offset de arquivo
  0x00175E28 (0,000 % do binário).
- Os 4 bytes em 0x00175E28 formam um u32 little-endiano por disco:
  230.861 (disc 1) e 251.635 (disc 2) — ambos iguais a
  `iso_root_lba + 143` (230718+143 e 251492+143). Conclusão: é **um
  binário com um dword constante por disco** (auto-referência ao layout da
  própria sessão high-density), não dois builds de código.
- Módulos carregáveis: o inventário de extensões não mostra módulos de
  sistema externos (não há .PRX/.KMF/dump de firmware). O driver
  `AUDIO64.DRV` (18.608 bytes) é **hash idêntico nos dois discos**
  (`9fcdbb82d761157f…dc7f947`) e carregável pelo título. O IP.BIN tem a
  área de checksum (0x900..0xC00) não nula.
- Overlays: nenhuma região de dados dos setores gravados fica sem dono —
  ambas as árvores ISO cobrem 372 extents (212+160) conferidos por
  lba/tamanho/hash (§6), portanto não há overlay fora da árvore.
- Katana vs. CE: bit 0 de peripherals = 0 nas duas leituras; a assinatura
  do IP.BIN é `SEGA SEGAKATANA`; não há qualquer sinal de runtime CE.
  Confirmado Katana.

## 6. Árvores, hashes e contrato de troca de disco

```powershell
python tools\forensics\hh_discinfo.py 'ISO/HeadHunter (Disc 1).cdi' --tree analysis\hh01\disc1_tree.json   # exit 0
python tools\forensics\hh_discinfo.py 'ISO/HeadHunter (Disc 2).cdi' --tree analysis\hh01\disc2_tree.json   # exit 0
python analysis\hh01\compare_manifest.py    # exit 0
python analysis\hh01\cross_manifest.py      # exit 0
```

Comparação contra os manifests do bs-extract
(`game-data/hh01/disc{1,2}_ref/manifest.json`):

| Disco | arquivos ref | arquivos hh_discinfo | só-ref | só-meu | campos divergentes |
|---|---:|---:|---:|---:|---:|
| disc1 | 212 | 212 | 0 | 0 | 0 |
| disc2 | 160 | 160 | 0 | 0 | 0 |

**100 % verbatim em lba/size/sha256/crc32 nos 372 arquivos.** Zonas de
dados: sessão 1 (low density) volume `CDROM` com 120/89 arquivos;
sessão 2 (high density) volumes `HHS1_DCRES`/`HHS2_DCRES` com 212/160.

Contrato de troca de disco (evidência nas duas ferramentas):

- `DISC/DISK1.ID` (138 bytes, disc 1) e `DISC/DISK2.ID` (108 bytes,
  disc 2): arquivos textuais exclusivos de cada árvore; identidade do
  disco pela presença/leitura de `DISC/DISK{N}.ID` na sessão 2.
- Dword embutida em `1ST_READ.BIN` em 0x00175E28 = `iso_root_lba + 143`
  do próprio disco (§5): impossível passar um disco pelo outro sem
  recompilar esse valor.
- `MOVIES/SCENE0.4XM` também difere (abertura por disco).
- Os dois discos compartilham 69 caminhos; 67 com hash idêntico -
  divergem apenas `1ST_READ.BIN` e `MOVIES/SCENE0.4XM`.

## 7. Nomenclatura canônica e matriz de versões aceitas

| id | disco | boot sha256 | tamanho | produto | versão |
|---|---|---|---:|---|---|
| `hedhunter_v1006_a` | 1 | `aa585325…40d7` | 1.545.024 | MK-5104150 | V1.006 |
| `hedhunter_v1006_b` | 2 | `2d1ead04…b0a` | 1.545.024 | MK-5104150 | V1.006 |

Configs preliminares: `config/discs/hedhunter_v1006_{a,b}.toml`
(TOMLs válidos, sem conteúdo proprietário). Convenção:
`<título>_<versão>_<letra-de-disco>`.

## 8. Relatório REA/Ghidra (entry point, arquitetura, mapa inicial)

REA não expõe SH-4 (famílias 6502/x86/arm/m68k). A análise SH-4 usou o
Ghidra headless do ambiente com o processador `SuperH4`:

```powershell
& C:\Tools\ghidra_12.1.3_PUBLIC\support\analyzeHeadless.bat analysis\hh01\ghdproj HH01 `
  -import "<game-data>\hh01\disc1_ref\1ST_READ.BIN" `
  -processor SuperH4:LE:32:default -cspec default -loader BinaryLoader `
  -loader-baseAddr 0x8C010000 `
  -scriptPath analysis\hh01\ghscripts -postScript HHMapSummary.java
& C:\Tools\ghidra_12.1.3_PUBLIC\support\analyzeHeadless.bat analysis\hh01\ghdproj HH01 `
  -process 1ST_READ.BIN -noanalysis -postScript HHEntry.java ...
```

Artefatos: `analysis/hh01/ghscripts/{HHMapSummary,HHEntry}.java`,
`analysis/hh01/ghidra_out{,2,3}.txt` e o projeto `analysis/hh01/ghdproj/HH01`
(todos em caminhos ignorados).

- **Arquitetura**: `SuperH4:LE:32:default` (SH-4 little-endian), carga bruta
  em **entry `0x8C010000`** — padrão Dreamcast para `1ST_READ.BIN`.
- **Mapa**: um bloco `ram` de exatamente [0x8C010000 .. 0x8C18933F] =
  1.545.024 bytes (o binário completo, sem buracos úteis).
- **Divisão**: 3.893 funções descobertas pela análise automática; 2.485
  dados declarados com valor de string, entre 54.977 itens.
- **Entry**: 6 nops seguidos da sequência mov.l/and do crt0 SH-4 (padrão
  Katana): início de imagem confirmado em código SH-4 válido, não zeros.

Interpretação (inferência, não observação): build única compilada para
Katana SH-4, código+dados num concentrado de 1,5 MB, sem módulos
carregáveis externos e sem camada Windows CE.

Regra de dependência para etapas seguintes: REA vale para alvos x86/ARM;
SH-4 prossegue com Ghidra headless + `DreamcastRE.java` (HH03+).

## 9. Comandos de build/teste (portões de validação)

```powershell
cmake --build build --config Debug     # configure + build       -> exit 0
& build\tests\Debug\hh_smoke.exe        # porta V0 de HH00        -> exit 0
```

Validações específicas da HH01 (todas com código de saída 0):
`--info` nos 2 discos; `--tree` nos 2 discos (372 arquivos + hashes);
`--extract` de `1ST_READ.BIN` (hash igual ao campo `[boot]` da §4);
`compare_manifest.py` (§6). Obs.: durante o desenvolvimento três bugs
foram pegos por essas validações e corrigidos — hash em leitura contínua
antes da correção do stride (2336), LBA relativo vs. absoluto no
caminhamento ISO (o disco 1 tem start_lba 0, o disco 2 não) e um crash
de `crc32` fora do `hashlib` (contradiz o relatório anterior que omitia
tais falhas; todos corrigidos com teste rodado antes/depois).

## 10. Riscos, limitações e trabalho deliberadamente deixado

- O binário `bs-extract.exe` disponível é uma build anterior à sua fonte
  (aviso de hash desatualizado); em HH02 o produto passa a ter seu
  `hh-extract` em C++20 (sem depender do binário de referência).
- `hh_discinfo.py` é caminho lateral forense (não substitui HH02).
- As imagens CDI têm rodapé/pregap; o campo `total` inclui o pregap de 150
  setores e a soma dos `total` (× 2336) bate com o início da área de dados
  nos dois discos — qualquer divergência futura deve ser interpretada
  contra essa invariância, não contra tamanhos de arquivo.
- `dream-recomp/` permanece apenas oráculo externo GPL-2.0 (não integrado).

## 11. Inventário de dados privados — nada versionado

- `ISO/*.cdi` (2 imagens GD-ROM proprietárias);
- `game-data/hh01/*_ref/` (372 arquivos extraídos + manifests, conteúdo proprietário);
- `analysis/hh01/*` (scripts, JSONs derivados, projeto Ghidra, logs);
- `build/`, `dist/` (saídas locais).

Verificação executada (`git status --short`; todos ignorados via
`.gitignore`, saída registrada; amostras conferidas com `git check-ignore`):

- [x] `ISO/HeadHunter (Disc 1).cdi` — ignorado;
- [x] `game-data/hh01/disc1_ref/1ST_READ.BIN` — ignorado;
- [x] `analysis/hh01/disc1_tree.json` — ignorado;
- [x] `analysis/hh01/ghdproj/HH01` — ignorado;
- [x] `build/CMakeCache.txt` — ignorado;
- [x] `dist/` — ignorado.

## 12. Checklist do critério de saída

- [x] metadados críticos confirmados por duas implementações independentes (§4);
- [x] árvores completas comparadas por nome/tamanho/hash (§6);
- [x] binários do boot diferenciados e explicados (§5);
- [x] overlays/módulos/Windows CE determinados (§5);
- [x] contrato de troca de disco localizado (§6);
- [x] Katana vs. CE resolvido (§4/§5);
- [x] configs preliminares por disco/versão (§7);
- [x] nomenclatura canônica e matriz (§7);
- [x] relatório REA/Ghidra com entry point/arquitetura/mapa (§8);
- [x] nenhum dado proprietário versionado (§11);
- [x] portão V0/HH00 verde (§9).

## 13. Próximas etapas

HH02 (`EM_ANDAMENTO`): implementar `hh-extract` em C++20 (CMake), leitura
somente, CDI multissessão, arquivos que cruzam sessões, extração segura
(path-traversal repelido), hashes/manifest completos dos 2 discos,
identificação por `known_versions` e detecção de rip divergente, com
testes sintéticos (CDI fabricado) e reais opcionais por variável de
ambiente. Oráculo: manifests HH01 + reextração determinística.
