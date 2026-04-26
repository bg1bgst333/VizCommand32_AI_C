#include "output_panel.h"
#include "command.h"
#include <commctrl.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

// ================================================================
// ImageCtrl  GDI+ 画像表示カスタムコントロール
// ================================================================
#define IMAGE_CTRL_CLASS L"VizCmdImageCtrl"

struct ImageCtrlData {
    Gdiplus::Image* pImage = nullptr;
    std::wstring    path;
};

static LRESULT CALLBACK ImageCtrlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* d = (ImageCtrlData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)new ImageCtrlData{});
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        if (d && d->pImage && d->pImage->GetLastStatus() == Gdiplus::Ok) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

            UINT imgW = d->pImage->GetWidth();
            UINT imgH = d->pImage->GetHeight();
            int  ctrlW = rc.right - rc.left;
            int  ctrlH = rc.bottom - rc.top;

            float scaleX = (float)ctrlW / imgW;
            float scaleY = (float)ctrlH / imgH;
            float scale  = std::min(scaleX, scaleY);
            int drawW = (int)(imgW * scale);
            int drawH = (int)(imgH * scale);
            int drawX = (ctrlW - drawW) / 2;
            int drawY = 0;

            g.DrawImage(d->pImage, drawX, drawY, drawW, drawH);
        } else {
            SetTextColor(hdc, RGB(255, 80, 80));
            SetBkMode(hdc, TRANSPARENT);
            std::wstring msg2 = L"Cannot load image";
            if (d) msg2 += L"\n" + d->path;
            DrawText(hdc, msg2.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (d) { delete d->pImage; delete d; }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterImageCtrl(HINSTANCE hInst)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = ImageCtrlProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = IMAGE_CTRL_CLASS;
    RegisterClassEx(&wc);
}

// ================================================================
// PaintCtrl  画像編集 (ペイントツール)
// ================================================================
#define PAINT_CTRL_CLASS L"VizCmdPaintCtrl"

struct PaintCtrlData {
    HDC      hMemDC;
    HBITMAP  hBitmap;
    int      bmWidth, bmHeight;
    bool     painting;
    POINT    lastPt;
    COLORREF penColor;
    int      penSize;
};

static LRESULT CALLBACK PaintCtrlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* d = (PaintCtrlData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        auto* data = new PaintCtrlData{};
        data->painting  = false;
        data->penColor  = RGB(0, 0, 0);
        data->penSize   = 3;

        RECT rc;
        GetClientRect(hwnd, &rc);
        data->bmWidth  = std::max((int)rc.right,  1);
        data->bmHeight = std::max((int)rc.bottom, 1);

        HDC hdc = GetDC(hwnd);
        data->hMemDC  = CreateCompatibleDC(hdc);
        data->hBitmap = CreateCompatibleBitmap(hdc, data->bmWidth, data->bmHeight);
        ReleaseDC(hwnd, hdc);

        SelectObject(data->hMemDC, data->hBitmap);
        RECT fill = {0, 0, data->bmWidth, data->bmHeight};
        FillRect(data->hMemDC, &fill, (HBRUSH)GetStockObject(WHITE_BRUSH));

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (d) BitBlt(hdc, 0, 0, d->bmWidth, d->bmHeight, d->hMemDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
        if (d) {
            d->painting = true;
            d->lastPt   = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HPEN hPen = CreatePen(PS_SOLID, d->penSize, d->penColor);
            HPEN hOld = (HPEN)SelectObject(d->hMemDC, hPen);
            MoveToEx(d->hMemDC, d->lastPt.x, d->lastPt.y, nullptr);
            LineTo(d->hMemDC, d->lastPt.x, d->lastPt.y + 1);
            SelectObject(d->hMemDC, hOld);
            DeleteObject(hPen);
            InvalidateRect(hwnd, nullptr, FALSE);
            SetCapture(hwnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (d && d->painting) {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HPEN hPen = CreatePen(PS_SOLID, d->penSize, d->penColor);
            HPEN hOld = (HPEN)SelectObject(d->hMemDC, hPen);
            MoveToEx(d->hMemDC, d->lastPt.x, d->lastPt.y, nullptr);
            LineTo(d->hMemDC, pt.x, pt.y);
            SelectObject(d->hMemDC, hOld);
            DeleteObject(hPen);
            d->lastPt = pt;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (d) { d->painting = false; ReleaseCapture(); }
        return 0;

    case WM_DESTROY:
        if (d) {
            SelectObject(d->hMemDC, GetStockObject(BLACK_PEN)); // deselect bitmap
            DeleteDC(d->hMemDC);
            DeleteObject(d->hBitmap);
            delete d;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterPaintCtrl(HINSTANCE hInst)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = PaintCtrlProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = PAINT_CTRL_CLASS;
    RegisterClassEx(&wc);
}

// ================================================================
// ConsoleCtrl  テキスト入出力共用コンソール EDIT
// ================================================================
#define CONSOLE_CTRL_CLASS L"VizCmdConsoleCtrl"

struct ConsoleCtrlData {
    HWND  hEdit;
    HFONT hFont;
    int   inputStart;  // ユーザー入力開始位置 (この位置より前は保護)
    bool  finalized;   // true になると入力を受け付けない
};

// コマンド履歴 (全コンソール共有)
static std::vector<std::wstring> g_cmdHistory;
static int g_historyIdx = 0;

// \n → \r\n 正規化
static std::wstring NormalizeNewlines(const std::wstring& s)
{
    std::wstring r;
    r.reserve(s.size() * 2);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n' && (i == 0 || s[i-1] != L'\r'))
            r += L'\r';
        r += s[i];
    }
    return r;
}

// ConsoleCtrl の高さを計測して OutputPanel のアイテム高を更新しレイアウト
static void ResizeConsole(HWND hConsole);

// EDIT サブクラス
static LRESULT CALLBACK ConsoleEditSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR)
{
    HWND hConsole = GetParent(hwnd);
    auto* cd = (ConsoleCtrlData*)GetWindowLongPtr(hConsole, GWLP_USERDATA);

    switch (msg) {
    case WM_KEYDOWN: {
        if (!cd) break;
        DWORD selS = 0, selE = 0;
        SendMessage(hwnd, EM_GETSEL, (WPARAM)&selS, (LPARAM)&selE);

        switch (wParam) {
        case VK_RETURN: {
            if (cd->finalized) return 0;
            HWND hOutputPanel = GetParent(hConsole);

            int len = GetWindowTextLength(hwnd);
            std::wstring full(len + 1, L'\0');
            GetWindowText(hwnd, &full[0], len + 1);
            full.resize(len);

            // inputStart 以降がコマンド
            std::wstring cmd;
            if (cd->inputStart >= 0 && cd->inputStart <= len)
                cmd = full.substr(cd->inputStart);
            while (!cmd.empty() && (cmd.back() == L'\r' || cmd.back() == L'\n'))
                cmd.pop_back();

            if (!cmd.empty()) {
                if (g_cmdHistory.empty() || g_cmdHistory.back() != cmd)
                    g_cmdHistory.push_back(cmd);
                g_historyIdx = (int)g_cmdHistory.size();
            }

            ExecuteCommand(hOutputPanel, cmd);
            return 0;
        }
        case VK_UP:
            if (cd->finalized) break;
            if (!g_cmdHistory.empty() && g_historyIdx > 0) {
                --g_historyIdx;
                SendMessage(hwnd, EM_SETSEL, cd->inputStart, -1);
                SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)g_cmdHistory[g_historyIdx].c_str());
            }
            return 0;
        case VK_DOWN:
            if (cd->finalized) break;
            if (!g_cmdHistory.empty()) {
                ++g_historyIdx;
                if (g_historyIdx >= (int)g_cmdHistory.size()) {
                    g_historyIdx = (int)g_cmdHistory.size();
                    SendMessage(hwnd, EM_SETSEL, cd->inputStart, -1);
                    SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)L"");
                } else {
                    SendMessage(hwnd, EM_SETSEL, cd->inputStart, -1);
                    SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)g_cmdHistory[g_historyIdx].c_str());
                }
            }
            return 0;
        case VK_HOME:
            if (!cd->finalized) {
                SendMessage(hwnd, EM_SETSEL, cd->inputStart, cd->inputStart);
                return 0;
            }
            break;
        case VK_LEFT:
            if (!cd->finalized && (int)selS <= cd->inputStart && selS == selE)
                return 0;
            break;
        case VK_BACK:
            if (cd->finalized) return 0;
            if ((int)selS <= cd->inputStart && selS == selE) return 0;
            if ((int)selS < cd->inputStart) {
                SendMessage(hwnd, EM_SETSEL, cd->inputStart, selE);
                SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L"");
                return 0;
            }
            break;
        case VK_DELETE:
            if (cd->finalized) return 0;
            if ((int)selS < cd->inputStart && selS == selE) return 0;
            break;
        }
        break;
    }

    case WM_CHAR:
        // Enter の改行挿入を抑制
        if (wParam == L'\r' || wParam == L'\n') return 0;
        if (!cd || cd->finalized) return 0;
        // Backspace は WM_KEYDOWN で保護済み
        if (wParam == VK_BACK) {
            DWORD selS = 0, selE = 0;
            SendMessage(hwnd, EM_GETSEL, (WPARAM)&selS, (LPARAM)&selE);
            if ((int)selS <= cd->inputStart && selS == selE) return 0;
            break;
        }
        // 入力域より前にカーソルがある場合は末尾へ移動してから入力
        {
            DWORD selS = 0, selE = 0;
            SendMessage(hwnd, EM_GETSEL, (WPARAM)&selS, (LPARAM)&selE);
            if ((int)selS < cd->inputStart) {
                int len = GetWindowTextLength(hwnd);
                SendMessage(hwnd, EM_SETSEL, len, len);
            }
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ConsoleEditSubclassProc, uIdSubclass);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static HBRUSH g_hConBrush = nullptr;
static HBRUSH ConBrush()
{
    if (!g_hConBrush) g_hConBrush = CreateSolidBrush(CON_BG);
    return g_hConBrush;
}

