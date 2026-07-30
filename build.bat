@echo off
rem ---------------------------------------------------------------------
rem  Build ada83.exe on Windows.
rem
rem  Usage:  build            build the compiler
rem          build clean      remove everything this script produced
rem          build help       show this text
rem
rem  Nothing needs to be installed first.  If no C compiler is present the
rem  script fetches a private copy of Zig and uses that, leaving the rest
rem  of the machine untouched.
rem ---------------------------------------------------------------------
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

rem  Double-clicked from Explorer rather than run in a console: hold the
rem  window open so the result is readable.
echo %cmdcmdline% | find /i "/c" >nul && pause
exit /b %RESULT%

rem ===== top level ======================================================

:dispatch
if /i "%~1"=="help"  goto usage
if /i "%~1"=="/?"    goto usage
if /i "%~1"=="-h"    goto usage
if /i "%~1"=="clean" goto clean
if not "%~1"=="" (
    echo Unknown option "%~1"
    goto usage
)
goto build

:build
echo.
echo   Building the Ada 83 compiler
echo   ----------------------------
call :require ada83.c     || exit /b 1
call :require runtime.ada || exit /b 1
call :unpack_llvm         || exit /b 1
call :compile             || exit /b 1
call :verify              || exit /b 1
goto finish

:usage
echo.
echo   build          build ada83.exe
echo   build clean    remove ada83.exe, LLVM-C.dll and the private Zig
echo   build help     this text
echo.
exit /b 0

:clean
del /q "%EXE%" LLVM-C.dll zig.zip >nul 2>nul
rmdir /s /q zig >nul 2>nul
echo   Cleaned.
exit /b 0

:finish
echo.
echo   Done. %EXE% is ready to use.
echo.
echo   Compile and run a program:
echo       %EXE% myprogram.ada -o myprogram.exe
echo       myprogram.exe
echo.
echo   Keep LLVM-C.dll next to %EXE% -- it is loaded when the compiler runs.
echo.
exit /b 0

rem ===== steps ==========================================================

:require
if exist "%~1" exit /b 0
echo   [x] %~1 is missing.
echo       Run this script from the folder it came in, alongside the
echo       rest of the Ada83 sources.
exit /b 1

rem  LLVM-C.dll is opened at run time rather than linked against, so it
rem  only has to sit beside the executable.  It ships as a .zip because a
rem  stock Windows can already unpack one, with no extra tools.
:unpack_llvm
if exist LLVM-C.dll (
    echo   [ok] LLVM-C.dll is present
    exit /b 0
)
if not exist LLVM-C.zip (
    echo   [x] neither LLVM-C.dll nor LLVM-C.zip is here.
    echo       LLVM-C.zip ships with the sources -- restore it and re-run.
    exit /b 1
)
echo   [..] unpacking LLVM-C.dll ^(about 140 MB, takes a moment^)
tar -xf LLVM-C.zip >nul 2>nul
if not exist LLVM-C.dll (
    powershell -NoProfile -Command ^
        "Expand-Archive -LiteralPath 'LLVM-C.zip' -DestinationPath '.' -Force" >nul 2>nul
)
if not exist LLVM-C.dll (
    echo   [x] could not unpack LLVM-C.zip.
    echo       Unpack it by hand -- right-click, Extract All -- so that
    echo       LLVM-C.dll lands in this folder, then run build again.
    exit /b 1
)
echo   [ok] unpacked LLVM-C.dll
exit /b 0

rem  Try each toolchain in turn and stop at the first that works.  MSVC is
rem  deliberately not among them: ada83.c needs __int128, which cl has at
rem  no /std level.
:compile
call :attempt "GCC"   "gcc"         "gcc %CFLAGS% ada83.c -o %EXE% %LIBS%"                                   && exit /b 0
call :attempt "Clang" "clang"       "clang %CFLAGS% --target=x86_64-w64-windows-gnu ada83.c -o %EXE% %LIBS%" && exit /b 0
call :fetch_zig || exit /b 1
call :attempt "Zig"   "zig\zig.exe" "zig\zig.exe cc %CFLAGS% -target x86_64-windows-gnu ada83.c -o %EXE% %LIBS%" && exit /b 0
echo   [x] no toolchain was able to build ada83.c.
echo       The messages above say why.
exit /b 1

rem  attempt <label> <program> <command>
:attempt
call :present "%~2" || exit /b 1
echo   [..] compiling with %~1 ^(one large file, please wait^)
%~3
if errorlevel 1 (
    echo   [x] %~1 could not build ada83.c.
    exit /b 1
)
echo   [ok] built %EXE% with %~1
exit /b 0

rem  present <program> -- on PATH, or a path that exists
:present
where "%~1" >nul 2>nul && exit /b 0
if exist "%~1" exit /b 0
exit /b 1

:fetch_zig
if exist zig\zig.exe (
    echo   [ok] using the Zig fetched earlier
    exit /b 0
)
echo   [..] no C compiler found -- fetching Zig %ZIG_VERSION% ^(about 80 MB^)
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "Invoke-WebRequest '%ZIG_URL%' -OutFile 'zig.zip';" ^
    "Expand-Archive 'zig.zip' '.' -Force;" ^
    "Move-Item '%ZIG_NAME%' 'zig' -Force;" ^
    "Remove-Item 'zig.zip'"
if not exist zig\zig.exe (
    echo   [x] could not fetch Zig.
    echo       Check the network connection, or install MinGW-w64 GCC
    echo       and run build again.
    exit /b 1
)
echo   [ok] Zig is ready
exit /b 0

:verify
"%EXE%" --version >nul 2>nul
if errorlevel 1 (
    echo   [x] %EXE% was produced but does not run.
    exit /b 1
)
echo   [ok] %EXE% runs
exit /b 0
