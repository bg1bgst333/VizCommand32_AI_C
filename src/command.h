#pragma once
#include "vizcommand.h"

// カレントディレクトリ (walk コマンドで変更)
extern std::wstring g_currentDir;

// コマンドを解析・実行して出力パネルに結果を追加
void ExecuteCommand(HWND hOutputPanel, const std::wstring& cmdLine);
