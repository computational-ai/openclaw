/*
 * calculator.cpp
 * Windows x64 Calculator Application (Win32 API)
 *
 * Modes
 *   GUI (default)      – graphical calculator; toggle the console pane with
 *                        the "Console ▼/▲" button at the bottom of the window.
 *   Console (--console) – pure command-line REPL; no GUI window is shown.
 *
 * GUI console pane
 *   History area  – shows every expression entered and its result.
 *   Input field   – type any expression (e.g. "3 + 4 * 2") and press Enter
 *                   to evaluate; the result is also reflected in the main
 *                   display so subsequent button operations continue from it.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

/* ── Layout constants ──────────────────────────────────────── */
static const int DISPLAY_H       = 72;
static const int BTN_W           = 72;
static const int BTN_H           = 64;
static const int BTN_PAD         = 6;
static const int WIN_MARGIN      = 10;
static const int TOGGLE_H        = 28;   /* "Console ▼" button height    */
static const int CONSOLE_HIST_H  = 160;  /* history output area height   */
static const int CONSOLE_INPUT_H = 32;   /* expression input field height */
static const int CONSOLE_SEP     = 6;    /* gap between toggle and pane  */

/* 4 columns × 5 rows of buttons */
static const int COLS = 4;
static const int ROWS = 5;

static const int WIN_W =
    WIN_MARGIN * 2 + COLS * BTN_W + (COLS - 1) * BTN_PAD;

/* Outer window height without the console pane.
 * The +30 accounts for the non-client title-bar area (approximate). */
static const int WIN_H_BASE =
    WIN_MARGIN + DISPLAY_H + BTN_PAD
    + ROWS * BTN_H + (ROWS - 1) * BTN_PAD
    + BTN_PAD + TOGGLE_H + WIN_MARGIN + 30;

/* Extra outer height added when the console pane is shown */
static const int CONSOLE_EXTRA_H =
    CONSOLE_SEP + CONSOLE_HIST_H + BTN_PAD + CONSOLE_INPUT_H + WIN_MARGIN;

/* ── Button IDs ────────────────────────────────────────────── */
enum ButtonID {
    ID_BTN_0 = 100, ID_BTN_1, ID_BTN_2, ID_BTN_3, ID_BTN_4,
    ID_BTN_5, ID_BTN_6, ID_BTN_7, ID_BTN_8, ID_BTN_9,
    ID_BTN_DOT,
    ID_BTN_PLUS, ID_BTN_MINUS, ID_BTN_MUL, ID_BTN_DIV,
    ID_BTN_EQUALS,
    ID_BTN_CLEAR, ID_BTN_CLEAR_ENTRY,
    ID_BTN_SIGN, ID_BTN_PERCENT,
    ID_BTN_BACKSPACE,
    ID_BTN_TOGGLE_CONSOLE,
    ID_EDIT_CONSOLE_INPUT,
};

/* ── Button grid definition ────────────────────────────────── */
struct BtnDef { const wchar_t *label; int id; };

static const BtnDef GRID[ROWS][COLS] = {
    { {L"%",   ID_BTN_PERCENT},   {L"CE",  ID_BTN_CLEAR_ENTRY},
      {L"C",   ID_BTN_CLEAR},     {L"⌫",  ID_BTN_BACKSPACE} },
    { {L"7",   ID_BTN_7},         {L"8",   ID_BTN_8},
      {L"9",   ID_BTN_9},         {L"÷",   ID_BTN_DIV}       },
    { {L"4",   ID_BTN_4},         {L"5",   ID_BTN_5},
      {L"6",   ID_BTN_6},         {L"×",   ID_BTN_MUL}       },
    { {L"1",   ID_BTN_1},         {L"2",   ID_BTN_2},
      {L"3",   ID_BTN_3},         {L"−",   ID_BTN_MINUS}      },
    { {L"+/−", ID_BTN_SIGN},      {L"0",   ID_BTN_0},
      {L".",   ID_BTN_DOT},       {L"+",   ID_BTN_PLUS}       },
};