static LRESULT CALLBACK ConsoleCtrlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* d = (ConsoleCtrlData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        auto* cs   = (CREATESTRUCT*)lParam;
        auto* data = new ConsoleCtrlData{};
        data->inputStart = 0;
        data->finalized  = false;
        data->hFont = CreateFont(
            16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        RECT rc = {};
        GetClientRect(hwnd, &rc);
        int editW = std::max((int)rc.right, 1);

        data->hEdit = CreateWindowEx(
            0, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
            0, 0, editW, CMDBAR_HEIGHT,
            hwnd, nullptr, cs->hInstance, nullptr);
        SendMessage(data->hEdit, WM_SETFONT, (WPARAM)data->hFont, FALSE);
        SendMessage(data->hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(2, 0));
        { RECT fmt = {2, 0, editW, CMDBAR_HEIGHT}; SendMessage(data->hEdit, EM_SETRECT, 0, (LPARAM)&fmt); }
        SetWindowSubclass(data->hEdit, ConsoleEditSubclassProc, 1, 0);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        break;
    }

    case WM_SIZE:
        if (d && d->hEdit) {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            int editH = std::max(h, CMDBAR_HEIGHT);
            MoveWindow(d->hEdit, 0, 0, w, editH, TRUE);
            RECT fmt = {2, 0, w, editH};
            SendMessage(d->hEdit, EM_SETRECT, 0, (LPARAM)&fmt);
        }
        break;

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, ConBrush());
        return 1;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, CON_BG);
        SetTextColor(hdc, CON_FG);
        return (LRESULT)ConBrush();
    }

    case WM_DESTROY:
        if (d) {
            if (d->hFont) DeleteObject(d->hFont);
            delete d;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterConsoleCtrl(HINSTANCE hInst)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = ConsoleCtrlProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = CONSOLE_CTRL_CLASS;
    RegisterClassEx(&wc);
}

