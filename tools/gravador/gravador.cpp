// SPDX-License-Identifier: MIT
// HeadHunter-Gravador (HH16.1): a tela da gravacao da campanha (docs/CAMPANHA.md). "Iniciar gravacao" roda o monitor
// (tools/dev/campanha.py jogar --criar), que abre o jogo gravando; fechar o jogo termina a sessao, e o monitor sela, copia
// e informa. Toda a logica da campanha continua no monitor: este programa so o inicia, mostra a saida e, no "Parar
// gravacao", pede ao jogo que feche (WM_CLOSE nas janelas dele, o mesmo que o X da janela). Ferramenta do workspace, fora
// do produto.
//
// Uso: HeadHunter-Gravador.exe [--raiz <workspace>] [--teste]
//      teste automatico (tests/campanha/test_gravador.py): --automatico [--parar-apos <s>] [--registro <arquivo>]
//      inicia a gravacao sozinho, pede o fim depois de <s> segundos (se dado), grava a saida no registro e sai com o
//      codigo do monitor.
// HH_CAMPANHA_PYTHON escolhe o Python (senao o do CMake, senao python.exe/py.exe do PATH).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

enum : UINT { WM_LINHA = WM_APP + 1, WM_FIM };
enum : int { ID_INICIAR = 101, ID_PARAR, ID_VERIFICAR, ID_STATUS, ID_TESTE, ID_LOG, ID_SITUACAO };
enum class Acao { nenhuma, gravar, verificar, status };

struct Estado {
    HWND        wnd = nullptr, situacao = nullptr, log = nullptr, teste = nullptr;
    HWND        iniciar = nullptr, parar = nullptr, verificar = nullptr, status = nullptr;
    HFONT       fonte = nullptr, fonteGrande = nullptr, fonteLog = nullptr;
    HBRUSH      fundoGravando = nullptr;
    fs::path    raiz;
    std::wstring python;
    Acao        acao = Acao::nenhuma;
    bool        gravando = false;
    bool        statusDepois = false;  // depois da gravacao, mostra o status da campanha
    DWORD       pidMonitor = 0;
    HANDLE      processo = nullptr;
    std::wstring ultimaSessao;  // "sessao N gravada (rc 0)", do monitor
    // teste automatico
    bool         automatico = false;
    int          pararApos = -1;
    ULONGLONG    inicioGravacao = 0;
    bool         pedidoParar = false;
    fs::path     registro;
    std::wstring textoRegistro;
    int          rcGravacao = -1;
} g;

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int    n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int   n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

// regras do CommandLineToArgvW
std::wstring aspas(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\n\v\"") == std::wstring::npos) return a;
    std::wstring r = L"\"";
    size_t       barras = 0;
    for (wchar_t c : a) {
        if (c == L'\\') {
            ++barras;
            continue;
        }
        r.append(c == L'"' ? barras * 2 + 1 : barras, L'\\');
        barras = 0;
        r += c;
    }
    r.append(barras * 2, L'\\');
    return r + L'"';
}

int escala(int v) { return MulDiv(v, int(GetDpiForWindow(g.wnd)), 96); }

void anexar(const std::wstring& texto) {
    g.textoRegistro += texto;
    if (!g.log) return;
    // a saida do jogo pode ser longa: guarda o fim
    const int tam = GetWindowTextLengthW(g.log);
    if (tam > 2'000'000) {
        SendMessageW(g.log, EM_SETSEL, 0, 1'000'000);
        SendMessageW(g.log, EM_REPLACESEL, FALSE, LPARAM(L""));
    }
    const int fim = GetWindowTextLengthW(g.log);
    SendMessageW(g.log, EM_SETSEL, fim, fim);
    SendMessageW(g.log, EM_REPLACESEL, FALSE, LPARAM(texto.c_str()));
}

void situacao(const std::wstring& texto) {
    SetWindowTextW(g.situacao, texto.c_str());
    InvalidateRect(g.situacao, nullptr, TRUE);
}

void atualizarBotoes() {
    const bool livre = g.acao == Acao::nenhuma;
    EnableWindow(g.iniciar, livre);
    EnableWindow(g.verificar, livre);
    EnableWindow(g.status, livre);
    EnableWindow(g.teste, livre);
    EnableWindow(g.parar, g.gravando);
}

fs::path pastaDoExe() {
    std::wstring b(32768, L'\0');
    b.resize(GetModuleFileNameW(nullptr, b.data(), DWORD(b.size())));
    return fs::path(b).parent_path();
}

