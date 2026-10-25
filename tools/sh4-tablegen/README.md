# sh4-tablegen

Gera `translator/sh4/src/decoder_table.inc` e
`translator/sh4/include/sh4/op_enum.hpp` (HH03). Todas as entradas são
sintéticas: nenhum byte do jogo participa.

| Arquivo | Papel |
|---|---|
| `atlas_shapes.txt` | Retângulos `(mask,value) -> (mnemônico, shape)` minerados do oráculo SLEIGH (versionado) |
| `gen_decoder_table.py` | Mapeia shape → `OperandFmt`, verifica os 65.536 opcodes contra a verdade SLEIGH e grava os arquivos gerados |
| `make_all_ops.py` | Binário sintético com os 65.536 opcodes, cada um seguido de `nop` |
| `HH03Oracle.java` | Script Ghidra que desmonta todos os slots e imprime `0xIIII\|hhhh\|MNEM\|FULL\|FLOW` |
| `extract_truth.py` | Converte o log do Ghidra (UTF-16) na tabela de verdade (65.536 linhas) |

## Regenerar

A tabela de verdade (~3 MB) é saída local, em `analysis/` (ignorada).

```powershell
python -I tools\sh4-tablegen\make_all_ops.py analysis\hh03\all_ops2.bin
& C:\Tools\ghidra_12.1.3_PUBLIC\support\analyzeHeadless.bat analysis\hh03\ghdproj HH03 `
  -import analysis\hh03\all_ops2.bin -overwrite `
  -processor SuperH4:LE:32:default -cspec default -loader BinaryLoader `
  -noanalysis -scriptPath tools\sh4-tablegen -postScript HH03Oracle.java `
  *> analysis\hh03\oracle_run5.txt
python -I tools\sh4-tablegen\extract_truth.py analysis\hh03\oracle_run5.txt analysis\hh03\sleigh_truth.txt
python -I tools\sh4-tablegen\gen_decoder_table.py --truth analysis\hh03\sleigh_truth.txt
```

O `*>` do PowerShell grava o log em UTF-16, que é o que `extract_truth.py`
espera.

Verificado em 2026-10-24: `make_all_ops.py` reproduz `all_ops2.bin` byte a
byte; `extract_truth.py` sobre `oracle_run5.txt` reproduz
`sleigh_truth2.txt` (a menos de CRLF); a regeneração produz
`decoder_table.inc`/`op_enum.hpp` idênticos aos versionados, exceto pelas
linhas de cabeçalho (SPDX e caminho do gerador).

Única divergência aceita vs SLEIGH: `0xFFFD` é ilegal (`MANUAL_WINS`; ver
`docs/relatorios/HH03-RELATORIO.md` §3.5).
