// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_transitional.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// N0: modelo de interoperabilidade da fase hibrida (transitional/).
#include "test.h"

#include "boundary_registry.h"
#include "guest_abi.h"
#include "handle_table.h"
#include "mods/mod_api.h"

#include <cstring>
#include <string>
#include <vector>

using namespace hh;
using namespace hh::transitional;

HH_TEST(hh07_transitional_handle_table_generations) {
    HandleTable<std::string> t;
    const uint32_t a = t.insert("a"), b = t.insert("b");
    HH_CHECK(a && b && a != b && t.size() == 2);
    HH_CHECK(*t.get(a) == "a" && *t.get(b) == "b");
    HH_CHECK(!t.get(0) && !t.get(a ^ 0x00100000u));  // geracao errada
    HH_CHECK(t.remove(a) && !t.remove(a) && !t.get(a) && t.size() == 1);
    const uint32_t c = t.insert("c");  // reaproveita o indice de a, com outra geracao
    HH_CHECK((c & HandleTable<std::string>::kIndexMask) == (a & HandleTable<std::string>::kIndexMask));
    HH_CHECK(c != a && !t.get(a) && *t.get(c) == "c");
    t.clear();  // fim de sessao
    HH_CHECK(t.size() == 0 && !t.get(b) && !t.get(c));
    // a geracao da volta sem passar por 0 (handle 0 nunca e emitido)
    HandleTable<int> w;
    uint32_t         h = 0;
    for (int i = 0; i < 5000; ++i) {
        h = w.insert(i);
        HH_CHECK(h != 0);
        w.remove(h);
    }
    HH_CHECK(!w.get(h));
}

HH_TEST(hh07_transitional_guest_memory_refuses_hardware) {
    std::vector<uint8_t> ram(16u << 20);
    Sh4Context           c{};
    c.ram = ram.data();
    GuestMemory m(&c);
    HH_CHECK(m.store<uint32_t>(0x8C001000, 0x11223344) && *m.load<uint32_t>(0xAC001000) == 0x11223344);
    HH_CHECK(!m.load<uint32_t>(0x8C001002));                 // desalinhado
    HH_CHECK(!m.load<uint32_t>(0xA05F8000));                 // PVR
    HH_CHECK(!m.store<uint32_t>(0xFF000038, 1));             // P4 (CCN)
    HH_CHECK(!m.load<uint32_t>(0xE0000000));                 // store queue
    HH_CHECK(!m.load<uint8_t>(0xA4000000));                  // VRAM
    HH_CHECK(!m.load<uint32_t>(0x8CFFFFFE) && m.load<uint16_t>(0x8CFFFFFE));  // fim da RAM
    std::memcpy(ram.data() + 0x2000, "\\MOVIES\\INTRO.4XM", 18);
    HH_CHECK(m.cstring(0x8C002000, 64) == std::string("\\MOVIES\\INTRO.4XM"));
    HH_CHECK(!m.cstring(0x8C002000, 4));  // sem terminador dentro do limite
}

HH_TEST(hh07_transitional_guest_call_convention) {
    std::vector<uint8_t> ram(16u << 20);
    Sh4Context           c{};
    c.ram   = ram.data();
    c.r[4]  = 10, c.r[5] = 11, c.r[6] = 12, c.r[7] = 13;
    c.r[15] = 0x8C100000;
    const uint32_t a4 = 14, a5 = 15;
    std::memcpy(ram.data() + 0x100000, &a4, 4);
    std::memcpy(ram.data() + 0x100004, &a5, 4);
    const float f = 2.5f;
    std::memcpy(&c.fr[5], &f, 4);
    GuestCall call(&c);
    HH_CHECK(*call.arg(0) == 10 && *call.arg(3) == 13 && *call.arg(4) == 14 && *call.arg(5) == 15);
    HH_CHECK(*call.farg(1) == 2.5f && !call.farg(8));
    call.ret(0x1234);
    call.fret(1.5f);
    float r;
    std::memcpy(&r, &c.fr[0], 4);
    HH_CHECK(c.r[0] == 0x1234 && r == 1.5f);
    // estrutura com layout declarado: campo alem do tamanho e recusado
    const GuestLayout kLayout{"TesteV1", 1, 8};
    const GuestStruct st(call.memory(), 0x8C100000, kLayout);
    HH_CHECK(st.valid() && *st.field<uint32_t>(4) == 15 && !st.field<uint32_t>(8));
}

