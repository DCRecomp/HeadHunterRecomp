// SPDX-License-Identifier: MIT
#pragma once
// HH08.6: arquivos do jogo conferidos contra o manifesto do Builder (disc.manifest: tamanho e
// SHA-256 de cada arquivo do disco). Envolve o DataFileSystem: na primeira abertura de cada
// arquivo, le o arquivo inteiro e confere; um arquivo alterado ou truncado devolve
// Error::Corrupt (o adaptador gdFs para o jogo com mensagem). Recursos substituidos por mod nao
// sao conferidos (diferem do disco por definicao) e arquivos fora do manifesto sao recusados.
// Nada de setor, LBA ou CDI: o manifesto entra so como lista (nome, tamanho, SHA-256).
#include "platform/data_files.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace hh {

class VerifiedFileSystem final : public native::files::IFileSystem {
public:
    // Lanca std::runtime_error se o manifesto nao puder ser lido ou nao listar arquivos.
    VerifiedFileSystem(native::files::DataFileSystem& inner, const std::filesystem::path& manifest);

    native::Result<native::files::FileHandle> open(std::string_view path) override;
    native::Result<uint64_t> size(native::files::FileHandle f) const override { return inner_.size(f); }
    native::Result<size_t>   read(native::files::FileHandle f, uint64_t offset, std::span<std::byte> out) override {
        return inner_.read(f, offset, out);
    }
    void close(native::files::FileHandle f) override { inner_.close(f); }
    native::Result<native::files::RequestId> read_async(native::files::FileHandle f, uint64_t offset,
                                                        std::span<std::byte> out) override {
        return inner_.read_async(f, offset, out);
    }
    native::files::RequestStatus poll(native::files::RequestId r) override { return inner_.poll(r); }
    void                         cancel(native::files::RequestId r) override { inner_.cancel(r); }
    native::Result<std::vector<native::files::DirEntry>> list(std::string_view directory) const override {
        return inner_.list(directory);
    }

    size_t      verified() const { return ok_.size(); }
    std::string last_problem() const { return problem_; }  // ultimo arquivo recusado e o motivo

private:
    struct Expected {
        uint64_t    size = 0;
        std::string sha256;
    };
    native::files::DataFileSystem&  inner_;
    std::map<std::string, Expected> expected_;  // nome logico canonico
    std::set<std::string>           ok_, modded_;
    std::string                     problem_;
};

} // namespace hh
