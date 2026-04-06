/*
 * calculator.cpp
 * Windows x64 Calculator Application (Win32 API)
 *
 * Supports: addition, subtraction, multiplication, division,
 *           decimal input, sign toggle (+/-), percentage, and clear.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>

#pragma comment(lib, "comctl32.lib")

/* ── Layout constants ──────────────────────────────────────── */
static const int DISPLAY_H  = 72;
static const int BTN_W      = 72;
static const int BTN_H      = 64;
static const int BTN_PAD    = 6;
static const int WIN_MARGIN = 10;

/* 4 columns × 5 rows of buttons */
static const int COLS = 4;
static const int ROWS = 5;
static const int WIN_W = WIN_MARGIN * 2 + COLS * BTN_W + (COLS - 1) * BTN_PAD;
static const int WIN_H = WIN_MARGIN * 2 + DISPLAY_H + BTN_PAD
                       + ROWS * BTN_H + (ROWS - 1) * BTN_PAD + 30;

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
static HWND      g_hDisplay = nullptr;

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

    default:
        break;
    }
}

/* ── Window procedure ───────────────────────────────────────── */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {

    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED)
            HandleButton(hwnd, LOWORD(wp));
        return 0;

    /* Keyboard support */
    case WM_KEYDOWN:
    case WM_CHAR: {
        int vk = (int)wp;
        int mapped = 0;
        if (vk >= '0' && vk <= '9')                   mapped = ID_BTN_0 + (vk - '0');
        else if (vk == VK_NUMPAD0)                    mapped = ID_BTN_0;
        else if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) mapped = ID_BTN_1 + (vk - VK_NUMPAD1);
        else if (vk == '+')                            mapped = ID_BTN_PLUS;
        else if (vk == '-')                            mapped = ID_BTN_MINUS;
        else if (vk == '*')                            mapped = ID_BTN_MUL;
        else if (vk == '/')                            mapped = ID_BTN_DIV;
        else if (vk == VK_ADD)                         mapped = ID_BTN_PLUS;
        else if (vk == VK_SUBTRACT)                    mapped = ID_BTN_MINUS;
        else if (vk == VK_MULTIPLY)                    mapped = ID_BTN_MUL;
        else if (vk == VK_DIVIDE)                      mapped = ID_BTN_DIV;
        else if (vk == '.' || vk == VK_DECIMAL)        mapped = ID_BTN_DOT;
        else if (vk == VK_RETURN || vk == '=')         mapped = ID_BTN_EQUALS;
        else if (vk == VK_ESCAPE)                      mapped = ID_BTN_CLEAR;
        else if (vk == VK_BACK)                        mapped = ID_BTN_BACKSPACE;
        else if (vk == '%')                            mapped = ID_BTN_PERCENT;
        if (mapped) HandleButton(hwnd, mapped);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ── Entry point ─────────────────────────────────────────────── */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    /* Enable visual styles */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    /* Register window class */
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

    /* Create main window (fixed size) */
    HWND hwnd = CreateWindowExW(
        0, L"OpenClawCalc", L"Calculator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
        nullptr, nullptr, hInst, nullptr);

    /* ── Display (read-only edit control) ── */
    g_hDisplay = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT", L"0",
        WS_CHILD | WS_VISIBLE | ES_RIGHT | ES_READONLY,
        WIN_MARGIN,
        WIN_MARGIN,
        WIN_W - WIN_MARGIN * 2,
        DISPLAY_H,
        hwnd, nullptr, hInst, nullptr);

    /* Enlarge display font */
    HFONT hDisplayFont = CreateFontW(
        36, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(g_hDisplay, WM_SETFONT, (WPARAM)hDisplayFont, TRUE);

    /* Normal button font */
    HFONT hBtnFont = CreateFontW(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    /* ── Create buttons ── */
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

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* ── Message loop ── */
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        /* Allow keyboard input on the main window even when a button has focus */
        if (msg.message == WM_KEYDOWN || msg.message == WM_CHAR)
            SendMessageW(hwnd, msg.message, msg.wParam, msg.lParam);
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(hDisplayFont);
    DeleteObject(hBtnFont);
    return (int)msg.wParam;
}
