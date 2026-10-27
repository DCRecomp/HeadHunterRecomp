// SPDX-License-Identifier: MIT
// HH14: FLAC e OGG/Vorbis so servem aos mods (fase F, docs/planos/HH14-PLANO.md). Ate o porte de sound_flac.cpp e
// sound_vorbis.cpp do BS (dr_flac; libogg + libvorbis), a leitura falha com erro explicito, nunca em silencio.
#include "audio/sound_file.h"

namespace hh::native::audio {

std::shared_ptr<SoundData> parse_flac(std::span<const uint8_t>, std::string* err) {
    if (err) *err = "FLAC ainda nao suportado (HH14 fase F: mods)";
    return nullptr;
}

std::shared_ptr<SoundData> parse_ogg(std::span<const uint8_t>, std::string* err) {
    if (err) *err = "OGG/Vorbis ainda nao suportado (HH14 fase F: mods)";
    return nullptr;
}

} // namespace hh::native::audio