// ================================================================
// OutputPanel  縦スクロール可能なアイテムリスト
// ================================================================
struct PanelData {
    std::vector<OutputItem> items;
    int         totalHeight  = 0;
    int         scrollOffset = 0;
    HINSTANCE   hInst        = nullptr;
    HFONT       hFont        = nullptr;
};

static PanelData* GetData(HWND hwnd)
{
    return (PanelData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static void LayoutPanel(HWND hPanel)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    RECT rc;
    GetClientRect(hPanel, &rc);
    int panelW = rc.right;
    int panelH = rc.bottom;

    int y = -d->scrollOffset;
    for (auto& item : d->items) {
        MoveWindow(item.hwnd, 0, y, panelW, item.height, TRUE);
        y += item.height;
    }

    d->totalHeight = 0;
    for (auto& item : d->items)
        d->totalHeight += item.height;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin   = 0;
    si.nMax   = std::max(0, d->totalHeight - 1);
    si.nPage  = (UINT)panelH;
    si.nPos   = d->scrollOffset;
    SetScrollInfo(hPanel, SB_VERT, &si, TRUE);
}

static void DoScroll(HWND hPanel, PanelData* d, int newOffset)
{
    RECT rc;
    GetClientRect(hPanel, &rc);
    int maxOff = std::max(0, d->totalHeight - (int)rc.bottom);
    int oldOffset = d->scrollOffset;
    d->scrollOffset = std::max(0, std::min(newOffset, maxOff));
    int dy = oldOffset - d->scrollOffset;
    if (dy == 0) return;

    // 子ウィンドウごとスクロールし、露出した帯域のみ無効化
    ScrollWindowEx(hPanel, 0, dy, nullptr, nullptr,
                   nullptr, nullptr,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_POS;
    si.nPos   = d->scrollOffset;
    SetScrollInfo(hPanel, SB_VERT, &si, TRUE);
}

static LRESULT CALLBACK OutputPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PanelData* d = GetData(hwnd);

    switch (msg) {
    case WM_CREATE: {
        auto* data    = new PanelData{};
        data->hInst   = ((CREATESTRUCT*)lParam)->hInstance;
        data->hFont   = CreateFont(
            16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        break;
    }

    case WM_SIZE:
        LayoutPanel(hwnd);
        break;

    case WM_VSCROLL: {
        if (!d) break;
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask  = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);

        int newOff = d->scrollOffset;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:        newOff -= 20;            break;
        case SB_LINEDOWN:      newOff += 20;            break;
        case SB_PAGEUP:        newOff -= (int)si.nPage; break;
        case SB_PAGEDOWN:      newOff += (int)si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: newOff  = si.nTrackPos;  break;
        case SB_TOP:           newOff  = 0;             break;
        case SB_BOTTOM:        newOff  = si.nMax;       break;
        }
        DoScroll(hwnd, d, newOff);
        break;
    }

    case WM_MOUSEWHEEL: {
        if (!d) break;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        DoScroll(hwnd, d, d->scrollOffset - delta / WHEEL_DELTA * 60);
        break;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, CON_BG);
        SetTextColor(hdc, CON_FG);
        return (LRESULT)ConBrush();
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, ConBrush());
        return 1;
    }

    case WM_DESTROY:
        if (d) { if (d->hFont) DeleteObject(d->hFont); delete d; }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ================================================================
