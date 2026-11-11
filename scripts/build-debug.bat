@echo off
rem ============================================================
rem  Build Debug:
rem    1) platform (Debug): configure + build + install + deploy_product
rem       -> platform\deploy\Debug (the self-contained runtime set)
rem    2) platform (Release) control layer: configure + build only
rem       -> CTL_TL_RM.exe for the Release platform tree. The control layer
rem          has no install rules and is always Release, and the old flow
rem          built it on every run - keep that.
rem    3) REV2 (Debug) -> build\Debug\ (and output\interface\rpc\lib\debug\)
rem  Touches no git - run scripts\update_platform.bat for that.
rem
rem  Usage:
rem    build-debug.bat                  platform + full REV2 build (default)
rem    build-debug.bat -t <target>      platform as usual, then build only
rem                                     that REV2 target (+ its deps)
rem
rem  Why -t exists: VSCode runs a build before every launch (preLaunchTask).
rem  Building ALL REV2 targets would relink the exe of the OTHER
rem  configuration that is still running, and that fails with
rem      LINK : fatal error LNK1168: cannot open <exe> for writing
rem  after which VSCode aborts the launch. So each launch configuration builds
rem  only the target it is about to start (see .vscode\tasks.json).
rem  Only the REV2 build is narrowed: REV2 links against the platform
rem  libraries, so the platform steps must have run first.
rem
rem  The deploy shape (output\exe\ + output\interface\...\release) is
rem  Release-only, so a debug-only run does not produce it (same rule as the
rem  other projects: cmake\istar_install.cmake).
rem
rem  ASCII-only on purpose: see the note in build-release.bat.
rem ============================================================
setlocal
set "HERE=%~dp0"
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "TARGET="

call :main %*
set "RC=%ERRORLEVEL%"

if defined TARGET (
    set "SUMMARY_NOTE=Debug build (single REV2 target: %TARGET%; platform steps unchanged)"
    call "%HERE%private_scripts\_lib.bat" summary %RC% "%ROOT%\build\Debug\%TARGET%.exe"
    exit /b %RC%
)

set "SUMMARY_NOTE=Debug build (platform debug + platform release control + REV2 debug)"

call "%HERE%private_scripts\_lib.bat" summary %RC% ^
    "%ROOT%\build\Debug\GUI_TL_RM_CHIP.exe" ^
    "%ROOT%\build\Debug\GUI_TL_RM_Wafer.exe" ^
    "%ROOT%\build\Debug\libUIRE_CHIPd.dll" ^
    "%ROOT%\build\Debug\libUIRE_Waferd.dll" ^
    "%ROOT%\output\interface\gui\lib\debug\libUIRE_CHIPd.dll" ^
    "--" ^
    "%ROOT%\output\exe\CTL_TL_RM.exe" ^
    "%ROOT%\output\interface\gui\lib\release\libUIRE_CHIP.dll"

exit /b %RC%

:main
cd /d "%ROOT%" || exit /b 1
call "%HERE%private_scripts\_lib.bat" check_qtdir

if /i "%~1"=="-t"       set "TARGET=%~2"
if /i "%~1"=="--target" set "TARGET=%~2"

call "%HERE%private_scripts\_build-platform.bat" Debug             || exit /b 1
call "%HERE%private_scripts\_build-platform.bat" Release --control || exit /b 1

cmake --preset win64         || exit /b 1
if defined TARGET (
    cmake --build --preset debug --target %TARGET% || exit /b 1
    exit /b 0
)
cmake --build --preset debug || exit /b 1
exit /b 0
