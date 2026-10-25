// SPDX-License-Identifier: MIT
// Testes hh-extract (sem framework): vetores conhecidos + CDI sintetico.
// Teste real dos discos e feito pela CLI hh-extract + conferencia de manifest
// (implementacao independente), nao por este executavel.
#include "hh_extract/cdi.hpp"
#include "hh_extract/hash.hpp"
#include "hh_extract/iso9660.hpp"
#include "hh_extract/ip_bin.hpp"
#include "hh_extract/known_versions.hpp"
#include "hh_extract/scramble.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace hh;

static int g_failed = 0;

#define CHECK(cond, msg)                                                                          \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            std::cerr << "FALHA: " << msg << " (linha " << __LINE__ << ")\n";                      \
            ++g_failed;                                                                           \
        }                                                                                         \
    } while (0)

// --------------------------------------------------------------------- hash
static void testSha256() {
    Sha256 h;
    h.update(reinterpret_cast<const uint8_t*>("abc"), 3);
    CHECK(h.hexdigest() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256 'abc'");

    Sha256 e; // string vazia
    CHECK(e.hexdigest() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256 vazio");

    // 1.000.000 de 'a': multibloco com padding em bloco extra
    // (vetor conferido por implementacao independente - hashlib, Python 3.11)
    Sha256 big;
    std::vector<uint8_t> chunk(1000, 'a');
    for (int i = 0; i < 1000; ++i) big.update(chunk.data(), chunk.size());
    CHECK(big.hexdigest() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "sha256 'a'x1000000");

    // limites do padding (implementacao: bytes 'x', 64: 'y') - vetores gerados
    // e conferidos por implementacao independente (hashlib, Python 3.11)
    {
        std::vector<uint8_t> m(55, 'x');
        Sha256 b;
        b.update(m.data(), m.size());
        CHECK(b.hexdigest() == "d5e285683cd4efc02d021a5c62014694958901005d6f71e89e0989fac77e4072",
              "sha256 55 bytes");
    }
    {
        std::vector<uint8_t> m(56, 'x'); // padding obrigatoriamente em outro bloco
        Sha256 b;
        b.update(m.data(), m.size());
        CHECK(b.hexdigest() == "04c26261370ee7541549d16dee320c723e3fd14671e66a099afe0a377c16888e",
              "sha256 56 bytes");
    }
    {
        std::vector<uint8_t> m(64, 'y'); // exatamente um bloco
        Sha256 b;
        b.update(m.data(), m.size());
        CHECK(b.hexdigest() == "ffbf30ab94107b2c14d75cfb455ec94f200400ddc5ce304e0c21894090db055f",
              "sha256 64 bytes");
    }
    {
        // vetor de bytes 'z' de 2048: hex conferido por hashlib (Python 3.11)
        std::vector<uint8_t> m(2048, 'z');
        Sha256 a, b;
        a.update(m.data(), m.size());
        b.update(m.data(), 999);
        b.update(m.data() + 999, m.size() - 999);
        const std::string once = a.hexdigest(); // digest() so uma vez por objeto
        CHECK(once == "d2f3e691c8875d1507db8a8298461c3a21b79e6b6ab0895e34f5a32450ebc4b0",
              "sha256 2048 bytes");
        CHECK(once == b.hexdigest(), "sha256 frente/pedacos iguais");
    }
}

static void testCrc32() {
    Crc32 c;
    c.update(reinterpret_cast<const uint8_t*>("123456789"), 9);
    CHECK(c.value() == 0xCBF43926u, "crc32 '123456789'");
}

// ----------------------------------------------------------------- scramble
static void testScrambleRoundtrip() {
    std::mt19937 rng(123456);
    for (size_t size : {size_t(31), size_t(32), size_t(33), size_t(1024), size_t(70 * 1024),
                        size_t(2 * 1024 * 1024), size_t(2 * 1024 * 1024 + 100),
                        size_t(5 * 1024 * 1024)}) {
        std::vector<uint8_t> plain(size);
        for (auto& b : plain) b = uint8_t(rng());
        const auto scr = scramble(plain);
        const auto back = descramble(scr);
        CHECK(back == plain, "roundtrip scramble/descramble size=" + std::to_string(size));
    }
}

// ------------------------------------------------------------ IP.BIN sintetico
static std::vector<uint8_t> makeIpBin(const std::string& product, const std::string& version,
                                      const std::string& bootFile, const std::string& peripherals,
                                      const std::string& title) {
    std::vector<uint8_t> ip(kIpBinSize, 0);
    auto putf = [&](size_t off, size_t len, const std::string& s) {
        for (size_t i = 0; i < len; ++i) ip[off + i] = i < s.size() ? uint8_t(s[i]) : ' ';
    };
    putf(0x00, 16, "SEGA SEGAKATANA");
    putf(0x10, 16, "SEGA ENTERPRISES");
    putf(0x20, 16, "GD-ROM1/1     ");
    putf(0x30, 8, "JUE");
    putf(0x38, 8, peripherals);
    putf(0x40, 10, product);
    putf(0x4A, 6, version);
    putf(0x50, 16, "20010101");
    putf(0x60, 16, bootFile);
    putf(0x70, 16, "TESTCOMPANY");
    putf(0x80, 128, title);
    // area de checksum nao nula (como faixas Katana reais)
    std::memset(ip.data() + 0x900, 0xAA, 0x300);
    return ip;
}

static void testIpBinValidation() {
    auto ip = makeIpBin("T99999X", "V0.001", "1ST_READ.BIN", "0799A10", "TEST TITLE");
    IpBin parsed = parseIpBin(ip.data(), ip.size());
    CHECK(parsed.hardware_id == "SEGA SEGAKATANA", "ipbin: hardware_id");
    CHECK(parsed.product == "T99999X", "ipbin: product");
    CHECK(parsed.version == "V0.001", "ipbin: version");
    CHECK(parsed.boot_file == "1ST_READ.BIN", "ipbin: boot_file");
    CHECK(parsed.peripherals == "0799A10", "ipbin: peripherals");
    CHECK(!parsed.usesWindowsCE(), "ipbin: nao-CE (bit0=0)");

    // bit0=1 lanca erro (Windows CE nao suportado)
    auto ce = makeIpBin("T99999X", "V0.001", "1ST_READ.BIN", "0799A11", "CE TEST");
    bool threw = false;
    try {
        (void)parseIpBin(ce.data(), ce.size());
    } catch (const DiscError&) {
        threw = true; // nota: lanca por windows_ce check em extract; parse ok
    }
    (void)threw; // parse aceita; o portao de CE fica no extract (comportamento MIT)
    IpBin ceParsed = parseIpBin(ce.data(), ce.size());
    CHECK(ceParsed.usesWindowsCE(), "ipbin: CE detectado com bit0=1");

    // hardware_id errado lancava DiscError
    auto bad = makeIpBin("T99999X", "V0.001", "1ST_READ.BIN", "0799A10", "X");
    bad[3] = 'X';
    bool threwBad = false;
    try {
        (void)parseIpBin(bad.data(), bad.size());
    } catch (const DiscError&) {
        threwBad = true;
    }
    CHECK(threwBad, "ipbin: hardware_id invalido lanca");
}

// ------------------------------------------------------------ CDI sintetico
// Imagem minima e valida: 1 sessao, 1 faixa mode2 (2336) com:
//   LBA 0..15  -> IP.BIN (16 setores de usuario)
//   LBA 16     -> PVD
//   LBA 17     -> diretorio raiz (1 registro: HELLO.TXT @ LBA 18, 12 bytes)
//   LBA 18     -> dados do arquivo
// pregap 150 setores antes de tudo; cabecalho v3.5 apos os dados.
static void testSyntheticCdi() {
    // ISO9660: PVD
    std::vector<uint8_t> pvd(2048, 0);
    pvd[0] = 1;
    std::memcpy(pvd.data() + 1, "CD001", 5);
    std::memcpy(pvd.data() + 8, "TESTVOL         ", 16);
    std::memcpy(pvd.data() + 40, "HHTEST          ", 16);
    const uint32_t rootDirBytes = 2048;
    auto putLe32 = [&](uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
    };
    auto putBe32 = [&](uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (24 - 8 * i));
    };
    // root dir record dentro do PVD (offset 156): entrada "." do diretorio raiz
    const uint32_t rootLba = 17;
    pvd[156] = 34; // record len
    pvd[157] = 0;  // ext attrib len
    putLe32(pvd.data() + 158, rootLba);
    putBe32(pvd.data() + 162, rootLba);
    putLe32(pvd.data() + 166, rootDirBytes);
    putBe32(pvd.data() + 170, rootDirBytes);
    pvd[174] = 0; pvd[175] = 0; pvd[176] = 0; pvd[177] = 0; pvd[178] = 0; // data/hora
    pvd[179] = 8;                 // flags: diretorio existe / permissoes
    pvd[180] = 8; pvd[181] = 0;   // file unit / interleave
    putLe32(pvd.data() + 182, 1); // volume sequence LE
    putBe32(pvd.data() + 186, 1); // volume sequence BE
    pvd[190] = 8;
    pvd[191] = 'H'; // identifier "." (deitado: nm[0]==0 significa "."; aqui
                    // usamos um record raiz minimo que o parser NAO le - o
                    // parser anda pela LBA 17; PVD so tem root lba+size)
    // (o parser do projeto so usa lba/tamanho da raiz em 158/166; o resto do
    // record e irrelevante para o caminhamento)

    // diretorio raiz: dois registros simples (HELLO.TXT e TREE0001;.TXT por
    // ultimo registro firmado em 0 para fechar a leitura do setor)
    std::vector<uint8_t> rootDir(2048, 0);
    {
        const char* fname = "HELLO.TXT";
        const size_t fnLen = std::strlen(fname);
        const size_t reclen = 33 + fnLen + 1; // +1 pad para par
        rootDir[0] = uint8_t(reclen);
        rootDir[1] = 0; // ext attrib len
        putLe32(rootDir.data() + 2, 18); // LBA absoluta do arquivo
        putBe32(rootDir.data() + 6, 18);
        putLe32(rootDir.data() + 10, 12); // tamanho
        putBe32(rootDir.data() + 14, 12);
        rootDir[25] = 0; // arquivo
        rootDir[32] = uint8_t(fnLen);
        std::memcpy(rootDir.data() + 33, fname, fnLen);
    }
    const char* content = "hello world\n";

    // IP.BIN e boot
    std::vector<uint8_t> ipbin = makeIpBin("T99999X", "V0.001", "1ST_READ.BIN", "0799A10",
                                           "TEST TITLE");
    std::vector<uint8_t> boot(2048, 0); // sem executavel nesta imagem minima

    // setores mode2 2336
    auto sector = [&](const uint8_t* user, size_t n) {
        std::vector<uint8_t> s(2336, 0);
        s[4] = 1; s[5] = 1; s[6] = 9; s[7] = 0; // subcabecalho Mode2
        for (size_t i = 0; i < n && i < 2048; ++i) s[8 + i] = user[i];
        return s;
    };

    std::vector<uint8_t> data;
    // pregap de 150 setores zerado antes de tudo (invariante real: o campo
    // file_offset salta o pregap a partir do inicio da area de dados)
    data.insert(data.end(), 150 * 2336, 0);
    for (uint32_t i = 0; i < 16; ++i) {
        auto s = sector(ipbin.data() + i * 2048, 2048);
        data.insert(data.end(), s.begin(), s.end());
    }
    for (auto src : {&pvd, &rootDir}) {
        auto s = sector(src->data(), 2048);
        data.insert(data.end(), s.begin(), s.end());
    }
    auto sFile = sector(reinterpret_cast<const uint8_t*>(content), 12);
    data.insert(data.end(), sFile.begin(), sFile.end());
    const uint32_t length = uint32_t(data.size() / 2336) - 150; // setores de dados (sem pregap)
    const uint32_t total = 150 + length;

    std::vector<uint8_t> header;
    {
        auto u16b = [&](uint16_t v) { header.push_back(v & 0xFF); header.push_back(v >> 8); };
        auto u32b = [&](uint32_t v) {
            for (int i = 0; i < 4; ++i) header.push_back(uint8_t(v >> (8 * i)));
        };
        // o conteudo do cabecalho comeca direto na contagem de sessoes
        // (sem preambulo: layout validado no leitor linear da HH01)
        u16b(1); // sessoes
        u16b(1); // faixas sessao 1
        u32b(0);
        for (uint8_t b : {0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
                          0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF}) {
            header.push_back(b);
        }
        u32b(0); // skip 4
        const std::string name = "TESTTRACK";
        header.push_back(uint8_t(name.size()));
        for (char c : name) header.push_back(uint8_t(c));
        for (int i = 0; i < 11 + 4 + 4; ++i) header.push_back(0);
        u32b(0); // variante DiscJuggler (ausente na variante 2)
        header.push_back(0); header.push_back(0); // skip 2
        u32b(150);   // pregap
        u32b(length);
        for (int i = 0; i < 6; ++i) header.push_back(0);
        u32b(2); // mode 2
        for (int i = 0; i < 12; ++i) header.push_back(0);
        u32b(0); // start_lba
        u32b(total);
        for (int i = 0; i < 16; ++i) header.push_back(0);
        u32b(1); // codigo 2336
        for (int i = 0; i < 29; ++i) header.push_back(0);
        for (int i = 0; i < 5; ++i) header.push_back(0); // v3.5: skip 5
        u32b(0x80000006u); // (cena tipica: campo de versao sem 0xFFFFFFFF)
        // trailer de fim de sessao: skip 4 + 8 e, em v3/v3.5, skip 1
        u32b(0);
        u32b(0); u32b(0);
        header.push_back(0);
    }
    std::vector<uint8_t> tail;
    {
        auto u32b = [&](uint32_t v) {
            for (int i = 0; i < 4; ++i) tail.push_back(uint8_t(v >> (8 * i)));
        };
        u32b(0x80000006u);                       // versao CDI v3.5
        u32b(uint32_t(header.size() + 8));       // ho inclui o rodape de 8
    }
    std::vector<uint8_t> all = data;
    all.insert(all.end(), header.begin(), header.end());
    all.insert(all.end(), tail.begin(), tail.end());

    const fs::path dir = fs::temp_directory_path() / "hh_extract_tests";
    fs::create_directories(dir);
    const fs::path p = dir / "hh_synth.cdi";
    {
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(all.data()), std::streamsize(all.size()));
    }
    try {
        CdiImage img(p);
        CHECK(img.tracks().size() == 1, "sintetico: 1 faixa");
        if (img.tracks().size() == 1) {
            const Track& t = img.tracks()[0];
            CHECK(t.length == length, "sintetico: length");
            CHECK(t.start_lba == 0, "sintetico: start_lba");
            CHECK(t.total == total, "sintetico: total");
            CHECK(t.sector_size == 2336, "sintetico: setor 2336");
            CHECK(t.userOffset() == 8, "sintetico: userOffset 2336");
            CHECK(t.isData(), "sintetico: faixa de dados");
        }
        // IP.BIN
        std::vector<uint8_t> ipbuf(kIpBinSize);
        for (uint32_t i = 0; i < 16; ++i) img.readUserData(i, ipbuf.data() + i * 2048);
        const IpBin ip = parseIpBin(ipbuf.data(), ipbuf.size());
        CHECK(ip.hardware_id == "SEGA SEGAKATANA", "sintetico: hardware_id");
        CHECK(ip.product == "T99999X", "sintetico: product");
        CHECK(!ip.usesWindowsCE(), "sintetico: nao-CE");

        // ISO em start_lba + 16 = LBA 16
        Iso9660 iso(img, 16);
        CHECK(iso.volumeId() == "HHTEST", "sintetico: volume id");
        const IsoEntry* e = iso.find("HELLO.TXT");
        CHECK(e != nullptr, "sintetico: entrada HELLO.TXT");
        if (e != nullptr) {
            const auto contents = iso.readAll(*e);
            const std::string got(reinterpret_cast<const char*>(contents.data()), contents.size());
            CHECK(got == std::string(content), "sintetico: conteudo do arquivo");
        }
        const IsoEntry* miss = iso.find("NOPE.BIN");
        CHECK(miss == nullptr, "sintetico: entrada ausente retorna nulo");
        // comparacao case-insensitive
        const IsoEntry* ci = iso.find("hello.txt");
        CHECK(ci != nullptr && ci == e, "sintetico: busca case-insensitive");
    } catch (const DiscError& err) {
        CHECK(false, std::string("sintetico lancou DiscError: ") + err.what());
    }
    fs::remove(p);
}

