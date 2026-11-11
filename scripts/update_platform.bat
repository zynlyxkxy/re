@echo off
rem ============================================================
rem  update_platform.bat - align re's PLATFORM tree to a version.
rem  re owns its platform tree; the top level only delegates here
rem  (see scripts\update_platform.bat).
rem
rem    update_platform.bat           align to the highest version tag
rem                                    of b on origin/master (= "latest")
rem    update_platform.bat 2.0.1     align to tag 2.0.1
rem    update_platform.bat latest    same as no argument
rem    update_platform.bat show      show the current version, change nothing
rem    update_platform.bat -h        this help
rem
rem  re's platform tree is nested, and a has NO 2.x tag of its own:
rem    a = platform                        the platform SOURCE repo
rem    b = platform\basiclibrary_output    the shared output library
rem
rem  re's "platform version" IS b's version. So:
rem    b is checked out at the tag (V), and
rem    a is put on the LATEST commit of a's develop whose recorded
rem    submodule pin for basiclibrary_output equals V's commit.
rem  Example: a#1 -> b1.0, a#2 -> b1.0, a#3 -> b1.1.
rem           Target 1.0  =>  a = a#2,  b = 1.0
rem  The mapping is NOT re-derived here. It is done by _platform_commit.ps1,
rem  re's own private helper (submodule-pin archaeology, plus an
rem  "update <ver>" commit-message fallback), which now lives beside this
rem  script in private_scripts\ - %HERE% is this script's own directory.
rem  It resolves b's tag to b's commit internally and prints the newest a
rem  commit whose recorded pin matches, so this script needs no mapping of
rem  its own.
rem
rem  The skip-worktree dance is NOT optional. platform has committed build
rem  artifacts and generator-rewritten files:
rem    RM/lib/rm.dll, RM/lib/rmd.dll        overwritten by every install
rem    RM/code/rm/rmclient/*.cpp/*.h        rewritten (LF) by the RPC generator
rem  Without it the submodule is permanently dirty AND a checkout/pull
rem  refuses with "local changes would be overwritten by merge" (skip-worktree
rem  only hides git status, it does not protect the checkout).
rem  So, in this exact order: un-skip the 6 files -> restore them ->
rem  checkout/update -> skip them again.
rem  Keep the file list and the 4-step order exactly as they are.
rem
rem  If a's pointer changes, commit it in the re repo to bake the version.
rem
rem  ASCII-only on purpose: see the note in build-release.bat.
rem ============================================================
setlocal enabledelayedexpansion
set "HERE=%~dp0"
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "PLATSRC=%ROOT%\platform"
set "PBASIC=%PLATSRC%\basiclibrary_output"
set "PS1=%HERE%private_scripts\_platform_commit.ps1"
set "PLATFILES=RM/lib/rm.dll RM/lib/rmd.dll RM/code/rm/rmclient/rm_client_call.cpp RM/code/rm/rmclient/rm_client_call.h RM/code/rm/rmclient/rm_client_manager.cpp RM/code/rm/rmclient/rm_client_manager.h"
set "MOWN=platform (a) + platform\basiclibrary_output (b)"

rem  Quiet mode: the top level (scripts\update_platform.bat) drives us with
rem  _CHILD_QUIET=1 and owns the summary, so we print no report block of our
rem  own - only the per-mount lines and the [ERROR] lines. Run this script
rem  directly (no _CHILD_QUIET) and the full report is printed as before.
set "_QUIET="
if "%_CHILD_QUIET%"=="1" set "_QUIET=1"

call :main %*
set "RC=%ERRORLEVEL%"
if defined SUB_TAG (
    set "SUMMARY_NOTE=platform tree aligned (b = !SUB_TAG!)"
) else (
    set "SUMMARY_NOTE=platform tree alignment"
)
if defined FAILED_SUB set "SUMMARY_NOTE=failed on !FAILED_SUB! - no change was discarded"

if not defined _QUIET call "%HERE%private_scripts\_lib.bat" summary %RC% ^
    "%PLATSRC%\.git" ^
    "%PBASIC%\.git"
exit /b %RC%

:main
cd /d "%ROOT%" || exit /b 1

set "VER=%~1"
if /i "%VER%"=="-h" goto :usage
if /i "%VER%"=="/?" goto :usage
if /i "%VER%"=="show" goto :show
if "%VER%"=="" set "VER=latest"

where git >nul 2>nul
if errorlevel 1 (
    echo [ERROR] no git found
    exit /b 1
)

echo [1/5] init the a/b submodule tree ...
git submodule update --init --recursive 2>nul
if not exist "%PLATSRC%\.git" (
    echo [ERROR] platform ^(a^) failed to initialize
    set "FAILED_SUB=platform"
    exit /b 1
)
if not exist "%PBASIC%\.git" (
    echo [ERROR] platform\basiclibrary_output ^(b^) failed to initialize
    set "FAILED_SUB=platform\basiclibrary_output"
    exit /b 1
)

rem  ---- dance step 1 + 2: un-skip, then restore the 6 files ----
echo [2/5] un-skip and restore the 6 generated files ...
git -C "%PLATSRC%" update-index --no-skip-worktree %PLATFILES% >nul 2>nul
git -C "%PLATSRC%" checkout -- RM/lib/rm.dll RM/lib/rmd.dll RM/code/rm/rmclient/ >nul 2>nul

rem  ---- dance step 3a: get a onto develop, where the pin mapping looks ----
echo [3/5] put a (platform source) on develop ...
git -C "%PLATSRC%" fetch origin develop || (
    echo [ERROR] platform: git fetch origin develop failed - network or credentials?
    set "FAILED_SUB=platform"
    exit /b 1
)
rem  checkout -B puts the local develop on what we just fetched. A plain
rem  "checkout develop" would leave a stale branch in place - fetch moves only
rem  origin/develop - and the version mapping below reads ONLY local develop,
rem  so a clone that ran this before would fail to map any version newer than
rem  its own stale branch (ERROR: no platform commit corresponds to tag ...).
rem  -B moves just the branch pointer (this branch is the script's own
rem  bookkeeping; a always ends up detached at the mapped commit) and still
rem  refuses to trample uncommitted work.
git -C "%PLATSRC%" checkout -B develop origin/develop 2>nul
if errorlevel 1 (
    echo [ERROR] platform: cannot put a ^(platform^) onto develop - dirty worktree?
    set "FAILED_SUB=platform"
    exit /b 1
)
set "PLAT_BR="
for /f "delims=" %%b in ('git -C "%PLATSRC%" rev-parse --abbrev-ref HEAD 2^>nul') do set "PLAT_BR=%%b"
if /i not "!PLAT_BR!"=="develop" (
    echo [ERROR] platform: cannot get onto develop ^(HEAD is "!PLAT_BR!"^) - dirty worktree?
    set "FAILED_SUB=platform"
    exit /b 1
)

echo [4/5] resolve the version ...
if /i not "%VER%"=="latest" goto :have_ver
git -C "%PBASIC%" fetch --tags origin || (
    echo [ERROR] basiclibrary_output: git fetch failed - network or credentials?
    set "FAILED_SUB=platform\basiclibrary_output"
    exit /b 1
)
set "VER="
rem  --merged origin/master keeps tags made on a test/release branch out
for /f %%t in ('git -C "%PBASIC%" tag --sort=-v:refname --merged origin/master 2^>nul') do (
    if not defined VER set "VER=%%t"
)
if not defined VER (
    echo [ERROR] basiclibrary_output: no version tag reachable from origin/master
    set "FAILED_SUB=platform\basiclibrary_output"
    exit /b 1
)
echo       latest tag of b on master = !VER!
:have_ver

rem  ---- the pin mapping: version -> the commit of a that belongs to it ----
set "PCOMMIT="
for /f "tokens=1,* delims= " %%a in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%" -Tag "!VER!" -Platform "%PLATSRC%" 2^>nul') do (
    if /i "%%a"=="OK" set "PCOMMIT=%%b"
    if /i "%%a"=="ERROR:" echo [ERROR] %%b
    if /i "%%a"=="WARN:" echo [WARN] %%b
)
if not defined PCOMMIT (
    echo [ERROR] cannot map version !VER! to a platform commit - left untouched
    set "FAILED_SUB=platform (version !VER!)"
    exit /b 1
)
echo [info] version !VER! =^> platform commit !PCOMMIT:~0,7!

rem  ---- dance step 3b: check a out at that commit, then b at its pin ----
echo [5/5] checkout a and update b under it ...
git -C "%PLATSRC%" checkout "!PCOMMIT!" || (
    echo [ERROR] platform: git checkout !PCOMMIT! failed - dirty worktree? no change was discarded
    set "FAILED_SUB=platform"
    exit /b 1
)
git -C "%PLATSRC%" submodule update --init --recursive || (
    echo [ERROR] platform: git submodule update failed - check the output above
    set "FAILED_SUB=platform\basiclibrary_output"
    exit /b 1
)

rem  ---- dance step 4: hide the 6 files from git status again ----
git -C "%PLATSRC%" update-index --skip-worktree %PLATFILES% >nul 2>nul

for /f %%h in ('git -C "%PLATSRC%" rev-parse --short HEAD 2^>nul') do echo   platform : %%h ^(detached, belongs to !VER!^)
for /f %%t in ('git -C "%PBASIC%" describe --tags --abbrev^=0 2^>nul') do echo   platform\basiclibrary_output : %%t
set "SUB_TAG=!VER!"

rem  The "[OK] ... aligned" trailer is part of the report, so it is silent
rem  when the top level drives us (_CHILD_QUIET=1): the parent prints the
rem  verdict for this project itself. The per-mount lines above stay.
if not defined _QUIET (
    echo.
    echo [OK] update_platform: platform tree aligned to !VER!
    echo [info] re repo note: if the platform pointer changed, commit it in module\re
    echo        to bake this version ^(a rollback only changes the working tree^).
)
exit /b 0

:show
rem  Quiet mode: the banner and the hint block are report furniture, so the
rem  top level suppresses them (_CHILD_QUIET=1) and prints its own one-line
rem  header instead. The per-mount lines below are the data and always print.
if not defined _QUIET (
    echo.
    echo ============================================================
    echo  module\re platform tree - current versions
    echo ============================================================
)
call :show_branch "%PLATSRC%" "platform (a)"
call :show_one    "%PBASIC%"  "platform\basiclibrary_output (b)"
if not defined _QUIET (
    echo ============================================================
    echo  To just look:        update_platform.bat show
    echo  To chase the newest: update_platform.bat          ^(default, = latest^)
    echo  To pin a version:    update_platform.bat 2.0.1
    echo ============================================================
)
exit /b 0

:show_branch
set "M=%~1"
set "L=%~2"
set "B=n/a"
set "H="
if exist "%M%\.git" (
    for /f %%b in ('git -C "%M%" rev-parse --abbrev-ref HEAD 2^>nul') do set "B=%%b"
    for /f %%h in ('git -C "%M%" rev-parse --short HEAD 2^>nul') do set "H=%%h"
)
echo   %L%  :  %B%  %H%
exit /b 0

:show_one
set "M=%~1"
set "L=%~2"
set "T=no tag"
if exist "%M%\.git" (
    for /f %%t in ('git -C "%M%" describe --tags --abbrev^=0 2^>nul') do set "T=%%t"
) else (
    set "T=not a repo"
)
echo   %L%  :  %T%
exit /b 0

:usage
echo.
echo Usage: update_platform.bat [^<tag^>^|latest^|show] [-h]
echo.
echo   no argument : align to the highest tag on origin/master ^(= latest^)
echo   2.0.1       : align to tag 2.0.1
echo   latest      : same as no argument
echo   show        : only show the current version, change nothing
echo.
echo   Owns ^(this level^): %MOWN%
echo   Other levels own theirs - this script does not run git on those.
echo.
echo   Non-destructive: a plain "git checkout" - a dirty worktree makes
echo   the run fail rather than being silently discarded.
echo.
echo Examples:
echo   update_platform.bat
echo   update_platform.bat 2.0.1
echo   update_platform.bat show
echo.
exit /b 0
