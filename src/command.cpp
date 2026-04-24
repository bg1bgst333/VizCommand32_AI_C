#include "command.h"
#include "output_panel.h"
#include <algorithm>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

std::wstring g_currentDir;

// ----------------------------------------------------------------
// 文字列ユーティリティ
// ----------------------------------------------------------------
static std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t");
    return s.substr(a, b - a + 1);
}

static void SplitCmdArgs(const std::wstring& line,
                         std::wstring& cmd, std::wstring& args)
{
    std::wstring s = Trim(line);
    size_t sp = s.find(L' ');
    if (sp == std::wstring::npos) {
        cmd  = s;
        args = L"";
    } else {
        cmd  = s.substr(0, sp);
        args = Trim(s.substr(sp + 1));
    }
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::towlower);
}

// 相対パスを g_currentDir 基準で絶対パスに変換
static std::wstring ResolvePath(const std::wstring& path) {
    if (PathIsRelative(path.c_str())) {
        wchar_t full[MAX_PATH];
        std::wstring base = g_currentDir + L"\\" + path;
        if (GetFullPathName(base.c_str(), MAX_PATH, full, nullptr))
            return full;
        return path;
    }
    return path;
}

// ----------------------------------------------------------------
// コマンド実行
// ----------------------------------------------------------------
void ExecuteCommand(HWND hOutputPanel, const std::wstring& cmdLine) {
    std::wstring cmd, args;
    SplitCmdArgs(cmdLine, cmd, args);
    if (cmd.empty()) {
        OutputPanel_AddPrompt(hOutputPanel);
        OutputPanel_ScrollToBottom(hOutputPanel);
        return;
    }

    // ---- hello ------------------------------------------------
    if (cmd == L"hello") {
        OutputPanel_AddText(hOutputPanel, L"hello");
    }
    // ---- walk <パス> -----------------------------------------
    else if (cmd == L"walk") {
        if (args.empty()) {
            OutputPanel_AddText(hOutputPanel, g_currentDir);
        } else {
            std::wstring newDir = ResolvePath(args);
            DWORD attr = GetFileAttributes(newDir.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                g_currentDir = newDir;
                OutputPanel_AddText(hOutputPanel, L"カレントディレクトリ: " + g_currentDir);
            } else {
                OutputPanel_AddText(hOutputPanel, L"エラー: ディレクトリが存在しません: " + newDir);
            }
        }
    }
    // ---- list ------------------------------------------------
    else if (cmd == L"list") {
        OutputPanel_AddFileList(hOutputPanel, g_currentDir);
    }
    // ---- view <ファイル> -------------------------------------
    else if (cmd == L"view") {
        if (args.empty()) {
            OutputPanel_AddText(hOutputPanel, L"使い方: view <ファイルパス>");
        } else {
            OutputPanel_AddImage(hOutputPanel, ResolvePath(args));
        }
    }
    // ---- edit <ファイル> -------------------------------------
    else if (cmd == L"edit") {
        if (args.empty()) {
            OutputPanel_AddText(hOutputPanel, L"使い方: edit <ファイルパス>");
        } else {
            OutputPanel_AddEdit(hOutputPanel, ResolvePath(args));
        }
    }
    // ---- clear / cls -----------------------------------------
    else if (cmd == L"clear" || cmd == L"cls") {
        OutputPanel_Clear(hOutputPanel);
        return;
    }
    // ---- help ------------------------------------------------
    else if (cmd == L"help") {
        std::wstring help =
            L"VizCommand  利用可能なコマンド\n"
            L"─────────────────────────────────\n"
            L"  hello              \"hello\" を表示\n"
            L"  walk [パス]        カレントディレクトリを変更 (引数なしで現在地を表示)\n"
            L"  list               カレントディレクトリのファイル一覧\n"
            L"  view <ファイル>    画像を表示 (BMP/PNG/JPG/GIF 等)\n"
            L"  edit <ファイル>    テキストファイルを編集\n"
            L"  clear / cls        出力をクリア\n"
            L"  help               このヘルプを表示\n"
            L"\n"
            L"  ↑↓キーでコマンド履歴を参照できます。";
        OutputPanel_AddText(hOutputPanel, help);
    }
    // ---- 不明コマンド ----------------------------------------
    else {
        OutputPanel_AddText(hOutputPanel,
            L"不明なコマンド: " + cmd + L"\n\"help\" で一覧を確認できます。");
    }

    OutputPanel_AddPrompt(hOutputPanel);
    OutputPanel_ScrollToBottom(hOutputPanel);
}
