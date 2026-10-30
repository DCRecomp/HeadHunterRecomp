// SPDX-License-Identifier: MIT
#pragma once
// Angulos do jogo (HH15.10, elevacao): radianos em precisao simples, calculados como o jogo calcula (cada volta somada
// ou subtraida e uma operacao arredondada, no modo de arredondamento corrente).
#include <numbers>

namespace hh::native::game {

inline constexpr float kPi    = std::numbers::pi_v<float>;
inline constexpr float kVolta = 2.0f * kPi;

// Os testes de cada volta da normalizacao (o jogo relê o angulo a cada passo).
inline bool abaixo_do_intervalo(float a) { return a < -kPi; }
inline bool acima_do_intervalo(float a) { return a > kPi; }

// Leva o angulo a [-pi, pi] somando ou subtraindo uma volta de cada vez. NaN fica como esta.
inline float normalizar_angulo(float a) {
    while (abaixo_do_intervalo(a)) a += kVolta;
    while (acima_do_intervalo(a)) a -= kVolta;
    return a;
}

} // namespace hh::native::game
