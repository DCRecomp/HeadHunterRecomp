// SPDX-License-Identifier: MIT
// HH12: 4X Movie -> MP4 (estrutura do sfd_convert do N5 do BS, MIT, Copyright (c) 2026 Blue Stinger Recompiled
// contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt). O FFmpeg so decodifica, como processo
// separado (A11); nada dele e ligado ou copiado.
#include "video/fourxm_convert.h"

#include "child_process.h"
#include "video/mf_movie.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <span>
#include <vector>

namespace hh::builder::video {

namespace fs = std::filesystem;
namespace nv = hh::native::video;

namespace {

bool fail(std::string* err, const std::string& m) {
    if (err) *err = m;
    return false;
}

fs::path ffprobeOf(const fs::path& ffmpeg) {
    fs::path p = ffmpeg;
    p.replace_filename(ffmpeg.extension().empty() ? L"ffprobe" : L"ffprobe" + ffmpeg.extension().native());
    return p;
}

// Processo com o stdout num pipe binario (o stderr vai para um arquivo temporario, lido se falhar). Com `cancel`, uma
// thread vigia o pedido e encerra o processo (o que tambem destrava uma leitura bloqueada no pipe).
class Pipe {
public:
    bool start(const std::wstring& cmd, std::string* err, const std::atomic<bool>* cancel = nullptr) {
        SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
        HANDLE              w = nullptr;
        if (!CreatePipe(&rd_, &w, &sa, 1 << 20)) return fail(err, "CreatePipe falhou");
        SetHandleInformation(rd_, HANDLE_FLAG_INHERIT, 0);
        wchar_t tmp[MAX_PATH], name[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        GetTempFileNameW(tmp, L"hhf", 0, name);
        errPath_ = name;
        errFile_ = CreateFileW(name, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        STARTUPINFOW si{};
        si.cb         = sizeof si;
        si.dwFlags    = STARTF_USESTDHANDLES;
        si.hStdInput  = nullptr;
        si.hStdOutput = w;
        si.hStdError  = errFile_;
        std::wstring line = cmd;
        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        CloseHandle(w);
        if (!ok) return fail(err, "nao foi possivel executar o FFmpeg (instale o FFmpeg ou informe o caminho): " + narrow(cmd));
        CloseHandle(pi.hThread);
        proc_ = pi.hProcess;
        if (cancel)
            watch_ = std::thread([this, cancel] {
                while (!done_.load()) {
                    if (cancel->load()) {
                        cancelled_ = true;
                        TerminateProcess(proc_, 1);
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });
        return true;
    }
    bool cancelled() const { return cancelled_.load(); }
    // le exatamente n bytes; false no fim do stream
    bool read(void* dst, size_t n) {
        auto* p = static_cast<uint8_t*>(dst);
        while (n) {
            DWORD got = 0;
            if (!ReadFile(rd_, p, DWORD(std::min<size_t>(n, 1 << 20)), &got, nullptr) || got == 0) return false;
            p += got, n -= got;
        }
        return true;
    }
    // o que houver (ate n bytes); 0 no fim do stream
    size_t read_some(void* dst, size_t n) {
        DWORD got = 0;
        return ReadFile(rd_, dst, DWORD(std::min<size_t>(n, 1 << 20)), &got, nullptr) ? size_t(got) : 0;
    }
    // espera o fim; devolve o codigo e o stderr (ultimas linhas) em `log`
    int finish(std::string& log) {
        if (rd_) CloseHandle(rd_), rd_ = nullptr;
        DWORD code = DWORD(-1);
        done_ = true;
        if (watch_.joinable()) watch_.join();
        if (proc_) {
            // o pipe ja foi lido ate o fim (ou fechado acima): o processo termina logo; preso, e encerrado
            if (WaitForSingleObject(proc_, 30000) == WAIT_TIMEOUT) TerminateProcess(proc_, 1), WaitForSingleObject(proc_, 5000);
            GetExitCodeProcess(proc_, &code);
            CloseHandle(proc_), proc_ = nullptr;
        }
        if (errFile_) CloseHandle(errFile_), errFile_ = nullptr;
        std::ifstream in(errPath_, std::ios::binary);
        log.assign(std::istreambuf_iterator<char>(in), {});
        if (log.size() > 2000) log = log.substr(log.size() - 2000);
        in.close();
        DeleteFileW(errPath_.c_str());
        return int(code);
    }
    void kill() {
        if (proc_) TerminateProcess(proc_, 1);
    }
    ~Pipe() {
        kill();
        std::string ignored;
        finish(ignored);
    }

private:
    HANDLE            rd_ = nullptr, proc_ = nullptr, errFile_ = nullptr;
    std::wstring      errPath_;
    std::thread       watch_;
    std::atomic<bool> done_{false}, cancelled_{false};
};

std::wstring q(const fs::path& p) { return quote_arg(p.native()); }

// Video da fonte em BGRA (ARGB em uint32 little-endian), a taxa constante.
std::wstring videoCmd(const ConvertOptions& o, const fs::path& src, const SourceInfo& s) {
    return q(o.ffmpeg) + L" -v error -nostdin -i " + q(src) + L" -map 0:v:0 -fps_mode cfr -r " +
           std::to_wstring(s.fpsNum) + L"/" + std::to_wstring(s.fpsDen) + L" -f rawvideo -pix_fmt bgra pipe:1";
}

// Audio da fonte em PCM 16 bits a o.audioRate, com os canais da fonte.
std::wstring audioCmd(const ConvertOptions& o, const fs::path& src, const SourceInfo& s) {
    return q(o.ffmpeg) + L" -v error -nostdin -i " + q(src) + L" -map 0:a:0 -f s16le -acodec pcm_s16le -ar " +
           std::to_wstring(o.audioRate) + L" -ac " + std::to_wstring(s.channels) + L" pipe:1";
}

bool readAll(const std::wstring& cmd, std::vector<int16_t>& out, std::string* err, const std::atomic<bool>* cancel) {
    Pipe p;
    if (!p.start(cmd, err, cancel)) return false;
    std::vector<uint8_t> raw;
    std::vector<uint8_t> buf(1 << 20);
    for (size_t n; (n = p.read_some(buf.data(), buf.size())) != 0;) raw.insert(raw.end(), buf.begin(), buf.begin() + ptrdiff_t(n));
    std::string log;
    const int   rc = p.finish(log);
    if (p.cancelled()) return fail(err, "cancelado");
    if (rc != 0) return fail(err, "o FFmpeg falhou ao decodificar o audio: " + log);
    out.resize(raw.size() / 2);
    std::memcpy(out.data(), raw.data(), out.size() * 2);
    return true;
}

double psnrRgb(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    double se = 0;
    for (size_t i = 0; i < a.size(); ++i)
        for (int sh = 0; sh < 24; sh += 8) {
            const int d = int((a[i] >> sh) & 0xFF) - int((b[i] >> sh) & 0xFF);
            se += double(d) * d;
        }
    const double mse = se / (double(a.size()) * 3);
    return mse <= 1e-12 ? 99.0 : 10 * std::log10(255.0 * 255.0 / mse);
}

} // namespace

bool probe_4xm(const fs::path& src, const ConvertOptions& o, SourceInfo& info, std::string* err) {
    info = {};
    std::error_code ec;
    if (!fs::is_regular_file(src, ec)) return fail(err, "filme nao encontrado: " + path_text(src));
    const std::wstring cmd = q(ffprobeOf(o.ffmpeg)) +
                             L" -v error -show_entries stream=codec_type,width,height,r_frame_rate,sample_rate,channels"
                             L" -of default=nw=1 " + q(fs::absolute(src, ec));
    std::vector<std::string> lines;
    const ProcessResult r = run_process(cmd, fs::absolute(src, ec).parent_path(), {}, [&](const std::string& l) { lines.push_back(l); },
                                        o.cancel);
    if (!r.started) return fail(err, "nao foi possivel executar o ffprobe (instale o FFmpeg ou informe o caminho)");
    if (r.exitCode != 0) return fail(err, "o ffprobe recusou " + path_text(src.filename()));
    std::string type;
    for (const std::string& l : lines) {
        const size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = l.substr(0, eq), v = l.substr(eq + 1);
        if (k == "codec_type") type = v;
        else if (type == "video" && k == "width") info.width = uint32_t(std::stoul(v));
        else if (type == "video" && k == "height") info.height = uint32_t(std::stoul(v));
        else if (type == "video" && k == "r_frame_rate") {
            const size_t sl = v.find('/');
            info.fpsNum = uint32_t(std::stoul(v.substr(0, sl)));
            info.fpsDen = sl == std::string::npos ? 1 : uint32_t(std::stoul(v.substr(sl + 1)));
        } else if (type == "audio" && k == "sample_rate" && !info.channels) info.sampleRate = uint32_t(std::stoul(v));
        else if (type == "audio" && k == "channels" && !info.channels) info.channels = uint32_t(std::stoul(v));
    }
    if (!info.width || !info.height || !info.fpsNum || !info.fpsDen) return fail(err, path_text(src.filename()) + ": sem video 4X");
    return true;
}

bool convert_4xm(const fs::path& src, const fs::path& mp4, const ConvertOptions& o, ConvertResult& r, std::string* err) {
    r = {};
    if (!read_4xm_header(src, r.hdr, err) || !probe_4xm(src, o, r.src, err)) return false;
    if (r.hdr.codedWidth != r.src.width || r.hdr.codedHeight != r.src.height)
        return fail(err, path_text(src.filename()) + ": tamanho do vtrk diferente do decodificado pelo FFmpeg");
    const SourceInfo& s = r.src;
    if (s.width % 2 || s.height % 2) return fail(err, path_text(src.filename()) + ": dimensoes impares (H.264 4:2:0)");
    std::vector<int16_t> pcm;
    if (s.hasAudio() && !readAll(audioCmd(o, src, s), pcm, err, o.cancel)) return false;
    nv::WriterOptions wo;
    wo.width = s.width, wo.height = s.height, wo.fpsNum = s.fpsNum, wo.fpsDen = s.fpsDen, wo.quality = o.quality;
    wo.audio = s.hasAudio(), wo.sampleRate = o.audioRate, wo.channels = std::max(1u, s.channels);
    wo.audioBytesPerSecond = o.audioBytesPerSecond;
    nv::MfMovieWriter w;
    if (!w.open(mp4, wo, err)) return false;
    Pipe v;
    if (!v.start(videoCmd(o, src, s), err, o.cancel)) return false;
    std::vector<uint32_t> frame(size_t(s.width) * s.height);
    const uint32_t        ch = wo.channels;
    size_t                ap = 0;
    while (v.read(frame.data(), frame.size() * 4)) {
        if (o.cancel && o.cancel->load()) return v.kill(), fail(err, "cancelado");
        for (uint32_t& px : frame) px |= 0xFF000000u;
        if (!w.write_frame(frame, err)) return false;
        ++r.frames;
        // audio intercalado: ate o fim deste quadro
        if (wo.audio) {
            const size_t until = std::min(pcm.size() / ch,
                                          size_t(uint64_t(r.frames) * o.audioRate * s.fpsDen / s.fpsNum)) * ch;
            if (until > ap && !w.write_audio(std::span<const int16_t>(pcm.data() + ap, until - ap), err)) return false;
            ap = std::max(ap, until);
        }
    }
    std::string log;
    const int rc = v.finish(log);
    if (v.cancelled()) return fail(err, "cancelado");
    if (rc != 0) return fail(err, "o FFmpeg falhou ao decodificar o video: " + log);
    if (!r.frames) return fail(err, path_text(src.filename()) + ": nenhum quadro decodificado");
    if (r.frames != r.hdr.frames)  // o laco do jogo faz uma volta por LIST FRAM (vtrk[4])
        return fail(err, path_text(src.filename()) + ": o FFmpeg decodificou " + std::to_string(r.frames) + " quadros, o vtrk declara " +
                             std::to_string(r.hdr.frames));
    if (wo.audio && ap < pcm.size() && !w.write_audio(std::span<const int16_t>(pcm.data() + ap, pcm.size() - ap), err)) return false;
    r.encoderNotes = w.notes();
    if (!w.finish(err)) return false;
    r.videoEnd100ns = int64_t(std::llround(double(r.frames) * 1e7 * s.fpsDen / s.fpsNum));
    r.audioSamples  = uint32_t(pcm.size() / ch);
    r.audioEnd100ns = wo.audio ? int64_t(r.audioSamples) * 10'000'000 / o.audioRate : 0;
    return true;
}

bool verify_4xm(const fs::path& src, const fs::path& mp4, const ConvertOptions& o, Fidelity& fd, std::string* err) {
    fd = {};
    SourceInfo s;
    if (!probe_4xm(src, o, s, err)) return false;
    nv::MfMovieReader rd;
    if (!rd.open(mp4, err)) return false;
    Pipe v;
    if (!v.start(videoCmd(o, src, s), err, o.cancel)) return false;
    std::vector<uint32_t> want(size_t(s.width) * s.height);
    nv::VideoFrame        got;
    double                psum = 0;
    fd.psnrMin = 1e9;
    while (v.read(want.data(), want.size() * 4)) {
        ++fd.framesRef;
        if (!rd.next_frame(got, err)) continue;
        ++fd.framesMp4;
        if (got.width != s.width || got.height != s.height) return v.kill(), fail(err, "MP4 com dimensoes diferentes da fonte");
        const double p = psnrRgb(want, got.argb);
        psum += p, fd.psnrMin = std::min(fd.psnrMin, p), fd.framesBelow35 += p < 35.0;
    }
    std::string log;
    const int rc = v.finish(log);
    if (v.cancelled()) return fail(err, "cancelado");
    if (rc != 0) return fail(err, "o FFmpeg falhou ao decodificar o video: " + log);
    while (rd.next_frame(got)) ++fd.framesMp4;
    if (fd.framesRef) fd.psnrAvg = psum / fd.framesRef;
    if (!s.hasAudio()) return true;
    // audio: atraso pelo maximo da correlacao em 2 s (canal 0) a partir do primeiro trecho com sinal (um comeco em
    // silencio nao tem pico: SCENE41), SNR depois de alinhado
    std::vector<int16_t> ref, mp4a, blk;
    if (!readAll(audioCmd(o, src, s), ref, err, o.cancel)) return false;
    int64_t t = 0;
    while (rd.next_audio(blk, t)) mp4a.insert(mp4a.end(), blk.begin(), blk.end());
    const uint32_t ch     = std::max(1u, s.channels);
    const size_t   frames = ref.size() / ch;
    size_t         from   = 0;
    while (from < frames && std::abs(int(ref[from * ch])) < 1024) ++from;
    if (from >= frames) from = 0;  // silencio do comeco ao fim: qualquer atraso serve
    const size_t win  = std::min<size_t>(size_t(o.audioRate) * 2, frames - from);
    double       best = -1e300;
    for (int32_t lag = 0; lag <= 4096; ++lag) {
        double c = 0;
        for (size_t i = from; i < from + win && (i + size_t(lag)) * ch < mp4a.size(); i += 2)
            c += double(ref[i * ch]) * mp4a[(i + size_t(lag)) * ch];
        if (c > best) best = c, fd.audioDelaySamples = lag;
    }
    double sig = 0, noise = 0;
    for (size_t i = 0; i < ref.size() / ch && (i + size_t(fd.audioDelaySamples)) * ch < mp4a.size(); ++i)
        for (uint32_t c = 0; c < ch; ++c) {
            const double x = ref[i * ch + c], y = mp4a[(i + size_t(fd.audioDelaySamples)) * ch + c];
            sig += x * x, noise += (x - y) * (x - y);
        }
    fd.audioSnr = noise > 0 ? 10 * std::log10(sig / noise) : 99.0;
    return true;
}

} // namespace hh::builder::video
