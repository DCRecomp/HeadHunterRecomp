# SPDX-License-Identifier: MIT
"""Perfil do RecompLauncher (tools/dist/recomplauncher.json; docs/DEVELOPERS.md e SETUP-INPUTS.md do RecompLauncher).

Confere o perfil contra os contratos do proprio projeto, sem dados do jogo:
- formato 1, campos conhecidos, um input por disco (CDI), toda referencia {input:x} declarada, so os marcadores de
  caminho que o launcher conhece, caminhos relativos sem '..';
- a etapa prepare roda o hh-builder com argumentos que ele aceita (codigo 1 com discos inexistentes, nao 2 = uso) e
  imprime o progresso no formato do launcher ("progress <0..1> <passo>") com --progresso-launcher;
- start.program e o executavel que o prepare instala (o primeiro arquivo de prepare.done);
- saves = a pasta padrao do jogo (app/hh_window.cpp), exit_codes so com codigos que o jogo devolve;
- prepare.version e o marcador que o make_builder.ps1 troca pela versao do produto.
Com HH_RECOMPLAUNCHER_SCHEMA (o esquema baixado de recomplauncher.com) e o jsonschema disponivel, valida tambem por ele.

uso: test_recomplauncher.py <raiz do projeto> [<hh-builder.exe>]
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
BUILDER = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else ''
PERFIL = os.path.join(ROOT, 'tools', 'dist', 'recomplauncher.json')
CAMPOS = {'$schema', 'format', 'name', 'repo', 'inputs', 'game', 'start', 'prepare', 'config', 'folders', 'exit_codes',
          'settings', 'groups', 'saves', 'games', 'looks', 'mods', 'actions', 'requires', 'min'}
MARCADORES = {'install', 'game', 'documents', 'local', 'config', 'home', 'pad'}


def textos(v):
    if isinstance(v, str):
        yield v
    elif isinstance(v, list):
        for x in v:
            yield from textos(x)
    elif isinstance(v, dict):
        for x in v.values():
            yield from textos(x)


class Perfil(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(PERFIL, encoding='utf-8') as f:
            cls.p = json.load(f)

    def test_formato_e_campos(self):
        self.assertEqual(self.p['format'], 1)
        self.assertLessEqual(set(self.p), CAMPOS)
        self.assertEqual(self.p['$schema'], 'https://recomplauncher.com/schema/recomplauncher.json')

    def test_um_input_por_disco_cdi(self):
        ins = self.p['inputs']
        self.assertEqual([i['id'] for i in ins], ['disc1', 'disc2'])
        for i in ins:
            self.assertRegex(i['id'], r'^[A-Za-z0-9_-]{1,64}$')
            self.assertEqual(i['role'], 'disc')
            self.assertFalse(i.get('optional', False))
            self.assertEqual(i['formats'], [{'type': 'file', 'extensions': ['cdi']}])
            self.assertIn('MK-5104150 V1.006', i['label'])

    def test_marcadores_e_caminhos(self):
        ids = {i['id'] for i in self.p['inputs']}
        usados = set()
        for t in textos({k: v for k, v in self.p.items() if k != '$schema'}):
            for m in re.findall(r'\{([^}]*)\}', t):
                if m.startswith('input:'):
                    self.assertIn(m[6:], ids, t)
                    usados.add(m[6:])
                else:
                    self.assertIn(m, MARCADORES, t)
        self.assertEqual(usados, ids, 'todo disco declarado chega ao prepare')
        for prog in self.p['start']['program'] + self.p['prepare']['program']:
            self.assertFalse(os.path.isabs(prog) or '..' in prog.replace('\\', '/').split('/'), prog)
        for d in self.p['prepare']['done']:
            self.assertTrue(d.startswith('{install}/'), d)

    def test_start_e_o_que_o_prepare_instala(self):
        a = self.p['prepare']['args']
        out, nome = a[a.index('--out') + 1], a[a.index('--name') + 1]
        self.assertEqual(out, '{install}')
        exe = '%s/HeadHunter.exe' % nome
        self.assertEqual(self.p['start']['program'], [exe])
        self.assertEqual(self.p['prepare']['done'][0], '{install}/' + exe)
        self.assertIn('--documentos', a)  # os saves ficam onde "saves" diz (instalacao.toml)
        self.assertIn('--progresso-launcher', a)
        self.assertGreaterEqual(self.p['prepare']['timeout'], 3600)  # a compilacao do jogo leva minutos
        self.assertEqual(self.p['prepare']['version'], '@VERSAO@')

    def test_saves_e_codigos_de_saida_do_jogo(self):
        with open(os.path.join(ROOT, 'app', 'hh_window.cpp'), encoding='utf-8') as f:
            src = f.read()
        self.assertIn('fs::path(docs) / L"DC-Recomp" / L"Head Hunter" / L"saves"', src)
        self.assertEqual(self.p['saves'], ['{documents}/DC-Recomp/Head Hunter/saves'])
        codigos = set(re.findall(r'\breturn (\d+);', src))
        self.assertLessEqual(set(self.p['exit_codes']), codigos)
        self.assertEqual(self.p['exit_codes']['0'], 'ok')  # janela fechada
        self.assertEqual(self.p['exit_codes']['3'], 'ok')  # o jogo pediu para sair
        self.assertIn('case hh::HostEnd::GameExit: std::printf("o jogo pediu para sair (codigo %08X)\\n", result.exitCode); return 3;', src)

    def test_make_builder_troca_a_versao_e_poe_no_topo(self):
        with open(os.path.join(ROOT, 'tools', 'dist', 'make_builder.ps1'), encoding='utf-8') as f:
            src = f.read()
        self.assertIn('.Replace("@VERSAO@", $product)', src)
        self.assertIn('"$stage\\recomplauncher.json"', src)
        self.assertIn('HeadHunter-Builder-v$product-Windows-x64.zip', src)

    def test_esquema_oficial(self):
        esquema = os.environ.get('HH_RECOMPLAUNCHER_SCHEMA')
        if not esquema:
            self.skipTest('HH_RECOMPLAUNCHER_SCHEMA nao definido')
        try:
            import jsonschema  # so com o esquema dado
        except ImportError:
            self.skipTest('jsonschema indisponivel (python -I ignora PYTHONPATH)')
        with open(esquema, encoding='utf-8') as f:
            s = json.load(f)
        erros = [list(e.path) for e in jsonschema.Draft7Validator(s).iter_errors(self.p)]
        self.assertEqual(erros, [])

    def test_prepare_no_hh_builder(self):
        if not BUILDER:
            self.skipTest('sem o hh-builder.exe')
        d = tempfile.mkdtemp(prefix='hh_launcher_')
        mapa = {'{input:disc1}': os.path.join(d, 'nao existe 1.cdi'), '{input:disc2}': os.path.join(d, 'nao existe 2.cdi'),
                '{install}': os.path.join(d, 'instalacao')}
        args = [mapa.get(a, a) for a in self.p['prepare']['args']]
        r = subprocess.run([BUILDER] + args, capture_output=True, text=True, encoding='utf-8', errors='replace',
                           timeout=600, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        saida = r.stdout + r.stderr
        self.assertEqual(r.returncode, 1, saida)  # argumentos aceitos (2 = uso); a falha e dos discos inexistentes
        linhas = [l for l in r.stdout.splitlines() if l.startswith('progress ')]
        self.assertTrue(linhas, saida)
        for l in linhas:
            self.assertRegex(l, r'^progress (0|1)\.\d\d \S')
            self.assertLessEqual(float(l.split()[1]), 1.0)
        self.assertNotIn('[progresso]', r.stdout)
        self.assertFalse(os.path.exists(os.path.join(d, 'instalacao', 'HeadHunter', 'HeadHunter.exe')))


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]], verbosity=2)
