#include "output_panel.h"
#include "command.h"
#include <commctrl.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <commdlg.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// EditPanel / ImageEditPanel → OutputPanel 編集終了通知
#define WM_APP_EDIT_DONE        (WM_APP + 1)
#define WM_APP_IMAGE_EDIT_DONE  (WM_APP + 2)

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
    bool     modified;
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
        data->modified  = false;
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

    case WM_SIZE: {
        if (!d) break;
        int newW = std::max((int)LOWORD(lParam), 1);
        int newH = std::max((int)HIWORD(lParam), 1);
        if (newW == d->bmWidth && newH == d->bmHeight) break;

        HDC hdc = GetDC(hwnd);
        HBITMAP hNewBmp = CreateCompatibleBitmap(hdc, newW, newH);
        ReleaseDC(hwnd, hdc);

        HDC hTmpDC = CreateCompatibleDC(nullptr);
        HGDIOBJ hOldTmp = SelectObject(hTmpDC, hNewBmp);
        RECT fill = { 0, 0, newW, newH };
        FillRect(hTmpDC, &fill, (HBRUSH)GetStockObject(WHITE_BRUSH));
        BitBlt(hTmpDC, 0, 0, d->bmWidth, d->bmHeight, d->hMemDC, 0, 0, SRCCOPY);
        SelectObject(hTmpDC, hOldTmp);
        DeleteDC(hTmpDC);

        SelectObject(d->hMemDC, hNewBmp);
        DeleteObject(d->hBitmap);
        d->hBitmap  = hNewBmp;
        d->bmWidth  = newW;
        d->bmHeight = newH;
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
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
            d->modified = true;
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
// ImageEditPanel  画像編集 (ツールバー + PaintCtrl)
// ================================================================
#define IMAGE_PANEL_CLASS     L"VizCmdImageEditPanel"
#define IMAGE_PANEL_TOOLBAR_H 34

static const COLORREF kPaletteColors[] = {
    RGB(0,   0,   0  ),  // 黒
    RGB(255, 255, 255),  // 白
    RGB(128, 128, 128),  // グレー
    RGB(255, 0,   0  ),  // 赤
    RGB(0,   128, 0  ),  // 緑
    RGB(0,   0,   255),  // 青
    RGB(255, 255, 0  ),  // 黄
    RGB(255, 128, 0  ),  // オレンジ
    RGB(128, 0,   0  ),  // 茶
    RGB(128, 0,   128),  // 紫
};
static const int kPaletteCount = 10;

#define IDC_IEP_SAVE       2001
#define IDC_IEP_SAVEAS     2002
#define IDC_IEP_DISCARD    2003
#define IDC_IEP_SIZE_DOWN  2004
#define IDC_IEP_SIZE_UP    2005

#define IEP_SWATCH_X    308
#define IEP_SWATCH_SIZE  20
#define IEP_SWATCH_GAP    2

struct ImageEditPanelData {
    HWND         hPaint;
    HWND         hBtnSave;
    HWND         hBtnSaveAs;
    HWND         hBtnDiscard;
    HWND         hBtnSizeDown;
    HWND         hBtnSizeUp;
    HWND         hLabelSize;
    HFONT        hUiFont;
    std::wstring filePath;
    int          penSize;
    int          selectedColorIdx;
};

static bool GetEncoderClsid(const wchar_t* mimeType, CLSID* pClsid)
{
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    auto* pInfo = (Gdiplus::ImageCodecInfo*)malloc(size);
    if (!pInfo) return false;
    Gdiplus::GetImageEncoders(num, size, pInfo);
    bool found = false;
    for (UINT i = 0; i < num; i++) {
        if (wcscmp(pInfo[i].MimeType, mimeType) == 0) {
            *pClsid = pInfo[i].Clsid;
            found = true;
            break;
        }
    }
    free(pInfo);
    return found;
}

static bool SaveImageFile(const std::wstring& path, HDC hMemDC, int w, int h)
{
    std::wstring ext;
    size_t pos = path.rfind(L'.');
    if (pos != std::wstring::npos) {
        ext = path.substr(pos + 1);
        for (auto& c : ext) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    }
    const wchar_t* mimeType = L"image/png";
    if (ext == L"jpg" || ext == L"jpeg") mimeType = L"image/jpeg";
    else if (ext == L"bmp")              mimeType = L"image/bmp";

    CLSID clsid;
    if (!GetEncoderClsid(mimeType, &clsid)) return false;

    // hMemDC のビットマップを別ビットマップにコピー (FromHBITMAP は非選択状態が必要)
    HDC hTmpDC = CreateCompatibleDC(hMemDC);
    HBITMAP hTmpBmp = CreateCompatibleBitmap(hMemDC, w, h);
    HGDIOBJ hOld = SelectObject(hTmpDC, hTmpBmp);
    BitBlt(hTmpDC, 0, 0, w, h, hMemDC, 0, 0, SRCCOPY);
    SelectObject(hTmpDC, hOld);
    DeleteDC(hTmpDC);

    Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromHBITMAP(hTmpBmp, nullptr);
    bool ok = false;
    if (bmp) {
        ok = (bmp->Save(path.c_str(), &clsid, nullptr) == Gdiplus::Ok);
        delete bmp;
    }
    DeleteObject(hTmpBmp);
    return ok;
}

static void IEP_UpdatePenState(HWND hwnd)
{
    auto* d = (ImageEditPanelData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!d) return;

    wchar_t buf[8];
    swprintf_s(buf, L"%d", d->penSize);
    SetWindowTextW(d->hLabelSize, buf);

    auto* pd = (PaintCtrlData*)GetWindowLongPtr(d->hPaint, GWLP_USERDATA);
    if (pd) {
        pd->penColor = kPaletteColors[d->selectedColorIdx];
        pd->penSize  = d->penSize;
    }

    int swatchEnd = IEP_SWATCH_X + kPaletteCount * (IEP_SWATCH_SIZE + IEP_SWATCH_GAP);
    RECT rc = { IEP_SWATCH_X - 4, 0, swatchEnd + 4, IMAGE_PANEL_TOOLBAR_H };
    InvalidateRect(hwnd, &rc, TRUE);
}

static void IEP_Layout(HWND hwnd)
{
    auto* d = (ImageEditPanelData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!d) return;
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    int y = (IMAGE_PANEL_TOOLBAR_H - 24) / 2, btnH = 24;
    int x = 4;
    MoveWindow(d->hBtnSave,    x, y,  90, btnH, TRUE); x += 94;
    MoveWindow(d->hBtnSaveAs,  x, y, 130, btnH, TRUE); x += 134;
    MoveWindow(d->hBtnDiscard, x, y,  60, btnH, TRUE);

    // ペンサイズコントロール (スウォッチの右)
    x = IEP_SWATCH_X + kPaletteCount * (IEP_SWATCH_SIZE + IEP_SWATCH_GAP) + 8;
    MoveWindow(d->hBtnSizeDown, x, y, 24, btnH, TRUE); x += 28;
    MoveWindow(d->hLabelSize,   x, y, 30, btnH, TRUE); x += 34;
    MoveWindow(d->hBtnSizeUp,   x, y, 24, btnH, TRUE);

    MoveWindow(d->hPaint, 0, IMAGE_PANEL_TOOLBAR_H,
               W, std::max(H - IMAGE_PANEL_TOOLBAR_H, 50), TRUE);
}

static LRESULT CALLBACK ImageEditPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* d = (ImageEditPanelData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        auto* cs   = (CREATESTRUCT*)lParam;
        auto* data = new ImageEditPanelData{};
        data->hUiFont          = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        data->penSize          = 3;
        data->selectedColorIdx = 0;

        auto mkBtn = [&](LPCWSTR text, int id) -> HWND {
            return CreateWindowEx(0, L"BUTTON", text,
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)id, cs->hInstance, nullptr);
        };
        data->hBtnSave     = mkBtn(L"上書き保存",       IDC_IEP_SAVE);
        data->hBtnSaveAs   = mkBtn(L"名前を付けて保存",  IDC_IEP_SAVEAS);
        data->hBtnDiscard  = mkBtn(L"破棄",             IDC_IEP_DISCARD);
        data->hBtnSizeDown = mkBtn(L"－",               IDC_IEP_SIZE_DOWN);
        data->hBtnSizeUp   = mkBtn(L"＋",               IDC_IEP_SIZE_UP);
        data->hLabelSize   = CreateWindowEx(0, L"STATIC", L"3",
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
            0, 0, 10, 10, hwnd, nullptr, cs->hInstance, nullptr);

        HWND buttons[] = { data->hBtnSave, data->hBtnSaveAs, data->hBtnDiscard,
                           data->hBtnSizeDown, data->hBtnSizeUp, data->hLabelSize };
        for (HWND h : buttons)
            SendMessage(h, WM_SETFONT, (WPARAM)data->hUiFont, FALSE);

        data->hPaint = CreateWindowEx(
            WS_EX_CLIENTEDGE, PAINT_CTRL_CLASS, nullptr,
            WS_CHILD | WS_VISIBLE,
            0, IMAGE_PANEL_TOOLBAR_H, 10, 10,
            hwnd, nullptr, cs->hInstance, nullptr);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        break;
    }

    case WM_SIZE:
        if (d) IEP_Layout(hwnd);
        break;

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT tbrc; GetClientRect(hwnd, &tbrc);
        tbrc.bottom = IMAGE_PANEL_TOOLBAR_H;
        FillRect(hdc, &tbrc, (HBRUSH)(COLOR_BTNFACE + 1));
        RECT line = { 0, IMAGE_PANEL_TOOLBAR_H - 1, tbrc.right, IMAGE_PANEL_TOOLBAR_H };
        FillRect(hdc, &line, (HBRUSH)GetStockObject(GRAY_BRUSH));
        return 1;
    }

    case WM_PAINT: {
        if (!d) break;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        int y0 = (IMAGE_PANEL_TOOLBAR_H - IEP_SWATCH_SIZE) / 2;
        for (int i = 0; i < kPaletteCount; i++) {
            int x0 = IEP_SWATCH_X + i * (IEP_SWATCH_SIZE + IEP_SWATCH_GAP);
            RECT sr = { x0, y0, x0 + IEP_SWATCH_SIZE, y0 + IEP_SWATCH_SIZE };
            HBRUSH hBr = CreateSolidBrush(kPaletteColors[i]);
            FillRect(hdc, &sr, hBr);
            DeleteObject(hBr);
            // 選択枠
            if (i == d->selectedColorIdx) {
                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
                HPEN hOld = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, x0 - 2, y0 - 2,
                          x0 + IEP_SWATCH_SIZE + 2, y0 + IEP_SWATCH_SIZE + 2);
                SelectObject(hdc, hOld);
                SelectObject(hdc, hOldBr);
                DeleteObject(hPen);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (!d) break;
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        int y0 = (IMAGE_PANEL_TOOLBAR_H - IEP_SWATCH_SIZE) / 2;
        if (my >= y0 && my < y0 + IEP_SWATCH_SIZE) {
            for (int i = 0; i < kPaletteCount; i++) {
                int x0 = IEP_SWATCH_X + i * (IEP_SWATCH_SIZE + IEP_SWATCH_GAP);
                if (mx >= x0 && mx < x0 + IEP_SWATCH_SIZE) {
                    d->selectedColorIdx = i;
                    IEP_UpdatePenState(hwnd);
                    break;
                }
            }
        }
        break;
    }

    case WM_COMMAND: {
        if (!d) break;
        int id = LOWORD(wParam);

        if (id == IDC_IEP_SIZE_DOWN) {
            if (d->penSize > 1) { d->penSize--; IEP_UpdatePenState(hwnd); }
        }
        else if (id == IDC_IEP_SIZE_UP) {
            if (d->penSize < 20) { d->penSize++; IEP_UpdatePenState(hwnd); }
        }
        else if (id == IDC_IEP_SAVE || id == IDC_IEP_SAVEAS) {
            std::wstring savePath = d->filePath;
            if (id == IDC_IEP_SAVEAS || savePath.empty()) {
                wchar_t szFile[MAX_PATH] = {};
                if (!d->filePath.empty())
                    wcsncpy_s(szFile, d->filePath.c_str(), MAX_PATH - 1);
                // 拡張子からデフォルトフィルタを決定 (1=PNG, 2=JPEG, 3=BMP, 4=全て)
                DWORD filterIdx = 1;
                {
                    std::wstring ext;
                    size_t dp = d->filePath.rfind(L'.');
                    if (dp != std::wstring::npos) {
                        ext = d->filePath.substr(dp + 1);
                        for (auto& c : ext) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
                    }
                    if (ext == L"jpg" || ext == L"jpeg") filterIdx = 2;
                    else if (ext == L"bmp")              filterIdx = 3;
                    else if (!ext.empty() && ext != L"png") filterIdx = 4;
                }
                OPENFILENAMEW ofn = {};
                ofn.lStructSize  = sizeof(ofn);
                ofn.hwndOwner    = hwnd;
                ofn.lpstrFile    = szFile;
                ofn.nMaxFile     = MAX_PATH;
                ofn.lpstrFilter  = L"PNG画像\0*.png\0JPEG画像\0*.jpg;*.jpeg\0BMP画像\0*.bmp\0すべてのファイル\0*.*\0";
                ofn.nFilterIndex = filterIdx;
                ofn.Flags        = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
                if (!GetSaveFileNameW(&ofn)) break;
                savePath = szFile;
            }
            auto* pd = (PaintCtrlData*)GetWindowLongPtr(d->hPaint, GWLP_USERDATA);
            if (!pd || !SaveImageFile(savePath, pd->hMemDC, pd->bmWidth, pd->bmHeight)) {
                MessageBoxW(hwnd, (L"保存に失敗しました:\n" + savePath).c_str(),
                    L"エラー", MB_ICONERROR | MB_OK);
                break;
            }
            d->filePath = savePath;
            if (pd) pd->modified = false;
            PostMessage(GetParent(hwnd), WM_APP_IMAGE_EDIT_DONE, 0, (LPARAM)hwnd);
        }
        else if (id == IDC_IEP_DISCARD) {
            auto* pd = (PaintCtrlData*)GetWindowLongPtr(d->hPaint, GWLP_USERDATA);
            if (pd && pd->modified) {
                int ret = MessageBoxW(hwnd,
                    L"変更を破棄しますか？\n保存されていない変更は失われます。",
                    L"確認", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
                if (ret != IDYES) break;
            }
            PostMessage(GetParent(hwnd), WM_APP_IMAGE_EDIT_DONE, 0, (LPARAM)hwnd);
        }
        break;
    }

    case WM_DESTROY:
        if (d) { delete d; SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterImageEditPanel(HINSTANCE hInst)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = ImageEditPanelProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = IMAGE_PANEL_CLASS;
    RegisterClassEx(&wc);
}

// 前方宣言
static bool SaveTextFile(const std::wstring& path, HWND hEdit, TextEncoding enc, LineEnding le);

// ================================================================
// EditPanel  テキストファイル編集 (ツールバー付きモーダルエディタ)
// ================================================================
#define EDIT_PANEL_CLASS  L"VizCmdEditPanel"
#define EDIT_TOOLBAR_H    32

#define IDC_EP_SAVE    1001
#define IDC_EP_SAVEAS  1002
#define IDC_EP_DISCARD 1003
#define IDC_EP_ENC     1004
#define IDC_EP_LINEEND 1005

struct EditPanelData {
    HWND         hEdit;
    HWND         hBtnSave;
    HWND         hBtnSaveAs;
    HWND         hBtnDiscard;
    HWND         hCmbEnc;
    HWND         hCmbEnd;
    HFONT        hEditFont;
    HFONT        hUiFont;
    std::wstring filePath;
};

static const wchar_t* kEncNames[] = {
    L"UTF-8", L"UTF-8 BOM", L"UTF-16 LE", L"UTF-16 BE",
    L"Shift-JIS", L"EUC-JP", L"JIS (ISO-2022-JP)"
};
static const TextEncoding kEncodings[] = {
    TextEncoding::UTF8, TextEncoding::UTF8_BOM,
    TextEncoding::UTF16LE, TextEncoding::UTF16BE,
    TextEncoding::ShiftJIS, TextEncoding::EUCJP, TextEncoding::JIS
};
static const int kEncCount = 7;

static const wchar_t* kLineEndNames[] = { L"CRLF", L"LF", L"CR" };
static const LineEnding kLineEndings[] = { LineEnding::CRLF, LineEnding::LF, LineEnding::CR };
static const int kLineEndCount = 3;

static int EncodingToIndex(TextEncoding enc) {
    for (int i = 0; i < kEncCount; i++) if (kEncodings[i] == enc) return i;
    return 0;
}

static void EP_LayoutControls(HWND hwnd)
{
    auto* d = (EditPanelData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!d) return;

    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    // ツールバー: 左側にボタン、右側にコンボ
    int y = 4, btnH = 24;
    int x = 4;
    MoveWindow(d->hBtnSave,    x, y,  90, btnH, TRUE); x += 94;
    MoveWindow(d->hBtnSaveAs,  x, y, 130, btnH, TRUE); x += 134;
    MoveWindow(d->hBtnDiscard, x, y,  60, btnH, TRUE);

    // コンボボックスは右寄せ
    int rx = W - 4;
    int cmbEndW = 70; rx -= cmbEndW;
    MoveWindow(d->hCmbEnd, rx, y, cmbEndW, 200, TRUE); rx -= 6;
    int cmbEncW = 130; rx -= cmbEncW;
    MoveWindow(d->hCmbEnc, rx, y, cmbEncW, 200, TRUE);

    // EDIT 本体
    MoveWindow(d->hEdit, 0, EDIT_TOOLBAR_H, W, std::max(H - EDIT_TOOLBAR_H, 50), TRUE);
}

static LRESULT CALLBACK EditPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* d = (EditPanelData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        auto* cs   = (CREATESTRUCT*)lParam;
        auto* data = new EditPanelData{};

        data->hEditFont = CreateFont(
            16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        data->hUiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        auto mkBtn = [&](LPCWSTR text, int id) -> HWND {
            return CreateWindowEx(0, L"BUTTON", text,
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)id, cs->hInstance, nullptr);
        };
        data->hBtnSave    = mkBtn(L"上書き保存",       IDC_EP_SAVE);
        data->hBtnSaveAs  = mkBtn(L"名前を付けて保存",  IDC_EP_SAVEAS);
        data->hBtnDiscard = mkBtn(L"破棄",             IDC_EP_DISCARD);
        SendMessage(data->hBtnSave,    WM_SETFONT, (WPARAM)data->hUiFont, FALSE);
        SendMessage(data->hBtnSaveAs,  WM_SETFONT, (WPARAM)data->hUiFont, FALSE);
        SendMessage(data->hBtnDiscard, WM_SETFONT, (WPARAM)data->hUiFont, FALSE);

        data->hCmbEnc = CreateWindowEx(0, L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 10, 200, hwnd, (HMENU)(INT_PTR)IDC_EP_ENC, cs->hInstance, nullptr);
        SendMessage(data->hCmbEnc, WM_SETFONT, (WPARAM)data->hUiFont, FALSE);
        for (int i = 0; i < kEncCount; i++)
            SendMessage(data->hCmbEnc, CB_ADDSTRING, 0, (LPARAM)kEncNames[i]);

        data->hCmbEnd = CreateWindowEx(0, L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 10, 200, hwnd, (HMENU)(INT_PTR)IDC_EP_LINEEND, cs->hInstance, nullptr);
        SendMessage(data->hCmbEnd, WM_SETFONT, (WPARAM)data->hUiFont, FALSE);
        for (int i = 0; i < kLineEndCount; i++)
            SendMessage(data->hCmbEnd, CB_ADDSTRING, 0, (LPARAM)kLineEndNames[i]);

        data->hEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
            0, EDIT_TOOLBAR_H, 10, 10,
            hwnd, nullptr, cs->hInstance, nullptr);
        SendMessage(data->hEdit, WM_SETFONT, (WPARAM)data->hEditFont, FALSE);
        SendMessage(data->hEdit, EM_SETLIMITTEXT, 10 * 1024 * 1024, 0);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        break;
    }

    case WM_SIZE:
        EP_LayoutControls(hwnd);
        break;

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        rc.bottom = EDIT_TOOLBAR_H;
        FillRect(hdc, &rc, (HBRUSH)(COLOR_BTNFACE + 1));
        // セパレータライン
        RECT line = { 0, EDIT_TOOLBAR_H - 1, rc.right, EDIT_TOOLBAR_H };
        FillRect(hdc, &line, (HBRUSH)GetStockObject(GRAY_BRUSH));
        return 1;
    }

    case WM_COMMAND: {
        if (!d) break;
        int id = LOWORD(wParam);

        if (id == IDC_EP_SAVE || id == IDC_EP_SAVEAS) {
            int encIdx = (int)SendMessage(d->hCmbEnc, CB_GETCURSEL, 0, 0);
            int leIdx  = (int)SendMessage(d->hCmbEnd, CB_GETCURSEL, 0, 0);
            TextEncoding enc = (encIdx >= 0 && encIdx < kEncCount)    ? kEncodings[encIdx]   : TextEncoding::UTF8;
            LineEnding   le  = (leIdx  >= 0 && leIdx  < kLineEndCount) ? kLineEndings[leIdx]  : LineEnding::CRLF;

            std::wstring savePath = d->filePath;

            if (id == IDC_EP_SAVEAS) {
                wchar_t szFile[MAX_PATH] = {};
                if (!d->filePath.empty())
                    wcsncpy_s(szFile, d->filePath.c_str(), MAX_PATH - 1);
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner   = hwnd;
                ofn.lpstrFile   = szFile;
                ofn.nMaxFile    = MAX_PATH;
                ofn.lpstrFilter = L"テキストファイル\0*.txt;*.log;*.csv\0すべてのファイル\0*.*\0";
                ofn.Flags       = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
                if (!GetSaveFileNameW(&ofn)) break; // キャンセル
                savePath = szFile;
            }

            if (!SaveTextFile(savePath, d->hEdit, enc, le)) {
                MessageBoxW(hwnd,
                    (L"保存に失敗しました:\n" + savePath).c_str(),
                    L"エラー", MB_ICONERROR | MB_OK);
                break;
            }
            d->filePath = savePath;
            SendMessage(d->hEdit, EM_SETMODIFY, FALSE, 0);
            PostMessage(GetParent(hwnd), WM_APP_EDIT_DONE, 0, (LPARAM)hwnd);
        }
        else if (id == IDC_EP_DISCARD) {
            if (SendMessage(d->hEdit, EM_GETMODIFY, 0, 0)) {
                int ret = MessageBoxW(hwnd,
                    L"変更を破棄しますか？\n保存されていない変更は失われます。",
                    L"確認", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
                if (ret != IDYES) break;
            }
            PostMessage(GetParent(hwnd), WM_APP_EDIT_DONE, 0, (LPARAM)hwnd);
        }
        break;
    }

    case WM_DESTROY:
        if (d) {
            if (d->hEditFont) DeleteObject(d->hEditFont);
            // hUiFont は GetStockObject なので DeleteObject 不要
            delete d;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterEditPanel(HINSTANCE hInst)
{
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = EditPanelProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = EDIT_PANEL_CLASS;
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
    // Tab補完
    std::vector<std::wstring> tabCandidates;
    int          tabIdx;     // 現在のサイクル位置
    std::wstring tabBase;    // 補完開始時の元の入力
    bool         tabActive;  // 補完サイクル中
};

// コマンド履歴 (全コンソール共有)
static std::vector<std::wstring> g_cmdHistory;
static int g_historyIdx = 0;

// ================================================================
// テキストファイル読み込み / 保存 (文字コード自動判別)
// ================================================================

// UTF-8 らしさスコア
static int ScoreUTF8(const std::vector<uint8_t>& d)
{
    int score = 0;
    for (size_t i = 0; i < d.size(); ) {
        uint8_t b = d[i];
        if (b < 0x80) { i++; continue; }
        int extra = 0;
        if      ((b & 0xE0) == 0xC0) extra = 1;
        else if ((b & 0xF0) == 0xE0) extra = 2;
        else if ((b & 0xF8) == 0xF0) extra = 3;
        else { score -= 2; i++; continue; }
        bool ok = true;
        for (int j = 1; j <= extra && i + j < d.size(); j++)
            if ((d[i + j] & 0xC0) != 0x80) { ok = false; break; }
        if (ok) { score += extra * 2; i += extra + 1; }
        else    { score -= 2; i++; }
    }
    return score;
}

// EUC-JP らしさスコア
static int ScoreEUCJP(const std::vector<uint8_t>& d)
{
    int score = 0;
    for (size_t i = 0; i < d.size(); ) {
        uint8_t b = d[i];
        if (b < 0x80) { i++; continue; }
        if ((b >= 0xA1 && b <= 0xFE) && i + 1 < d.size()) {
            uint8_t b2 = d[i + 1];
            if (b2 >= 0xA1 && b2 <= 0xFE) { score += 2; i += 2; continue; }
        }
        if (b == 0x8E && i + 1 < d.size()) {
            uint8_t b2 = d[i + 1];
            if (b2 >= 0xA1 && b2 <= 0xDF) { score += 2; i += 2; continue; }
        }
        score -= 2; i++;
    }
    return score;
}

// Shift-JIS らしさスコア
static int ScoreSJIS(const std::vector<uint8_t>& d)
{
    int score = 0;
    for (size_t i = 0; i < d.size(); ) {
        uint8_t b = d[i];
        if (b < 0x80) { i++; continue; }
        if (((b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC)) && i + 1 < d.size()) {
            uint8_t b2 = d[i + 1];
            if ((b2 >= 0x40 && b2 <= 0x7E) || (b2 >= 0x80 && b2 <= 0xFC)) {
                score += 2; i += 2; continue;
            }
        }
        if (b >= 0xA1 && b <= 0xDF) { score += 1; i++; continue; } // 半角カナ
        score -= 2; i++;
    }
    return score;
}

// 改行コード検出 (wstring から)
static LineEnding DetectLineEndingWStr(const std::wstring& ws)
{
    bool hasCRLF = false, hasCR = false, hasLF = false;
    for (size_t i = 0; i < ws.size(); i++) {
        if (ws[i] == L'\r') {
            if (i + 1 < ws.size() && ws[i+1] == L'\n') { hasCRLF = true; i++; }
            else hasCR = true;
        } else if (ws[i] == L'\n') {
            hasLF = true;
        }
    }
    if (hasCRLF) return LineEnding::CRLF;
    if (hasLF)   return LineEnding::LF;
    if (hasCR)   return LineEnding::CR;
    return LineEnding::CRLF;
}

struct TextFileInfo {
    std::wstring content;
    TextEncoding encoding   = TextEncoding::UTF8;
    LineEnding   lineEnding = LineEnding::CRLF;
};

// ファイルを読み込み (BOM/ヒューリスティックで文字コード判別、改行コードも検出)
static TextFileInfo ReadTextFile(const std::wstring& path)
{
    TextFileInfo result;

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return result;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    std::vector<uint8_t> data(fileSize);
    DWORD readBytes = 0;
    ReadFile(hFile, data.data(), fileSize, &readBytes, nullptr);
    CloseHandle(hFile);
    data.resize(readBytes);
    if (data.empty()) return result;

    size_t offset  = 0;
    UINT   cp      = 0;
    bool   utf16le = false, utf16be = false;

    // BOM 判定
    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE)
        { utf16le = true; offset = 2; result.encoding = TextEncoding::UTF16LE; }
    else if (data.size() >= 2 && data[0] == 0xFE && data[1] == 0xFF)
        { utf16be = true; offset = 2; result.encoding = TextEncoding::UTF16BE; }
    else if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        { cp = CP_UTF8; offset = 3; result.encoding = TextEncoding::UTF8_BOM; }

    // UTF-16 LE
    if (utf16le) {
        const wchar_t* p = reinterpret_cast<const wchar_t*>(data.data() + offset);
        result.content = std::wstring(p, (data.size() - offset) / 2);
    }
    // UTF-16 BE (バイトスワップ)
    else if (utf16be) {
        size_t len = (data.size() - offset) / 2;
        result.content.resize(len);
        for (size_t i = 0; i < len; i++)
            result.content[i] = (wchar_t)((data[offset + i*2] << 8) | data[offset + i*2 + 1]);
    }
    else {
        // JIS 判定 (ESC シーケンス)
        if (cp == 0) {
            for (size_t i = 0; i + 2 < data.size(); i++) {
                if (data[i] == 0x1B) {
                    if ((data[i+1] == '$' && (data[i+2] == 'B' || data[i+2] == '@')) ||
                        (data[i+1] == '(' && (data[i+2] == 'J' || data[i+2] == 'B')))
                    { cp = 50220; result.encoding = TextEncoding::JIS; break; }
                }
            }
        }

        // ヒューリスティック
        if (cp == 0) {
            bool allAscii = true;
            for (auto b : data) if (b >= 0x80) { allAscii = false; break; }
            if (allAscii) {
                cp = CP_UTF8; result.encoding = TextEncoding::UTF8;
            } else {
                int su = ScoreUTF8(data);
                int se = ScoreEUCJP(data);
                int ss = ScoreSJIS(data);
                if (su >= se && su >= ss)  { cp = CP_UTF8; result.encoding = TextEncoding::UTF8;     }
                else if (se >= ss)         { cp = 20932;   result.encoding = TextEncoding::EUCJP;    }
                else                       { cp = 932;     result.encoding = TextEncoding::ShiftJIS; }
            }
        }

        // MultiByteToWideChar で変換
        const char* src    = reinterpret_cast<const char*>(data.data() + offset);
        int         srcLen = (int)(data.size() - offset);
        int wlen = MultiByteToWideChar(cp, 0, src, srcLen, nullptr, 0);
        if (wlen > 0) {
            result.content.resize(wlen);
            MultiByteToWideChar(cp, 0, src, srcLen, &result.content[0], wlen);
        }
    }

    result.lineEnding = DetectLineEndingWStr(result.content);
    return result;
}

static std::wstring ReadTextFileAsWString(const std::wstring& path)
{
    return ReadTextFile(path).content;
}

// ファイルへの保存 (文字コード・改行コード指定)
static bool SaveTextFile(const std::wstring& path, HWND hEdit,
                         TextEncoding enc, LineEnding le)
{
    int len = GetWindowTextLengthW(hEdit);
    std::wstring ws(len + 1, L'\0');
    GetWindowTextW(hEdit, &ws[0], len + 1);
    ws.resize(len);

    // 改行コード変換 (EDIT 内は常に \r\n)
    std::wstring converted;
    converted.reserve(ws.size());
    for (size_t i = 0; i < ws.size(); i++) {
        if (ws[i] == L'\r' && i + 1 < ws.size() && ws[i+1] == L'\n') {
            switch (le) {
            case LineEnding::CRLF: converted += L'\r'; converted += L'\n'; break;
            case LineEnding::LF:   converted += L'\n'; break;
            case LineEnding::CR:   converted += L'\r'; break;
            }
            i++; // \n をスキップ
        } else {
            converted += ws[i];
        }
    }

    std::vector<uint8_t> bytes;
    if (enc == TextEncoding::UTF16LE) {
        bytes = { 0xFF, 0xFE };
        const uint8_t* p = reinterpret_cast<const uint8_t*>(converted.c_str());
        bytes.insert(bytes.end(), p, p + converted.size() * 2);
    } else if (enc == TextEncoding::UTF16BE) {
        bytes = { 0xFE, 0xFF };
        for (wchar_t c : converted) {
            bytes.push_back((uint8_t)(c >> 8));
            bytes.push_back((uint8_t)(c & 0xFF));
        }
    } else {
        UINT cp;
        bool addBOM = (enc == TextEncoding::UTF8_BOM);
        switch (enc) {
        case TextEncoding::UTF8_BOM: // fall through
        case TextEncoding::UTF8:     cp = CP_UTF8; break;
        case TextEncoding::ShiftJIS: cp = 932;     break;
        case TextEncoding::EUCJP:    cp = 20932;   break;
        case TextEncoding::JIS:      cp = 50220;   break;
        default:                     cp = CP_UTF8; break;
        }
        if (addBOM) bytes = { 0xEF, 0xBB, 0xBF };

        int mbLen = WideCharToMultiByte(cp, 0, converted.c_str(), (int)converted.size(),
                                        nullptr, 0, nullptr, nullptr);
        if (mbLen > 0) {
            size_t off = bytes.size();
            bytes.resize(off + mbLen);
            WideCharToMultiByte(cp, 0, converted.c_str(), (int)converted.size(),
                               (char*)bytes.data() + off, mbLen, nullptr, nullptr);
        }
    }

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(hFile, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
    CloseHandle(hFile);
    return written == (DWORD)bytes.size();
}

// CR/CRLF/LF → \r\n 正規化
static std::wstring NormalizeNewlines(const std::wstring& s)
{
    std::wstring r;
    r.reserve(s.size() * 2);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r') {
            r += L'\r'; r += L'\n';
            if (i + 1 < s.size() && s[i + 1] == L'\n') ++i; // CRLF → skip \n
        } else if (s[i] == L'\n') {
            r += L'\r'; r += L'\n';
        } else {
            r += s[i];
        }
    }
    return r;
}

