@echo off
rem ============================================================
rem  update_thirdparty.bat - DEFAULT IMPLEMENTATION.
rem
rem  module\re owns no third-party submodule, so there is nothing to pull.
rem  (module\re\platform is the platform tree, not a third-party repo - it
rem  belongs to update_platform.bat, at this level and at the top level.)
rem  This is not a stub that can fail: like the real implementations in the
rem  other projects it returns 0, so the top level can call every project
rem  the same way, with no special case ("the subclass inherits the base
rem  implementation and returns true").
rem
rem    update_thirdparty.bat           report the fact, succeed
rem    update_thirdparty.bat latest    same as no argument
rem    update_thirdparty.bat show      same, read-only
rem    update_thirdparty.bat -h        this help
rem
rem  ASCII-only on purpose: see the note in build-release.bat.
rem ============================================================
setlocal
set "HERE=%~dp0"
set "MOWN=none - default implementation (succeeds, does nothing)"

rem  Quiet mode: the top level (scripts\update_thirdparty.bat) drives us with
rem  _CHILD_QUIET=1 and owns the summary, so we skip our own RESULT block.
rem  Run this script directly (no _CHILD_QUIET) for the full report.
set "_QUIET="
if "%_CHILD_QUIET%"=="1" set "_QUIET=1"

call :main %*
set "RC=%ERRORLEVEL%"
set "SUMMARY_NOTE=re owns no third-party submodule (default implementation)"
if defined FAILED_SUB set "SUMMARY_NOTE=unexpected argument %FAILED_SUB% - see -h"

if not defined _QUIET call "%HERE%private_scripts\_lib.bat" summary %RC%
exit /b %RC%

:main
set "ARG=%~1"
if /i "%ARG%"=="-h" goto :usage
if /i "%ARG%"=="/?" goto :usage
if /i "%ARG%"=="show" goto :nothing
if "%ARG%"=="" goto :nothing
if /i "%ARG%"=="latest" goto :nothing
echo [ERROR] update_thirdparty.bat: unexpected argument "%ARG%" - third-party has no tag semantics
echo         run "update_thirdparty.bat -h" for the contract
set "FAILED_SUB=%ARG%"
exit /b 1

:nothing
echo [n/a] update_thirdparty: re owns no third-party submodule
exit /b 0

:usage
echo.
echo Usage: update_thirdparty.bat [latest^|show] [-h]
echo.
echo   no argument : pull the third-party submodule ^(= latest^)
echo   latest      : same as no argument
echo   show        : only show the current version, change nothing
echo.
echo   Owns ^(this level^): %MOWN%
echo   Other levels own theirs - this script does not run git on those.
echo   A dirty or diverged worktree fails the run - no change is discarded.
echo.
echo Examples:
echo   update_thirdparty.bat
echo   update_thirdparty.bat show
echo.
exit /b 0
