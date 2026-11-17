# SPDX-License-Identifier: MIT
"""HH16.1: testes do HeadHunter-Gravador (tools/gravador), a tela da gravacao, com o programa falso no lugar do jogo:
iniciar grava uma sessao e fechar o "jogo" a termina; a primeira vez cria a campanha real e as seguintes a continuam;
a caixa de teste usa so campanhas de teste; o "Parar gravacao" fecha a janela do jogo (WM_CLOSE) e a sessao fecha
normal; sem Python, o erro aparece e nada e criado.

uso: python -I tests/campanha/test_gravador.py <HeadHunter-Gravador.exe>   (pastas temporarias em TMP)
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(os.path.dirname(AQUI))
FALSO = os.path.join(AQUI, 'programa_falso.py')
GRAVADOR = None


class Gravador(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='hh_gravador_')
        self.inst = os.path.join(self.dir, 'instalacao')
        os.makedirs(os.path.join(self.inst, 'data'))
        with open(os.path.join(self.inst, 'HeadHunter.exe'), 'wb') as f:
            f.write(b'programa falso')
        with open(os.path.join(self.inst, 'data', 'arquivos.manifest'), 'w') as f:
            f.write('dados falsos\n')
        self.camps = os.path.join(self.dir, 'campanhas')

    def gravar(self, *args, timeout=120, extra=None, **fake):
        env = {k: v for k, v in os.environ.items() if not k.startswith('FAKE_') and k != 'HH_INPUT_SCRIPT'}
        env.update(HH_CAMPANHAS=self.camps, HH_CAMPANHA_PROGRAMA=FALSO, HH_CAMPANHA_INSTALACAO=self.inst,
                   HH_CAMPANHA_PYTHON=sys.executable)
        env.update({'FAKE_' + k: str(v) for k, v in fake.items()})
        env.update(extra or {})
        reg = os.path.join(self.dir, 'registro_%d.txt' % len(os.listdir(self.dir)))
        p = subprocess.run([GRAVADOR, '--raiz', RAIZ, '--automatico', '--registro', reg] + list(args), env=env,
                           timeout=timeout)
        with open(reg, encoding='utf-8') as f:
            return p.returncode, f.read()

    def campanhas(self):
        r = {}
        for d in sorted(os.listdir(self.camps)):
            if os.path.isfile(os.path.join(self.camps, d, 'campanha.json')):
                with open(os.path.join(self.camps, d, 'campanha.json'), encoding='utf-8') as f:
                    r[d] = json.load(f)
        return r

    def test_iniciar_cria_grava_e_continua(self):
        rc, out = self.gravar(ENTRADA='100 A;300 SAVE', FIM=600)
        self.assertEqual(rc, 0, out)
        self.assertIn('campanha.py jogar --criar', out)
        self.assertIn('sessao 1 gravada (rc 0)', out)
        self.assertIn('campanha ', out)  # o status depois da gravacao
        c = self.campanhas()
        self.assertEqual(len(c), 1, out)
        m = list(c.values())[0]
        self.assertFalse(m['teste'])
        self.assertEqual([s['situacao'] for s in m['sessoes']], ['gravada'])
        # a segunda vez continua a mesma campanha
        rc, out = self.gravar(ENTRADA='50 B', FIM=300)
        self.assertEqual(rc, 0, out)
        self.assertIn('sessao 2 gravada (rc 0)', out)
        c = self.campanhas()
        self.assertEqual(len(c), 1)
        self.assertEqual(len(list(c.values())[0]['sessoes']), 2)

    def test_caixa_de_teste_nunca_mistura_com_a_real(self):
        rc, out = self.gravar(ENTRADA='100 A', FIM=300)
        self.assertEqual(rc, 0, out)
        rc, out = self.gravar('--teste', ENTRADA='100 A', FIM=300)
        self.assertEqual(rc, 0, out)
        rc, out = self.gravar('--teste', ENTRADA='100 B', FIM=300)
        self.assertEqual(rc, 0, out)
        c = self.campanhas()
        reais = [m for m in c.values() if not m['teste']]
        testes = [m for m in c.values() if m['teste']]
        self.assertEqual((len(reais), len(testes)), (1, 1), out)
        self.assertEqual(len(reais[0]['sessoes']), 1)
        self.assertEqual(len(testes[0]['sessoes']), 2)

    def test_parar_fecha_a_janela_do_jogo(self):
        rc, out = self.gravar('--parar-apos', '2', ENTRADA='100 A;200 SAVE', FIM=300, JANELA=1)
        self.assertEqual(rc, 0, out)
        self.assertIn('sessao 1 gravada (rc 0)', out)
        m = list(self.campanhas().values())[0]
        ps = os.path.join(self.camps, m['id'], 'sessao_001')
        self.assertTrue(os.path.isfile(os.path.join(ps, 'janela_fechada.flag')), out)
        self.assertEqual(m['sessoes'][0]['situacao'], 'gravada')

    def test_sem_python_avisa_e_nao_cria_nada(self):
        rc, out = self.gravar(extra={'HH_CAMPANHA_PYTHON': os.path.join(self.dir, 'nao_existe', 'python.exe')})
        self.assertNotEqual(rc, 0)
        self.assertIn('Python nao encontrado', out)
        self.assertFalse(os.path.isdir(self.camps) and self.campanhas())


if __name__ == '__main__':
    if len(sys.argv) < 2 or not os.path.isfile(sys.argv[1]):
        sys.exit('uso: test_gravador.py <HeadHunter-Gravador.exe>')
    GRAVADOR = os.path.abspath(sys.argv.pop(1))
    unittest.main(verbosity=2)
