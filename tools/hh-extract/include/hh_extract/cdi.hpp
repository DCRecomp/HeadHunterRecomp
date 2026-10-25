// SPDX-License-Identifier: MIT
// Leitor CDI v3.x (linear, deterministico) - HH02.
// Implementado a partir do formato de arquivo CDI (DiscJuggler), espelhando
// o leitor linear validado empiricamente (hh_discinfo.py, HH01).
#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace hh {

// Erro de disco/formatacao (arquivo invalido, truncado ou nao suportado).
struct DiscError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Track {
    uint32_t session = 0;
    uint32_t number = 0; // 1-based global
    uint32_t mode = 0;   // 0=audio, 1=mode1, 2=mode2
    uint32_t pregap = 0;
    uint32_t length = 0;     // setores de dados
    uint32_t start_lba = 0;
    uint32_t total = 0;      // pregap + length (+ margem, se houver)
    uint32_t sector_size = 0;
    uint32_t code = 0;       // 0/1/2 conforme arquivo CDI
    uint64_t file_offset = 0;
    std::string name;

    uint32_t userOffset() const;
    uint32_t endLba() const { return start_lba + length; }
    bool contains(uint32_t lba) const { return start_lba <= lba && lba < endLba(); }
    bool isData() const { return mode != 0; }
    uint64_t pos(uint32_t lba) const {
        return file_offset + (uint64_t)(lba - start_lba) * sector_size + userOffset();
    }
};

// Imagem CDI aberta somente para leitura. User data e sempre lido com
// passada por setor (nunca leitura continua atraves de faixas/pregaps).
class CdiImage {
public:
    explicit CdiImage(const std::filesystem::path& path);

    const std::filesystem::path& path() const { return path_; }
    const std::vector<Track>& tracks() const { return tracks_; }
    std::string formatName() const { return format_; }
    uint64_t fileSize() const { return file_size_; }

    // Faixa de boot: ultima faixa de dados (sessao final), padrao do formato.
    const Track& bootTrack() const;

    // Le os 2048 bytes de usuario do setor lba (little por faixa).
    void readUserData(uint32_t lba, void* dst);

    // Le `size` bytes de usuario a partir de lba, chamando sink por bloco.
    template <typename Sink>
    void readExtent(uint32_t lba, uint64_t size, Sink&& sink) {
        uint32_t lbaCur = lba;
        uint64_t left = size;
        static constexpr size_t kBufSectors = 64;
        std::vector<uint8_t> buf(kBufSectors * 2048);
        while (left > 0) {
            const Track* t = findTrack(lbaCur);
            // setores contiguos na mesma faixa
            uint32_t run = 1;
            while (run < kBufSectors && run < left && t->contains(lbaCur + run)) ++run;
            uint64_t raw = (uint64_t)run * 2048;
            file_.clear();
            file_.seekg((std::streamoff)t->pos(lbaCur), std::ios::beg);
            if (!file_.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)raw)
                || (uint64_t)file_.gcount() != raw) {
                throw DiscError("leitura truncada na LBA " + std::to_string(lbaCur));
            }
            uint64_t take = (uint64_t)run * 2048;
            if (take > left) take = left;
            sink(buf.data(), (size_t)take);
            left -= take;
            lbaCur += (uint32_t)((take + 2047) / 2048);
        }
    }

    // Extracao completa para memoria (arquivos pequenos; boot ~1,5 MB ok).
    std::vector<uint8_t> readExtentAll(uint32_t lba, uint64_t size);

private:
    const Track* findTrack(uint32_t lba) const;
    void parseHeader(const std::vector<uint8_t>& content, uint32_t version);

    std::filesystem::path path_;
    std::ifstream file_;
    uint64_t file_size_ = 0;
    uint64_t header_pos_ = 0;
    std::string format_;
    std::vector<Track> tracks_;
};

} // namespace hh
