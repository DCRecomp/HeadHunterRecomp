"""Gera os icones dos executaveis (como tools/dev/gerar_icone.py do Blue Stinger, MIT).

Dois icones, que cmake/HHIcone.cmake escolhe:

    python tools/dev/gerar_icone.py
        app/res/headhunter.ico, versionado: desenho ORIGINAL do projeto, so geometria (sem logotipo, arte, fonte ou
        imagem do jogo): um visor escuro sobre uma mira, em quadrado escuro arredondado. E o das distribuicoes publicaveis.

    python tools/dev/gerar_icone.py --titulo <captura da tela de titulo>
        icone-local/headhunter.ico, FORA DO GIT (LEGAL.md): feito da tela de titulo do proprio jogo, capturada da copia
        do proprio usuario (por exemplo hh-run --data <pasta> --capture <pasta>; 640x476). Arte do jogo: so uso local.

Com o icone local presente, os builds locais o usam em todos os executaveis; a composicao e o rosto do Jack Wade com o
logotipo nos tamanhos grandes (a 512 px, para reduzir) e o rosto recortado de perto nos pequenos, onde o logotipo nao
se le. Tamanhos 16, 24, 32, 48, 64, 128 e 256 (o Windows escolhe pelo uso).
"""
import argparse
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TAMANHOS = [16, 24, 32, 48, 64, 128, 256]
FUNDO = (9, 12, 20, 255)
BORDA = (46, 96, 150, 255)

# Regioes da tela de titulo a 640x476 (x0, y0, x1, y1), medidas na captura
LOGO = (58, 84, 588, 132)
ROSTO = (196, 112, 444, 340)     # cabeca inteira, sem o logotipo por cima da testa
ROSTO_PERTO = (240, 142, 398, 300)  # oculos e barba: o que ainda se reconhece em 16 px


def mascara(px, s):
    """Quadrado arredondado de lado px (a s vezes, reduzido: borda suave)."""
    g = Image.new("L", (px * s, px * s), 0)
    ImageDraw.Draw(g).rounded_rectangle([0, 0, px * s - 1, px * s - 1], radius=px * s * 0.16, fill=255)
    return g.resize((px, px), Image.LANCZOS)


def moldura(im):
    """Fundo escuro, bordas arredondadas e um fio azul: icone preto some numa barra de tarefas escura."""
    px = im.width
    base = Image.new("RGBA", (px, px), FUNDO)
    base.alpha_composite(im.convert("RGBA"))
    if px >= 32:
        s = 4
        fio = Image.new("RGBA", (px * s, px * s), (0, 0, 0, 0))
        m = px * s * 0.004
        ImageDraw.Draw(fio).rounded_rectangle([m, m, px * s - 1 - m, px * s - 1 - m], radius=px * s * 0.155, outline=BORDA,
                                              width=max(s, int(px * s * 0.012)))
        base.alpha_composite(fio.resize((px, px), Image.LANCZOS))
    base.putalpha(ImageChops.multiply(base.getchannel("A"), mascara(px, 4)))
    return base


def desenho(px):
    """O icone original: mira fina atras de um visor escuro. So geometria."""
    s = px * 8
    im = Image.new("RGBA", (s, s), FUNDO)
    d = ImageDraw.Draw(im)
    c = s / 2
    r = s * 0.31
    azul, ambar = (70, 165, 225, 255), (235, 150, 45, 255)
    d.ellipse([c - r, c - r, c + r, c + r], outline=azul, width=max(1, int(s * 0.03)))
    for a in range(0, 360, 90):  # quatro tracos da mira, saindo do anel
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        d.line([c + ca * r * 0.72, c + sa * r * 0.72, c + ca * r * 1.32, c + sa * r * 1.32], fill=azul, width=max(1, int(s * 0.03)))
    # visor: duas lentes inclinadas unidas por uma ponte, com um brilho na esquerda
    y = s * 0.47
    vis = [(s * 0.16, y - s * 0.075), (s * 0.84, y - s * 0.075), (s * 0.80, y + s * 0.045), (s * 0.70, y + s * 0.115),
           (s * 0.57, y + s * 0.09), (s * 0.5, y + s * 0.025), (s * 0.43, y + s * 0.09), (s * 0.30, y + s * 0.115),
           (s * 0.20, y + s * 0.045)]
    d.polygon(vis, fill=(198, 210, 224, 255))
    d.line([(s * 0.25, y - s * 0.03), (s * 0.41, y - s * 0.03)], fill=(255, 255, 255, 255), width=max(1, int(s * 0.022)))
    r2 = s * 0.035
    d.ellipse([s * 0.66 - r2, y - s * 0.015 - r2, s * 0.66 + r2, y - s * 0.015 + r2], fill=ambar)
    return moldura(im.resize((px, px), Image.LANCZOS))


