@echo off
setlocal
cd /d "%~dp0"

set "SOURCE=ada83.c"
set "RUNTIME=ada83-runtime.ada"
set "BUNDLE=ada83-extension.html"
set "MANUAL=manual.md"
set "VSIX=ada83.vsix"
set "ICON=ada83-icon"
set "LIBRARIES=bin-windows.zip"
set "STAGE=bin-windows"
set "ZIG_VERSION=0.16.0"
set "ZIG_NAME=zig-x86_64-windows-%ZIG_VERSION%"
set "ZIG_URL=https://ziglang.org/download/%ZIG_VERSION%/%ZIG_NAME%.zip"

set "TOOLCHAIN_windows=GCC, Clang or Zig"
set "TOOLCHAIN_linux=Zig"
set "TOOLCHAIN_macos=Zig and llvm-lipo"
set "EXECUTABLE_windows=ada83.exe"
set "EXECUTABLE_linux=ada83"
set "EXECUTABLE_macos=ada83"
set "COMPILER_FLAGS_windows=-O2 -Wall -g0 -std=gnu2x"
set "COMPILER_FLAGS_linux=-O3 -Wall -g0 -std=gnu17 -mcpu=baseline"
set "COMPILER_FLAGS_macos=-O3 -Wall -g0 -std=gnu2x"
set "LINK_LIBRARIES_windows=-lm"
set "LINK_LIBRARIES_linux=-lm -lpthread"
set "LINK_LIBRARIES_macos=-lm -lpthread"
set "ARTWORK_windows=%ICON%.ico"
set "ARTWORK_linux=%ICON%.png"
set "ARTWORK_macos=%ICON%.icns"
set "ICON_SOURCE=%ICON%.png"
set "ARCHITECTURES_linux=x86_64-linux-gnu.2.34"
set "ARCHITECTURES_macos=aarch64-macos x86_64-macos"
set "LAUNCHER_linux=ada83.desktop"
set "SHARED_LIBRARIES_windows=*.dll"

call :dispatch %1 %2
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
if "%~1"=="" goto make
echo Invalid parameter - %~1
call :usage
exit /b 1

:usage
echo Makes the Ada 83 compiler.
echo.
echo MAKE [command] [target]
echo.
echo   clean      Deletes the bin-^<target^> folders, any Zig, and the leftovers
echo              of older builds that wrote to this folder.
echo   package    Makes the compiler and %VSIX%, then packs bin-^<target^>.zip.
echo   vsix       Makes %VSIX%, the VS Code extension, only.
echo   help       Displays this help.
echo.
echo   windows    Packages for this machine with %TOOLCHAIN_windows%. The default.
echo   linux      Cross-packages with %TOOLCHAIN_linux%.
echo   macos      Cross-packages with %TOOLCHAIN_macos%.
echo.
echo Everything built lands in bin-^<target^>, so bin-windows\%EXECUTABLE_windows%
echo with no command. Cross-packaging downloads nothing; install the toolchain
echo it names first.
echo.
echo LLVM-C.dll and its companion DLLs are unpacked from %LIBRARIES%
echo and must stay with %EXECUTABLE_windows%, which loads them when it runs.
exit /b 0

:select
set "TARGET=%~1"
if not defined TARGET set "TARGET=windows"
if not defined EXECUTABLE_%TARGET% (
    echo Target is '%TARGET%'; it must be linux, macos or windows.
    exit /b 1
)
call set "EXECUTABLE=%%EXECUTABLE_%TARGET%%%"
call set "COMPILER_FLAGS=%%COMPILER_FLAGS_%TARGET%%%"
call set "LINK_LIBRARIES=%%LINK_LIBRARIES_%TARGET%%%"
call set "ARTWORK=%%ARTWORK_%TARGET%%%"
call set "ARCHITECTURES=%%ARCHITECTURES_%TARGET%%%"
call set "LAUNCHER=%%LAUNCHER_%TARGET%%%"
call set "SHARED_LIBRARIES=%%SHARED_LIBRARIES_%TARGET%%%"
set "ZIP=bin-%TARGET%.zip"
set "STAGE=bin-%TARGET%"
if not exist "%STAGE%" mkdir "%STAGE%"
exit /b 0

:clean
del /q "%EXECUTABLE_windows%" ada83.pdb LLVM-C.dll libffi-8.dll libstdc++-6.dll libzstd.dll ^
    libgcc_s_seh-1.dll libwinpthread-1.dll libxml2-16.dll libiconv-2.dll ^
    zlib1.dll zig.zip bin-*.zip.new "%VSIX%" icon.rc icon.res icon.res.o >nul 2>nul
