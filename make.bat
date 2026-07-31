@echo off
setlocal
cd /d "%~dp0"

set "EXE=ada83.exe"
set "ZIP=bin-windows.zip"
set "RUNTIME=ada83-runtime.ada"
set "BUNDLE=ada83-extension.js"
set "MANUAL=manual.md"
set "VSIX=ada83.vsix"
set "CFLAGS=-O2 -std=gnu2x"
set "LIBS=-lm"
set "ZIG_VERSION=0.16.0"
set "ZIG_NAME=zig-x86_64-windows-%ZIG_VERSION%"
set "ZIG_URL=https://ziglang.org/download/%ZIG_VERSION%/%ZIG_NAME%.zip"

call :dispatch %1
set "RESULT=%errorlevel%"
echo %cmdcmdline% | find /i "/c" >nul && pause
exit /b %RESULT%

:dispatch
if /i "%~1"=="/?"      goto usage
if /i "%~1"=="-h"      goto usage
if /i "%~1"=="help"    goto usage
if /i "%~1"=="clean"   goto clean
if /i "%~1"=="package" goto package
if /i "%~1"=="vsix"    goto vsix
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
echo   clean      Deletes %EXE%, %VSIX%, the LLVM DLLs and any downloaded Zig.
echo   package    Makes %EXE% and %VSIX%, then repacks %ZIP%.
echo   vsix       Makes %VSIX%, the VS Code extension, only.
echo   help       Displays this help.
echo.
echo Makes with GCC, Clang or Zig, whichever is found first. The archives for
echo Linux and macOS are cut with the makefile, on any host.
echo.
echo LLVM-C.dll and its companion DLLs are unpacked from %ZIP%
echo and must stay with %EXE%, which loads them when it runs.
exit /b 0

:clean
del /q "%EXE%" LLVM-C.dll libffi-8.dll libstdc++-6.dll libgcc_s_seh-1.dll ^
    libwinpthread-1.dll libxml2-16.dll libiconv-2.dll libzstd.dll zlib1.dll ^
    zig.zip "%ZIP%.new" "%VSIX%" >nul 2>nul
rmdir /s /q zig staging >nul 2>nul
echo Cleaned.
exit /b 0

:make
call :require ada83.c   || exit /b 1
call :require %RUNTIME% || exit /b 1
call :unpack_llvm       || exit /b 1
call :compile           || exit /b 1
"%EXE%" --version >nul 2>nul || (
    echo %EXE% was built but does not run.
    exit /b 1
)
echo Built %EXE%.
exit /b 0

:package
call :make || exit /b 1
call :vsix || exit /b 1
del /q "%ZIP%.new" >nul 2>nul
setlocal enabledelayedexpansion
set "CONTENTS=%EXE% %RUNTIME% %VSIX%"
for %%D in (*.dll) do set "CONTENTS=!CONTENTS! %%D"
tar --format zip -c -f "%ZIP%.new" !CONTENTS!
endlocal
if not exist "%ZIP%.new" (
    echo Cannot write %ZIP%.
    exit /b 1
)
move /y "%ZIP%.new" "%ZIP%" >nul || exit /b 1
echo Packaged %ZIP%.
exit /b 0

:vsix
call :require %BUNDLE% || exit /b 1
if exist staging rmdir /s /q staging
mkdir staging\extension\syntaxes
copy /y "%BUNDLE%" staging\extension\ >nul
if exist "%MANUAL%" (
    copy /y "%MANUAL%" staging\extension\ >nul
) else (
    echo %MANUAL% is missing; packaging without the manual search tool.
)
call :split
del /q "%VSIX%" >nul 2>nul
tar --format zip -c -f "%VSIX%" -C staging ^
    extension extension.vsixmanifest "[Content_Types].xml"
rmdir /s /q staging
if not exist "%VSIX%" (
    echo Cannot write %VSIX%.
    exit /b 1
)
echo Built %VSIX%.
exit /b 0

:split
setlocal disabledelayedexpansion
set "OUT="
for /f "delims=" %%L in (%BUNDLE%) do (
    set "LINE=%%L"
    setlocal enabledelayedexpansion
    if "!LINE:~0,5!"=="//== " (
        for %%N in ("!LINE:~5!") do endlocal & call :destination %%N
    ) else (
        if defined OUT if "!LINE:~0,2!"=="//" if not "!LINE:~0,4!"=="//= " >>"!OUT!" echo(!LINE:~2!
        endlocal
    )
)
endlocal
exit /b 0

:destination
set "NAME=%~1"
set "OUT=staging\extension\%NAME%"
if "%NAME%"=="extension.vsixmanifest" set "OUT=staging\%NAME%"
if "%NAME:~0,1%"=="[" set "OUT=staging\%NAME%"
if "%NAME%"=="end" set "OUT="
if defined OUT type nul >"%OUT%"
exit /b 0

:require
if exist "%~1" exit /b 0
echo Cannot find %~1. Run this script from the folder it came in.
exit /b 1

:unpack_llvm
if exist LLVM-C.dll exit /b 0
if not exist "%ZIP%" (
    echo Cannot find LLVM-C.dll or %ZIP%.
    exit /b 1
)
echo Unpacking LLVM-C.dll...
tar -xf "%ZIP%" *.dll
if exist LLVM-C.dll exit /b 0
echo Cannot unpack %ZIP%. Extract the DLLs here by hand and try again.
exit /b 1

:compile
call :attempt "GCC"   "gcc"   "gcc %CFLAGS% ada83.c -o %EXE% %LIBS%" && exit /b 0
call :attempt "Clang" "clang" "clang %CFLAGS% --target=x86_64-w64-windows-gnu ada83.c -o %EXE% %LIBS%" && exit /b 0
call :find_zig || exit /b 1
call :attempt "Zig" "%ZIG%" "%ZIG% cc %CFLAGS% -target x86_64-windows-gnu ada83.c -o %EXE% %LIBS%" && exit /b 0
echo No compiler was able to make ada83.c.
exit /b 1

:attempt
call :present "%~2" || exit /b 1
echo Compiling with %~1...
%~3
exit /b %errorlevel%

:present
where "%~1" >nul 2>nul && exit /b 0
if exist "%~1" exit /b 0
exit /b 1

:find_zig
set "ZIG=zig"
call :present "%ZIG%" && exit /b 0
set "ZIG=zig\zig.exe"
if exist "%ZIG%" exit /b 0
echo No C compiler was found. Zig %ZIG_VERSION% is one download and carries
echo everything needed to build ada83.c.
set /p "REPLY=Download it now? [y/N] "
if /i not "%REPLY%"=="y" (
    echo Install MinGW-w64 GCC, Clang or Zig and run this again.
    exit /b 1
)
curl -L -o zig.zip "%ZIG_URL%"
tar -xf zig.zip
move /y "%ZIG_NAME%" zig >nul
del /q zig.zip >nul 2>nul
if exist "%ZIG%" exit /b 0
echo Cannot download Zig. Install MinGW-w64 GCC and try again.
exit /b 1
