#pragma once
#include <windows.h>
#include <string>
#include <vector>

// ウィンドウ定数
#define APP_TITLE           L"VizCommand"
#define CMDBAR_HEIGHT       26      // プロンプト行の高さ (Consolas 18pt 1行分)
#define ITEM_MARGIN         0       // 各出力アイテム間の余白
#define CMD_EDIT_ID         100

// コンソール配色 (CMD風黒背景)
#define CON_BG      RGB(0,   0,   0  )  // 背景
#define CON_FG      RGB(192, 192, 192)  // テキスト
#define CON_GREEN   RGB(0,   204, 0  )  // プロンプト ">"

// 出力アイテムの種類
enum class OutputType {
    Console,    // テキストコンソール (入力・出力共用)
    Image,      // 画像表示 (読み取り専用)
    TextView,   // テキスト表示 (読み取り専用)
    Edit,       // テキスト編集
    FileList,   // ファイル一覧 (サムネイルリスト)
    Paint,      // 画像編集 (ペイントツール)
};

// ウィンドウリストビューの各エントリ
struct OutputItem {
    OutputType  type;
    HWND        hwnd;   // 子ウィンドウ
    int         height; // 表示高さ (px)
};