// ConsoleCtrl ヘルパー (OutputPanel 内部)
// ================================================================

// ConsoleCtrl の内容高を計測して OutputPanel のアイテム高を更新
static void ResizeConsole(HWND hConsole)
{
    HWND hPanel = GetParent(hConsole);
    PanelData* pd = GetData(hPanel);
    if (!pd) return;

    auto* cd = (ConsoleCtrlData*)GetWindowLongPtr(hConsole, GWLP_USERDATA);
    if (!cd || !cd->hEdit) return;

    // 現在の EDIT 幅を取得
    RECT rc;
    GetClientRect(hConsole, &rc);
    int editW = std::max((int)rc.right, 200);

    // 計測のため大きな高さに一時設定
    MoveWindow(cd->hEdit, 0, 0, editW, 9999, FALSE);

    // ES_AUTOHSCROLL (折り返しなし) なので EM_GETLINECOUNT = 実際の行数
    int lineCount = (int)SendMessage(cd->hEdit, EM_GETLINECOUNT, 0, 0);
    lineCount = std::max(lineCount, 1);

    TEXTMETRIC tm;
    HDC hdc = GetDC(cd->hEdit);
    HFONT hOld = cd->hFont ? (HFONT)SelectObject(hdc, cd->hFont) : nullptr;
    GetTextMetrics(hdc, &tm);
    if (hOld) SelectObject(hdc, hOld);
    ReleaseDC(cd->hEdit, hdc);

    int lineH    = tm.tmHeight + tm.tmExternalLeading;
    int newHeight = std::max(lineCount * lineH + 2, CMDBAR_HEIGHT);

    for (auto& item : pd->items) {
        if (item.hwnd == hConsole) {
            item.height = newHeight;
            break;
        }
    }
    LayoutPanel(hPanel); // MoveWindow(hConsole, newHeight) → WM_SIZE → hEdit + EM_SETRECT
}

