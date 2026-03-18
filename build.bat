@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set CMAKE_EXE=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set NINJA_EXE=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe

cd /d "C:\Project\Cloud\github.com\VizCommand32_AI_C"
if not exist build mkdir build
cd build

echo --- CMake configure ---
"%CMAKE_EXE%" -G Ninja -DCMAKE_BUILD_TYPE=Release ..
if errorlevel 1 goto :fail

echo --- Ninja build ---
"%NINJA_EXE%"
if errorlevel 1 goto :fail

echo.
echo *** Build succeeded: build\VizCommand.exe ***
goto :end

:fail
echo.
echo *** Build FAILED ***
exit /b 1

:end
endlocal