// ConsoleCtrl の高さを計測して OutputPanel のアイテム高を更新しレイアウト
static void ResizeConsole(HWND hConsole);

// ================================================================
// Tab補完ヘルパー
// ================================================================

static const wchar_t* s_tabCommands[] = {
    L"clear", L"cls", L"edit", L"hello", L"help", L"list", L"view", L"walk"
};

// input からTab補完候補リストを生成して返す
static std::vector<std::wstring> BuildTabCandidates(const std::wstring& input)
{
    std::vector<std::wstring> candidates;

    size_t spacePos = input.find(L' ');

    if (spacePos == std::wstring::npos) {
        // コマンド名補完
        for (auto* cmd : s_tabCommands) {
            if (_wcsnicmp(cmd, input.c_str(), input.size()) == 0)
                candidates.push_back(std::wstring(cmd) + L" ");
        }
        return candidates;
    }

    // パス補完
    std::wstring cmdPart = input.substr(0, spacePos + 1);  // "walk " など
    std::wstring pathPart = input.substr(spacePos + 1);

    // ディレクトリ部とプレフィックスに分解
    std::wstring baseDir, prefix;
    size_t lastSlash = pathPart.find_last_of(L"\\/");
    if (lastSlash == std::wstring::npos) {
        baseDir = g_currentDir;
        prefix  = pathPart;
    } else {
        std::wstring dirPart = pathPart.substr(0, lastSlash + 1);
        prefix = pathPart.substr(lastSlash + 1);
        // 絶対パス判定: ドライブ文字 or UNC パスなら絶対
        bool isAbsolute = (dirPart.size() >= 2 && dirPart[1] == L':') ||
                          (dirPart.size() >= 2 && dirPart[0] == L'\\' && dirPart[1] == L'\\');
        if (!isAbsolute)
            baseDir = g_currentDir + L"\\" + dirPart;
        else
            baseDir = dirPart;
    }

    // ファイル列挙
    std::wstring searchPath = baseDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return candidates;

    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        if (!prefix.empty() && _wcsnicmp(name.c_str(), prefix.c_str(), prefix.size()) != 0)
            continue;

        std::wstring matched;
        if (lastSlash == std::wstring::npos)
            matched = name;
        else
            matched = pathPart.substr(0, lastSlash + 1) + name;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            matched += L"\\";

        candidates.push_back(cmdPart + matched);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    std::sort(candidates.begin(), candidates.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });

    return candidates;
}

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

        // Tab以外のキーで補完状態リセット
        if (wParam != VK_TAB && wParam != VK_SHIFT)
            cd->tabActive = false;

        switch (wParam) {
        case VK_TAB: {
            if (cd->finalized) return 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            // 現在の入力を取得
            int len = GetWindowTextLength(hwnd);
            std::wstring full(len + 1, L'\0');
            GetWindowText(hwnd, &full[0], len + 1);
            full.resize(len);
            std::wstring curInput = full.substr(cd->inputStart);
            while (!curInput.empty() && (curInput.back() == L'\r' || curInput.back() == L'\n'))
                curInput.pop_back();

            if (!cd->tabActive) {
                // 新規補完: 候補リストを構築
                cd->tabBase = curInput;
                cd->tabCandidates = BuildTabCandidates(curInput);
                if (cd->tabCandidates.empty()) return 0;
                cd->tabIdx   = shift ? (int)cd->tabCandidates.size() - 1 : 0;
                cd->tabActive = true;
            } else {
                // サイクル
                if (shift) {
                    if (--cd->tabIdx < 0)
                        cd->tabIdx = (int)cd->tabCandidates.size() - 1;
                } else {
                    if (++cd->tabIdx >= (int)cd->tabCandidates.size())
                        cd->tabIdx = 0;
                }
            }

            SendMessage(hwnd, EM_SETSEL, cd->inputStart, -1);
            SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)cd->tabCandidates[cd->tabIdx].c_str());
            return 0;
        }
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
        // Enter / Tab の文字挿入を抑制
        if (wParam == L'\r' || wParam == L'\n' || wParam == L'\t') return 0;
        if (!cd || cd->finalized) return 0;
        // Tab以外の文字入力で補完状態リセット
        if (wParam != VK_BACK)
            cd->tabActive = false;
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

    case WM_APP_EDIT_DONE: {
        // EditPanel から編集終了通知を受け取る
        // パネルは破棄せず操作類を無効化・読み取り専用にして残す
        HWND hEP = (HWND)lParam;
        bool found = false;
        for (auto& item : d->items)
            if (item.hwnd == hEP) { found = true; break; }

        if (found) {
            auto* epd = (EditPanelData*)GetWindowLongPtr(hEP, GWLP_USERDATA);
            if (epd) {
                EnableWindow(epd->hBtnSave,    FALSE);
                EnableWindow(epd->hBtnSaveAs,  FALSE);
                EnableWindow(epd->hBtnDiscard, FALSE);
                EnableWindow(epd->hCmbEnc,     FALSE);
                EnableWindow(epd->hCmbEnd,     FALSE);
                SendMessage(epd->hEdit, EM_SETREADONLY, TRUE, 0);
            }
            OutputPanel_AddPrompt(hwnd);
            OutputPanel_ScrollToBottom(hwnd);
        }
        break;
    }

    case WM_APP_IMAGE_EDIT_DONE: {
        // ImageEditPanel から編集終了通知
        HWND hIP = (HWND)lParam;
        bool found = false;
        for (auto& item : d->items)
            if (item.hwnd == hIP) { found = true; break; }

        if (found) {
            auto* ipd = (ImageEditPanelData*)GetWindowLongPtr(hIP, GWLP_USERDATA);
            if (ipd) {
                EnableWindow(ipd->hBtnSave,     FALSE);
                EnableWindow(ipd->hBtnSaveAs,   FALSE);
                EnableWindow(ipd->hBtnDiscard,  FALSE);
                EnableWindow(ipd->hBtnSizeDown, FALSE);
                EnableWindow(ipd->hBtnSizeUp,   FALSE);
                EnableWindow(ipd->hLabelSize,   FALSE);
                EnableWindow(ipd->hPaint,       FALSE);
            }
            OutputPanel_AddPrompt(hwnd);
            OutputPanel_ScrollToBottom(hwnd);
        }
        break;
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
    RegisterImageEditPanel(hInst);
    RegisterEditPanel(hInst);
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

// ---- テキストエディタ (EditPanel) --------------------------------
void OutputPanel_AddEdit(HWND hPanel, const std::wstring& filePath)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    TextFileInfo info;
    if (!filePath.empty()) {
        info = ReadTextFile(filePath);
        if (info.content.empty() && GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            OutputPanel_AddText(hPanel, L"Error: cannot open file: " + filePath);
            return;
        }
    }
    std::wstring content = NormalizeNewlines(info.content);

    FinalizeActiveConsole(hPanel);

    RECT rcPanel;
    GetClientRect(hPanel, &rcPanel);
    int panelW = std::max((int)rcPanel.right, 200);
    int panelH = std::max((int)rcPanel.bottom, 300);
    int yPos   = d->totalHeight - d->scrollOffset;

    HWND hEP = CreateWindowEx(
        0, EDIT_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, yPos, panelW, panelH,
        hPanel, nullptr, d->hInst, nullptr);

    auto* epd = (EditPanelData*)GetWindowLongPtr(hEP, GWLP_USERDATA);
    if (epd) {
        epd->filePath = filePath;
        SetWindowTextW(epd->hEdit, content.c_str());
        SendMessage(epd->hEdit, EM_SETMODIFY, FALSE, 0);
        SendMessage(epd->hCmbEnc, CB_SETCURSEL, EncodingToIndex(info.encoding), 0);
        int leIdx = 0;
        for (int i = 0; i < kLineEndCount; i++) {
            if (kLineEndings[i] == info.lineEnding) { leIdx = i; break; }
        }
        SendMessage(epd->hCmbEnd, CB_SETCURSEL, leIdx, 0);
        // ファイルパスなし (新規作成) は上書き保存を無効化
        EnableWindow(epd->hBtnSave, !filePath.empty());
        SetFocus(epd->hEdit);
    }

    d->items.push_back({ OutputType::Edit, hEP, panelH });
    LayoutPanel(hPanel);
}

