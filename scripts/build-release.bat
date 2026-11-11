@echo off
rem ============================================================
rem  Build Release:
rem    1) platform (Release): configure + build + install + deploy_product
rem    2) REV2 (Release)      -> build\Release\ + output\exe\ + output\interface\
rem  Touches no git - run scripts\update_platform.bat for that.
rem
rem  Usage:
rem    build-release.bat                platform + full REV2 build (default)
rem    build-release.bat -t <target>    platform as usual, then build only
rem                                     that REV2 target (+ its deps)
rem
rem  -t is for VSCode launch configurations: building ALL REV2 targets would
rem  relink the exe of the other configuration that is still running ->
rem  LNK1168 -> the launch is aborted. See the same note in build-debug.bat.
rem  Only the REV2 build is narrowed: REV2 links against the platform
rem  libraries, so the platform step must have run first.
rem
rem  NOTE: .bat files must stay ASCII-only. cmd parses them with the
rem  console code page (GBK/936 here), so UTF-8 Chinese comments can
rem  swallow the line ending and corrupt the next line - depending on
rem  byte alignment. Chinese explanations live in the cmake\istar_*.cmake
rem  files and CMakeLists.txt.
rem ============================================================
setlocal
set "HERE=%~dp0"
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "TARGET="

call :main %*
set "RC=%ERRORLEVEL%"

if defined TARGET (
    set "SUMMARY_NOTE=Release build (single REV2 target: %TARGET%; platform steps unchanged)"
    call "%HERE%private_scripts\_lib.bat" summary %RC% "%ROOT%\build\Release\%TARGET%.exe"
    exit /b %RC%
)

set "SUMMARY_NOTE=Release build (platform + REV2)"

call "%HERE%private_scripts\_lib.bat" summary %RC% ^
    "%ROOT%\build\Release\GUI_TL_RM_CHIP.exe" ^
    "%ROOT%\build\Release\GUI_TL_RM_Wafer.exe" ^
    "%ROOT%\build\Release\libUIRE_CHIP.dll" ^
    "%ROOT%\build\Release\libUIRE_Wafer.dll" ^
    "%ROOT%\output\exe\CTL_TL_RM.exe" ^
    "%ROOT%\output\exe\rm.dll" ^
    "%ROOT%\output\interface\gui\lib\release\libUIRE_CHIP.dll" ^
    "%ROOT%\output\interface\rpc\lib\release\rm.dll" ^
    "--" ^
    "%ROOT%\output\interface\gui\lib\debug\libUIRE_CHIPd.dll"

exit /b %RC%

:main
cd /d "%ROOT%" || exit /b 1
call "%HERE%private_scripts\_lib.bat" check_qtdir

rem output\ comes from the "deploy" target that is part of the default build
rem (see cmake\istar_install.cmake), so no separate cmake --install here.
call "%HERE%private_scripts\_build-platform.bat" Release || exit /b 1

if /i "%~1"=="-t"       set "TARGET=%~2"
if /i "%~1"=="--target" set "TARGET=%~2"

cmake --preset win64           || exit /b 1
if defined TARGET (
    cmake --build --preset release --target %TARGET% || exit /b 1
    exit /b 0
)
cmake --build --preset release || exit /b 1
exit /b 0