/* ── Calculator state ──────────────────────────────────────── */
struct CalcState {
    double  operand      = 0.0;
    int     pendingOp    = 0;       /* 0 = none, or button ID of last op */
    bool    justOpPressed = false;  /* next digit starts a fresh number   */
    bool    hasDecimal   = false;
    bool    errorState   = false;

    std::wstring display  = L"0";
};

static CalcState g_calc;
static HWND      g_hDisplay       = nullptr;
static HWND      g_hToggleConsole = nullptr;
static HWND      g_hConsoleHist   = nullptr;  /* history output (read-only multiline edit) */
static HWND      g_hConsoleInput  = nullptr;  /* expression input field                   */
static bool      g_consoleVisible = false;

/* ═══════════════════════════════════════════════════════════
   Expression evaluator  –  recursive-descent parser
   Supports: numbers, + − * /  ( )  unary minus
   Used by both the GUI console pane and the CLI REPL.
   ═══════════════════════════════════════════════════════════ */
namespace Expr {

enum TokType { T_NUM, T_PLUS, T_MINUS, T_MUL, T_DIV,
               T_LPAREN, T_RPAREN, T_END, T_ERR };

struct Token { TokType type; double value = 0.0; };

struct Parser {
    const wchar_t *src;
    size_t         pos;
    std::wstring   error;

    void skipWS() {
        while (src[pos] == L' ' || src[pos] == L'\t') ++pos;
    }

    Token next() {
        skipWS();
        wchar_t c = src[pos];
        if (c == L'\0')                    return {T_END};
        if (c == L'+') { ++pos;            return {T_PLUS};   }
        if (c == L'-') { ++pos;            return {T_MINUS};  }
        if (c == L'*' || c == L'\xd7' /* × */) { ++pos; return {T_MUL}; }
        if (c == L'/' || c == L'\xf7' /* ÷ */) { ++pos; return {T_DIV}; }
        if (c == L'(') { ++pos;            return {T_LPAREN}; }
        if (c == L')') { ++pos;            return {T_RPAREN}; }
        if (iswdigit(c) || c == L'.') {
            wchar_t *end = nullptr;
            double v = wcstod(src + pos, &end);
            pos = (size_t)(end - src);
            return {T_NUM, v};
        }
        error = L"Unexpected character: ";
        error += c;
        return {T_ERR};
    }

    Token peek() {
        size_t save = pos;
        Token t = next();
        pos = save;
        return t;
    }

    /* expr = term ( ('+' | '-') term )* */
    double parseExpr(bool &ok) {
        double lhs = parseTerm(ok);
        if (!ok) return 0;
        while (true) {
            Token t = peek();
            if (t.type == T_PLUS || t.type == T_MINUS) {
                next();
                double rhs = parseTerm(ok);
                if (!ok) return 0;
                lhs = (t.type == T_PLUS) ? lhs + rhs : lhs - rhs;
            } else break;
        }
        return lhs;
    }

    /* term = factor ( ('*' | '/') factor )* */
    double parseTerm(bool &ok) {
        double lhs = parseFactor(ok);
        if (!ok) return 0;
        while (true) {
            Token t = peek();
            if (t.type == T_MUL || t.type == T_DIV) {
                next();
                double rhs = parseFactor(ok);
                if (!ok) return 0;
                if (t.type == T_DIV) {
                    if (rhs == 0.0) { error = L"Division by zero"; ok = false; return 0; }
                    lhs /= rhs;
                } else {
                    lhs *= rhs;
                }
            } else break;
        }
        return lhs;
    }