// アクティブな (最後の未完了) ConsoleCtrl を返す。なければ新規作成
static HWND GetOrCreateActiveConsole(HWND hPanel)
{
    PanelData* d = GetData(hPanel);
    if (!d) return nullptr;

    for (int i = (int)d->items.size() - 1; i >= 0; --i) {
        if (d->items[i].type == OutputType::Console) {
            auto* cd = (ConsoleCtrlData*)GetWindowLongPtr(d->items[i].hwnd, GWLP_USERDATA);
            if (cd && !cd->finalized)
                return d->items[i].hwnd;
        }
    }

    // 新規作成
    RECT rc;
    GetClientRect(hPanel, &rc);
    int panelW = std::max((int)rc.right, 200);

    HWND hConsole = CreateWindowEx(
        0, CONSOLE_CTRL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, panelW, CMDBAR_HEIGHT,
        hPanel, nullptr, d->hInst, nullptr);

    d->items.push_back({ OutputType::Console, hConsole, CMDBAR_HEIGHT });
    LayoutPanel(hPanel);
    return hConsole;
}

// アクティブな ConsoleCtrl をテキスト追記不可にする (GUI アイテム挿入前)
static void FinalizeActiveConsole(HWND hPanel)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;
    for (int i = (int)d->items.size() - 1; i >= 0; --i) {
        if (d->items[i].type == OutputType::Console) {
            auto* cd = (ConsoleCtrlData*)GetWindowLongPtr(d->items[i].hwnd, GWLP_USERDATA);
            if (cd && !cd->finalized) {
                cd->finalized = true;
                // ES_READONLY に変更 (選択・コピーは引き続き可能)
                LONG style = GetWindowLong(cd->hEdit, GWL_STYLE);
                SetWindowLong(cd->hEdit, GWL_STYLE, style | ES_READONLY);
                SetWindowPos(cd->hEdit, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
            return;
        }
    }
}

// ================================================================
// Public API
// ================================================================
void OutputPanel_Register(HINSTANCE hInst)
{
    RegisterImageCtrl(hInst);
    RegisterPaintCtrl(hInst);
    RegisterConsoleCtrl(hInst);

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = OutputPanelProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = OUTPUT_PANEL_CLASS;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wc);
}

HWND OutputPanel_Create(HWND hParent, HINSTANCE hInst, int x, int y, int w, int h)
{
    return CreateWindowEx(
        0,
        OUTPUT_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN,
        x, y, w, h,
        hParent, nullptr, hInst, nullptr);
}

// ---- テキストをアクティブな ConsoleCtrl に追記 -------------------
void OutputPanel_AddText(HWND hPanel, const std::wstring& text)
{
    HWND hConsole = GetOrCreateActiveConsole(hPanel);
    if (!hConsole) return;

    auto* cd = (ConsoleCtrlData*)GetWindowLongPtr(hConsole, GWLP_USERDATA);
    if (!cd || !cd->hEdit) return;

    int len = GetWindowTextLength(cd->hEdit);
    std::wstring current(len + 1, L'\0');
    GetWindowText(cd->hEdit, &current[0], len + 1);
    current.resize(len);

    // テキストを追記 (\n → \r\n 正規化)
    std::wstring appended = (len > 0) ? L"\r\n" : L"";
    appended += NormalizeNewlines(text);

    SetWindowText(cd->hEdit, (current + appended).c_str());
    cd->inputStart = GetWindowTextLength(cd->hEdit); // 入力域をここより後ろに設定 (AddPromptで正確に設定)

    ResizeConsole(hConsole);
}

