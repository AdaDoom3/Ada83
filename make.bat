@echo off
setlocal
cd /d "%~dp0"

set "EXE=ada83.exe"
set "CFLAGS=-O2 -std=gnu2x"
set "LIBS=-lm -lpthread"
set "ZIG_VERSION=0.16.0"
set "ZIG_NAME=zig-x86_64-windows-%ZIG_VERSION%"
set "ZIG_URL=https://ziglang.org/download/%ZIG_VERSION%/%ZIG_NAME%.zip"

call :dispatch %1
set "RESULT=%errorlevel%"
echo %cmdcmdline% | find /i "/c" >nul && pause
exit /b %RESULT%

:dispatch
if /i "%~1"=="/?"    goto usage
if /i "%~1"=="-h"    goto usage
if /i "%~1"=="help"  goto usage
if /i "%~1"=="clean" goto clean
if not "%~1"=="" (
    echo Invalid parameter - %~1
    goto usage
)
goto make

:usage
echo Makes the Ada 83 compiler.
echo.
echo MAKE [command]
echo.
echo   clean      Deletes ada83.exe, LLVM-C.dll and the downloaded Zig.
echo   help       Displays this help.
echo.
echo Makes with GCC, Clang or Zig, whichever is found first.
echo.
echo LLVM-C.dll is unpacked from LLVM-C.zip and must stay with
echo ada83.exe, which loads it when it runs.
exit /b 0

:clean
del /q "%EXE%" LLVM-C.dll zig.zip >nul 2>nul
rmdir /s /q zig >nul 2>nul
echo Cleaned.
exit /b 0

:make
call :require ada83.c     || exit /b 1
call :require runtime.ada || exit /b 1
call :unpack_llvm         || exit /b 1
call :compile             || exit /b 1
call :verify              || exit /b 1
echo Built %EXE%.
exit /b 0

:require
if exist "%~1" exit /b 0
echo Cannot find %~1. Run this script from the folder it came in.
exit /b 1

:unpack_llvm
if exist LLVM-C.dll exit /b 0
if not exist LLVM-C.zip (
    echo Cannot find LLVM-C.dll or LLVM-C.zip.
    exit /b 1
)
echo Unpacking LLVM-C.dll...
tar -xf LLVM-C.zip >nul 2>nul
if not exist LLVM-C.dll (
    powershell -NoProfile -Command ^
        "Expand-Archive -LiteralPath 'LLVM-C.zip' -DestinationPath '.' -Force" >nul 2>nul
)
if not exist LLVM-C.dll (
    echo Cannot unpack LLVM-C.zip. Extract it here by hand and try again.
    exit /b 1
)
exit /b 0

:compile
call :attempt "GCC"   "gcc"         "gcc %CFLAGS% ada83.c -o %EXE% %LIBS%"                                   && exit /b 0
call :attempt "Clang" "clang"       "clang %CFLAGS% --target=x86_64-w64-windows-gnu ada83.c -o %EXE% %LIBS%" && exit /b 0
call :fetch_zig || exit /b 1
call :attempt "Zig"   "zig\zig.exe" "zig\zig.exe cc %CFLAGS% -target x86_64-windows-gnu ada83.c -o %EXE% %LIBS%" && exit /b 0
echo No compiler was able to make ada83.c.
exit /b 1

:attempt
call :present "%~2" || exit /b 1
echo Compiling with %~1...
%~3
if errorlevel 1 exit /b 1
exit /b 0

:present
where "%~1" >nul 2>nul && exit /b 0
if exist "%~1" exit /b 0
exit /b 1

:fetch_zig
if exist zig\zig.exe exit /b 0
echo No C compiler found. Downloading Zig %ZIG_VERSION%...
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "Invoke-WebRequest '%ZIG_URL%' -OutFile 'zig.zip';" ^
    "Expand-Archive 'zig.zip' '.' -Force;" ^
    "Move-Item '%ZIG_NAME%' 'zig' -Force;" ^
    "Remove-Item 'zig.zip'"
if not exist zig\zig.exe (
    echo Cannot download Zig. Install MinGW-w64 GCC and try again.
    exit /b 1
)
exit /b 0

:verify
"%EXE%" --version >nul 2>nul
if errorlevel 1 (
    echo %EXE% was built but does not run.
    exit /b 1
)
exit /b 0
