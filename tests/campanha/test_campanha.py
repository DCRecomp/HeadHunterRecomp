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
import time
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
            f.write(b'programa falso com o contrato: fim da gravacao reproduzido, [entrada] ultima leitura, --estado-cada')
        with open(os.path.join(self.inst, 'data', 'arquivos.manifest'), 'w') as f:
            f.write('dados falsos\n')
        self.camps = os.path.join(self.dir, 'campanhas')

    def env(self, extra=None, **fake):
        env = dict(os.environ, HH_CAMPANHAS=self.camps, HH_CAMPANHA_PROGRAMA=FALSO)
        for k in [k for k in env if k.startswith('FAKE_') or k == 'HH_INPUT_SCRIPT']:
            del env[k]
        env.update({'FAKE_' + k: str(v) for k, v in fake.items()})
        env.update(extra or {})
        return env

    def run_monitor(self, *args, _env=None, **fake):
        p = subprocess.run([sys.executable, '-I', MONITOR] + list(args), capture_output=True, text=True,
                           env=self.env(_env, **fake))
        return p.returncode, p.stdout + p.stderr

    def abre_monitor(self, *args, **fake):
        """monitor em segundo plano com a "janela" do jogo aberta (FAKE_DORME); espera a sessao comecar"""
        p = subprocess.Popen([sys.executable, '-I', MONITOR] + list(args), stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL, env=self.env(**fake))
        for _ in range(200):
            if os.path.isdir(self.camps) and any(os.path.exists(os.path.join(self.camps, c, 'sessao_%03d' % n, 'aberto.flag'))
                                                 for c in os.listdir(self.camps) for n in range(1, 4)):
                return p
            time.sleep(0.05)
        p.kill()
        self.fail('a sessao nao abriu')

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
        ini = os.path.join(self.camps, cid, 'sessao_002', 'saves_inicio', 'card0', 'H_HUNTER.001')
        grav = os.path.join(self.camps, cid, 'sessao_001', 'saves', 'card0', 'H_HUNTER.001')
        rep = os.path.join(self.camps, cid, 'sessao_001', 'reproducao', 'saves', 'card0', 'H_HUNTER.001')
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
        p = os.path.join(self.camps, cid, 'sessao_002', 'saves_inicio', 'card0', 'H_HUNTER.001')
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

    def test_sessao_termina_fora_da_grade_dos_hashes(self):
        # achado com o jogo real (hh16_campanha_real): a sessao acaba num VBlank qualquer e a leitura se defasa do VBlank;
        # parar a reproducao no ultimo hash perdia os estados finais e toda sessao saia REPROVADA
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=650, ADIANTE=30)
        rc, out = self.run_monitor('verificar', ADIANTE=30)  # a defasagem e do jogo: vale tambem na reproducao
        self.assertEqual(rc, 0, out)
        with open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8') as f:
            r = json.load(f)['sessoes'][0]
        self.assertEqual(r['veredito'], 'REPRODUZIDA')
        self.assertEqual(r['ultima_leitura'], 680)
        self.assertTrue(r['chegou_ao_fim'])
        self.assertEqual(r['estados_conferidos'], 11)  # leituras 60..660 (VBlanks 30..630)

    def test_negativo_reproducao_que_nao_chega_ao_fim(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=650, ADIANTE=30)
        rc, out = self.run_monitor('verificar', SEM_PARADA=1, ADIANTE=30)  # um programa que ignora o fim da gravacao
        self.assertEqual(rc, 1, out)
        with open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8') as f:
            self.assertFalse(json.load(f)['sessoes'][0]['chegou_ao_fim'])

    def test_negativo_segunda_instancia(self):
        self.nova()
        p = self.abre_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600, DORME=4)
        try:
            for cmd in ('jogar', 'verificar'):
                rc, out = self.run_monitor(cmd, ENTRADA='1 A', FIM=60)
                self.assertNotEqual(rc, 0, out)
                self.assertIn('ja esta aberta em outro monitor', out)
        finally:
            p.wait(30)
        self.assertEqual(p.returncode, 0)
        with open(os.path.join(self.camps, self.cid(), 'campanha.json'), encoding='utf-8') as f:
            self.assertEqual([s['situacao'] for s in json.load(f)['sessoes']], ['gravada'])
        self.assertFalse(os.path.exists(os.path.join(self.camps, self.cid(), '.trava')))

    def test_monitor_cai_e_a_sessao_e_recuperada(self):
        self.nova()
        p = self.abre_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600, DORME=3)
        p.kill()  # o monitor cai; o "jogo" continua aberto
        p.wait(10)
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60)
        self.assertNotEqual(rc, 0, out)
        self.assertIn('ainda esta aberto', out)
        cid = self.cid()
        with open(os.path.join(self.camps, cid, 'campanha.json'), encoding='utf-8') as f:
            pid = json.load(f)['sessoes'][0]['pid']
        self.assertRegex(pid, r'^\d+:\d*$')  # pid e momento de criacao (um PID reaproveitado nao engana)
        time.sleep(3.5)  # o jogo fecha normalmente (FAKE_DORME), com o comentario de fim
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 0, out)
        self.assertIn('sessao 1 recuperada', out)
        self.assertIn('vale inteira', out)
        with open(os.path.join(self.camps, cid, 'relatorio.json'), encoding='utf-8') as f:
            self.assertEqual(json.load(f)['sessoes'][0]['veredito'], 'REPRODUZIDA')  # nao fica "interrompida" para sempre
        rc, out = self.run_monitor('status')
        self.assertIn('(recuperada)', out)

    def test_monitor_cai_e_o_jogo_cai_depois(self):
        self.nova()
        p = self.abre_monitor('jogar', ENTRADA='100 A;200 SAVE', FIM=600, QUEDA=350, DORME=3, PARCIAL_LOG=1)
        p.kill()
        p.wait(10)
        time.sleep(3.5)
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 0, out)
        self.assertIn('vale o que foi gravado ate ali', out)
        with open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8') as f:
            self.assertEqual(json.load(f)['sessoes'][0]['veredito'], 'REPRODUZIDA_ATE_A_QUEDA')  # logs pela metade tolerados

    def test_trava_de_monitor_antigo_com_pid_reaproveitado(self):
        self.nova()
        # uma trava deixada por um monitor que caiu, com o PID de um processo vivo (este teste) e outro momento de criacao
        with open(os.path.join(self.camps, self.cid(), '.trava'), 'w') as f:
            f.write('%d:1' % os.getpid())
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60)
        self.assertEqual(rc, 0, out)
        self.assertTrue(os.path.exists(os.path.join(self.camps, self.cid(), '.trava.antiga')))

    def test_reproducao_ignora_variaveis_hh_do_terminal(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        rc, out = self.run_monitor('verificar', _env={'HH_INPUT_SCRIPT': 'roteiro.txt'})  # o produto recusaria
        self.assertEqual(rc, 0, out)
        self.assertIn('sessao 1: REPRODUZIDA', out)

    def test_negativo_sessao_sem_quadros_reprova_ate_campanha_de_teste(self):
        self.nova('--teste')
        self.run_monitor('jogar', ENTRADA='100 A', FIM=120, SEM_QUADROS=1)
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 1, out)
        self.assertIn('REPROVADA', out)
        self.assertNotIn('TESTE REPRODUZIDO', out)
        cid = self.cid()
        rc, out = self.run_monitor('reproduzir-em', cid, '--instalacao', self.inst)
        self.assertEqual(rc, 1, out)
        self.assertIn('DIVERGENTE', out)

    def test_negativo_reproduzir_em_com_a_campanha_aberta(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        p = self.abre_monitor('jogar', ENTRADA='1 A', FIM=60, DORME=3)
        try:
            rc, out = self.run_monitor('reproduzir-em', self.cid(), '--instalacao', self.inst)
            self.assertNotEqual(rc, 0, out)
            self.assertIn('ja esta aberta', out)
        finally:
            p.wait(30)
    def test_queda_com_a_ultima_linha_pela_metade(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;200 SAVE', FIM=600, QUEDA=350, PARCIAL='40')
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 0, out)
        cid = self.cid()
        with open(os.path.join(self.camps, cid, 'relatorio.json'), encoding='utf-8') as f:
            self.assertEqual(json.load(f)['sessoes'][0]['veredito'], 'REPRODUZIDA_ATE_A_QUEDA')
        # a gravacao selada continua como foi gravada
        with open(os.path.join(self.camps, cid, 'sessao_001', 'entrada.txt'), encoding='ascii') as f:
            self.assertTrue(f.read().endswith('\n40'))

    def test_negativo_campanha_real_recusa_roteiro_de_teste(self):
        self.nova()
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60, _env={'HH_INPUT_SCRIPT': 'roteiro.txt'})
        self.assertNotEqual(rc, 0, out)
        self.assertIn('recusa roteiro de teste', out)
        self.assertFalse(os.path.exists(os.path.join(self.camps, self.cid(), 'sessao_001')))

    def test_campanha_de_teste_aceita_roteiro(self):
        self.nova('--teste')
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60, _env={'HH_INPUT_SCRIPT': 'roteiro.txt'})
        self.assertEqual(rc, 0, out)

    def test_variaveis_hh_nao_chegam_ao_jogo(self):
        self.nova()
        saida = os.path.join(self.dir, 'env.txt')
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60, ENV_SAIDA=saida, _env={'HH_DATA_DIR': 'x'})
        self.assertEqual(rc, 0, out)
        self.assertEqual(open(saida).read().split(), ['HH_CAMPANHA_EXE'])  # so a do proprio teste

    def test_verificacao_paralela_e_cobertura(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        self.run_monitor('jogar', ENTRADA='50 B;400 SAVE;500 CREDITOS', FIM=600)
        rc, out = self.run_monitor('verificar', '--paralelo', '2')
        self.assertEqual(rc, 0, out)
        self.assertIn('CAMPANHA REPRODUZIDA ATE OS CREDITOS', out)
        self.assertLess(out.index('sessao 1:'), out.index('sessao 2:'))  # na ordem, mesmo em paralelo
        with open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8') as f:
            rel = json.load(f)
        self.assertEqual(rel['cobertura']['areas'], ['AHFDATA/GAMEEND.AHF'])
        self.assertEqual(rel['sessoes'][1]['cobertura']['arquivos'], 1)

    def test_jogar_criar_sem_campanha(self):
        env = {'HH_CAMPANHA_INSTALACAO': self.inst, 'HH_CAMPANHA_COPIA': os.path.join(self.dir, 'copia')}
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60, _env=env)
        self.assertNotEqual(rc, 0, out)  # sem --criar, nada e criado
        rc, out = self.run_monitor('jogar', '--criar', ENTRADA='1 A', FIM=60, _env=env)
        self.assertEqual(rc, 0, out)
        rc, out = self.run_monitor('jogar', '--criar', ENTRADA='2 B', FIM=60, _env=env)
        self.assertEqual(rc, 0, out)
        self.assertEqual(len([d for d in os.listdir(self.camps) if not d.startswith('_')]), 1)  # continua a mesma
        cid = self.cid()
        self.assertTrue(os.path.isdir(os.path.join(self.dir, 'copia', cid, 'sessao_002')))  # copia da variavel

    def test_jogar_criar_nunca_continua_campanha_de_teste(self):
        env = {'HH_CAMPANHA_INSTALACAO': self.inst}
        self.nova('--teste')  # a sessao piloto
        teste = self.cid()
        time.sleep(1.1)  # ids por segundo
        rc, out = self.run_monitor('jogar', '--criar', ENTRADA='1 A', FIM=60, _env=env)
        self.assertEqual(rc, 0, out)
        ids = sorted(d for d in os.listdir(self.camps) if not d.startswith('_'))
        self.assertEqual(len(ids), 2)
        self.assertFalse(os.path.exists(os.path.join(self.camps, teste, 'sessao_001')))  # o teste ficou intocado
        real = [d for d in ids if d != teste][0]
        self.assertTrue(os.path.isdir(os.path.join(self.camps, real, 'sessao_001')))

    def test_quadros_da_janela_vulkan_so_informam_e_a_referencia_vincula(self):
        # achado na revisao de 2026-11-15: a janela grava no Vulkan, que nao e identico bit a bit ao software da
        # reproducao; comparar os dois reprovaria toda sessao real
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600, JANELA_VK=1)
        rc, out = self.run_monitor('verificar')
        self.assertEqual(rc, 0, out)
        self.assertIn('referencia de quadros em software fixada', out)
        cid = self.cid()
        res = lambda: json.load(open(os.path.join(self.camps, cid, 'relatorio.json'), encoding='utf-8'))['sessoes'][0]
        r = res()
        self.assertEqual(r['veredito'], 'REPRODUZIDA')
        self.assertEqual(r['quadro_da_janela_diferente'], 60)
        self.assertTrue(r['referencia_criada'])
        with open(os.path.join(self.camps, cid, 'campanha.json'), encoding='utf-8') as f:
            self.assertIn(os.path.join('sessao_001', 'quadros_referencia.txt'), json.load(f)['selos'])
        rc, out = self.run_monitor('verificar')  # a segunda verificacao compara com a referencia
        self.assertEqual(rc, 0, out)
        r = res()
        self.assertEqual(r['quadros_comparados_com'], 'referencia')
        self.assertIsNone(r['primeiro_quadro_diferente'])
        # negativo: depois de fixada, a referencia vincula (um programa cuja imagem diverge e acusado)
        rc, out = self.run_monitor('reproduzir-em', cid, '--instalacao', self.inst, '--rotulo', 'x', DIVERGE_QUADROS=240)
        self.assertEqual(rc, 1, out)
        r = json.load(open(os.path.join(self.camps, cid, 'outros', 'x', 'sessao_001', 'resultado.json'), encoding='utf-8'))
        self.assertEqual(r['primeiro_quadro_diferente'], 240)

    def test_negativo_imagem_diverge_sem_referencia_com_estado_igual_nao_reprova_mas_informa(self):
        self.nova()
        self.run_monitor('jogar', ENTRADA='100 A;300 SAVE', FIM=600)
        rc, out = self.run_monitor('verificar', DIVERGE_QUADROS=240)
        self.assertEqual(rc, 0, out)  # o estado e os saves iguais: a imagem da janela so informa
        r = json.load(open(os.path.join(self.camps, self.cid(), 'relatorio.json'), encoding='utf-8'))['sessoes'][0]
        self.assertEqual(r['quadro_da_janela_diferente'], 240)

    def test_janela_monta(self):
        rc, out = self.run_monitor('janela', '--so-montar')
        self.assertEqual(rc, 0, out)
        self.assertIn('4 botoes', out)

    def test_negativo_programa_anterior_ao_contrato_recusado(self):
        # um programa que reproduz mas nao para no fim da gravacao (o de 2026-11-14): a prova por comportamento recusa
        rc, out = self.run_monitor('nova', '--instalacao', self.inst, SEM_PARADA=1)
        self.assertNotEqual(rc, 0, out)
        self.assertIn('nao cumpre o contrato da gravacao', out)
        self.assertEqual([d for d in os.listdir(self.camps) if not d.startswith('_')], [])

    def test_dados_trocados_recusados(self):
        self.nova()
        with open(os.path.join(self.inst, 'data', 'arquivos.manifest'), 'a') as f:
            f.write('outro disco\n')
        rc, out = self.run_monitor('jogar', ENTRADA='1 A', FIM=60)
        self.assertNotEqual(rc, 0, out)
        self.assertIn('dados da instalacao mudaram', out)


if __name__ == '__main__':
    unittest.main(verbosity=2)
