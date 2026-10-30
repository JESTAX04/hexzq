@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"

cl.exe /nologo /EHsc /O2 /MD /std:c++20 /DRED_PREVIEW ^
  /I. /Iimgui /Isrc ^
  preview_main.cpp ^
  ui.cpp ^
  src\engine_stub.cpp ^
  src\vehicle_tab.cpp ^
  src\weapon_tab.cpp ^
  imgui\imgui.cpp ^
  imgui\imgui_draw.cpp ^
  imgui\imgui_widgets.cpp ^
  imgui\imgui_impl_win32.cpp ^
  imgui\imgui_impl_dx11.cpp ^
  TextEditor.cpp ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup ^
  d3d11.lib dxgi.lib dwmapi.lib user32.lib gdi32.lib shell32.lib comdlg32.lib ^
  /OUT:"..\preview.exe"

if errorlevel 1 (echo [preview build FAILED] & exit /b 1)
echo [preview build OK] ..\preview.exe
exit /b 0