// o workspace: a primeira pasta acima do executavel com tools/dev/campanha.py
fs::path acharRaiz() {
    std::error_code ec;
    for (fs::path p = pastaDoExe(); !p.empty(); p = p.parent_path()) {
        if (fs::is_regular_file(p / L"tools" / L"dev" / L"campanha.py", ec)) return p;
        if (p == p.parent_path()) break;
    }
    return {};
}

std::wstring acharPython() {
    wchar_t b[32768];
    if (DWORD n = GetEnvironmentVariableW(L"HH_CAMPANHA_PYTHON", b, 32768); n && n < 32768) return b;
#ifdef HH_PYTHON_PADRAO
    {
        std::error_code ec;
        const fs::path p = widen(HH_PYTHON_PADRAO);
        if (fs::is_regular_file(p, ec)) return p.native();
    }
#endif
    for (const wchar_t* nome : {L"python.exe", L"py.exe"})
        if (SearchPathW(nullptr, nome, nullptr, 32768, b, nullptr)) return b;
    return {};
}

// roda o monitor com estes argumentos; a saida chega linha a linha (WM_LINHA) e o fim no WM_FIM
bool rodarMonitor(Acao acao, const std::vector<std::wstring>& args) {
    const fs::path monitor = g.raiz / L"tools" / L"dev" / L"campanha.py";
    std::error_code ec;
    if (g.raiz.empty() || !fs::is_regular_file(monitor, ec)) {
        anexar(L"ERRO: tools\\dev\\campanha.py nao encontrado acima de " + pastaDoExe().native() +
               L" (use --raiz <workspace>)\r\n");
        return false;
    }
    if (g.python.empty() || !fs::is_regular_file(g.python, ec)) {
        anexar(L"ERRO: Python nao encontrado (" + (g.python.empty() ? std::wstring(L"PATH") : g.python) +
               L"); instale o Python 3 ou defina HH_CAMPANHA_PYTHON\r\n");
        return false;
    }
    // -u: sem buffer (a saida aparece na hora); -X utf8: acentos e caminhos em UTF-8 no cano
    std::wstring cmd = aspas(g.python) + L" -I -u -X utf8 " + aspas(monitor.native());
    for (const auto& a : args) cmd += L" " + aspas(a);

    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE              ler = nullptr, escrever = nullptr;
    if (!CreatePipe(&ler, &escrever, &sa, 0)) return false;
    SetHandleInformation(ler, HANDLE_FLAG_INHERIT, 0);
    HANDLE nulo = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{sizeof si};
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = nulo;
    si.hStdOutput = escrever;
    si.hStdError  = escrever;
    PROCESS_INFORMATION pi{};
    // sem job object: fechar esta tela nunca derruba o jogo nem o monitor no meio de uma sessao
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                                   nullptr, g.raiz.c_str(), &si, &pi);
    CloseHandle(escrever);
    if (nulo != INVALID_HANDLE_VALUE) CloseHandle(nulo);
    if (!ok) {
        CloseHandle(ler);
        anexar(L"ERRO: nao consegui iniciar o Python (" + std::to_wstring(GetLastError()) + L")\r\n");
        return false;
    }
    CloseHandle(pi.hThread);
    g.processo   = pi.hProcess;
    g.pidMonitor = pi.dwProcessId;
    g.acao       = acao;
    anexar(L"\r\n> campanha.py");
    for (const auto& a : args) anexar(L" " + a);
    anexar(L"\r\n");
    atualizarBotoes();
    std::thread([ler, proc = pi.hProcess, wnd = g.wnd] {
        std::string pend;
        char        buf[4096];
        DWORD       n = 0;
        auto        enviar = [&](std::string linha) {
            if (!linha.empty() && linha.back() == '\r') linha.pop_back();
            std::wstring w = widen(linha);
            if (w.empty() && !linha.empty()) {  // nao era UTF-8 (a saida do jogo): pagina ANSI
                const int k = MultiByteToWideChar(CP_ACP, 0, linha.data(), int(linha.size()), nullptr, 0);
                w.assign(size_t(k), L'\0');
                MultiByteToWideChar(CP_ACP, 0, linha.data(), int(linha.size()), w.data(), k);
            }
            PostMessageW(wnd, WM_LINHA, 0, LPARAM(new std::wstring(w + L"\r\n")));
        };
        while (ReadFile(ler, buf, sizeof buf, &n, nullptr) && n) {
            pend.append(buf, n);
            for (size_t p; (p = pend.find('\n')) != std::string::npos; pend.erase(0, p + 1)) enviar(pend.substr(0, p));
        }
        if (!pend.empty()) enviar(pend);
        CloseHandle(ler);
        WaitForSingleObject(proc, INFINITE);
        DWORD rc = DWORD(-1);
        GetExitCodeProcess(proc, &rc);
        PostMessageW(wnd, WM_FIM, WPARAM(rc), 0);
    }).detach();
    return true;
}

