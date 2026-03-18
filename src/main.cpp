#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include "vizcommand.h"
#include "output_panel.h"
#include "command.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

// Common Controls v6 (ビジュアルスタイル有効化)
#pragma comment(linker, "\"/manifestdependency:type='win32' " \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static HINSTANCE g_hInst;
static HWND      g_hMainWnd;
static HWND      g_hCmdEdit;
static HWND      g_hPrompt;     // ">" ラベル
static HWND      g_hOutputPanel;
static HFONT     g_hCmdFont;

// コマンド履歴
static std::vector<std::wstring> g_history;
static int g_historyIdx = 0;

// ================================================================
// コマンド入力エディットのサブクラス
// ================================================================
static LRESULT CALLBACK CmdEditSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR)
{
    switch (msg) {
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_RETURN: {
            wchar_t buf[4096] = {};
            GetWindowText(hwnd, buf, 4096);
            std::wstring cmd(buf);
            if (!cmd.empty()) {
                if (g_history.empty() || g_history.back() != cmd)
                    g_history.push_back(cmd);
                g_historyIdx = (int)g_history.size();
                SetWindowText(hwnd, L"");
                ExecuteCommand(g_hOutputPanel, cmd);
            }
            return 0;
        }
        case VK_UP:
            if (!g_history.empty() && g_historyIdx > 0) {
                --g_historyIdx;
                SetWindowText(hwnd, g_history[g_historyIdx].c_str());
                int len = (int)g_history[g_historyIdx].size();
                SendMessage(hwnd, EM_SETSEL, len, len);
            }
            return 0;
        case VK_DOWN:
            if (!g_history.empty()) {
                ++g_historyIdx;
                if (g_historyIdx >= (int)g_history.size()) {
                    g_historyIdx = (int)g_history.size();
                    SetWindowText(hwnd, L"");
                } else {
                    SetWindowText(hwnd, g_history[g_historyIdx].c_str());
                    int len = (int)g_history[g_historyIdx].size();
                    SendMessage(hwnd, EM_SETSEL, len, len);
                }
            }
            return 0;
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, CmdEditSubclassProc, uIdSubclass);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ================================================================
// メインウィンドウ プロシージャ
// ================================================================
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right;
        int h = rc.bottom;

        // Consolas フォント (コマンドバー用)
        g_hCmdFont = CreateFont(
            18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        // ">" プロンプトラベル
        g_hPrompt = CreateWindow(
            L"STATIC", L">",
            WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
            0, 0, 26, CMDBAR_HEIGHT,
            hwnd, nullptr, g_hInst, nullptr);
        SendMessage(g_hPrompt, WM_SETFONT, (WPARAM)g_hCmdFont, FALSE);

        // コマンド入力エディット (横幅いっぱい)
        g_hCmdEdit = CreateWindowEx(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            26, 0, w - 26, CMDBAR_HEIGHT,
            hwnd, (HMENU)(UINT_PTR)CMD_EDIT_ID, g_hInst, nullptr);
        SendMessage(g_hCmdEdit, WM_SETFONT, (WPARAM)g_hCmdFont, FALSE);
        SendMessage(g_hCmdEdit, EM_SETCUEBANNER, 0,
                    (LPARAM)L"コマンドを入力してください (help で一覧)");
        SetWindowSubclass(g_hCmdEdit, CmdEditSubclassProc, 1, 0);

        // ウィンドウリストビュー (出力パネル)
        g_hOutputPanel = OutputPanel_Create(
            hwnd, g_hInst, 0, CMDBAR_HEIGHT, w, h - CMDBAR_HEIGHT);

        // カレントディレクトリ初期化
        wchar_t cwd[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, cwd);
        g_currentDir = cwd;

        // 起動メッセージ
        OutputPanel_AddText(g_hOutputPanel,
            L"VizCommand へようこそ。\n"
            L"カレントディレクトリ: " + g_currentDir + L"\n"
            L"\"help\" と入力するとコマンド一覧が表示されます。");

        SetFocus(g_hCmdEdit);
        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        MoveWindow(g_hPrompt,      0,  0, 26,      CMDBAR_HEIGHT, TRUE);
        MoveWindow(g_hCmdEdit,    26,  0, w - 26,  CMDBAR_HEIGHT, TRUE);
        MoveWindow(g_hOutputPanel, 0, CMDBAR_HEIGHT, w, h - CMDBAR_HEIGHT, TRUE);
        return 0;
    }

    // コマンドバー領域の背景色 (薄いグレー)
    case WM_CTLCOLOREDIT:
        if ((HWND)lParam == g_hCmdEdit) {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(245, 245, 245));
            SetTextColor(hdc, RGB(30, 30, 30));
            static HBRUSH hBr = CreateSolidBrush(RGB(245, 245, 245));
            return (LRESULT)hBr;
        }
        break;

    case WM_CTLCOLORSTATIC:
        if ((HWND)lParam == g_hPrompt) {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(245, 245, 245));
            SetTextColor(hdc, RGB(0, 128, 0));
            static HBRUSH hBrS = CreateSolidBrush(RGB(245, 245, 245));
            return (LRESULT)hBrS;
        }
        break;

    case WM_SETFOCUS:
        SetFocus(g_hCmdEdit);
        return 0;

    case WM_DESTROY:
        if (g_hCmdFont) DeleteObject(g_hCmdFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ================================================================
// WinMain
// ================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    g_hInst = hInst;

    // GDI+ 初期化
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    // Common Controls (ListView等)
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    // ウィンドウリストビュークラスを登録
    OutputPanel_Register(hInst);

    // メインウィンドウクラスを登録
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = CreateSolidBrush(RGB(245, 245, 245));
    wc.lpszClassName = L"VizCommandMain";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wc);

    g_hMainWnd = CreateWindowEx(
        0,
        L"VizCommandMain", APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 680,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