    /* factor = ['-'] ( number | '(' expr ')' ) */
    double parseFactor(bool &ok) {
        Token t = peek();
        if (t.type == T_MINUS) {
            next();
            double v = parseFactor(ok);
            return ok ? -v : 0;
        }
        if (t.type == T_LPAREN) {
            next();
            double v = parseExpr(ok);
            if (!ok) return 0;
            if (peek().type != T_RPAREN) {
                error = L"Missing closing parenthesis";
                ok = false; return 0;
            }
            next();
            return v;
        }
        if (t.type == T_NUM) { next(); return t.value; }
        error = L"Expected number";
        ok = false;
        return 0;
    }
};

/* Evaluate expression string. Returns false and sets errMsg on failure. */
static bool Evaluate(const std::wstring &expr, double &result, std::wstring &errMsg)
{
    Parser p;
    p.src = expr.c_str();
    p.pos = 0;
    bool ok = true;
    result = p.parseExpr(ok);
    if (!ok) { errMsg = p.error; return false; }
    p.skipWS();
    if (p.src[p.pos] != L'\0') {
        errMsg = L"Unexpected token after expression";
        return false;
    }
    return true;
}

} // namespace Expr

/* ── Helpers ───────────────────────────────────────────────── */

static void UpdateDisplay()
{
    SetWindowTextW(g_hDisplay, g_calc.display.c_str());
}

/* Format a double without unnecessary trailing zeros */
static std::wstring FormatNumber(double v)
{
    if (std::isinf(v) || std::isnan(v)) return L"Error";

    wchar_t buf[64];
    /* Use up to 15 significant digits, then strip trailing zeros */
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.14g", v);
    return std::wstring(buf);
}

