// SPDX-License-Identifier: MIT
// hh-builder (HH06, Builder inicial): do CDI do usuario ao Preview HeadHunter.exe, na maquina
// do usuario. Etapas (inspiradas no Builder do Blue Stinger, builder/builder_core.h, MIT):
//   1. dados de execucao: prepara <saida>\.hh-build\data a partir do CDI e confere todos os
//      setores de dados contra a imagem (exige a versao suportada);
//   2. recompila o 1ST_READ.BIN dos dados em C++ (<saida>\.hh-build\generated);
//   3. configura e compila o kit de fontes com CMake (<saida>\.hh-build\cmake);
//   4. monta <saida>\HeadHunter\ (HeadHunter.exe + data\) numa pasta nova e so entao troca a
//      instalacao anterior;
//   5. verifica a instalacao montada: o proprio HeadHunter.exe copiado, --headless, ate um
//      numero de VBlanks, com log de hashes. Pastas so sao removidas se tiverem o marcador do
//      Builder; troca interrompida e restaurada na execucao seguinte; uma execucao por saida.
// Uma falha em qualquer etapa deixa a instalacao anterior intacta. O jogo instalado nao le o CDI.
//
//   hh-builder --disc <disco 1.cdi> --disc2 <disco 2.cdi> --out <pasta> [--kit <fontes>] [--verify-vblanks N]
// HH08: os dois discos (cada um conferido contra o proprio CDI e a versao: o disco 1 e
// hedhunter_v1006_a, o 2 e hedhunter_v1006_b) viram uma instalacao unica, sem troca de disco.
#include "app/recomp.h"
#include "hh_extract/cdi.hpp"
#include "hh_extract/runtime_data.hpp"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

#ifndef HH_KIT_DIR
#define HH_KIT_DIR "."
#endif

namespace {

std::string text(const fs::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::string quote(const fs::path& p) { return "\"" + text(p) + "\""; }

void step(int n, const char* what) { std::cout << "[" << n << "/5] " << what << std::endl; }

// Executa um processo externo (CMake, verificacao) pela linha de comando; rc 0 = sucesso.
int run(const std::string& cmd) {
    std::cout << "  > " << cmd << std::endl;
    // cmd.exe remove o primeiro e o ultimo aspas de /c "..."; o par externo protege os internos
    return std::system(("\"" + cmd + "\"").c_str());
}

int usage() {
    std::fprintf(stderr, "uso: hh-builder --disc <disco 1.cdi> --disc2 <disco 2.cdi> --out <pasta> [--kit <fontes>] [--verify-vblanks N]\n");
    return 2;
}

// Pastas que o Builder cria e pode apagar levam este marcador; sem ele, nada e removido.
constexpr const char* kMarker = ".hh-builder";

bool owned(const fs::path& dir) { return fs::exists(dir / kMarker); }

void mark(const fs::path& dir) {
    std::ofstream(dir / kMarker) << "pasta criada pelo hh-builder\n";
}

// Remove so pastas do proprio Builder (revisao Codex HH06: nada de remove_all sem dono).
void removeOwned(const fs::path& dir) {
    if (!fs::exists(dir)) return;
    if (!owned(dir)) throw std::runtime_error("a pasta " + text(dir) + " existe e nao foi criada pelo Builder; remova-a ou escolha outra saida");
    fs::remove_all(dir);
}

// Caminhos vao para a linha de comando (cmd.exe): aspas e % mudariam o comando.
void checkPath(const fs::path& p, const char* what) {
    const std::string t = text(p);
    if (t.find('"') != std::string::npos || t.find('%') != std::string::npos)
        throw std::runtime_error(std::string(what) + " com caractere nao suportado (\" ou %): " + t);
}

// Uma execucao por saida: o diretorio de trava e criado de forma atomica.
struct Lock {
    fs::path dir;
    explicit Lock(const fs::path& d) : dir(d) {
        if (!fs::create_directory(dir))
            throw std::runtime_error("outro Builder parece estar rodando nesta saida (" + text(dir) +
                                     "); se nao estiver, apague essa pasta");
    }
    ~Lock() {
        std::error_code ec;
        fs::remove(dir, ec);
    }
};

// HH11: dados do usuario dentro da instalacao (saves\ do modo portatil, controles.toml). Passa de `from` para `to`,
// arquivo por arquivo, o que ainda nao existe em `to`; um arquivo igual nos dois sai de `from`; um diferente fica em
// `from` (nunca sobrescrito: a decisao e do usuario). Devolve true se `from` ficou sem nenhum dado do usuario (so
// entao pode ser apagada; revisao Codex HH11).
bool sameFile(const fs::path& a, const fs::path& b) {
    std::ifstream x(a, std::ios::binary), y(b, std::ios::binary);
    return x && y && std::equal(std::istreambuf_iterator<char>(x), {}, std::istreambuf_iterator<char>(y), {});
}

bool carryOne(const fs::path& a, const fs::path& b) {  // um arquivo
    std::error_code ec;
    if (!fs::exists(b)) {
        fs::create_directories(b.parent_path(), ec);
        fs::rename(a, b, ec);
    } else if (sameFile(a, b)) fs::remove(a, ec);
    if (fs::exists(a)) {
        std::cerr << "aviso: " << text(a) << " nao passou para " << text(b) << (ec ? ": " + ec.message() : " (ja existe diferente)")
                  << std::endl;
        return false;
    }
    std::cout << "dados do usuario mantidos: " << text(b) << std::endl;
    return true;
}

bool carryUserData(const fs::path& from, const fs::path& to) {
    bool clean = true;
    for (const char* item : {"saves", "controles.toml"}) {
        const fs::path a = from / item, b = to / item;
        if (!fs::exists(a)) continue;
        if (!fs::is_directory(a)) {
            clean = carryOne(a, b) && clean;
            continue;
        }
        std::vector<fs::path> files;
        for (const auto& e : fs::recursive_directory_iterator(a))
            if (e.is_regular_file()) files.push_back(e.path());
        for (const fs::path& f : files) clean = carryOne(f, b / fs::relative(f, a)) && clean;
        // pastas vazias que sobraram (de baixo para cima)
        std::vector<fs::path> dirs;
        for (const auto& e : fs::recursive_directory_iterator(a))
            if (e.is_directory()) dirs.push_back(e.path());
        std::error_code ec;
        for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) fs::remove(*it, ec);  // so remove pasta vazia
        fs::remove(a, ec);
        if (fs::exists(a)) clean = false;
    }
    if (!clean) std::cerr << "aviso: " << text(from) << " fica guardada com os dados que nao passaram" << std::endl;
    return clean;
}

// Apaga uma copia de seguranca do Builder so sem dados do usuario dentro; com eles, para (nada e perdido).
void removeBackup(const fs::path& backup) {
    for (const char* item : {"saves", "controles.toml"})
        if (fs::exists(backup / item))
            throw std::runtime_error(text(backup) + " ainda guarda " + item + " de uma reinstalacao anterior que nao passou para "
                                     "a instalacao; mova esses dados e rode o Builder de novo");
    removeOwned(backup);
}

} // namespace