// ---- プロンプト "> " をアクティブな ConsoleCtrl に追記 -----------
void OutputPanel_AddPrompt(HWND hPanel)
{
    HWND hConsole = GetOrCreateActiveConsole(hPanel);
    if (!hConsole) return;

    auto* cd = (ConsoleCtrlData*)GetWindowLongPtr(hConsole, GWLP_USERDATA);
    if (!cd || !cd->hEdit) return;

    int len = GetWindowTextLength(cd->hEdit);
    std::wstring current(len + 1, L'\0');
    GetWindowText(cd->hEdit, &current[0], len + 1);
    current.resize(len);

    // 先頭なら "> "、それ以外は "\r\n> "
    std::wstring prompt = (len > 0) ? L"\r\n> " : L"> ";
    std::wstring newText = current + prompt;
    SetWindowText(cd->hEdit, newText.c_str());

    cd->inputStart = GetWindowTextLength(cd->hEdit); // "> " の直後

    ResizeConsole(hConsole);

    // フォーカスをここへ
    SetFocus(cd->hEdit);
    SendMessage(cd->hEdit, EM_SETSEL, cd->inputStart, cd->inputStart);
}

// ---- 画像表示 (ConsoleCtrl を分割して挿入) -----------------------
void OutputPanel_AddImage(HWND hPanel, const std::wstring& path)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    RECT rcPanel;
    GetClientRect(hPanel, &rcPanel);
    int panelW = std::max((int)rcPanel.right, 100);

    // 画像サイズを先に取得して高さを確定
    Gdiplus::Image* pImg = Gdiplus::Image::FromFile(path.c_str());
    if (pImg && pImg->GetLastStatus() != Gdiplus::Ok) { delete pImg; pImg = nullptr; }

    int height = 300;
    if (pImg) {
        UINT iw = pImg->GetWidth();
        UINT ih = pImg->GetHeight();
        if (iw > 0 && ih > 0) {
            height = (int)((float)ih / iw * panelW);
            height = std::max(50, std::min(height, 500));
        }
    }

    FinalizeActiveConsole(hPanel);

    // 確定したサイズ・位置で直接生成
    int yPos = d->totalHeight - d->scrollOffset;
    HWND hImg = CreateWindowEx(
        0, IMAGE_CTRL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        0, yPos, panelW, height,
        hPanel, nullptr, d->hInst, nullptr);

    auto* imgData = (ImageCtrlData*)GetWindowLongPtr(hImg, GWLP_USERDATA);
    if (imgData) { imgData->path = path; imgData->pImage = pImg; pImg = nullptr; }
    delete pImg;

    d->items.push_back({ OutputType::Image, hImg, height });
    LayoutPanel(hPanel);
}

// ---- テキストエディタ --------------------------------------------
void OutputPanel_AddEdit(HWND hPanel, const std::wstring& filePath)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    std::wstring content;
    std::wifstream fs(filePath);
    if (fs) {
        fs.imbue(std::locale(""));
        std::wstringstream ss;
        ss << fs.rdbuf();
        content = ss.str();
    } else {
        OutputPanel_AddText(hPanel, L"Error: cannot open file: " + filePath);
        return;
    }

    FinalizeActiveConsole(hPanel);

    RECT rcPanel;
    GetClientRect(hPanel, &rcPanel);
    int panelW = std::max((int)rcPanel.right, 200);
    int editH  = 280;
    int yPos   = d->totalHeight - d->scrollOffset;

    HWND hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", content.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL
            | WS_VSCROLL | WS_HSCROLL,
        0, yPos, panelW, editH,
        hPanel, nullptr, d->hInst, nullptr);

    if (d->hFont) SendMessage(hEdit, WM_SETFONT, (WPARAM)d->hFont, FALSE);
    SetProp(hEdit, L"FilePath", (HANDLE)new std::wstring(filePath));

    d->items.push_back({ OutputType::Edit, hEdit, editH });
    LayoutPanel(hPanel);
}

