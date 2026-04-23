#pragma once
#include "vizcommand.h"

#define OUTPUT_PANEL_CLASS L"VizCmdOutputPanel"

// ウィンドウリストビューを登録・作成
void OutputPanel_Register(HINSTANCE hInst);
HWND OutputPanel_Create(HWND hParent, HINSTANCE hInst, int x, int y, int w, int h);

// コマンド結果を追加 (末尾にエントリが積まれる)
void OutputPanel_AddText    (HWND hPanel, const std::wstring& text);
void OutputPanel_AddImage   (HWND hPanel, const std::wstring& path);
void OutputPanel_AddEdit    (HWND hPanel, const std::wstring& filePath);
void OutputPanel_AddFileList(HWND hPanel, const std::wstring& dirPath);

// ユーティリティ
void OutputPanel_Clear         (HWND hPanel);
void OutputPanel_ScrollToBottom(HWND hPanel);
void OutputPanel_AddPrompt     (HWND hPanel);
void OutputPanel_FocusPrompt   (HWND hPanel);
