// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/bu_machine.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Protocolo assincrono da biblioteca de backup "bu" do Katana sobre um ISaveStore sincrono (HH11,
// docs/planos/HH11-PLANO.md, "Oraculo SAVE/LOAD no Flycast: resultados"). Sem Sh4Context nem RAM do
// guest: o adaptador (bu_adapter) le os argumentos, chama esta maquina e entrega os eventos que ela
// devolve aos callbacks do jogo. Testada sozinha com um store em memoria.
//
// O que o oraculo fixou (jogo original no Flycast com VMU, 2026-10-19):
//   - cada operacao termina com progresso(drive, op, k, total) para k = 1..n e depois
//     conclusao(drive, op, resultado); a conclusao vem ANTES de status = 0;
//   - insercao do cartao (op 0x12, resultado = blocos de trabalho 15) depois do init, sem pedido do
//     jogo; montagem (op 1, total 0x11, k ate 0x10); SAVE (op 0x0C, total 10 para 4 blocos, com
//     consultas "ocupada" antes); LOAD (op 0x0E, total = blocos lidos);
//   - status: -1 com operacao pendente na unidade, senao 0; exists: 0 ou FF05.
// O tempo e abstraido: cada operacao conclui num numero fixo de passadas do servidor bu (tick),
// o mesmo nos dois modos de tempo, nunca pelo relogio de parede.
#include "platform/saves.h"

#include <array>
#include <optional>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace hh::transitional::bu {

constexpr int      kDrives = 8;  // unidades da bu (a mascara do init habilita as que o jogo usa)
// HH11: op 0x0B = leitura do arquivo inteiro (oraculo do Flycast: H_HUNTER.SYS, progresso 1..n de n blocos)
constexpr uint32_t kOpMount = 1, kOpRemove = 2, kOpLoadFile = 0x0B, kOpSave = 0x0C, kOpLoad = 0x0E, kOpInsert = 0x12;
constexpr uint32_t kWorkBlocks = 15;  // resultado da insercao: o jogo aloca 15 * 512 + 32 de trabalho
// Codigos de resultado (o jogo os compara sign-extended: mov.w)
constexpr int32_t kOk = 0, kBusy = -1, kNoCard = int32_t(0xFFFFFF01), kUnformatted = int32_t(0xFFFFFF03),
                  kNotFound = int32_t(0xFFFFFF05), kCannotOpen = int32_t(0xFFFFFF07);
constexpr size_t  kDiskInfoSize = 56;  // escritos por disk_info (bloco do jogo +20 .. +75)
// HH11 (oraculo c3 + disassembly de 8C171CD8): file_info grava 18 bytes: +0 tamanho em bytes (u32), +4 blocos
// (u16), +6 arquivo de jogo (tipo 0xCC: 1; dados: 0), +7 protecao de copia (u8), +8 deslocamento do cabecalho
// (u16; 0 nos arquivos de dados), +0xA a data (BuTime, 8 bytes).
constexpr size_t kFileInfoSize = 18;

// Data do bu (o mesmo leiaute no argumento de tempo do SAVE e no file_info): ano u16, mes, dia, hora, minuto,
// segundo, dia da semana (0 = domingo). Convertida sem fuso: a volta e exata.
int64_t bu_time_to_unix(const uint8_t t[8]);
void    unix_to_bu_time(int64_t unix, uint8_t t[8]);

int32_t result_code(native::saves::SaveError e);  // erro do store -> codigo da bu

struct Event {
    enum class Kind : uint8_t { Progress, Complete } kind;
    int      drive;
    uint32_t op;
    uint32_t a = 0, b = 0;      // progresso: k, total; conclusao: a = resultado
    std::vector<std::byte> data;  // conclusao de LOAD com sucesso: bytes para o destino
};

struct Ticks {  // passadas do servidor ate concluir (>= 1)
    int insert = 1, mount = 2, save = 4, load = 1;
};

class Machine {
public:
    explicit Machine(native::saves::ISaveStore& store, Ticks t = {}) : store_(store), ticks_(t) {}

    // init(mascara): as unidades com cartao recebem a insercao na proxima passada.
    void    init(uint32_t mask);
    int32_t mount(int drive);
    int32_t disk_info(int drive, std::span<std::byte, kDiskInfoSize> out);
    int32_t exists(int drive, const std::string& name);
    // `time`: a data do arquivo (a que o jogo passa; sem ela, o relogio do store)
    int32_t save(int drive, const std::string& name, std::vector<std::byte> image, std::optional<int64_t> time = {});
    // Sincrono (o oraculo responde na hora, sem callback): kNotFound sem o arquivo.
    int32_t file_info(int drive, const std::string& name, std::span<std::byte, kFileInfoSize> out);
    // op: kOpLoad (bloco inicial e quantidade; 0 = ate o fim) ou kOpLoadFile (arquivo inteiro)
    int32_t load(int drive, const std::string& name, uint32_t startBlock, uint32_t nblocks, uint32_t op = kOpLoad);
    int32_t status(int drive) const;
    int32_t last_error(int drive) const;
    int32_t exit();  // -1 com alguma unidade ocupada (como o exit da bu: 8C15FC2A no HH)
    // Uma passada do servidor: eventos a entregar, em ordem. Com `hold`, a unidade que concluiu continua
    // ocupada (status -1) ate release(drive): o adaptador entrega os progressos assim e libera logo
    // antes do callback de conclusao, como o oraculo (dentro dele o jogo ja consulta disk_info).
    std::vector<Event> tick(bool hold = false);
    void               release(int drive);

    bool busy(int drive) const { return valid(drive) && u_[drive].op != 0; }

private:
    struct Unit {
        bool                   enabled = false, mounted = false;
        uint32_t               op = 0;  // operacao pendente (0 = livre)
        bool                   releasing = false;  // concluida, aguardando release()
        int                    left = 0;
        std::string            name;
        std::vector<std::byte> image;
        std::optional<int64_t> time;
        uint32_t               start = 0, nblocks = 0;
        int32_t                lastError = 0;
    };
    static bool valid(int d) { return d >= 0 && d < kDrives; }
    bool        present(int drive) { return card_code(drive) == kOk; }
    int32_t     card_code(int drive);  // kOk, kNoCard ou kUnformatted
    int32_t     begin(int drive, uint32_t op, int ticks);
    void        finish(int drive, Unit& u, std::vector<Event>& out, bool hold);

    native::saves::ISaveStore& store_;
    Ticks                      ticks_;
    std::array<Unit, kDrives>  u_{};
};

} // namespace hh::transitional::bu
