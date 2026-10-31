// SPDX-License-Identifier: MIT
#pragma once
// HH15.11b (elevacao): a biblioteca matematica do SDK em C++ nativo, em precisao simples, com as mesmas operacoes na
// mesma ordem (o arredondamento e o modo corrente de ponto flutuante, o do jogo), para o resultado sair bit a bit igual.
#include <cmath>
#include <cstdint>
#include <cstring>

namespace hh::native::sdk {

inline uint32_t bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
inline float de_bits(uint32_t u) {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

inline constexpr uint32_t kExpoente = 0x7F800000u, kMantissa = 0x007FFFFFu, kSinal = 0x80000000u;

// Conversao de float para inteiro do jogo: truncada para zero e saturada (NaN da 0x80000000).
inline int32_t truncar_saturado(double v) {
    if (std::isnan(v)) return INT32_MIN;
    if (v >= 2147483648.0) return INT32_MAX;
    if (v < -2147483648.0) return INT32_MIN;
    return int32_t(v);
}

// Resto de x por y com o quociente truncado (a rotina de reducao do SDK): resto = |x| - trunc(|x|/|y|) * |y| com o sinal
// de x; quociente = o truncado (acima de 2^31, menos 2^32), saturado. x infinito ou NaN, y NaN ou zero: quociente 0 e
// `invalido` (o NaN do SDK); y infinito: quociente 0 e x.
struct RestoComQuociente {
    enum class Caso : uint8_t { XEspecial, YNaN, YInfinito, YZero, Normal };
    float   resto      = 0;
    int32_t quociente  = 0;
    Caso    caso       = Caso::Normal;
    bool    jaInteiro  = false;  // |x|/|y| >= 2^23: ja inteiro, sem truncar
    bool    acimaDe2a31 = false;
    // os valores intermediarios do caso normal (a elevada os deixa onde a rotina do SDK deixa)
    float divisao = 0, produto = 0, restoSemSinal = 0;  // |x|/|y|, t * |y|, |x| - t * |y|
};

inline RestoComQuociente resto_com_quociente(float x, float y, float invalido) {
    using Caso = RestoComQuociente::Caso;
    RestoComQuociente r;
    const uint32_t    xb = bits(x), yb = bits(y);
    if ((xb & kExpoente) == kExpoente) r.caso = Caso::XEspecial;
    else if ((yb & kExpoente) == kExpoente) r.caso = (yb & kMantissa) ? Caso::YNaN : Caso::YInfinito;
    else if (y == 0.0f) r.caso = Caso::YZero;
    if (r.caso == Caso::YInfinito) r.resto = x;
    if (r.caso != Caso::Normal) {
        if (r.caso != Caso::YInfinito) r.resto = invalido;
        return r;
    }
    const float    ax = de_bits(xb & ~kSinal), ay = de_bits(yb & ~kSinal);
    const float    q  = ax / ay;
    const uint32_t qb = bits(q);
    float          t  = q;
    r.jaInteiro       = int32_t(qb & kExpoente) >= 0x4B000000;
    if (!r.jaInteiro) t = float(truncar_saturado(double(q)));
    t                 = de_bits(bits(t) | (qb & kSinal));
    r.acimaDe2a31     = t > 2147483648.0f;
    r.quociente       = truncar_saturado(double(r.acimaDe2a31 ? t + -4294967296.0f : t));
    const float m     = t * ay;
    r.divisao         = q;
    r.produto         = m;
    r.restoSemSinal   = ax - m;
    r.resto           = de_bits(bits(r.restoSemSinal) | (xb & kSinal));
    return r;
}

// ldexpf do SDK: x * 2^n pelo expoente. Infinito ou NaN: NaN da `invalido`, infinito fica (com o sinal); zero ou
// denormalizado: zero com o sinal; expoente resultante ate 0: zero com o sinal; de 255 em diante: infinito com o sinal
// (n >= 0) ou zero (n < 0).
struct Ldexp {
    enum class Caso : uint8_t { Nan, MenosInfinito, MaisInfinito, Zero, Normal, Estouro, Subestouro };
    float resultado = 0;
    Caso  caso      = Caso::Normal;
    bool  negativo  = false;
};

inline Ldexp ldexp_sdk(float x, int32_t n, float invalido) {
    using Caso = Ldexp::Caso;
    Ldexp          r;
    const uint32_t xb = bits(x);
    r.negativo        = (xb & kSinal) != 0;
    const uint32_t e  = xb & kExpoente;
    if (e == kExpoente) {
        r.caso      = (xb & kMantissa) ? Caso::Nan : r.negativo ? Caso::MenosInfinito : Caso::MaisInfinito;
        r.resultado = r.caso == Caso::Nan ? invalido : x;
        return r;
    }
    if (e == 0) {
        r.caso      = Caso::Zero;
        r.resultado = de_bits(xb & kSinal);
        return r;
    }
    const int32_t e2 = int32_t((e >> 23) + uint32_t(n));  // soma com volta em 32 bits, como o SH-4
    if (e2 > 0 && e2 < 255) {
        r.resultado = de_bits((xb & kSinal) | (uint32_t(e2) << 23) | (xb & kMantissa));
        return r;
    }
    if (n >= 0) {
        r.caso      = Caso::Estouro;
        r.resultado = de_bits((xb & kSinal) | kExpoente);
    } else {
        r.caso      = Caso::Subestouro;
        r.resultado = de_bits(xb & kSinal);
    }
    return r;
}

// Horner em x2 com 7 coeficientes: p = c[0]; p = x2 * p + c[i] (o produto e a soma arredondados um de cada vez, como o
// fmac do SH-4).
inline float horner7(float x2, const float (&c)[7]) {
    float p = c[0];
    for (int i = 1; i < 7; ++i) {
        const float m = x2 * p;
        p             = m + c[i];
    }
    return p;
}

// Os nucleos de cosseno e de seno do SDK no intervalo reduzido: P(x^2) e x * Q(x^2).
inline float cos_nucleo(float x, const float (&c)[7]) { return horner7(x * x, c); }
inline float sen_nucleo(float x, const float (&c)[7]) {
    const float p = horner7(x * x, c);
    return x * p;
}

} // namespace hh::native::sdk
