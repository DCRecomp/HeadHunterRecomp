// SPDX-License-Identifier: MIT
#pragma once
// HH15.2 (revisao Codex): classificacao fechada das variaveis de ambiente HH_* que o codigo do produto le. O produto
// (validacao_stubs.cpp) recusa na partida as de kTestVars, com saida 2; as de kObservationalVars so registram, gravam
// traces ou despejos, ou sao funcoes do usuario (modo espectador, escolha da GPU), e ficam (como no N8.2 do BS: a
// instrumentacao das referencias e inerte sem a variavel). O teste hh15_variaveis_classificadas varre app/, runtime/,
// transitional/ e native/ e reprova uma leitura de HH_* que nao esteja numa das duas listas.
namespace hh::validation_vars {

// Mudam a execucao: sondas, provas, mutacoes dos negativos, modos de comparacao e de diagnostico.
inline constexpr const char* kTestVars[] = {
    "HH_HW_PROBE", "HH_RENDER_PROBE", "HH_CICLOS_RUIDO", "HH_CADENCIA", "HH_IRQ_QUANTUM", "HH_TMU_MUTACAO",
    "HH_TEMPO_MUTACAO", "HH_ARQUIVOS_MUTACAO", "HH_BU_MUTACAO", "HH_AICA_MUTACAO", "HH_PAD_MUTACAO",
    "HH_PAD_NATIVO_MUTACAO", "HH_PAD_CONTRATO_MUTACAO", "HH_SB_MUTACAO", "HH_STOP_ON_STARTRENDER", "HH_AUDIO_SEM_DSP",
    "HH_KAMUI", "HH_KAMUI_CUSTO", "HH_MAPLE_FUNCAO", "HH_MAPLE_PORTS", "HH_PAD_MODO", "HH_PAD_CONTRATO",
    "HH_PROGRESSO_LIMITE", "HH_GDROM_TIMING", "HH_TMU_DIAG", "HH_VIDEO_DADOS", "HH_VK_ATRASO_MS", "HH_RENDER_SEQ",
    "HH_WW_DECLARAR", "HH_WW_VOLTAS", "HH_INPUT", "HH_INPUT_MAP", "HH_RENDER_SOFT_MUTACAO",
};

// So observam (registros, traces, despejos, perfis) ou sao funcoes do usuario.
inline constexpr const char* kObservationalVars[] = {
    "HH_ARQUIVOS_LOG", "HH_AUDIO_CMDS", "HH_AUDIO_LEITURAS", "HH_AUDIO_LOG", "HH_AUDIO_LOG_PORTA", "HH_AUDIO_RAM",
    "HH_AUDIO_RAM_CMD", "HH_AUDIO_RAM_EM", "HH_AUDIO_WAV", "HH_AICA_DUMP_AT", "HH_AICA_LOG", "HH_AICA_LOG_TUDO",
    "HH_AICA_TRACE", "HH_CALL_TRACE", "HH_CLOCK_LOG", "HH_EVENT_TRACE", "HH_FILE_LOG", "HH_FN_PERFIL", "HH_HLE_LOG",
    "HH_INPUT_SCRIPT", "HH_IRQ_DEBUG", "HH_KAMUI_LOG", "HH_KAMUI_ORDEM", "HH_LEITOR", "HH_ESCRITOR", "HH_MAPLE_DEBUG",
    "HH_PAD_LEITURAS", "HH_PAD_TRACE", "HH_PAD_FECHAMENTO", "HH_PROGRESS_TRACE", "HH_PROGRESSO_PCS", "HH_PVR_DEBUG",
    "HH_PVR_WATCH", "HH_RAM_DUMP", "HH_RENDER_IR", "HH_RENDER_IR_BMP", "HH_RENDER_IR_BMP_CADA", "HH_RENDER_IR_BMP_DIFF",
    "HH_RENDER_IR_CADA", "HH_RENDER_IR_GUARDAR", "HH_RENDER_PROFILE", "HH_SB_LOG", "HH_TA_DUMP", "HH_TA_FONTES",
    "HH_TA_LOG", "HH_TIME_TRACE", "HH_VK_LENTOS", "HH_VRAM_WATCH", "HH_VULKAN_GPU", "HH_WATCH_MAX", "HH_YUV_DEBUG",
    "HH_YUV_DUMP",
};

} // namespace hh::validation_vars
