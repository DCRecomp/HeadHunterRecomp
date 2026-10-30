// SPDX-License-Identifier: MIT
// HH15.7d4c: o lugar da captura da IR (render_capture.h) no produto (HeadHunter.exe). Compilado como fonte do proprio
// produto (app/CMakeLists.txt): o que o host e o provedor nativo do Kamui chamam da captura, entao o linker nao puxa
// transitional/render_capture (catraca hh15_produto_papeis). O produto desenha pelo renderizador de render_devices.h; a
// captura do caminho do TA e os modos de verificacao (HH_RENDER_IR, recusados na partida do produto) nao existem aqui.
#include "render_capture.h"

#include <stdexcept>

namespace hh::transitional::render_capture {

void install(Mode m, uint32_t) {
    // Native e Software sao do renderizador do produto (install_renderer); Off nao tem o que desinstalar
    if (m == Mode::Capture || m == Mode::Check || m == Mode::Compare)
        throw std::runtime_error("captura da IR (HH_RENDER_IR): so na verificacao (hh-run)");
}

const Stats& stats() {
    static const Stats none;
    return none;
}

std::string summary() { return {}; }

void set_compare_dump(const std::string&, uint32_t) {
    throw std::runtime_error("comparacao da captura (HH_RENDER_IR_BMP): so na verificacao (hh-run)");
}

void set_list_sink(const native::render::kamui::ListSink*) {}  // nada a conferir sem o caminho do TA

const std::vector<uint32_t>* last_image(uint32_t&, uint32_t&) { return nullptr; }

} // namespace hh::transitional::render_capture
