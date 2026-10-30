# Sistemas nativos definitivos (ARCHITECTURE.md, docs/planos/HH07-PLANO.md). No HH07 so as
# interfaces: C++20 puro, sem contexto do SH-4, enderecos do guest ou registradores (conferido
# pelo teste hh07_native_limpo, que tambem le este arquivo). Implementacoes chegam do HH08 ao HH14.
add_library(hh_native_api INTERFACE)
target_include_directories(hh_native_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(hh_native_api INTERFACE cxx_std_20)

if(BUILD_TESTS)
  # Compila todos os cabecalhos so com native/ no include path: nada do legado pode vazar.
  add_executable(hh_native_api_check ${PROJECT_SOURCE_DIR}/tests/hh07/native_api_check.cpp)
  target_link_libraries(hh_native_api_check PRIVATE hh_native_api)
endif()

# HH08: arquivos do jogo sobre arquivos comuns (data\disc + mods\assets).
add_library(hh_native_files STATIC platform/data_files.cpp)
target_link_libraries(hh_native_files PUBLIC hh_native_api)

# HH09: relogio logico, fila de eventos e monitor de progresso (portados do N2a/N2b do BS).
add_library(hh_native_time STATIC platform/clock.cpp platform/event_queue.cpp platform/progress.cpp)
target_link_libraries(hh_native_time PUBLIC hh_native_api)

# HH10: entrada nativa (SDL3/XInput, mapa de acoes em TOML, perfis), portada do N3 do BS.
add_library(hh_native_input STATIC platform/input_map.cpp platform/input_system.cpp)
target_link_libraries(hh_native_input PUBLIC hh_native_api PRIVATE tomlplusplus::tomlplusplus xinput)

# HH18: instalacao.toml (local dos saves gravado pelo Recompiler; portado do BS).
add_library(hh_native_install STATIC platform/install_config.cpp)
target_link_libraries(hh_native_install PUBLIC hh_native_api PRIVATE tomlplusplus::tomlplusplus)

# HH11: saves (formato VMS, envelope versionado e escrita atomica em disco; portados do N4 do BS).
add_library(hh_native_saves STATIC platform/saves_format.cpp platform/saves_store.cpp)
target_link_libraries(hh_native_saves PUBLIC hh_native_api)

# HH12: filmes (catalogo, Media Foundation, reproducao pelo relogio logico; portados do N5 do BS) e a saida de audio
# dos filmes (XAudio2 / nula).
add_library(hh_native_video STATIC video/movie_catalog.cpp video/mf_movie.cpp video/movie_playback.cpp video/subtitles.cpp)
target_link_libraries(hh_native_video PUBLIC hh_native_api PRIVATE tomlplusplus::tomlplusplus mfplat mfreadwrite mfuuid ole32 d2d1 dwrite windowscodecs)
# HH14: audio de gameplay portavel (ADPCM da AICA, envelope, mixer, leitura de WAV, DSP de efeitos da AICA; portados do
# N6 do BS); sem Win32 - a saida fica no hh_native_audio
add_library(hh_native_audio_core STATIC audio/aica_adpcm.cpp audio/aica_dsp.cpp audio/mixer.cpp audio/sound_file.cpp
            audio/sound_mod_codecs.cpp audio/aica_channels.cpp)
target_link_libraries(hh_native_audio_core PUBLIC hh_native_api)
add_library(hh_native_audio STATIC audio/audio_output.cpp)
target_link_libraries(hh_native_audio PUBLIC hh_native_api PRIVATE xaudio2 ole32)

# HH13: regras portaveis do renderer nativo (IR de render, texturas do PVR para RGBA8, preparo dos triangulos para a
# GPU, contrato das texturas HD e dos shaders de pos-processamento), portadas do N7 do BS; sem API grafica
add_library(hh_native_render STATIC render/texture_convert.cpp render/render_ir.cpp render/fidelity_scenes.cpp
            render/texture_replacement.cpp render/frame_prep.cpp render/post_shader.cpp)
target_link_libraries(hh_native_render PUBLIC hh_native_api PRIVATE tomlplusplus::tomlplusplus)

# HH15.7a: renderizador em software sobre a IR (o rasterizador de referencia, movido do runtime, e o dispositivo)
add_library(hh_native_render_soft STATIC render/soft/soft_raster.cpp render/soft/soft_device.cpp)
target_link_libraries(hh_native_render_soft PUBLIC hh_native_render)
# HH15.7b-c: os servicos nativos da camada grafica do Kamui: as listas decodificadas direto para a IR, o estado do
# quadro, os texels das texturas e a imagem exibida
add_library(hh_native_render_kamui STATIC render/kamui/list_sink.cpp render/kamui/frame_state.cpp
            render/kamui/texture_source.cpp render/kamui/display_service.cpp
            render/kamui/event_status.cpp render/kamui/spg_timing.cpp)
target_link_libraries(hh_native_render_kamui PUBLIC hh_native_render_soft)

# HH13: backend Vulkan (offscreen; o Vulkan e carregado em tempo de execucao, sem SDK; cabecalhos Khronos em
# third_party/vulkan, Apache-2.0; SPIR-V versionado em render/vulkan/spirv, gerado dos shaders do BS)
add_library(hh_native_render_vk STATIC render/vulkan/vk_device.cpp render/vulkan/vk_present.cpp
            render/vulkan/vk_post.cpp)
target_include_directories(hh_native_render_vk PRIVATE ${PROJECT_SOURCE_DIR}/third_party/vulkan)
target_link_libraries(hh_native_render_vk PUBLIC hh_native_render PRIVATE ${CMAKE_DL_LIBS})
