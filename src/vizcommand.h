#pragma once
#include <windows.h>
#include <string>
#include <vector>

// ウィンドウ定数
#define APP_TITLE           L"VizCommand"
#define CMDBAR_HEIGHT       36      // コマンド入力バーの高さ
#define ITEM_MARGIN         6       // 各出力アイテム間の余白
#define CMD_EDIT_ID         100

// 出力アイテムの種類
enum class OutputType {
    Text,       // テキスト表示 (読み取り専用)
    Image,      // 画像表示
    Edit,       // テキスト編集
    FileList,   // ファイル一覧 (サムネイルリスト)
    Prompt      // コマンド入力行
};

// ウィンドウリストビューの各エントリ
struct OutputItem {
    OutputType  type;
    HWND        hwnd;   // 子ウィンドウ
    int         height; // 表示高さ (px)
};
