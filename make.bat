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
echo   clean      Deletes %EXE%, the LLVM DLLs and any downloaded Zig.
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
if exist staging rmdir /s /q staging
mkdir staging
call :vsix_into staging || exit /b 1
copy /y "%EXE%" staging\     >nul
copy /y "%RUNTIME%" staging\ >nul
copy /y *.dll staging\       >nul
del /q "%ZIP%.new" >nul 2>nul
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$parts = Get-ChildItem -LiteralPath 'staging' -Force | ForEach-Object FullName;" ^
    "Compress-Archive -LiteralPath $parts -DestinationPath '%ZIP%.new' -Force"
if not exist "%ZIP%.new" (
    echo Cannot write %ZIP%.
    exit /b 1
)
move /y "%ZIP%.new" "%ZIP%" >nul || exit /b 1
rmdir /s /q staging
echo Packaged %ZIP%.
exit /b 0

:vsix
if exist staging rmdir /s /q staging
mkdir staging
call :vsix_into staging || exit /b 1
move /y "staging\%VSIX%" "%VSIX%" >nul
rmdir /s /q staging
echo Built %VSIX%.
exit /b 0

:vsix_into
if exist "%~1\vsix" rmdir /s /q "%~1\vsix"
mkdir "%~1\vsix\extension\syntaxes"
copy /y "%BUNDLE%" "%~1\vsix\extension\" >nul
if exist "%MANUAL%" (
    copy /y "%MANUAL%" "%~1\vsix\extension\" >nul
) else (
    echo %MANUAL% is missing; packaging without the manual search tool.
)
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$out = $null;" ^
    "foreach ($line in Get-Content -LiteralPath '%BUNDLE%') {" ^
    "  if ($line -eq '//== end') { $out = $null; continue }" ^
    "  if ($line -like '//== *') {" ^
    "    $name = $line.Substring(5);" ^
    "    $root = ($name -eq 'extension.vsixmanifest') -or $name.StartsWith('[');" ^
    "    $out = Join-Path '%~1\vsix' ($(if ($root) { $name } else { \"extension\$name\" }));" ^
    "    New-Item -ItemType File -Force -Path $out | Out-Null; continue }" ^
    "  if ($line -like '//= *') { continue }" ^
    "  if ($out -and $line.StartsWith('//')) {" ^
    "    Add-Content -LiteralPath $out -Value $line.Substring(2) } }" ^
    "$parts = Get-ChildItem -LiteralPath '%~1\vsix' -Force | ForEach-Object FullName;" ^
    "Compress-Archive -LiteralPath $parts -DestinationPath '%~1\%VSIX%' -Force"
rmdir /s /q "%~1\vsix"
if not exist "%~1\%VSIX%" (
    echo Cannot write %VSIX%.
    exit /b 1
)
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
tar -xf "%ZIP%" >nul 2>nul
if not exist LLVM-C.dll powershell -NoProfile -Command ^
    "Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '.' -Force" >nul 2>nul
if not exist LLVM-C.dll (
    echo Cannot unpack %ZIP%. Extract it here by hand and try again.
    exit /b 1
)
exit /b 0

:compile
call :attempt "GCC"   "gcc"   "gcc %CFLAGS% ada83.c -o %EXE% %LIBS%" && exit /b 0
call :attempt "Clang" "clang" "clang %CFLAGS% --target=x86_64-w64-windows-gnu ada83.c -o %EXE% %LIBS%" && exit /b 0
call :fetch_zig || exit /b 1
call :attempt "Zig" "zig\zig.exe" "zig\zig.exe cc %CFLAGS% -target x86_64-windows-gnu ada83.c -o %EXE% %LIBS%" && exit /b 0
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

:fetch_zig
if exist zig\zig.exe exit /b 0
where zig >nul 2>nul && exit /b 0
echo No C compiler was found. Zig %ZIG_VERSION% is one download and carries
echo everything needed to build ada83.c.
set /p "REPLY=Download it now? [y/N] "
if /i not "%REPLY%"=="y" (
    echo Install MinGW-w64 GCC, Clang or Zig and run this again.
    exit /b 1
)
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