// ---- テキストビューワ (読み取り専用) ----------------------------
void OutputPanel_AddTextView(HWND hPanel, const std::wstring& filePath)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    std::wstring content;
    std::wifstream fs(filePath);
    if (fs) {
        fs.imbue(std::locale(""));
        std::wstringstream ss;
        ss << fs.rdbuf();
        content = NormalizeNewlines(ss.str());
    } else {
        OutputPanel_AddText(hPanel, L"Error: cannot open file: " + filePath);
        return;
    }

    FinalizeActiveConsole(hPanel);

    RECT rcPanel;
    GetClientRect(hPanel, &rcPanel);
    int panelW = std::max((int)rcPanel.right, 200);
    int yPos   = d->totalHeight - d->scrollOffset;

    // スクロール不要な状態で作成して正確な高さを測定
    HWND hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", content.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        0, yPos, panelW, 9999,
        hPanel, nullptr, d->hInst, nullptr);

    if (d->hFont) SendMessage(hEdit, WM_SETFONT, (WPARAM)d->hFont, FALSE);

    // EM_POSFROMCHAR で最終文字のY座標を取得して正確な高さを算出
    TEXTMETRIC tm;
    HDC hdc = GetDC(hEdit);
    HFONT hOld = d->hFont ? (HFONT)SelectObject(hdc, d->hFont) : nullptr;
    GetTextMetrics(hdc, &tm);
    if (hOld) SelectObject(hdc, hOld);
    ReleaseDC(hEdit, hdc);

    int textLen = GetWindowTextLength(hEdit);
    int lastLineY = 0;
    if (textLen > 0) {
        LRESULT pos = SendMessage(hEdit, EM_POSFROMCHAR, (WPARAM)(textLen - 1), 0);
        lastLineY = (int)HIWORD((DWORD)pos);
    }
    int lineH    = tm.tmHeight + tm.tmExternalLeading;
    int edgePx   = GetSystemMetrics(SM_CYEDGE) * 2;
    int contentH = lastLineY + lineH + 4 + edgePx;  // +4 は EDIT 内部の上余白

    // パネル高さ - プロンプト1行分 を超えた場合のみ縦スクロールバーを付ける
    int panelH = std::max((int)rcPanel.bottom, CMDBAR_HEIGHT * 2);
    int maxH   = panelH - CMDBAR_HEIGHT;
    int viewH;
    if (contentH > maxH) {
        viewH = std::max(maxH, 24);
        SetWindowLong(hEdit, GWL_STYLE,
            GetWindowLong(hEdit, GWL_STYLE) | WS_VSCROLL | ES_AUTOVSCROLL);
        SetWindowPos(hEdit, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    } else {
        viewH = std::max(contentH, 24);
    }
    MoveWindow(hEdit, 0, yPos, panelW, viewH, TRUE);

    d->items.push_back({ OutputType::TextView, hEdit, viewH });
    LayoutPanel(hPanel);
}

// ---- ペイントツール ---------------------------------------------
void OutputPanel_AddPaint(HWND hPanel, const std::wstring& path)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    RECT rcPanel;
    GetClientRect(hPanel, &rcPanel);
    int panelW = std::max((int)rcPanel.right, 100);

    // 画像サイズを先に取得して高さを確定
    Gdiplus::Image* pImg = Gdiplus::Image::FromFile(path.c_str());
    if (pImg && pImg->GetLastStatus() != Gdiplus::Ok) { delete pImg; pImg = nullptr; }

    int height = 400;
    if (pImg) {
        UINT iw = pImg->GetWidth();
        UINT ih = pImg->GetHeight();
        if (iw > 0 && ih > 0) {
            height = (int)((float)ih / iw * panelW);
            height = std::max(50, std::min(height, 600));
        }
    }

    FinalizeActiveConsole(hPanel);

    int yPos = d->totalHeight - d->scrollOffset;
    HWND hPaint = CreateWindowEx(
        WS_EX_CLIENTEDGE, PAINT_CTRL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        0, yPos, panelW, height,
        hPanel, nullptr, d->hInst, nullptr);

    // 画像を PaintCtrl の memory DC に描画
    auto* pd = (PaintCtrlData*)GetWindowLongPtr(hPaint, GWLP_USERDATA);
    if (pd && pImg) {
        Gdiplus::Graphics g(pd->hMemDC);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(pImg, 0, 0, pd->bmWidth, pd->bmHeight);
        InvalidateRect(hPaint, nullptr, FALSE);
    }
    delete pImg;

    d->items.push_back({ OutputType::Paint, hPaint, height });
    LayoutPanel(hPanel);
}