/* Append a line to the console history area */
static void ConsoleAppend(const std::wstring &line)
{
    if (!g_hConsoleHist) return;
    int len = GetWindowTextLengthW(g_hConsoleHist);
    SendMessageW(g_hConsoleHist, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    std::wstring nl = (len > 0 ? L"\r\n" : L"") + line;
    SendMessageW(g_hConsoleHist, EM_REPLACESEL, FALSE, (LPARAM)nl.c_str());
    SendMessageW(g_hConsoleHist, WM_VSCROLL, SB_BOTTOM, 0);
}

/* ── Arithmetic ─────────────────────────────────────────────── */
static double ApplyOp(int op, double a, double b, bool &error)
{
    error = false;
    switch (op) {
        case ID_BTN_PLUS:  return a + b;
        case ID_BTN_MINUS: return a - b;
        case ID_BTN_MUL:   return a * b;
        case ID_BTN_DIV:
            if (b == 0.0) { error = true; return 0.0; }
            return a / b;
        default: return b;
    }
}

/* ── Button handler ─────────────────────────────────────────── */
static void HandleButton(HWND hwnd, int id)
{
    CalcState &c = g_calc;

    if (c.errorState && id != ID_BTN_CLEAR && id != ID_BTN_CLEAR_ENTRY)
        return;

    /* Digit buttons */
    if (id >= ID_BTN_0 && id <= ID_BTN_9) {
        wchar_t digit = L'0' + (wchar_t)(id - ID_BTN_0);
        if (c.justOpPressed || c.display == L"0") {
            c.display = std::wstring(1, digit);
            c.hasDecimal = false;
            c.justOpPressed = false;
        } else {
            if (c.display.size() < 15)
                c.display += digit;
        }
        UpdateDisplay();
        return;
    }

    switch (id) {

    case ID_BTN_DOT:
        if (c.justOpPressed) {
            c.display = L"0.";
            c.hasDecimal = true;
            c.justOpPressed = false;
        } else if (!c.hasDecimal) {
            c.display += L'.';
            c.hasDecimal = true;
        }
        UpdateDisplay();
        break;

    case ID_BTN_SIGN:
        if (c.display != L"0") {
            if (c.display[0] == L'-')
                c.display.erase(0, 1);
            else
                c.display.insert(0, 1, L'-');
            UpdateDisplay();
        }
        break;

    case ID_BTN_PERCENT: {
        double v = _wtof(c.display.c_str());
        v /= 100.0;
        c.display = FormatNumber(v);
        c.hasDecimal = (c.display.find(L'.') != std::wstring::npos);
        c.justOpPressed = false;
        UpdateDisplay();
        break;
    }

    case ID_BTN_BACKSPACE:
        if (!c.justOpPressed && c.display.size() > 1) {
            if (c.display.back() == L'.') c.hasDecimal = false;
            c.display.pop_back();
        } else {
            c.display = L"0";
            c.hasDecimal = false;
        }
        UpdateDisplay();
        break;

    case ID_BTN_CLEAR_ENTRY:
        c.display = L"0";
        c.hasDecimal = false;
        c.justOpPressed = false;
        c.errorState = false;
        UpdateDisplay();
        break;

    case ID_BTN_CLEAR:
        c = CalcState{};
        UpdateDisplay();
        break;

    case ID_BTN_PLUS:
    case ID_BTN_MINUS:
    case ID_BTN_MUL:
    case ID_BTN_DIV: {
        double current = _wtof(c.display.c_str());
        if (c.pendingOp && !c.justOpPressed) {
            bool err = false;
            double result = ApplyOp(c.pendingOp, c.operand, current, err);
            if (err) {
                c.display = L"Cannot divide by zero";
                c.errorState = true;
                c.pendingOp = 0;
                UpdateDisplay();
                return;
            }
            c.operand = result;
            c.display = FormatNumber(result);
        } else {
            c.operand = current;
        }
        c.pendingOp = id;
        c.justOpPressed = true;
        c.hasDecimal = false;
        UpdateDisplay();
        break;
    }

    case ID_BTN_EQUALS: {
        double current = _wtof(c.display.c_str());
        bool err = false;
        double result = (c.pendingOp)
                      ? ApplyOp(c.pendingOp, c.operand, current, err)
                      : current;
        if (err) {
            c.display = L"Cannot divide by zero";
            c.errorState = true;
        } else {
            c.display = FormatNumber(result);
            c.operand = result;
        }
        c.pendingOp = 0;
        c.justOpPressed = true;
        c.hasDecimal = (c.display.find(L'.') != std::wstring::npos);
        UpdateDisplay();
        break;
    }

    case ID_BTN_TOGGLE_CONSOLE: {
        g_consoleVisible = !g_consoleVisible;
        SetWindowTextW(g_hToggleConsole,
                       g_consoleVisible ? L"Console ▲" : L"Console ▼");
        ShowWindow(g_hConsoleHist,  g_consoleVisible ? SW_SHOW : SW_HIDE);
        ShowWindow(g_hConsoleInput, g_consoleVisible ? SW_SHOW : SW_HIDE);
        RECT rc;
        GetWindowRect(hwnd, &rc);
        int newH = (rc.bottom - rc.top)
                 + (g_consoleVisible ? CONSOLE_EXTRA_H : -CONSOLE_EXTRA_H);
        SetWindowPos(hwnd, nullptr, 0, 0,
                     rc.right - rc.left, newH,
                     SWP_NOMOVE | SWP_NOZORDER);
        if (g_consoleVisible) SetFocus(g_hConsoleInput);
        break;
    }

    default:
        break;
    }
}

/* ── Console input handler ──────────────────────────────────── */
static void HandleConsoleInput(HWND hwnd)
{
    wchar_t buf[512] = {};
    GetWindowTextW(g_hConsoleInput, buf, _countof(buf));
    SetWindowTextW(g_hConsoleInput, L"");

    /* Trim leading/trailing whitespace */
    std::wstring expr(buf);
    size_t s = expr.find_first_not_of(L" \t");
    if (s == std::wstring::npos) return;
    size_t e = expr.find_last_not_of(L" \t");
    expr = expr.substr(s, e - s + 1);

    ConsoleAppend(L"> " + expr);

    double result = 0.0;
    std::wstring errMsg;
    if (Expr::Evaluate(expr, result, errMsg)) {
        std::wstring resultStr = FormatNumber(result);
        ConsoleAppend(L"  = " + resultStr);
        /* Sync result to the main display so button operations continue from it */
        g_calc = CalcState{};
        g_calc.display      = resultStr;
        g_calc.operand      = result;
        g_calc.hasDecimal   = (resultStr.find(L'.') != std::wstring::npos);
        g_calc.justOpPressed = true;
        UpdateDisplay();
    } else {
        ConsoleAppend(L"  Error: " + errMsg);
    }
    (void)hwnd;
}

/* ── Window procedure ───────────────────────────────────────── */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {

    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED)
            HandleButton(hwnd, LOWORD(wp));
        return 0;

    /* Keyboard support (forwarded from the message loop) */
    case WM_KEYDOWN:
    case WM_CHAR: {
        int vk = (int)wp;
        int mapped = 0;
        if (vk >= '0' && vk <= '9')                     mapped = ID_BTN_0 + (vk - '0');
        else if (vk == VK_NUMPAD0)                      mapped = ID_BTN_0;
        else if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) mapped = ID_BTN_1 + (vk - VK_NUMPAD1);
        else if (vk == '+')                             mapped = ID_BTN_PLUS;
        else if (vk == '-')                             mapped = ID_BTN_MINUS;
        else if (vk == '*')                             mapped = ID_BTN_MUL;
        else if (vk == '/')                             mapped = ID_BTN_DIV;
        else if (vk == VK_ADD)                          mapped = ID_BTN_PLUS;
        else if (vk == VK_SUBTRACT)                     mapped = ID_BTN_MINUS;
        else if (vk == VK_MULTIPLY)                     mapped = ID_BTN_MUL;
        else if (vk == VK_DIVIDE)                       mapped = ID_BTN_DIV;
        else if (vk == '.' || vk == VK_DECIMAL)         mapped = ID_BTN_DOT;
        else if (vk == VK_RETURN || vk == '=')          mapped = ID_BTN_EQUALS;
        else if (vk == VK_ESCAPE)                       mapped = ID_BTN_CLEAR;
        else if (vk == VK_BACK)                         mapped = ID_BTN_BACKSPACE;
        else if (vk == '%')                             mapped = ID_BTN_PERCENT;
        if (mapped) HandleButton(hwnd, mapped);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ── Console-mode REPL ──────────────────────────────────────── */
static void RunConsoleMode()
{
    /* Attach to a parent console (cmd.exe) or open a new one */
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        AllocConsole();

    FILE *fIn = nullptr, *fOut = nullptr;
    freopen_s(&fIn,  "CONIN$",  "r", stdin);
    freopen_s(&fOut, "CONOUT$", "w", stdout);

    wprintf(L"\nOpenClaw Calculator – Console Mode\n");
    wprintf(L"Enter an expression (e.g.  3 + 4 * 2 ) or 'exit' to quit.\n\n");

    wchar_t line[512];
    for (;;) {
        wprintf(L"> ");
        fflush(stdout);
        if (!fgetws(line, _countof(line), stdin)) break;

        /* Strip trailing newline */
        size_t len = wcslen(line);
        while (len > 0 && (line[len - 1] == L'\n' || line[len - 1] == L'\r'))
            line[--len] = L'\0';

        if (wcscmp(line, L"exit") == 0 || wcscmp(line, L"quit") == 0) break;
        if (len == 0) continue;

        double result = 0.0;
        std::wstring errMsg;
        if (Expr::Evaluate(std::wstring(line), result, errMsg))
            wprintf(L"  = %s\n", FormatNumber(result).c_str());
        else
            wprintf(L"  Error: %s\n", errMsg.c_str());
    }

    if (fOut) fclose(fOut);
    if (fIn)  fclose(fIn);
}

/* ── Entry point ─────────────────────────────────────────────── */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    /* ── Parse command line ── */
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool consoleMode = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--console") == 0) consoleMode = true;
    }
    LocalFree(argv);

    if (consoleMode) {
        RunConsoleMode();
        return 0;
    }

    /* ── GUI mode ── */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"OpenClawCalc";
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, L"OpenClawCalc", L"Calculator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H_BASE,
        nullptr, nullptr, hInst, nullptr);

    /* ── Display (read-only edit) ── */
    g_hDisplay = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT", L"0",
        WS_CHILD | WS_VISIBLE | ES_RIGHT | ES_READONLY,
        WIN_MARGIN, WIN_MARGIN,
        WIN_W - WIN_MARGIN * 2, DISPLAY_H,
        hwnd, nullptr, hInst, nullptr);

    HFONT hDisplayFont = CreateFontW(
        36, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(g_hDisplay, WM_SETFONT, (WPARAM)hDisplayFont, TRUE);

    HFONT hBtnFont = CreateFontW(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    /* ── Button grid ── */
    int yStart = WIN_MARGIN + DISPLAY_H + BTN_PAD;
    for (int row = 0; row < ROWS; ++row) {
        int y = yStart + row * (BTN_H + BTN_PAD);
        for (int col = 0; col < COLS; ++col) {
            int x = WIN_MARGIN + col * (BTN_W + BTN_PAD);
            const BtnDef &b = GRID[row][col];
            HWND hBtn = CreateWindowExW(
                0, L"BUTTON", b.label,
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x, y, BTN_W, BTN_H,
                hwnd, (HMENU)(INT_PTR)b.id, hInst, nullptr);
            SendMessageW(hBtn, WM_SETFONT, (WPARAM)hBtnFont, TRUE);
        }
    }

    /* ── "Console ▼" toggle button ── */
    int yToggle = yStart + ROWS * (BTN_H + BTN_PAD);
    g_hToggleConsole = CreateWindowExW(
        0, L"BUTTON", L"Console \u25bc",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        WIN_MARGIN, yToggle,
        WIN_W - WIN_MARGIN * 2, TOGGLE_H,
        hwnd, (HMENU)(INT_PTR)ID_BTN_TOGGLE_CONSOLE, hInst, nullptr);

    HFONT hSmallFont = CreateFontW(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(g_hToggleConsole, WM_SETFONT, (WPARAM)hSmallFont, TRUE);

    /* ── Console pane (hidden by default) ── */
    int yConsole = yToggle + TOGGLE_H + CONSOLE_SEP;

    g_hConsoleHist = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT", L"",
        WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
        WIN_MARGIN, yConsole,
        WIN_W - WIN_MARGIN * 2, CONSOLE_HIST_H,
        hwnd, nullptr, hInst, nullptr);

    g_hConsoleInput = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL,
        WIN_MARGIN, yConsole + CONSOLE_HIST_H + BTN_PAD,
        WIN_W - WIN_MARGIN * 2, CONSOLE_INPUT_H,
        hwnd, (HMENU)(INT_PTR)ID_EDIT_CONSOLE_INPUT, hInst, nullptr);

    HFONT hMonoFont = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
    SendMessageW(g_hConsoleHist,  WM_SETFONT, (WPARAM)hMonoFont, TRUE);
    SendMessageW(g_hConsoleInput, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

    /* Placeholder text in the input field */
    SendMessageW(g_hConsoleInput, EM_SETCUEBANNER, FALSE,
                 (LPARAM)L"Enter expression, e.g.  3 + 4 * 2");

    /* Seed the history with a usage hint */
    ConsoleAppend(L"Type an expression and press Enter  (e.g. 3 + 4 * 2)");

    ShowWindow(g_hConsoleHist,  SW_HIDE);
    ShowWindow(g_hConsoleInput, SW_HIDE);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* ── Message loop ── */
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        /* Enter in the console input: evaluate the expression */
        if (msg.message == WM_KEYDOWN
            && msg.wParam == VK_RETURN
            && msg.hwnd == g_hConsoleInput)
        {
            HandleConsoleInput(hwnd);
            /* Skip TranslateMessage / DispatchMessage so the edit
               control does not beep or insert a newline. */
            continue;
        }

        /* Forward calculator keyboard shortcuts to the main window,
           but not while the user is typing in the console input. */
        if ((msg.message == WM_KEYDOWN || msg.message == WM_CHAR)
            && msg.hwnd != g_hConsoleInput)
        {
            SendMessageW(hwnd, msg.message, msg.wParam, msg.lParam);
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(hDisplayFont);
    DeleteObject(hBtnFont);
    DeleteObject(hSmallFont);
    DeleteObject(hMonoFont);
    return (int)msg.wParam;
}
