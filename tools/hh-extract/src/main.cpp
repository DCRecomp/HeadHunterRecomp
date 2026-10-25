// SPDX-License-Identifier: MIT
// hh-extract: extracao de IP.BIN, 1ST_READ.BIN e dados ISO9660 de uma imagem
// GD-ROM (CDI), somente leitura, com manifest.json de hashes.
//   --runtime: prepara os dados de execucao do jogo nativo (runtime_data.hpp) e confere
//   todos os setores de dados contra a imagem (HH05.1).
#include "hh_extract/cdi.hpp"
#include "hh_extract/extract.h"
#include "hh_extract/runtime_data.hpp"

#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

namespace {

void usage() {
    std::fprintf(stderr,
                 "uso: hh-extract [--info] [--force] <imagem.cdi> [saida]\n"
                 "     hh-extract --runtime <imagem.cdi> <pasta de dados>\n"
                 "     hh-extract --verify-data <imagem.cdi> <pasta de dados>\n"
                 "     hh-extract --juntar-discos <dados do disco 1> <dados do disco 2> [--links]\n"
                 "  saida      pasta de destino (padrao: game-data)\n"
                 "  --info     so mostra faixas, IP.BIN e hash do executavel\n"
                 "  --force    extrai mesmo com hash de executavel desconhecido\n"
                 "  --runtime  dados de execucao do jogo nativo (o jogo nao le o CDI), conferidos setor a setor\n"
                 "  --verify-data  compara todos os setores de dados da imagem com os da pasta\n"
                 "  --juntar-discos  junta o disco 2 nos dados do disco 1 (instalacao unica; MOVE os arquivos\n"
                 "                   do disco 2, ou --links para ligar sem mover)\n");
}

int runtimeData(bool prepare, const std::string& image, const std::string& out) {
    hh::CdiImage img(image);
    if (prepare) {
        const auto r = hh::disc::prepare_runtime_data(img, out);
        std::cout << "dados de execucao: " << r.versionId << ", " << r.files << " arquivos, " << r.bytes
                  << " bytes, " << r.metaSectors << " setores de sistema\n";
    }
    const std::string diff = hh::disc::verify_runtime_data(img, out);
    if (!diff.empty()) {
        std::cerr << "erro: dados de execucao diferem da imagem: " << diff << "\n";
        return 1;
    }
    const std::string files = hh::disc::check_runtime_files(out);
    if (!files.empty()) {
        std::cerr << "erro: " << files << "\n";
        return 1;
    }
    std::cout << "conferido: todos os setores de dados iguais aos da imagem; hashes dos arquivos ok\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    // HH08: hh-extract --juntar-discos <dados do disco 1> <dados do disco 2> [--links]
    // junta o disco 2 nos dados do disco 1 (instalacao unica); --links liga em vez de mover.
    if ((argc == 4 || argc == 5) && std::string(argv[1]) == "--juntar-discos") {
        if (argc == 5 && std::string(argv[4]) != "--links") {  // revisao Codex: nada de mover por engano
            std::cerr << "erro: opcao desconhecida '" << argv[4] << "' (so --links)\n";
            usage();
            return 2;
        }
        try {
            const bool links = argc == 5;
            const auto r     = hh::disc::merge_runtime_discs(argv[2], argv[3], links);
            std::cout << "discos juntos: " << r.added << " arquivos do disco 2, " << r.identical << " iguais, "
                      << r.variants << " variantes em " << hh::disc::kDisc2Variants << "\n";
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "erro: " << e.what() << "\n";
            return 1;
        }
    }
    if (argc == 4 && (std::string(argv[1]) == "--runtime" || std::string(argv[1]) == "--verify-data")) {
        try {
            return runtimeData(std::string(argv[1]) == "--runtime", argv[2], argv[3]);
        } catch (const std::exception& e) {
            std::cerr << "erro: " << e.what() << "\n";
            return 1;
        }
    }
    hh::ExtractOptions opt;
    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--info") opt.info_only = true;
        else if (a == "--force") opt.force = true;
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') { usage(); return 2; }
        else if (positional == 0) { opt.image = a; ++positional; }
        else if (positional == 1) { opt.out = a; ++positional; }
        else { usage(); return 2; }
    }
    if (positional == 0) { usage(); return 2; }
    try {
        return hh::runExtract(opt, std::cout);
    } catch (const std::exception& e) {
        std::cerr << "erro: " << e.what() << "\n";
        return 1;
    }
}