for %%D in (staging zig bin-linux bin-macos bin-windows) do rmdir /s /q %%D >nul 2>nul
echo Cleaned.
exit /b 0

:make
call :select   || exit /b 1
call :icons
call :native   || exit /b 1
echo Built %STAGE%\%EXECUTABLE%.
exit /b 0

:package
call :select "%~2" || exit /b 1
rmdir /s /q "%STAGE%" >nul 2>nul
mkdir "%STAGE%"
call :vsix  || exit /b 1
call :icons || (
    echo Cannot build the icons. Making archives needs PowerShell 5 or later.
    exit /b 1
)
if defined ARCHITECTURES (
    call :cross || exit /b 1
) else (
    call :native || exit /b 1
)
call :stage   || exit /b 1
call :archive || exit /b 1
rmdir /s /q staging >nul 2>nul
echo Packaged %ZIP%.
exit /b 0

:native
call :require %SOURCE%  || exit /b 1
call :require %RUNTIME% || exit /b 1
call :unpack_llvm       || exit /b 1
call :compile           || exit /b 1
"%STAGE%\%EXECUTABLE%" --version >nul 2>nul || (
    echo %STAGE%\%EXECUTABLE% was built but does not run.
    exit /b 1
)
exit /b 0

:cross
call :require %SOURCE%  || exit /b 1
call :require %RUNTIME% || exit /b 1
call :find_zig || (
    echo packaging for %TARGET% needs zig
    exit /b 1
)
set "SLICES="
set "MANY_SLICES="
for %%A in (%ARCHITECTURES%) do (
    call :slice %%A || exit /b 1
)
if defined MANY_SLICES (
    call :lipo || exit /b 1
) else (
    move /y %SLICES% "%STAGE%\%EXECUTABLE%" >nul
)
exit /b 0

:slice
set "SLICE=%STAGE%\%EXECUTABLE%-%~1"
echo Compiling for %~1 with Zig...
%ZIG% cc %COMPILER_FLAGS% -target %~1 -o %SLICE% %SOURCE% %LINK_LIBRARIES%
if errorlevel 1 exit /b 1
if defined SLICES set "MANY_SLICES=1"
set "SLICES=%SLICES% %SLICE%"
exit /b 0

:lipo
set "LIPO=lipo"
where lipo >nul 2>nul && goto join
set "LIPO=llvm-lipo"
where llvm-lipo >nul 2>nul && goto join
echo joining the %TARGET% slices needs lipo
exit /b 1
:join
%LIPO% -create -output "%STAGE%\%EXECUTABLE%" %SLICES%
if errorlevel 1 exit /b 1
del /q %SLICES% >nul 2>nul
exit /b 0

:stage
copy /y "%RUNTIME%" "%STAGE%\" >nul
if defined LAUNCHER call :launcher
exit /b 0

