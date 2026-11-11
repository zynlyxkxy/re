@echo off
rem ============================================================
rem  Internal helpers. NOT a command - never run this directly.
rem
rem  The command scripts one level up use it like this:
rem      call "%~dp0private_scripts\_lib.bat" require_qtdir || exit /b 1
rem      call "%~dp0private_scripts\_lib.bat" summary %RC% "req" ... "--" "opt" ...
rem
rem  NOTE: `call other.bat :label` does NOT jump to a label - cmd runs the
rem  file from the top and treats ":label" as a plain argument. So the name
rem  is dispatched here explicitly.
rem
rem  Why this file exists: every command must end with a summary and must
rem  NOT close by itself, whether it succeeded or failed. Keeping that in one
rem  place keeps the format identical everywhere.
rem
rem  _SCRIPT_NO_HOLD=1 -> skip the pause (a parent script is driving us)
rem  Full rule set: scripts\private_scripts\_common.bat :hold - the same gate on purpose,
rem  so a top-level script and a project script behave identically.
rem
rem  ASCII-only on purpose: see the note in build-release.bat.
rem ============================================================
setlocal
set "_FN=%~1"
if "%_FN%"=="" (
    echo [ERROR] _lib.bat: no helper name given
    exit /b 1
)
if /i "%_FN%"=="check_qtdir"   goto :dispatch
if /i "%_FN%"=="require_qtdir" goto :dispatch
if /i "%_FN%"=="summary"       goto :dispatch
echo [ERROR] _lib.bat: unknown helper "%_FN%"
exit /b 1

:dispatch
shift
goto :%_FN%

rem QTDIR is OPTIONAL: cmake/istar_common.cmake probes the usual Qt locations
rem anyway, so a machine that just has Qt installed works with no setup at all.
rem This is only a heads-up. (require_qtdir is kept as an alias.)
:check_qtdir
:require_qtdir
if not defined QTDIR (
    echo [note] QTDIR is not set - CMake will auto-detect Qt.
    echo        To pin a specific Qt:  setx QTDIR "D:\Qt\Qt5.12.12\5.12.12\msvc2017_64"
)
exit /b 0

rem ============================================================
rem  summary <exitcode> [required path...] [-- optional path...]
rem
rem  required : must exist after a successful run; a missing one is flagged
rem  optional : may legitimately be missing (e.g. the debug libraries before
rem             build-debug.bat has ever run) - listed, not treated as an error
rem ============================================================
:summary
setlocal enabledelayedexpansion
set "_RC=%~1"
shift
set "_REQ="
set "_OPT="
set "_SEP="

:sum_collect
if "%~1"=="" goto :sum_print
if "%~1"=="--" (
    set "_SEP=1"
    shift
    goto :sum_collect
)
if defined _SEP (
    set "_OPT=!_OPT! "%~1""
) else (
    set "_REQ=!_REQ! "%~1""
)
shift
goto :sum_collect

:sum_print
set "_MISS=0"
echo.
echo ============================================================
if "%_RC%"=="0" (
    echo  RESULT : OK
) else (
    echo  RESULT : FAILED  ^(exit code %_RC%^)
)
if defined SUMMARY_NOTE echo  %SUMMARY_NOTE%

if not "%_RC%"=="0" (
    echo.
    echo  Look at the [ERROR] lines above to find the failing step,
    echo  fix it, then run this script again.
    goto :sum_tail
)

if defined _REQ (
    echo.
    echo  Produced:
    for %%P in (!_REQ!) do (
        if exist "%%~P" (
            echo    [OK]   %%~P
        ) else (
            echo    [MISS] %%~P
            set "_MISS=1"
        )
    )
)
if defined _OPT (
    echo.
    echo  Optional ^(not produced by this script^):
    for %%P in (!_OPT!) do (
        if exist "%%~P" (
            echo    [have] %%~P
        ) else (
            echo    [none] %%~P
        )
    )
)

:sum_tail
if "!_MISS!"=="1" (
    echo.
    echo  WARNING: a file that should have been produced is missing ^(see [MISS]^).
)
echo ============================================================

call :hold
exit /b 0

rem ============================================================
rem  hold - same gate as scripts\private_scripts\_common.bat :hold. Hold by default, skip
rem  when the CALLER set _SCRIPT_NO_HOLD=1, and skip when this does not look
rem  like an interactive launch: Windows starts a double-clicked .bat as
rem  `cmd.exe /c "...bat"`, so a "/c" shows up in %cmdcmdline%, while a
rem  script typed into an already open console has none. The value is piped
rem  straight into findstr because it carries its own quotes.
rem ============================================================
:hold
if "%_SCRIPT_NO_HOLD%"=="1" exit /b 0
echo %cmdcmdline%| findstr /i /c:"/c" >nul
if errorlevel 1 exit /b 0
echo.
pause
exit /b 0
