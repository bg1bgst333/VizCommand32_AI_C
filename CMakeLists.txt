cmake_minimum_required(VERSION 3.20)
project(VizCommand CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(VizCommand WIN32
    src/main.cpp
    src/output_panel.cpp
    src/command.cpp
)

target_include_directories(VizCommand PRIVATE src)

target_compile_definitions(VizCommand PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    _HAS_STD_BYTE=0
)

# UTF-8 ソースファイルとして扱う (/utf-8)
target_compile_options(VizCommand PRIVATE /utf-8)

target_link_libraries(VizCommand
    comctl32
    shell32
    gdiplus
    shlwapi
)
