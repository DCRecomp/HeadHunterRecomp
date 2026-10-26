// SPDX-License-Identifier: MIT
#pragma once
// HH12 (docs/planos/HH12-PLANO.md; como o sfd_convert do N5 do BS, MIT): conversao de um filme 4X Movie (.4XM) do
// disco em MP4 (H.264 + AAC) pelo Media Foundation, e a medida da fidelidade da conversao.
// - o FFmpeg (A11: processo externo, nunca ligado) DECODIFICA o 4X: video em BGRA por pipe, a taxa constante da
//   fonte (o quadro sem chunk repete o anterior, como no jogo); audio em PCM 16 bits, reamostrado para 48 kHz (o
//   AAC do Windows so aceita 44,1/48 kHz), com os canais da fonte;
// - o codificador e o do Windows (MfMovieWriter): H.264 em modo qualidade, AAC 192 kbit/s;
// - verify decodifica o MP4 e o 4X de novo e mede PSNR (RGB) por quadro e o SNR do audio.
#include "video/fourxm_header.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

namespace hh::builder::video {

struct ConvertOptions {
    std::filesystem::path ffmpeg = L"ffmpeg";  // executavel (PATH se so o nome)
    uint32_t              quality = 100;       // H.264 do Windows, modo qualidade (o maximo dele)
    uint32_t              audioBytesPerSecond = 24000;
    uint32_t              audioRate = 48000;
    const std::atomic<bool>* cancel = nullptr;
};

struct SourceInfo {  // da fonte, pelo FFmpeg (-show_streams)
    uint32_t width = 0, height = 0;
    uint32_t fpsNum = 0, fpsDen = 1;
    uint32_t sampleRate = 0, channels = 0;  // 0 = sem audio
    bool     hasAudio() const { return channels != 0; }
};

struct ConvertResult {
    SourceInfo   src;
    FourXmHeader hdr;  // do proprio arquivo (quadros, tamanho exibido)
    uint32_t   frames = 0;          // quadros escritos (taxa constante)
    int64_t    videoEnd100ns = 0;   // frames / fps
    uint32_t   audioSamples = 0;    // por canal, a audioRate
    int64_t    audioEnd100ns = 0;
    std::string encoderNotes;
};

// Informacoes da fonte (ffmpeg -i, sem ffprobe). Falha com mensagem clara (sem FFmpeg, arquivo invalido).
bool probe_4xm(const std::filesystem::path& src, const ConvertOptions& o, SourceInfo& info, std::string* err = nullptr);

bool convert_4xm(const std::filesystem::path& src, const std::filesystem::path& mp4, const ConvertOptions& o,
                 ConvertResult& r, std::string* err = nullptr);

struct Fidelity {
    uint32_t framesRef = 0, framesMp4 = 0;
    double   psnrMin = 0, psnrAvg = 0;
    uint32_t framesBelow35 = 0;
    double   audioSnr = 0;  // dB, depois de descontado o atraso
    int32_t  audioDelaySamples = 0;
};

bool verify_4xm(const std::filesystem::path& src, const std::filesystem::path& mp4, const ConvertOptions& o, Fidelity& f,
                std::string* err = nullptr);

} // namespace hh::builder::video
