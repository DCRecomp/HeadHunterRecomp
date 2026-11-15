# SPDX-License-Identifier: MIT
"""HH16.1: testes do monitor de campanha (tools/dev/campanha.py) com o programa falso (programa_falso.py): criacao,
sessoes, cadeia de saves, creditos, queda, divergencia e arquivo alterado. Sem jogo, sem janela, sem dados privados.

uso: python -I tests/campanha/test_campanha.py   (pastas temporarias em TMP, que o ctest aponta para tmp/ctest)
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(os.path.dirname(AQUI))
MONITOR = os.path.join(RAIZ, 'tools', 'dev', 'campanha.py')
FALSO = os.path.join(AQUI, 'programa_falso.py')


class Campanha(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='hh_campanha_')
        self.inst = os.path.join(self.dir, 'instalacao')
        os.makedirs(os.path.join(self.inst, 'data'))
        with open(os.path.join(self.inst, 'HeadHunter.exe'), 'wb') as f:
            f.write(b'programa falso')
        with open(os.path.join(self.inst, 'data', 'arquivos.manifest'), 'w') as f:
            f.write('dados falsos\n')
        self.camps = os.path.join(self.dir, 'campanhas')

    def run_monitor(self, *args, **fake):
        env = dict(os.environ, HH_CAMPANHAS=self.camps, HH_CAMPANHA_PROGRAMA=FALSO)
        for k in [k for k in env if k.startswith('FAKE_')]:
            del env[k]
        env.update({'FAKE_' + k: str(v) for k, v in fake.items()})
        p = subprocess.run([sys.executable, '-I', MONITOR] + list(args), capture_output=True, text=True, env=env)
        return p.returncode, p.stdout + p.stderr

    def cid(self):
        return sorted(os.listdir(self.camps))[0]

    def nova(self, *extra):
        rc, out = self.run_monitor('nova', '--instalacao', self.inst, *extra)
        self.assertEqual(rc, 0, out)

    def test_sessoes_cadeia_e_creditos(self):
        self.nova()
        rc, out = self.run_monitor('jogar', ENTRADA='100 A;200 -;300 SAVE', FIM=600)
        self.assertEqual(rc, 0, out)
        self.assertIn('sessao 1 gravada', out)
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 0, out)
        self.assertIn('REPRODUZIDA, MAS INCOMPLETA', out)
        # a sessao 2 comeca dos saves reproduzidos da 1 e chega aos creditos
        rc, out = self.run_monitor('jogar', ENTRADA='50 B;400 SAVE;500 CREDITOS', FIM=600)
        self.assertEqual(rc, 0, out)
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 0, out)
        self.assertIn('CAMPANHA REPRODUZIDA ATE OS CREDITOS', out)
        with open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8') as f:
            rel = json.load(f)
        self.assertEqual([s['veredito'] for s in rel['sessoes']], ['REPRODUZIDA', 'REPRODUZIDA'])
        self.assertTrue(rel['sessoes'][1]['creditos'])

    def test_negativo_creditos_so_na_ultima_sessao_contam(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 CREDITOS;300 SAVE', FIM=600)
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        rc, out = self.run_monitor('verificar')
        self.assertIn('INCOMPLETA', out)

    def test_negativo_creditos_sem_gameend(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 SAVE;200 CREDITOS', FIM=300)
        rc, out = self.run_monitor('verificar', SEM_GAMEEND=1)
        self.assertEqual(rc, 0, out)
        self.assertIn('INCOMPLETA', out)

    def test_campanha_de_teste_nunca_vale(self):
        self.nova('--teste')
        self.run_monitor('jogar', ENTRADA='100 SAVE;200 CREDITOS', FIM=300)
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 0, out)
        self.assertIn('TESTE REPRODUZIDO', out)
        self.assertNotIn('ATE OS CREDITOS', out)

    def test_negativo_divergencia(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        rc, out = self.run_monitor('verificar', DIVERGE=240)
        self.assertEqual(rc, 1, out)
        self.assertIn('REPROVADA', out)
        with open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8') as f:
            self.assertEqual(json.load(f)['sessoes'][0]['primeiro_quadro_diferente'], 240)

    def test_negativo_estado_diverge_antes_da_imagem(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        rc, out = self.run_monitor('verificar', DIVERGE_ESTADO=180)
        self.assertEqual(rc, 1, out)
        with open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8') as f:
            r = json.load(f)['sessoes'][0]
        self.assertIsNone(r['primeiro_quadro_diferente'])  # a imagem ainda igual
        self.assertEqual(r['primeira_leitura_diferente'], 180)
        self.assertEqual(r['estados_conferidos'], 11)

    def test_negativo_sem_estado_nao_aprova(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600, SEM_ESTADO=1)
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 1, out)
        self.assertIn('REPROVADA', out)

    def test_reproducao_reprovada_nao_alimenta_a_proxima_sessao(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        rc, out = self.run_monitor('verificar', DIVERGE=240)  # a reproducao salva outra coisa
        self.assertEqual(rc, 1, out)
        self.run_monitor('jogar', ENTRADA='100 B', FIM=120)
        cid = self.cid()
        ini = os.path.join(self.camps, cid, 'sessao_002', 'saves_inicio', 'H_HUNTER.001')
        grav = os.path.join(self.camps, cid, 'sessao_001', 'saves', 'H_HUNTER.001')
        rep = os.path.join(self.camps, cid, 'sessao_001', 'reproducao', 'saves', 'H_HUNTER.001')
        self.assertEqual(open(ini).read(), open(grav).read())
        self.assertNotEqual(open(ini).read(), open(rep).read())

    def test_negativo_arquivo_alterado(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        with open(os.path.join(self.camps, self.cid(), 'sessao_001', 'entrada.txt'), 'a') as f:
            f.write('400 START\n')
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 1, out)
        self.assertIn('arquivos alterados', out)
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60)
        self.assertNotEqual(rc, 0, out)  # nao continua uma campanha adulterada

    def test_queda_vale_ate_a_queda(self):
        self.nova()
        rc, out = self.run_monitor('jogar', ENTRADA='100 A;200 SAVE;500 B', FIM=600, QUEDA=350)
        self.assertIn('sessao 1 interrompida', out)
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 0, out)
        with open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8') as f:
            r = json.load(f)['sessoes'][0]
        self.assertEqual(r['veredito'], 'REPRODUZIDA_ATE_A_QUEDA')
        self.assertEqual(r['ate_vblank'], 300)

    def test_negativo_cadeia_quebrada(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        self.run_monitor('jogar', ENTRADA='100 B;300 SAVE', FIM=600)
        # alguem troca o save de inicio da sessao 2 e "re-sela" so o manifesto: a cadeia acusa
        cid = self.cid()
        p = os.path.join(self.camps, cid, 'sessao_002', 'saves_inicio', 'H_HUNTER.001')
        with open(p, 'w') as f:
            f.write('outro save')
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 1, out)
        self.assertIn('cadeia quebrada', out)

    def test_copia_de_seguranca_a_cada_sessao(self):
        copia = os.path.join(self.dir, 'outro_disco')
        self.nova('--copia', copia)
        rc, out = self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        self.assertEqual(rc, 0, out)
        self.assertIn('copia de seguranca', out)
        cid = self.cid()
        rel = os.path.join('sessao_001', 'entrada.txt')
        self.assertEqual(open(os.path.join(copia, cid, rel)).read(), open(os.path.join(self.camps, cid, rel)).read())
        # um arquivo so da copia nunca e apagado; a segunda sessao acrescenta a pasta dela
        extra = os.path.join(copia, cid, 'anotacao.txt')
        with open(extra, 'w') as f:
            f.write('x')
        self.run_monitor('jogar', ENTRADA='50 B;400 SAVE;500 CREDITOS', FIM=600)
        self.assertTrue(os.path.exists(os.path.join(copia, cid, 'sessao_002', 'entrada.txt')))
        self.assertTrue(os.path.exists(extra))
        # a copia sozinha e uma campanha completa: verificar nela chega aos creditos
        env = dict(os.environ, HH_CAMPANHAS=copia, HH_CAMPANHA_PROGRAMA=FALSO)
        p = subprocess.run([sys.executable, '-I', MONITOR, 'verificar'], capture_output=True, text=True, env=env)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn('CAMPANHA REPRODUZIDA ATE OS CREDITOS', p.stdout)

    def test_copia_padrao_e_copiar_conserta_copia_alterada(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        cid = self.cid()
        alvo = os.path.join(self.camps, '_copias', cid, 'sessao_001', 'entrada.txt')
        self.assertTrue(os.path.exists(alvo))
        self.assertEqual(self.cid(), cid)  # a pasta das copias nao vira campanha
        with open(alvo, 'a') as f:
            f.write('estrago\n')
        rc, out = self.run_monitor('copiar')
        self.assertEqual(rc, 0, out)
        self.assertIn('1 arquivos novos ou atualizados', out)
        self.assertEqual(open(alvo).read(), open(os.path.join(self.camps, cid, 'sessao_001', 'entrada.txt')).read())

    def test_negativo_copia_dentro_da_campanha(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        rc, out = self.run_monitor('copiar', '--para', os.path.join(self.camps, self.cid()))
        self.assertNotEqual(rc, 0, out)
        self.assertIn('dentro da propria campanha', out)

    def test_dados_trocados_recusados(self):
        self.nova()
        with open(os.path.join(self.inst, 'data', 'arquivos.manifest'), 'a') as f:
            f.write('outro disco\n')
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60)
        self.assertNotEqual(rc, 0, out)
        self.assertIn('dados da instalacao mudaram', out)


if __name__ == '__main__':
    unittest.main(verbosity=2)
