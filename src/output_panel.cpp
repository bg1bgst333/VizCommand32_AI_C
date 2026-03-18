#include "output_panel.h"
#include <commctrl.h>
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
// ImageCtrl  GDI+ image display custom control
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
            int drawY = (ctrlH - drawH) / 2;

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
// OutputPanel  Vertically scrollable window list view
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

    int y = ITEM_MARGIN - d->scrollOffset;
    for (auto& item : d->items) {
        MoveWindow(item.hwnd, 0, y, panelW, item.height, TRUE);
        y += item.height + ITEM_MARGIN;
    }

    d->totalHeight = ITEM_MARGIN;
    for (auto& item : d->items)
        d->totalHeight += item.height + ITEM_MARGIN;

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
    d->scrollOffset = std::max(0, std::min(newOffset, maxOff));
    LayoutPanel(hPanel);
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

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        return 1;
    }

    case WM_DESTROY:
        if (d) { if (d->hFont) DeleteObject(d->hFont); delete d; }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ================================================================
// Public API
// ================================================================
void OutputPanel_Register(HINSTANCE hInst)
{
    RegisterImageCtrl(hInst);

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = OutputPanelProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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

// ---- Text output ------------------------------------------------
void OutputPanel_AddText(HWND hPanel, const std::wstring& text)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    RECT rcPanel;
    GetClientRect(hPanel, &rcPanel);
    int panelW = std::max((int)rcPanel.right, 200);

    HWND hEdit = CreateWindowEx(
        0, L"EDIT", text.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, panelW, 100,
        hPanel, nullptr, d->hInst, nullptr);

    if (d->hFont) SendMessage(hEdit, WM_SETFONT, (WPARAM)d->hFont, FALSE);

    int lineCount = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);
    lineCount = std::max(lineCount, 1);

    TEXTMETRIC tm;
    HDC hdc = GetDC(hEdit);
    HFONT hOld = d->hFont ? (HFONT)SelectObject(hdc, d->hFont) : nullptr;
    GetTextMetrics(hdc, &tm);
    if (hOld) SelectObject(hdc, hOld);
    ReleaseDC(hEdit, hdc);

    int lineH  = tm.tmHeight + tm.tmExternalLeading + 2;
    int height = lineCount * lineH + 10;
    height = std::max(height, 24);
    height = std::min(height, 400);

    d->items.push_back({ OutputType::Text, hEdit, height });
    LayoutPanel(hPanel);
}

// ---- Image display ----------------------------------------------
void OutputPanel_AddImage(HWND hPanel, const std::wstring& path)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    HWND hImg = CreateWindowEx(
        0, IMAGE_CTRL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        0, 0, 100, 100,
        hPanel, nullptr, d->hInst, nullptr);

    auto* imgData = (ImageCtrlData*)GetWindowLongPtr(hImg, GWLP_USERDATA);
    if (imgData) {
        imgData->path   = path;
        imgData->pImage = Gdiplus::Image::FromFile(path.c_str());
        if (imgData->pImage && imgData->pImage->GetLastStatus() != Gdiplus::Ok) {
            delete imgData->pImage;
            imgData->pImage = nullptr;
        }
    }

    int height = 300;
    if (imgData && imgData->pImage) {
        UINT iw = imgData->pImage->GetWidth();
        UINT ih = imgData->pImage->GetHeight();
        RECT rcPanel;
        GetClientRect(hPanel, &rcPanel);
        int pw = std::max((int)rcPanel.right, 100);
        if (iw > 0 && ih > 0) {
            height = (int)((float)ih / iw * pw);
            height = std::max(50, std::min(height, 500));
        }
    }

    d->items.push_back({ OutputType::Image, hImg, height });
    LayoutPanel(hPanel);
}

// ---- Text editor ------------------------------------------------
void OutputPanel_AddEdit(HWND hPanel, const std::wstring& filePath)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    // Read file content
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

    HWND hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", content.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL
            | WS_VSCROLL | WS_HSCROLL,
        0, 0, 100, 100,
        hPanel, nullptr, d->hInst, nullptr);

    if (d->hFont) SendMessage(hEdit, WM_SETFONT, (WPARAM)d->hFont, FALSE);
    SetProp(hEdit, L"FilePath", (HANDLE)new std::wstring(filePath));

    d->items.push_back({ OutputType::Edit, hEdit, 280 });
    LayoutPanel(hPanel);
}

// ---- File list --------------------------------------------------
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

    HWND hList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_ICON | LVS_AUTOARRANGE,
        0, 0, 100, 100,
        hPanel, nullptr, d->hInst, nullptr);

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
        OutputPanel_AddText(hPanel, L"Error: cannot read directory: " + std::wstring(fullPath));
        return;
    }

    int idx = 0;
    do {
        if (wcscmp(ffd.cFileName, L".") == 0) continue;

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

    d->items.push_back({ OutputType::FileList, hList, 260 });
    LayoutPanel(hPanel);
}

// ---- Clear ------------------------------------------------------
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
}

// ---- Scroll to bottom -------------------------------------------
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
