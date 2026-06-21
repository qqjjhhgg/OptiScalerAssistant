@echo off

setlocal
set ROOT=%~dp0
cd /d "%ROOT%"

set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
set OUT_DIR=exe
set OUT_EXE=%OUT_DIR%\OptiScalerAssistant.exe

if not exist %VCVARS% (
    echo [ERROR] MSVC not found: %VCVARS%
    goto :end
)
if not exist "C:\Tools\upx\upx.exe" (
    echo [ERROR] UPX not found: C:\Tools\upx\upx.exe
    goto :end
)

call %VCVARS% x64 >nul
if errorlevel 1 (
    echo [ERROR] vcvarsall failed
    goto :end
)

if not exist "build\obj" mkdir "build\obj"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo === Step 1/3: Compile resources ===
rc.exe /I"src" /Fo"build\obj\app.res" "src\app.rc"
if errorlevel 1 (
    echo [ERROR] rc.exe failed
    goto :end
)

echo === Step 2/3: Compile and link ===
cl /nologo /W3 /O2 /MT /std:c++17 /utf-8 /EHsc /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /I"src" /I"webview2\include" /Fo"build\obj\\" /Fe"%OUT_EXE%" src\main.cpp src\MainWindow.cpp src\WebView2Host.cpp src\GpuDetector.cpp src\GameScanner.cpp src\AntiCheatScanner.cpp src\ProfileAdvisor.cpp src\Installer.cpp src\ResourceExtractor.cpp src\Logger.cpp "build\obj\app.res" user32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib version.lib dxgi.lib d3d11.lib "webview2\x64\WebView2LoaderStatic.lib" /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:"src\app.manifest"
if errorlevel 1 (
    echo [ERROR] cl.exe failed
    goto :end
)

echo === Step 3/3: UPX compress (disabled to avoid Smart App Control false positives) ===

echo === Step 4/4: Cleanup external manifest ===
if exist "%OUT_DIR%\OptiScalerAssistant.exe.manifest" del "%OUT_DIR%\OptiScalerAssistant.exe.manifest"

echo === DONE: %OUT_EXE% ===
dir "%OUT_EXE%" | findstr "OptiScalerAssistant"

:end
endlocal
pause
