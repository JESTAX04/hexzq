@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"

echo [1/2] redengine.dll
cl.exe /nologo /EHsc /O2 /MD /std:c++20 /DRED_INJECTED ^
  /I. /Iimgui /Isrc ^
  src\dllmain.cpp ^
  src\overlay_host.cpp ^
  src\lua_engine.cpp ^
  src\detour.cpp ^
  src\sigscan.cpp ^
  src\vehicle_tab.cpp ^
  src\weapon_tab.cpp ^
  ui.cpp ^
  imgui\imgui.cpp ^
  imgui\imgui_draw.cpp ^
  imgui\imgui_widgets.cpp ^
  imgui\imgui_impl_win32.cpp ^
  imgui\imgui_impl_dx11.cpp ^
  TextEditor.cpp ^
  /link /DLL ^
  d3d11.lib dxgi.lib dwmapi.lib user32.lib gdi32.lib shell32.lib psapi.lib comdlg32.lib ^
  /OUT:"loader\redengine.dll"
if errorlevel 1 (echo [dll build FAILED] & exit /b 1)

echo [2/2] loader.exe
cl.exe /nologo /EHsc /O2 /MD /std:c++20 ^
  /Isrc ^
  loader\loader.cpp ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup ^
  user32.lib advapi32.lib ^
  /OUT:"loader\loader.exe"
if errorlevel 1 (echo [loader build FAILED] & exit /b 1)

echo [injected build OK] loader\redengine.dll + loader\loader.exe
exit /b 0
