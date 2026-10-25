// SPDX-License-Identifier: MIT
// Interface de memoria guest - HH03.
// O executor do nucleo SH-4 acessa memoria exclusivamente por esta
// interface; a implementacao concreta pertence ao host (HH05+, HLE de
// boot) ou aos testes (RAM plana). O SH-4 gera excecao em acessos nao
// alinhados: aqui o concreto decide (hook documentado; o modelo HH03 nao
// implementa o clock do barramento).
#pragma once

#include <cstdint>

namespace hh::sh4 {

class Sh4Bus {
public:
    virtual ~Sh4Bus() = default;

    virtual uint8_t read8(uint32_t addr) = 0;
    virtual uint16_t read16(uint32_t addr) = 0;
    virtual uint32_t read32(uint32_t addr) = 0;
    virtual void write8(uint32_t addr, uint8_t value) = 0;
    virtual void write16(uint32_t addr, uint16_t value) = 0;
    virtual void write32(uint32_t addr, uint32_t value) = 0;
};

} // namespace hh::sh4