namespace {
int  g_legacy = 0, g_native = 0;
void legacyFn(Sh4Context*) { ++g_legacy; }
void nativeFn(Sh4Context*) { ++g_native; }
const Sh4FuncEntry kTable[] = {{0x0C200000, &legacyFn}};
} // namespace

// Sem slots (build normal): a troca de provedor vale para as chamadas indiretas e o registro
// informa isso; voltar ao legado restaura o despacho.
HH_TEST(hh07_transitional_boundary_registry_selects_provider) {
    sh4_set_function_table(kTable, 1);
    BoundaryRegistry r;
    HH_CHECK(r.add(0x8C200000, dcmap::Subsystem::Files, "teste") && !r.add(0xAC200000, dcmap::Subsystem::Files, "x"));
    HH_CHECK(r.active(0x8C200000) == Provider::Legacy && r.count(Provider::Legacy) == 1);
    HH_CHECK(r.select(0x8C200000, Provider::Native) == SelectResult::NoNative);
    HH_CHECK(r.select(0x8C300000, Provider::Legacy) == SelectResult::Unknown);
    HH_CHECK(r.set_native(0x8C200000, &nativeFn));
    HH_CHECK(r.select(0x8C200000, Provider::Native) == SelectResult::IndirectOnly);
    Sh4Context c{};
    sh4_dispatch(&c, 0, 0x8C200000);
    HH_CHECK(g_native == 1 && g_legacy == 0 && r.count(Provider::Native, dcmap::Subsystem::Files) == 1);
    HH_CHECK(r.select(0x8C200000, Provider::Legacy) == SelectResult::Ok);
    sh4_dispatch(&c, 0, 0x8C200000);
    HH_CHECK(g_legacy == 1 && r.count(Provider::Native) == 0);
    r.select(0x8C200000, Provider::Native);
    r.reset();
    HH_CHECK(r.active(0x8C200000) == Provider::Legacy && sh4_lookup(0x8C200000) == &legacyFn);
    mods::reset();
    sh4_set_function_table(nullptr, 0);
}

// Revisao Codex 2026-10-15: um handle antigo nao volta a valer depois que a geracao do indice
// se esgota (o indice e aposentado).
HH_TEST(hh07_transitional_handle_table_retires_exhausted_index) {
    HandleTable<int> t;
    const uint32_t   first = t.insert(1);
    t.remove(first);
    std::vector<uint32_t> seen;
    for (int i = 0; i < 5000; ++i) {
        const uint32_t h = t.insert(i);
        HH_CHECK(h != 0 && h != first);
        HH_CHECK(!t.get(first));
        if ((h & HandleTable<int>::kIndexMask) != (first & HandleTable<int>::kIndexMask)) {
            t.remove(h);
            continue;
        }
        seen.push_back(h);
        t.remove(h);
    }
    HH_CHECK(seen.size() == HandleTable<int>::kMaxGen - 1);  // geracoes 2..4095 no indice 0
    const uint32_t live = t.insert(7);                        // indice 0 aposentado: vai para outro
    HH_CHECK((live & HandleTable<int>::kIndexMask) != 0 && !t.get(first));
    for (uint32_t h : seen) HH_CHECK(!t.get(h));
}

namespace {
int  g_user = 0;
void userMod(Sh4Context*) { ++g_user; }
void otherUser(Sh4Context*) { ++g_user; }
} // namespace

// Revisao Codex 2026-10-15: voltar ao legado restaura o mod do usuario que estava ativo.
HH_TEST(hh07_transitional_boundary_registry_preserves_user_mod) {
    sh4_set_function_table(kTable, 1);
    BoundaryRegistry r;
    r.add(0x8C200000, dcmap::Subsystem::Files, "teste");
    r.set_native(0x8C200000, &nativeFn);
    HH_CHECK(mods::replace(0x8C200000, &userMod));
    HH_CHECK(r.select(0x8C200000, Provider::Native) != SelectResult::Unknown);
    HH_CHECK(sh4_lookup(0x8C200000) == &nativeFn);
    HH_CHECK(r.select(0x8C200000, Provider::Legacy) == SelectResult::Ok);
    HH_CHECK(sh4_lookup(0x8C200000) == &userMod);
    // mod instalado enquanto o nativo esta ativo: fica
    r.select(0x8C200000, Provider::Native);
    mods::replace(0x8C200000, &otherUser);
    r.select(0x8C200000, Provider::Legacy);
    HH_CHECK(sh4_lookup(0x8C200000) == &otherUser);
    HH_CHECK(sh4_lookup_generated(0x8C200000) == &legacyFn);
    mods::reset();
    sh4_set_function_table(nullptr, 0);
}