int main(int argc, char** argv) {
    fs::path disc, disc2, out, kit = HH_KIT_DIR;
    unsigned verifyVblanks = 600;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--disc" && i + 1 < argc) disc = argv[++i];
        else if (a == "--disc2" && i + 1 < argc) disc2 = argv[++i];
        else if (a == "--out" && i + 1 < argc) out = argv[++i];
        else if (a == "--kit" && i + 1 < argc) kit = argv[++i];
        else if (a == "--verify-vblanks" && i + 1 < argc) verifyVblanks = unsigned(std::strtoul(argv[++i], nullptr, 10));
        else return usage();
    }
    if (disc.empty() || disc2.empty() || out.empty()) return usage();
    try {
        out = fs::absolute(out).lexically_normal();
        kit = fs::absolute(kit).lexically_normal();
        if (out == out.root_path() || out == kit) throw std::runtime_error("pasta de saida invalida: " + text(out));
        checkPath(out, "pasta de saida");
        checkPath(kit, "pasta do kit");
        fs::create_directories(out);
        const fs::path work = out / ".hh-build", data = work / "data", gen = work / "generated", cmake = work / "cmake";
        const fs::path staging = out / "HeadHunter.novo", install = out / "HeadHunter", backup = out / "HeadHunter.antigo";
        if (fs::exists(work) && !owned(work)) throw std::runtime_error(text(work) + " nao foi criada pelo Builder");
        fs::create_directories(work);
        mark(work);
        Lock lock(work / "trava");

        // Troca interrompida numa execucao anterior: a instalacao ficou so na copia de seguranca.
        if (!fs::exists(install) && fs::exists(backup) && owned(backup)) {
            fs::rename(backup, install);
            std::cout << "  instalacao anterior restaurada de uma troca interrompida" << std::endl;
        }
        // Copia de seguranca que ficou com dados do usuario (passagem anterior falhou): devolvidos a instalacao
        // atual antes de qualquer coisa ser apagada
        if (fs::exists(backup) && owned(backup) && fs::exists(install) && owned(install)) carryUserData(backup, install);
        // Sem o marcador do Builder a pasta nao e dele, mesmo com um HeadHunter.exe dentro: nunca e
        // adotada, trocada ou apagada (revisao Codex HH08)
        if (fs::exists(install) && !owned(install))
            throw std::runtime_error(text(install) + " existe e nao foi criada pelo Builder; remova-a ou escolha outra saida");
        for (const fs::path& d : {staging, backup})  // falha cedo, antes de compilar
            if (fs::exists(d) && !owned(d))
                throw std::runtime_error("a pasta " + text(d) + " existe e nao foi criada pelo Builder; remova-a ou escolha outra saida");

        step(1, "dados de execucao (os dois CDIs -> data, conferidos setor a setor e juntos)");
        const fs::path data2 = work / "data_disco2";  // disco 2 antes de juntar
        for (const fs::path& d : {data, data2})
            if (fs::exists(d)) fs::remove_all(d);  // dentro de .hh-build (marcada)
        const std::pair<fs::path, fs::path> discs[] = {{disc, data}, {disc2, data2}};
        for (int n = 1; n <= 2; ++n) {
            const auto& [cdi, dir] = discs[n - 1];
            const char*  want      = n == 1 ? "hedhunter_v1006_a" : "hedhunter_v1006_b";
            hh::CdiImage img(cdi);
            const auto   r = hh::disc::prepare_runtime_data(img, dir);
            std::cout << "  disco " << n << ": versao " << r.versionId << ", " << r.files << " arquivos" << std::endl;
            if (r.versionId != want)  // --disc com o disco 2 ou --disc2 com o disco 1
                throw std::runtime_error("o CDI de " + std::string(n == 1 ? "--disc" : "--disc2") + " (" + text(cdi) +
                                         ") e a versao " + r.versionId + ", esperada " + want + " (disco " +
                                         std::to_string(n) + ")");
            if (const std::string d = hh::disc::verify_runtime_data(img, dir); !d.empty())
                throw std::runtime_error("dados de execucao do disco " + std::to_string(n) + " diferem da imagem: " + d);
        }
        // HH08: instalacao unica, sem troca de disco (o disco 2 entra na arvore do disco 1)
        const auto mr = hh::disc::merge_runtime_discs(data, data2, false);
        std::cout << "  discos juntos: " << mr.added << " arquivos do disco 2, " << mr.identical << " iguais, "
                  << mr.variants << " variantes" << std::endl;
        fs::remove_all(data2);  // sobra: os iguais e o executavel do disco 2 (dentro de .hh-build)

        step(2, "recompilacao do executavel (SH-4 -> C++)");
        hh::RecompOptions ro;
        ro.config      = kit / "config" / "headhunter.toml";
        ro.executable  = data / "disc" / "1ST_READ.BIN";
        ro.out         = gen;
        ro.analysisDir = work / "analysis";
        if (fs::exists(gen)) fs::remove_all(gen);  // dentro de .hh-build (marcada)
        hh::recomp_run(ro, std::cout);

        step(3, "compilacao (CMake + compilador C++ desta maquina)");
        if (run("cmake -S " + quote(kit) + " -B " + quote(cmake) + " -DHH_GENERATED_DIR=" + quote(gen) +
                " -DBUILD_TESTS=OFF") != 0)
            throw std::runtime_error("configuracao do CMake falhou");
        if (run("cmake --build " + quote(cmake) + " --config Release --target HeadHunter -- -m") != 0)
            throw std::runtime_error("compilacao falhou");
        const fs::path built = cmake / "app" / "Release" / "HeadHunter.exe";
        if (!fs::is_regular_file(built)) throw std::runtime_error("executavel nao encontrado: " + text(built));

        step(4, "montagem da instalacao (pasta nova)");
        removeOwned(staging);
        fs::create_directories(staging);
        mark(staging);
        fs::copy_file(built, staging / "HeadHunter.exe");
        fs::rename(data, staging / "data");  // os dados ficam com a instalacao, nao no trabalho

        step(5, "verificacao (o HeadHunter.exe montado, headless, com os dados montados)");
        const fs::path vlog = work / "verificacao.log";
        std::error_code ec;
        fs::remove(vlog, ec);
        if (run(quote(staging / "HeadHunter.exe") + " --headless --data " + quote(staging / "data") + " --vblanks " +
                std::to_string(verifyVblanks) + " --rtc 1700000000 --log " + quote(vlog) + " --hash-every 60") != 0 ||
            !fs::exists(vlog) || fs::file_size(vlog) == 0)
            throw std::runtime_error("a instalacao montada nao passou na verificacao");

        // Troca: a instalacao atual vira copia de seguranca; se a promocao falhar, ela volta.
        removeBackup(backup);
        if (fs::exists(install)) {
            fs::rename(install, backup);
        }
        try {
            fs::rename(staging, install);
        } catch (...) {
            if (fs::exists(backup) && !fs::exists(install)) fs::rename(backup, install);
            throw;
        }
        // HH11: dados do usuario da instalacao anterior passam para a nova; se algum nao passar, a anterior fica
        // como copia de seguranca (nunca apagada com dados dentro; a proxima execucao tenta de novo).
        if (carryUserData(backup, install)) removeOwned(backup);  // so depois da nova instalacao no lugar
        std::cout << "pronto: " << text(install / "HeadHunter.exe") << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "erro: " << e.what() << " (a instalacao anterior, se havia, ficou intacta)" << std::endl;
        return 1;
    }
}
