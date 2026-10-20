@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "KIT=C:\Program Files\Microsoft Visual Studio\Windows Kits\10"
set "MSVC=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC"
for /f "delims=" %%i in ('dir /b /ad /o-n "%MSVC%"') do set "MSVCVER=%%i"
for /f "delims=" %%i in ('dir /b /ad /o-n "%KIT%\Include"') do set "KITVER=%%i"
set "MSVCROOT=%MSVC%\%MSVCVER%"
set "INCLUDE=%MSVCROOT%\include;%KIT%\Include\%KITVER%\ucrt;%KIT%\Include\%KITVER%\shared;%KIT%\Include\%KITVER%\um"
set "LIB=%MSVCROOT%\lib\x64;%KIT%\Lib\%KITVER%\ucrt\x64;%KIT%\Lib\%KITVER%\um\x64"
set "PATH=%MSVCROOT%\bin\Hostx64\x64;%PATH%"
cd /d "%~dp0"
cl /nologo /c /EHsc /utf-8 /DUNICODE /D_UNICODE /O2 /MD /W3 /Isrc tools\probe-charge.cpp
if errorlevel 1 exit /b 1
link /nologo /OUT:probe-charge.exe /SUBSYSTEM:CONSOLE probe-charge.obj kernel32.lib setupapi.lib hid.lib ucrt.lib uuid.lib
if errorlevel 1 exit /b 1
del probe-charge.obj
echo [OK] probe-charge.exe
