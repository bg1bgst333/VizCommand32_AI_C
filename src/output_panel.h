#pragma once
#include "vizcommand.h"

#define OUTPUT_PANEL_CLASS L"VizCmdOutputPanel"

// ウィンドウリストビューを登録・作成
void OutputPanel_Register(HINSTANCE hInst);
HWND OutputPanel_Create(HWND hParent, HINSTANCE hInst, int x, int y, int w, int h);

// テキスト応答をアクティブなコンソールに追記
void OutputPanel_AddText    (HWND hPanel, const std::wstring& text);
// GUI コントロールを追加 (コンソールを分割して挿入)
void OutputPanel_AddImage    (HWND hPanel, const std::wstring& path);
void OutputPanel_AddTextView (HWND hPanel, const std::wstring& filePath);
void OutputPanel_AddEdit     (HWND hPanel, const std::wstring& filePath);
void OutputPanel_AddFileList (HWND hPanel, const std::wstring& dirPath);
void OutputPanel_AddPaint    (HWND hPanel, const std::wstring& path);

// ユーティリティ
void OutputPanel_Clear         (HWND hPanel);
void OutputPanel_ScrollToBottom(HWND hPanel);
void OutputPanel_AddPrompt     (HWND hPanel);
void OutputPanel_FocusPrompt   (HWND hPanel);
