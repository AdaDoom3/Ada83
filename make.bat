@echo off
setlocal
cd /d "%~dp0"

set "EXE=ada83.exe"
set "ZIP=bin-windows.zip"
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
echo   clean      Deletes ada83.exe, the LLVM DLLs and the downloaded Zig.
echo   package    Makes ada83.exe and ada83.vsix, then repacks bin-windows.zip.
echo   vsix       Makes ada83.vsix, the VS Code extension, only.
echo   help       Displays this help.
echo.
echo Makes with GCC, Clang or Zig, whichever is found first.
echo.
echo LLVM-C.dll and its companion DLLs are unpacked from bin-windows.zip
echo and must stay with ada83.exe, which loads them when it runs.
exit /b 0

:clean
del /q "%EXE%" LLVM-C.dll libffi-8.dll libstdc++-6.dll libgcc_s_seh-1.dll ^
    libwinpthread-1.dll libxml2-16.dll libiconv-2.dll libzstd.dll zlib1.dll ^
    zig.zip "%ZIP%.new" "%VSIX%" >nul 2>nul
rmdir /s /q zig vsix >nul 2>nul
echo Cleaned.
exit /b 0

:make
call :require ada83.c           || exit /b 1
call :require ada83-runtime.ada || exit /b 1
call :unpack_llvm               || exit /b 1
call :compile                   || exit /b 1
call :verify                    || exit /b 1
echo Built %EXE%.
exit /b 0

REM The distributable: ada83.exe, the DLLs it loads, and ada83-runtime.ada,
REM which ada83.exe looks for beside itself. Without the runtime in the archive
REM an unpacked compiler cannot build anything that withs the standard library.
REM The extension is one file: the manifest, grammar, language configuration,
REM snippets and the two packaging files ride in ada83-extension.js as line
REM comments. Splitting them out gives the layout VS Code installs.
:vsix
if exist vsix rmdir /s /q vsix
mkdir vsix\extension\syntaxes
copy /y ada83-extension.js vsix\extension\ >nul
del /q "%VSIX%" >nul 2>nul
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$out = $null;" ^
    "foreach ($line in Get-Content -LiteralPath 'ada83-extension.js') {" ^
    "  if ($line -eq '//== end') { $out = $null; continue }" ^
    "  if ($line -like '//== *') {" ^
    "    $name = $line.Substring(5);" ^
    "    $root = ($name -eq 'extension.vsixmanifest') -or $name.StartsWith('[');" ^
    "    $out = Join-Path 'vsix' ($(if ($root) { $name } else { \"extension\$name\" }));" ^
    "    New-Item -ItemType File -Force -Path $out | Out-Null; continue }" ^
    "  if ($line -like '//= *') { continue }" ^
    "  if ($out -and $line.StartsWith('//')) {" ^
    "    Add-Content -LiteralPath $out -Value $line.Substring(2) } }" ^
    "$parts = Get-ChildItem -LiteralPath 'vsix' -Force | ForEach-Object FullName;" ^
    "Compress-Archive -LiteralPath $parts -DestinationPath '%VSIX%' -Force"
rmdir /s /q vsix
if not exist "%VSIX%" (
    echo Cannot write %VSIX%.
    exit /b 1
)
echo Built %VSIX%.
exit /b 0

REM The distributable: ada83.exe, the DLLs it loads, and ada83-runtime.ada,
REM which ada83.exe looks for beside itself. Without the runtime in the archive
REM an unpacked compiler cannot build anything that withs the standard library.
REM A .vsix is a zip holding the manifest and sources under extension\, with
REM the two packaging files at the root, so no tooling beyond the shell is
REM needed to build one.
:vsix
if exist vsix rmdir /s /q vsix
mkdir vsix\extension\syntaxes
copy /y vscode\package.json vsix\extension\ >nul
copy /y ada83-extension.js vsix\extension\ >nul
copy /y vscode\language-configuration.json vsix\extension\ >nul
copy /y vscode\snippets.json vsix\extension\ >nul
copy /y vscode\syntaxes\ada83.tmLanguage.json vsix\extension\syntaxes\ >nul
copy /y vscode\extension.vsixmanifest vsix\ >nul
copy /y "vscode\[Content_Types].xml" vsix\ >nul
del /q "%VSIX%" >nul 2>nul
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$parts = Get-ChildItem -LiteralPath 'vsix' -Force | ForEach-Object FullName;" ^
    "Compress-Archive -LiteralPath $parts -DestinationPath '%VSIX%' -Force"
rmdir /s /q vsix
if not exist "%VSIX%" (
    echo Cannot write %VSIX%.
    exit /b 1
)
echo Built %VSIX%.
exit /b 0

:package
call :make || exit /b 1
call :vsix || exit /b 1
echo Packing %ZIP%...
REM Build the new archive alongside the old one; the DLLs unpacked by :make are
REM the only copy on disk, so %ZIP% is only replaced once the new one is written.
del /q "%ZIP%.new" >nul 2>nul
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$names = @('%EXE%','ada83-runtime.ada','%VSIX%') + (Get-ChildItem *.dll | ForEach-Object Name);" ^
    "Compress-Archive -LiteralPath $names -DestinationPath '%ZIP%.new' -Force"
if not exist "%ZIP%.new" (
    echo Cannot write %ZIP%.
    exit /b 1
)
move /y "%ZIP%.new" "%ZIP%" >nul || exit /b 1
echo Packaged %ZIP%.
exit /b 0

:require
if exist "%~1" exit /b 0
echo Cannot find %~1. Run this script from the folder it came in.
exit /b 1

:unpack_llvm
if exist LLVM-C.dll exit /b 0
if not exist bin-windows.zip (
    echo Cannot find LLVM-C.dll or bin-windows.zip.
    exit /b 1
)
echo Unpacking LLVM-C.dll...
tar -xf bin-windows.zip >nul 2>nul
if not exist LLVM-C.dll (
    powershell -NoProfile -Command ^
        "Expand-Archive -LiteralPath 'bin-windows.zip' -DestinationPath '.' -Force" >nul 2>nul
)
if not exist LLVM-C.dll (
    echo Cannot unpack bin-windows.zip. Extract it here by hand and try again.
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
