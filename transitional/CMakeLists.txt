# Interoperabilidade da fase hibrida (docs/planos/HH07-PLANO.md, HH07.6): handles de 32 bits
# com geracao, adaptadores Sh4Context <-> native/, registro de fronteiras com provedor
# legado/nativo. Some quando os chamadores forem elevados (HH15).
add_library(hh_transitional STATIC boundary_registry.cpp gdfs_adapter.cpp time_adapter.cpp
            input_bridge.cpp pad_contract.cpp pad_adapter.cpp pad_probe.cpp bu_machine.cpp bu_adapter.cpp
            video_adapter.cpp render_devices.cpp render_capture.cpp kamui_device.cpp audio_device.cpp)
target_include_directories(hh_transitional PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hh_transitional PUBLIC hh_legacy hh_native_api hh_native_files hh_native_time hh_native_input hh_native_saves hh_native_video hh_native_audio hh_native_audio_core hh_native_render_vk hh_native_render_soft hh_native_render_kamui hh_dcmap)