:icons
if not exist "%ICON_SOURCE%" exit /b 1
if not exist "%STAGE%" mkdir "%STAGE%"
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$Png = [IO.File]::ReadAllBytes((Join-Path $PWD '%ICON_SOURCE%'));" ^
    "$Big = { param($v) [byte[]]@(($v -shr 24) -band 255,($v -shr 16) -band 255," ^
    "                             ($v -shr 8) -band 255,$v -band 255) };" ^
    "$Little = { param($v) [byte[]]@($v -band 255,($v -shr 8) -band 255," ^
    "                                ($v -shr 16) -band 255,($v -shr 24) -band 255) };" ^
    "$Wide = ($Png[16] -shl 24) + ($Png[17] -shl 16) + ($Png[18] -shl 8) + $Png[19];" ^
    "$Tall = ($Png[20] -shl 24) + ($Png[21] -shl 16) + ($Png[22] -shl 8) + $Png[23];" ^
    "$Chunk = @{16='icp4';32='icp5';64='icp6';128='ic07';256='ic08';512='ic09';1024='ic10'}[$Wide];" ^
    "if (-not $Chunk) { $Chunk = 'ic07' };" ^
    "$Stage = Join-Path $PWD '%STAGE%';" ^
    "$Ico = [byte[]]@(0,0,1,0,1,0,($Wide -band 255),($Tall -band 255),0,0,1,0,32,0) +" ^
    "       (^& $Little $Png.Length) + (^& $Little 22) + $Png;" ^
    "if ('%TARGET%' -eq 'windows') {" ^
    "  [IO.File]::WriteAllBytes((Join-Path $Stage '%ICON%.ico'), $Ico) };" ^
    "$Icns = [Text.Encoding]::ASCII.GetBytes('icns') + (^& $Big ($Png.Length + 16)) +" ^
    "        [Text.Encoding]::ASCII.GetBytes($Chunk) + (^& $Big ($Png.Length + 8)) + $Png;" ^
    "if ('%TARGET%' -eq 'macos') {" ^
    "  [IO.File]::WriteAllBytes((Join-Path $Stage '%ICON%.icns'), $Icns) };" ^
    "if ('%TARGET%' -eq 'linux') {" ^
    "  [IO.File]::WriteAllBytes((Join-Path $Stage '%ICON%.png'), $Png) };" ^
    "$Data = $Icns.Length + 4;" ^
    "$Map = [byte[]]::new(24) + [byte[]]@(0,28,0,46,0,0) +" ^
    "       [Text.Encoding]::ASCII.GetBytes('icns') + [byte[]]@(0,0,0,10,191,185,255,255,0,0,0,0);" ^
    "$Fork = (^& $Big 256) + (^& $Big (256 + $Data)) + (^& $Big $Data) + (^& $Big 46) +" ^
    "        [byte[]]::new(240) + (^& $Big $Icns.Length) + $Icns + $Map;" ^
    "$Double = (^& $Big 333319) + (^& $Big 131072) + [byte[]]::new(16) + [byte[]]@(0,2) +" ^
    "          (^& $Big 9) + (^& $Big 50) + (^& $Big 32) +" ^
    "          (^& $Big 2) + (^& $Big 82) + (^& $Big $Fork.Length) +" ^
    "          [byte[]]::new(8) + [byte[]]@(4,0) + [byte[]]::new(22) + $Fork;" ^
    "if ('%TARGET%' -eq 'macos') {" ^
    "  New-Item -ItemType Directory -Force -Path (Join-Path $Stage '__MACOSX') ^> $null;" ^
    "  [IO.File]::WriteAllBytes((Join-Path $Stage '__MACOSX\._ada83'), $Double) }"
if exist "%STAGE%\%ARTWORK%" exit /b 0
exit /b 1

:launcher
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$Entry = '[Desktop Entry]','Type=Application','Name=Ada 83'," ^
    "         'Comment=Ada 83 compiler','Exec=ada83 %%F','Icon=%ICON%'," ^
    "         'Terminal=true','Categories=Development;Building;';" ^
    "[IO.File]::WriteAllText((Join-Path $PWD '%STAGE%\%LAUNCHER%')," ^
    "                        ($Entry -join [char]10) + [char]10)"
if exist "%STAGE%\%LAUNCHER%" exit /b 0
echo Cannot write %LAUNCHER%. Making archives needs PowerShell 5 or later.
exit /b 1

:archive
set "CONTENTS='%STAGE%\%EXECUTABLE%','%STAGE%\%RUNTIME%','%STAGE%\%VSIX%'"
if exist "%STAGE%\%ARTWORK%"    set "CONTENTS=%CONTENTS%,'%STAGE%\%ARTWORK%'"
if defined LAUNCHER             set "CONTENTS=%CONTENTS%,'%STAGE%\%LAUNCHER%'"
if defined SHARED_LIBRARIES     set "CONTENTS=%CONTENTS%,'%STAGE%\%SHARED_LIBRARIES%'"
if exist "%STAGE%\__MACOSX"     set "CONTENTS=%CONTENTS%,'%STAGE%\__MACOSX'"
del /q "%ZIP%.new" >nul 2>nul
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "Compress-Archive -Path %CONTENTS% -DestinationPath '%ZIP%.new' -Force"
if not exist "%ZIP%.new" (
    echo Cannot write %ZIP%. Making archives needs PowerShell 5 or later.
    exit /b 1
)
move /y "%ZIP%.new" "%ZIP%" >nul || (
    echo Cannot replace %ZIP%; another program may be holding it open.
    exit /b 1
)
exit /b 0

:vsix
if not defined TARGET (
    call :select || exit /b 1
)
call :require %BUNDLE% || exit /b 1
del /q "%STAGE%\%VSIX%" >nul 2>nul
rmdir /s /q staging\vsix >nul 2>nul
mkdir staging\vsix\extension\syntaxes
for %%F in ("%MANUAL%" "%ICON%.png") do (
    if exist %%F ( copy /y %%F staging\vsix\extension\ >nul ) else (
        echo %%~F is missing; building %VSIX% without it.
    )
)
call :split
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$Parts = Get-ChildItem -LiteralPath 'staging\vsix' -Force | ForEach-Object FullName;" ^
    "Compress-Archive -LiteralPath $Parts -DestinationPath '%STAGE%\%VSIX%' -Force"
