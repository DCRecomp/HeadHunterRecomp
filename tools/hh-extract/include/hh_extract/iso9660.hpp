// SPDX-License-Identifier: MIT
// ISO9660 (volume unico por faixa de dados, ECMA-119) - HH02.
// Nomes sanitizados (o disco e dado nao confiavel), caminhamento com
// profundidade limitada e registros que nao cruzam setores. Convention do
// formato da referencia: paths SEM barra inicial ("1ST_READ.BIN",
// "DISC/DISK1.ID").
#pragma once
#include "hh_extract/cdi.hpp"

#include <functional>
#include <vector>

namespace hh {

struct IsoEntry {
    std::string path; // "DIR/ARQ.EXT" sem barra inicial, componentes sanitizados
    uint32_t lba = 0;
    uint32_t size = 0;
    bool is_dir = false;
};

class Iso9660 {
public:
    // pvdLba = LBA absoluta do Primary Volume Descriptor (start_lba da faixa + 16).
    Iso9660(CdiImage& img, uint32_t pvdLba);

    const std::vector<IsoEntry>& entries() const { return entries_; }
    std::string volumeId() const { return volume_id_; }
    const IsoEntry* find(const std::string& path) const; // comparacao case-insensitive

    // Le o extent completo chamando sink(p, n) por bloco (passada por setor).
    void read(const IsoEntry& e, const std::function<void(const uint8_t*, size_t)>& sink);

    // Le o extent para memoria.
    std::vector<uint8_t> readAll(const IsoEntry& e);

private:
    void walk(uint32_t lba, uint32_t size, const std::string& prefix, int depth);

    CdiImage& img_;
    std::vector<IsoEntry> entries_;
    std::string volume_id_;
};

} // namespace hh