// ---- テキストビューワ (読み取り専用) ----------------------------
void OutputPanel_AddTextView(HWND hPanel, const std::wstring& filePath)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    std::wstring content = ReadTextFileAsWString(filePath);
    if (content.empty() && GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        OutputPanel_AddText(hPanel, L"Error: cannot open file: " + filePath);
        return;
    }
    content = NormalizeNewlines(content);

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

// ---- ペイントツール (ImageEditPanel 経由) ------------------------
void OutputPanel_AddPaint(HWND hPanel, const std::wstring& path)
{
    PanelData* d = GetData(hPanel);
    if (!d) return;

    RECT rcPanel;
    GetClientRect(hPanel, &rcPanel);
    int panelW = std::max((int)rcPanel.right, 100);
    int panelH = std::max((int)rcPanel.bottom, 300);

    // 画像サイズを先に取得してキャンバス高さを確定
    Gdiplus::Image* pImg = Gdiplus::Image::FromFile(path.c_str());
    if (pImg && pImg->GetLastStatus() != Gdiplus::Ok) { delete pImg; pImg = nullptr; }

    int imgH = 400;
    if (pImg) {
        UINT iw = pImg->GetWidth();
        UINT ih = pImg->GetHeight();
        if (iw > 0 && ih > 0) {
            imgH = (int)((float)ih / iw * panelW);
            imgH = std::max(50, std::min(imgH, 600));
        }
    }
    int totalH = std::max(imgH + IMAGE_PANEL_TOOLBAR_H, panelH);

    FinalizeActiveConsole(hPanel);

    int yPos = d->totalHeight - d->scrollOffset;
    HWND hIP = CreateWindowEx(
        0, IMAGE_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, yPos, panelW, totalH,
        hPanel, nullptr, d->hInst, nullptr);

    // filePath を設定し、画像を PaintCtrl の memory DC に描画
    auto* ipd = (ImageEditPanelData*)GetWindowLongPtr(hIP, GWLP_USERDATA);
    if (ipd) {
        ipd->filePath = path;
        auto* pd = (PaintCtrlData*)GetWindowLongPtr(ipd->hPaint, GWLP_USERDATA);
        if (pd && pImg) {
            Gdiplus::Graphics g(pd->hMemDC);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(pImg, 0, 0, pd->bmWidth, pd->bmHeight);
            InvalidateRect(ipd->hPaint, nullptr, FALSE);
        }
    }
    delete pImg;

    d->items.push_back({ OutputType::Paint, hIP, totalH });
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

    for (auto& item : d->items)
        DestroyWindow(item.hwnd);
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