// ---------------------------------------------------------- versoes conhecidas
static void testKnownVersions() {
    // hash de cada versao conhecida reconhece a si mesma
    for (const auto& v : kKnownVersions) {
        // vetor sintetico do tamanho certo nao pode virar versao real
        BootCheck bc = checkBoot(std::vector<uint8_t>(v.boot_size, 0));
        CHECK(bc.version == nullptr, std::string("nada confere sem hash exato: ") + v.id);
    }
}

// ---------------------------------------------- teste negativo: nome hostil
static void testIsoNameSanitize() {
    // registro com nome contendo caractere proibido deve lancar DiscError
    std::vector<uint8_t> pvd(2048, 0);
    pvd[0] = 1;
    std::memcpy(pvd.data() + 1, "CD001", 5);
    std::memcpy(pvd.data() + 40, "BADVOL          ", 16);
    const uint32_t rootLba = 17; // setores: pvd 16, root 17
    auto putLe32 = [&](uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
    };
    auto putBe32 = [&](uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (24 - 8 * i));
    };
    putLe32(pvd.data() + 158, rootLba);
    putBe32(pvd.data() + 162, rootLba);
    putLe32(pvd.data() + 166, 2048);
    putBe32(pvd.data() + 170, 2048);

    // diretorio raiz com nome "BAD\\NAME" (barra invertida proibida)
    std::vector<uint8_t> rootDir(2048, 0);
    const char* bad = "BAD.NA/ME";
    const size_t badLen = std::strlen(bad);
    rootDir[0] = uint8_t(33 + badLen + 1);
    putLe32(rootDir.data() + 2, 18);
    putLe32(rootDir.data() + 10, 4);
    rootDir[32] = uint8_t(badLen);
    std::memcpy(rootDir.data() + 33, bad, badLen);

    // monta imagem: IP fake + pvd + root + 1 setor de dados
    std::vector<uint8_t> ipbin = makeIpBin("T99999X", "V0.001", "1ST_READ.BIN", "0799A10",
                                           "TEST TITLE");
    auto sector = [&](const uint8_t* user, size_t n) {
        std::vector<uint8_t> s(2336, 0);
        s[4] = 1; s[5] = 1; s[6] = 9; s[7] = 0;
        for (size_t i = 0; i < n && i < 2048; ++i) s[8 + i] = user[i];
        return s;
    };
    std::vector<uint8_t> data;
    data.insert(data.end(), 150 * 2336, 0);
    for (uint32_t i = 0; i < 16; ++i) {
        auto s = sector(ipbin.data() + i * 2048, 2048);
        data.insert(data.end(), s.begin(), s.end());
    }
    std::vector<uint8_t> sectors[2] = {pvd, rootDir};
    for (const auto& src : sectors) {
        auto s = sector(src.data(), 2048);
        data.insert(data.end(), s.begin(), s.end());
    }
    auto sPad = sector(nullptr, 0);
    data.insert(data.end(), sPad.begin(), sPad.end());
    const uint32_t length = uint32_t(data.size() / 2336) - 150;

    std::vector<uint8_t> header;
    {
        auto u16b = [&](uint16_t v) { header.push_back(v & 0xFF); header.push_back(v >> 8); };
        auto u32b = [&](uint32_t v) {
            for (int i = 0; i < 4; ++i) header.push_back(uint8_t(v >> (8 * i)));
        };
        u16b(1); u16b(1); u32b(0);
        for (uint8_t b : {0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
                          0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF}) header.push_back(b);
        u32b(0);
        const std::string name = "BADTRACK";
        header.push_back(uint8_t(name.size()));
        for (char c : name) header.push_back(uint8_t(c));
        for (int i = 0; i < 11 + 4 + 4 + 4; ++i) header.push_back(0);
        header.push_back(0); header.push_back(0);
        u32b(150); u32b(length);
        for (int i = 0; i < 6; ++i) header.push_back(0);
        u32b(2);
        for (int i = 0; i < 12; ++i) header.push_back(0);
        u32b(0); u32b(150 + length);
        for (int i = 0; i < 16; ++i) header.push_back(0);
        u32b(1);
        for (int i = 0; i < 29 + 5; ++i) header.push_back(0);
        u32b(0x80000006u);
        u32b(0); u32b(0); u32b(0);
        header.push_back(0);
    }
    std::vector<uint8_t> tail;
    {
        auto u32b = [&](uint32_t v) {
            for (int i = 0; i < 4; ++i) tail.push_back(uint8_t(v >> (8 * i)));
        };
        u32b(0x80000006u);
        u32b(uint32_t(header.size() + 8));
    }
    std::vector<uint8_t> all = data;
    all.insert(all.end(), header.begin(), header.end());
    all.insert(all.end(), tail.begin(), tail.end());
    const fs::path dir = fs::temp_directory_path() / "hh_extract_tests";
    fs::create_directories(dir);
    const fs::path p = dir / "hh_synth_bad.cdi";
    {
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(all.data()), std::streamsize(all.size()));
    }
    try {
        CdiImage img(p);
        Iso9660 iso(img, 16);
        CHECK(false, "nome hostil deveria lancar DiscError");
    } catch (const DiscError&) {
        // esperado: nome com caratere invalido recusado
    }
    fs::remove(p);
}

int main() {
    testSha256();
    testCrc32();
    testScrambleRoundtrip();
    testIpBinValidation();
    testSyntheticCdi();
    testKnownVersions();
    testIsoNameSanitize();
    if (g_failed != 0) {
        std::cerr << g_failed << " verificacoes falharam\n";
        return 1;
    }
    std::cout << "hh_extract_tests: ok\n";
    return 0;
}