rmdir /s /q staging\vsix >nul 2>nul
if not exist "%STAGE%\%VSIX%" (
    echo Cannot write %VSIX%. Making archives needs PowerShell 5 or later.
    exit /b 1
)
echo Built %STAGE%\%VSIX%.
exit /b 0

:split
setlocal disabledelayedexpansion
set "OUT="
for /f "delims=" %%L in ('findstr /n "^" "%BUNDLE%"') do (
    set "LINE=%%L"
    setlocal enabledelayedexpansion
    set "LINE=!LINE:*:=!"
    set "TAG="
    if "!LINE:~0,7!"=="<script" set "TAG=1"
    if "!LINE!"=="</script>" set "TAG=1"
    if defined TAG (
        endlocal
        set "OUT="
        for /f tokens^=2^ delims^=^" %%N in ("%%L") do call :destination "%%N"
    ) else (
        if defined OUT >>"!OUT!" echo(!LINE!
        endlocal
    )
)
endlocal
exit /b 0

:destination
set "NAME=%~1"
set "OUT=staging\vsix\extension\%NAME%"
if "%NAME:~0,22%"=="extension.vsixmanifest" set "OUT=staging\vsix\%NAME%"
if "%NAME:~0,1%"=="[" set "OUT=staging\vsix\%NAME%"
type nul >"%OUT%"
exit /b 0

:require
if exist "%~1" exit /b 0
echo Cannot find %~1. Run this script from the folder it came in.
exit /b 1

:unpack_llvm
if exist "%STAGE%\LLVM-C.dll" exit /b 0
if not exist "%LIBRARIES%" (
    echo Cannot find LLVM-C.dll or %LIBRARIES%.
    exit /b 1
)
echo Unpacking LLVM-C.dll into %STAGE%...
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "Expand-Archive -LiteralPath '%LIBRARIES%' -DestinationPath 'staging\llvm' -Force;" ^
    "Move-Item 'staging\llvm\*.dll' '%STAGE%' -Force"
rmdir /s /q staging\llvm >nul 2>nul
if exist "%STAGE%\LLVM-C.dll" exit /b 0
echo Cannot unpack %LIBRARIES%. Extract the DLLs into %STAGE% by hand and try again.
exit /b 1

:compile
set "TOOLCHAIN=GCC"
set "COMPILER=gcc"
where gcc >nul 2>nul && goto build
set "TOOLCHAIN=Clang"
set "COMPILER=clang --target=x86_64-w64-windows-gnu"
where clang >nul 2>nul && goto build
call :offer_zig || exit /b 1
set "TOOLCHAIN=Zig"
set "COMPILER=%ZIG% cc -target x86_64-windows-gnu"
:build
call :resource
echo Compiling with %TOOLCHAIN%...
%COMPILER% %COMPILER_FLAGS% %SOURCE% %RESOURCE% -o "%STAGE%\%EXECUTABLE%" %LINK_LIBRARIES%
exit /b %errorlevel%

:resource
set "RESOURCE=icon.res.o"
if defined ZIG set "RESOURCE=icon.res"
del /q icon.rc "%RESOURCE%" >nul 2>nul
if not exist "%STAGE%\%ICON%.ico" call :icons
rem  Resource scripts read a backslash as an escape, so name the icon with
rem  forward slashes; every Windows resource compiler accepts them.
set "ICON_PATH=%STAGE%/%ICON%.ico"
set "ICON_PATH=%ICON_PATH:\=/%"
if exist "%STAGE%\%ICON%.ico" >icon.rc echo 1 ICON "%ICON_PATH%"
if exist icon.rc if defined ZIG %ZIG% rc icon.rc "%RESOURCE%" >nul 2>nul
if exist icon.rc if not defined ZIG windres icon.rc -O coff -o "%RESOURCE%" >nul 2>nul
del /q icon.rc >nul 2>nul
if exist "%RESOURCE%" exit /b 0
set "RESOURCE="
echo Building without the icon; %ARTWORK% could not be compiled in.
exit /b 0

:find_zig
set "ZIG=zig"
where zig >nul 2>nul && exit /b 0
set "ZIG=zig\zig.exe"
if exist "%ZIG%" exit /b 0
set "ZIG="
exit /b 1

:offer_zig
call :find_zig && exit /b 0
echo No C compiler was found. Zig %ZIG_VERSION% is one download that builds %SOURCE%.
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
set "ZIG=zig\zig.exe"
if exist "%ZIG%" exit /b 0
set "ZIG="
echo Cannot download Zig. Install MinGW-w64 GCC or Clang and try again.
exit /b 1