def ajuste(im, cor=1.1, contraste=1.08, brilho=1.0):
    return ImageEnhance.Brightness(ImageEnhance.Contrast(ImageEnhance.Color(im).enhance(cor)).enhance(contraste)).enhance(brilho)


def do_titulo(caminho):
    """Os tamanhos do icone a partir da captura da tela de titulo do proprio usuario."""
    q = Image.open(caminho).convert("RGB")
    if q.size != (640, 476):
        raise SystemExit("esperava a tela de titulo em 640x476, nao %dx%d" % q.size)

    # grande: rosto ampliado embaixo; o logotipo, inteiro, por cima (soma de luz, como na tela original)
    lado = 512
    grande = Image.new("RGB", (lado, lado), (0, 0, 0))
    a = (lado * 0.94) / (ROSTO[2] - ROSTO[0])
    alt = int((ROSTO[3] - ROSTO[1]) * a)
    rosto = ajuste(q.crop(ROSTO).resize((int(lado * 0.94), alt), Image.BICUBIC), brilho=1.12)
    grande.paste(rosto, ((lado - rosto.width) // 2, lado - alt))
    # a borda de cima do recorte corta o cabelo: fade para o fundo escuro sob o logotipo
    fade = Image.linear_gradient("L").resize((lado, lado))
    topo = lado - alt
    fade = fade.point(lambda v: min(255, max(0, int((v / 255 * lado - topo) * 255 / (lado * 0.13)))))
    grande = Image.composite(grande, Image.new("RGB", (lado, lado), (0, 0, 0)), fade)

    faixa = q.crop(LOGO)
    fl = int(lado * 0.96)
    fh = int(fl * (LOGO[3] - LOGO[1]) / (LOGO[2] - LOGO[0]) * 1.45)  # mais alto que o original, para ler a 128 px
    faixa = ajuste(faixa.resize((fl, fh), Image.BICUBIC), cor=1.2, contraste=1.15, brilho=1.15)
    # a faixa recortada se dissolve em cima e embaixo: sem linha reta atravessando a testa
    borda = Image.linear_gradient("L").resize((fl, fh)).point(lambda v: min(255, int(min(v, 255 - v) * 255 / 42)))
    faixa = Image.composite(faixa, Image.new("RGB", (fl, fh), (0, 0, 0)), borda)
    logo = Image.new("RGB", (lado, lado), (0, 0, 0))
    logo.paste(faixa, ((lado - fl) // 2, int(lado * 0.075)))
    grande = ImageChops.screen(grande, logo)

    perto = ajuste(q.crop(ROSTO_PERTO).filter(ImageFilter.GaussianBlur(0.4)), cor=1.12, contraste=1.0, brilho=0.95)

    saida = []
    for p in TAMANHOS:
        fonte = grande if p >= 128 else perto
        t = fonte
        while t.width > 2 * p:
            t = t.resize((t.width // 2, t.height // 2), Image.LANCZOS)
        t = t.resize((p, p), Image.LANCZOS)
        forca = 90 if p <= 64 else 70
        t = t.filter(ImageFilter.UnsharpMask(radius=0.7 if p <= 32 else 1.1, percent=forca, threshold=2))
        saida.append(moldura(t))
    return saida


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("saida", nargs="?")
    ap.add_argument("--titulo", help="captura da tela de titulo (640x476) da copia do PROPRIO usuario; a saida padrao vira "
                                     "icone-local/headhunter.ico (fora do git: arte do jogo, so uso local)")
    a = ap.parse_args()
    if a.titulo:
        saida = a.saida or os.path.join(REPO, "icone-local", "headhunter.ico")
        imgs = do_titulo(a.titulo)
    else:
        saida = a.saida or os.path.join(REPO, "app", "res", "headhunter.ico")
        imgs = [desenho(p) for p in TAMANHOS]
    os.makedirs(os.path.dirname(os.path.abspath(saida)), exist_ok=True)
    imgs[-1].save(saida, format="ICO", sizes=[(p, p) for p in TAMANHOS], append_images=imgs[:-1])
    print(saida)


if __name__ == "__main__":
    sys.exit(main())