// ---- ファイル一覧 -----------------------------------------------
void OutputPanel_AddFileList(HWND hPanel, const std::wstring& dirPath)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    wchar_t fullPath[MAX_PATH];
    GetFullPathName(dirPath.c_str(), MAX_PATH, fullPath, nullptr);

    DWORD attr = GetFileAttributes(fullPath);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        OutputPanel_AddText(hPanel, L"Error: directory not found: " + std::wstring(fullPath));
        return;
    }

    FinalizeActiveConsole(hPanel);

    RECT rcPanel;
    GetClientRect(hPanel, &rcPanel);
    int panelW = std::max((int)rcPanel.right, 200);
    int listH  = 260;
    int yPos   = d->totalHeight - d->scrollOffset;

    HWND hList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_ICON | LVS_AUTOARRANGE,
        0, yPos, panelW, listH,
        hPanel, nullptr, d->hInst, nullptr);

    ListView_SetBkColor(hList, CON_BG);
    ListView_SetTextBkColor(hList, CON_BG);
    ListView_SetTextColor(hList, CON_FG);

    SHFILEINFO sfi = {};
    HIMAGELIST hSysImg = (HIMAGELIST)SHGetFileInfo(
        L"C:\\", 0, &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX | SHGFI_LARGEICON);
    ListView_SetImageList(hList, hSysImg, LVSIL_NORMAL);

    std::wstring searchPath = std::wstring(fullPath) + L"\\*";
    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        DestroyWindow(hList);
        // FinalizeActiveConsole は既に呼んでいるが、AddText で新しいコンソールを使う
        OutputPanel_AddText(hPanel, L"Error: cannot read directory: " + std::wstring(fullPath));
        return;
    }

    int idx = 0;
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;

        std::wstring fp = std::wstring(fullPath) + L"\\" + ffd.cFileName;
        SHFILEINFO fi   = {};
        SHGetFileInfo(fp.c_str(), 0, &fi, sizeof(fi),
                      SHGFI_SYSICONINDEX | SHGFI_LARGEICON);

        LVITEM lvi   = {};
        lvi.mask     = LVIF_TEXT | LVIF_IMAGE;
        lvi.iItem    = idx++;
        lvi.pszText  = ffd.cFileName;
        lvi.iImage   = fi.iIcon;
        ListView_InsertItem(hList, &lvi);
    } while (FindNextFile(hFind, &ffd));
    FindClose(hFind);

    d->items.push_back({ OutputType::FileList, hList, listH });
    LayoutPanel(hPanel);
}

// ---- クリア -----------------------------------------------------
void OutputPanel_Clear(HWND hPanel)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    for (auto& item : d->items) {
        if (item.type == OutputType::Edit) {
            auto* fp = (std::wstring*)GetProp(item.hwnd, L"FilePath");
            if (fp) { delete fp; RemoveProp(item.hwnd, L"FilePath"); }
        }
        DestroyWindow(item.hwnd);
    }
    d->items.clear();
    d->totalHeight  = 0;
    d->scrollOffset = 0;

    LayoutPanel(hPanel);
    InvalidateRect(hPanel, nullptr, TRUE);

    OutputPanel_AddPrompt(hPanel);
    OutputPanel_ScrollToBottom(hPanel);
}

// ---- 最下部へスクロール -----------------------------------------
void OutputPanel_ScrollToBottom(HWND hPanel)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    RECT rc;
    GetClientRect(hPanel, &rc);
    int maxOff = std::max(0, d->totalHeight - (int)rc.bottom);
    d->scrollOffset = maxOff;
    LayoutPanel(hPanel);
}

// ---- アクティブなコンソールにフォーカス -------------------------
void OutputPanel_FocusPrompt(HWND hPanel)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    for (int i = (int)d->items.size() - 1; i >= 0; --i) {
        if (d->items[i].type == OutputType::Console) {
            auto* cd = (ConsoleCtrlData*)GetWindowLongPtr(d->items[i].hwnd, GWLP_USERDATA);
            if (cd && cd->hEdit && !cd->finalized) {
                SetFocus(cd->hEdit);
                SendMessage(cd->hEdit, EM_SETSEL, cd->inputStart, cd->inputStart);
                return;
            }
        }
    }
}
