@echo off
rem ============================================================
rem  Remove this project's build tree and deploy shape (clean state).
rem
rem    clean.bat            -> build\ + output\
rem    clean.bat platform   -> also wipe the platform submodule's trees
rem                            (platform\build, \deploy, \output)
rem
rem  The platform build tree is NOT touched by default: it is the platform
rem  submodule's own incremental build (several minutes to redo) and not ours
rem  to throw away on every clean.
rem
rem  ASCII-only on purpose: see the note in build-release.bat.
rem ============================================================
setlocal
set "HERE=%~dp0"
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "CLEAN_PLATFORM=0"

call :main %*
set "RC=%ERRORLEVEL%"

if "%CLEAN_PLATFORM%"=="1" (
    set "SUMMARY_NOTE=build\ + output\ + platform build tree removed"
) else (
    set "SUMMARY_NOTE=build\ and output\ removed"
)

call "%HERE%private_scripts\_lib.bat" summary %RC%
exit /b %RC%

:main
cd /d "%ROOT%" || exit /b 1

if exist "%ROOT%\build"  rmdir /s /q "%ROOT%\build"  || exit /b 1
if exist "%ROOT%\output" rmdir /s /q "%ROOT%\output" || exit /b 1

if /i "%~1"=="platform" set "CLEAN_PLATFORM=1"
if "%CLEAN_PLATFORM%"=="1" (
    echo [INFO] removing the platform submodule build tree ...
    if exist "%ROOT%\platform\build"  rmdir /s /q "%ROOT%\platform\build"  || exit /b 1
    if exist "%ROOT%\platform\deploy" rmdir /s /q "%ROOT%\platform\deploy" || exit /b 1
    if exist "%ROOT%\platform\output" rmdir /s /q "%ROOT%\platform\output" || exit /b 1
)
exit /b 0