void iniciarGravacao() {
    std::vector<std::wstring> args{L"jogar", L"--criar"};
    const bool                teste = SendMessageW(g.teste, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (teste) args.push_back(L"--teste");
    g.ultimaSessao.clear();
    if (!rodarMonitor(Acao::gravar, args)) {
        situacao(L"Não foi possível iniciar a gravação (veja abaixo).");
        if (g.automatico) PostMessageW(g.wnd, WM_CLOSE, 0, 0);
        return;
    }
    g.gravando       = true;
    g.statusDepois   = true;
    g.inicioGravacao = GetTickCount64();
    situacao(teste ? L"GRAVANDO (campanha de teste). Jogue, SALVE no jogo e feche o jogo para parar."
                   : L"GRAVANDO. Jogue normalmente, SALVE no jogo e feche o jogo para parar a gravação.");
    atualizarBotoes();
}

// descendentes do monitor (o jogo e o que ele abrir), pela tabela de processos
std::set<DWORD> descendentes(DWORD raiz) {
    std::set<DWORD> r;
    HANDLE          snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return r;
    std::vector<std::pair<DWORD, DWORD>> pares;  // (pid, pai)
    PROCESSENTRY32W pe{sizeof pe};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        pares.emplace_back(pe.th32ProcessID, pe.th32ParentProcessID);
    CloseHandle(snap);
    std::set<DWORD> fronteira{raiz};
    while (!fronteira.empty()) {
        std::set<DWORD> nova;
        for (auto [pid, pai] : pares)
            if (fronteira.count(pai) && pid != raiz && !r.count(pid)) nova.insert(pid);
        r.insert(nova.begin(), nova.end());
        fronteira = std::move(nova);
    }
    return r;
}

// pede ao jogo que feche: WM_CLOSE nas janelas visiveis dele (o mesmo caminho do X; o jogo fecha a gravacao normalmente)
int pedirFimDoJogo() {
    struct Busca {
        std::set<DWORD> pids;
        int             janelas = 0;
    } b{descendentes(g.pidMonitor)};
    EnumWindows(
        [](HWND w, LPARAM p) -> BOOL {
            auto* b   = reinterpret_cast<Busca*>(p);
            DWORD pid = 0;
            GetWindowThreadProcessId(w, &pid);
            if (b->pids.count(pid) && IsWindowVisible(w) && !GetWindow(w, GW_OWNER)) {
                PostMessageW(w, WM_CLOSE, 0, 0);
                ++b->janelas;
            }
            return TRUE;
        },
        LPARAM(&b));
    return b.janelas;
}

void pararGravacao() {
    if (!g.gravando) return;
    if (!g.automatico &&
        MessageBoxW(g.wnd, L"O jogo vai ser fechado, como no X da janela. Você SALVOU no jogo?\n\nFechar o jogo e parar a gravação?",
                    L"Parar gravação", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
        return;
    if (pedirFimDoJogo()) {
        situacao(L"Fechando o jogo...");
        g.pedidoParar = true;
    } else if (!g.automatico) {
        anexar(L"a janela do jogo ainda não está aberta; tente de novo em alguns segundos\r\n");
    }
}

void fimDoMonitor(int rc) {
    CloseHandle(g.processo);
    g.processo        = nullptr;
    const Acao acao   = g.acao;
    g.acao            = Acao::nenhuma;
    g.gravando        = false;
    g.pidMonitor      = 0;
    anexar(L"(fim, codigo " + std::to_wstring(rc) + L")\r\n");
    if (acao == Acao::gravar) {
        g.rcGravacao = rc;
        if (!g.ultimaSessao.empty())
            situacao(L"Gravação parada: " + g.ultimaSessao + (rc == 0 ? L"." : L". Veja os avisos abaixo."));
        else
            situacao(L"Gravação não aconteceu ou terminou com erro (código " + std::to_wstring(rc) + L"). Veja abaixo.");
    } else if (acao == Acao::verificar) {
        situacao(rc == 0 ? L"Verificação aprovada (veja o veredito abaixo)." : L"Verificação REPROVADA ou com erro: veja abaixo.");
    }
    atualizarBotoes();
    if (acao == Acao::gravar && g.statusDepois) {
        g.statusDepois = false;
        if (rodarMonitor(Acao::status, {L"status"})) return;
    }
    if (g.automatico && g.acao == Acao::nenhuma) PostMessageW(g.wnd, WM_CLOSE, 0, 0);
}

void layout() {
    RECT c;
    GetClientRect(g.wnd, &c);
    const int m = escala(10), h = escala(30), larg = c.right - c.left;
    MoveWindow(g.situacao, m, m, larg - 2 * m, escala(48), TRUE);
    int       y = m + escala(56), x = m;
    const int b1 = escala(180), b2 = escala(220), b3 = escala(110);
    MoveWindow(g.iniciar, x, y, b1, h, TRUE);
    x += b1 + m;
    MoveWindow(g.parar, x, y, b2, h, TRUE);
    x += b2 + m;
    MoveWindow(g.verificar, x, y, b3, h, TRUE);
    x += b3 + m;
    MoveWindow(g.status, x, y, b3, h, TRUE);
    y += h + escala(6);
    MoveWindow(g.teste, m, y, larg - 2 * m, escala(24), TRUE);
    y += escala(30);
    MoveWindow(g.log, m, y, larg - 2 * m, std::max(0L, c.bottom - y - m), TRUE);
}

void criarFontes() {
    for (HFONT* f : {&g.fonte, &g.fonteGrande, &g.fonteLog})
        if (*f) DeleteObject(*f);
    const UINT        dpi = GetDpiForWindow(g.wnd);
    NONCLIENTMETRICSW ncm{sizeof ncm};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, dpi);
    g.fonte          = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW grande  = ncm.lfMessageFont;
    grande.lfHeight  = -MulDiv(13, int(dpi), 72);
    grande.lfWeight  = FW_BOLD;
    g.fonteGrande    = CreateFontIndirectW(&grande);
    g.fonteLog       = CreateFontW(-MulDiv(9, int(dpi), 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                   CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    for (HWND w : {g.iniciar, g.parar, g.verificar, g.status, g.teste}) SendMessageW(w, WM_SETFONT, WPARAM(g.fonte), TRUE);
    SendMessageW(g.situacao, WM_SETFONT, WPARAM(g.fonteGrande), TRUE);
    SendMessageW(g.log, WM_SETFONT, WPARAM(g.fonteLog), TRUE);
}

HWND filho(const wchar_t* cls, const wchar_t* texto, DWORD estilo, int id, DWORD ex = 0) {
    return CreateWindowExW(ex, cls, texto, WS_CHILD | WS_VISIBLE | estilo, 0, 0, 10, 10, g.wnd, HMENU(INT_PTR(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g.wnd       = w;
        g.situacao  = filho(L"STATIC", L"Pronto para gravar. Feche o jogo para parar a gravação.", SS_LEFT | SS_NOPREFIX,
                            ID_SITUACAO);
        g.iniciar   = filho(L"BUTTON", L"Iniciar gravação", BS_DEFPUSHBUTTON | WS_TABSTOP, ID_INICIAR);
        g.parar     = filho(L"BUTTON", L"Parar gravação (fecha o jogo)", BS_PUSHBUTTON | WS_TABSTOP, ID_PARAR);
        g.verificar = filho(L"BUTTON", L"Verificar", BS_PUSHBUTTON | WS_TABSTOP, ID_VERIFICAR);
        g.status    = filho(L"BUTTON", L"Status", BS_PUSHBUTTON | WS_TABSTOP, ID_STATUS);
        g.teste     = filho(L"BUTTON", L"Campanha de teste (sessão piloto; nunca mistura com a campanha de verdade)",
                            BS_AUTOCHECKBOX | WS_TABSTOP, ID_TESTE);
        g.log = filho(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL,
                      ID_LOG, WS_EX_CLIENTEDGE);
        SendMessageW(g.log, EM_SETLIMITTEXT, 0, 0);
        g.fundoGravando = CreateSolidBrush(RGB(255, 225, 225));
        criarFontes();
        layout();
        atualizarBotoes();
        anexar(L"Head Hunter Recompiled: gravação da campanha (guia: docs\\CAMPANHA.md)\r\n"
               L"Iniciar gravação abre o jogo gravando. Jogue, SALVE no jogo e feche a janela do jogo: a gravação para\r\n"
               L"sozinha, é selada e copiada. A primeira vez cria a campanha; as seguintes continuam a mesma.\r\n"
               L"Atenção: o ESC fecha o jogo na hora, sem perguntar.\r\n");
        anexar(L"workspace: " + (g.raiz.empty() ? std::wstring(L"(não encontrado)") : g.raiz.native()) + L"\r\n");
        anexar(L"python: " + (g.python.empty() ? std::wstring(L"(não encontrado)") : g.python) + L"\r\n");
        return 0;
    case WM_SIZE: layout(); return 0;
    case WM_DPICHANGED: {
        const RECT* r = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(w, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        criarFontes();
        layout();
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* mm             = reinterpret_cast<MINMAXINFO*>(lp);
        mm->ptMinTrackSize.x = MulDiv(720, int(GetDpiForWindow(w)), 96);
        mm->ptMinTrackSize.y = MulDiv(400, int(GetDpiForWindow(w)), 96);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        if (HWND(lp) == g.situacao && g.gravando) {
            SetBkColor(HDC(wp), RGB(255, 225, 225));
            SetTextColor(HDC(wp), RGB(160, 0, 0));
            return LRESULT(g.fundoGravando);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_INICIAR:
            if (g.acao == Acao::nenhuma) iniciarGravacao();
            return 0;
        case ID_PARAR: pararGravacao(); return 0;
        case ID_VERIFICAR:
            if (g.acao == Acao::nenhuma && rodarMonitor(Acao::verificar, {L"verificar", L"--paralelo", L"2"}))
                situacao(L"Verificando: reproduzindo as sessões sem janela (pode levar minutos)...");
            return 0;
        case ID_STATUS:
            if (g.acao == Acao::nenhuma) rodarMonitor(Acao::status, {L"status"});
            return 0;
        }
        break;
    case WM_LINHA: {
        std::unique_ptr<std::wstring> l(reinterpret_cast<std::wstring*>(lp));
        // "sessao N gravada (rc 0)" / "sessao N interrompida (rc 3)": o resultado da gravacao, do monitor
        if (g.acao == Acao::gravar && l->rfind(L"sessao ", 0) == 0 && l->find(L" (rc ") != std::wstring::npos)
            g.ultimaSessao = l->substr(0, l->find_last_not_of(L"\r\n") + 1);
        anexar(*l);
        return 0;
    }
    case WM_FIM: fimDoMonitor(int(wp)); return 0;
    case WM_TIMER:
        if (g.automatico && g.gravando && g.pararApos >= 0 && !g.pedidoParar &&
            GetTickCount64() - g.inicioGravacao >= ULONGLONG(g.pararApos) * 1000)
            pararGravacao();
        return 0;
    case WM_CLOSE:
        if (g.acao != Acao::nenhuma && !g.automatico) {
            MessageBoxW(w,
                        g.gravando ? L"A gravação está em andamento. Feche o jogo primeiro (ou use Parar gravação); "
                                     L"esta tela fecha depois."
                                   : L"Aguarde o fim da operação em andamento.",
                        L"Gravação da campanha", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (g.automatico && g.acao != Acao::nenhuma) return 0;  // o teste so sai depois do monitor
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    int     argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool    teste = false;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--raiz" && i + 1 < argc) g.raiz = argv[++i];
        else if (a == L"--teste") teste = true;
        else if (a == L"--automatico") g.automatico = true;
        else if (a == L"--parar-apos" && i + 1 < argc) g.pararApos = _wtoi(argv[++i]);
        else if (a == L"--registro" && i + 1 < argc) g.registro = argv[++i];
    }
    LocalFree(argv);
    if (g.raiz.empty()) g.raiz = acharRaiz();
    g.python = acharPython();

    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc   = proc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"HeadHunterGravador";
    wc.hIcon         = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm       = wc.hIcon;
    RegisterClassExW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"Head Hunter Recompiled: gravação da campanha", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 900, 620, nullptr, nullptr, inst, nullptr);
    if (!w) return 1;
    const UINT dpi = GetDpiForWindow(w);
    SetWindowPos(w, nullptr, 0, 0, MulDiv(900, int(dpi), 96), MulDiv(620, int(dpi), 96), SWP_NOMOVE | SWP_NOZORDER);
    if (teste) SendMessageW(g.teste, BM_SETCHECK, BST_CHECKED, 0);
    ShowWindow(w, g.automatico ? SW_SHOWMINNOACTIVE : show);
    if (g.automatico) {
        SetTimer(w, 1, 250, nullptr);
        iniciarGravacao();
    }

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(w, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (!g.registro.empty()) {
        std::ofstream f(g.registro, std::ios::binary);
        std::string   s = narrow(g.textoRegistro);
        f.write(s.data(), std::streamsize(s.size()));
    }
    for (HFONT f : {g.fonte, g.fonteGrande, g.fonteLog})
        if (f) DeleteObject(f);
    if (g.fundoGravando) DeleteObject(g.fundoGravando);
    return g.automatico ? (g.rcGravacao < 0 ? 1 : g.rcGravacao) : 0;
}
