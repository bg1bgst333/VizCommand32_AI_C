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
static HWND      g_hOutputPanel;

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

        // 出力パネル (ウィンドウ全体)
        g_hOutputPanel = OutputPanel_Create(hwnd, g_hInst, 0, 0, w, h);

        // カレントディレクトリ初期化
        wchar_t cwd[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, cwd);
        g_currentDir = cwd;

        // 起動メッセージ
        OutputPanel_AddText(g_hOutputPanel,
            L"VizCommand へようこそ。\n"
            L"カレントディレクトリ: " + g_currentDir + L"\n"
            L"\"help\" と入力するとコマンド一覧が表示されます。");

        // 最初のプロンプト
        OutputPanel_AddPrompt(g_hOutputPanel);

        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        MoveWindow(g_hOutputPanel, 0, 0, w, h, TRUE);
        return 0;
    }

    case WM_SETFOCUS:
        OutputPanel_FocusPrompt(g_hOutputPanel);
        return 0;

    case WM_DESTROY:
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
