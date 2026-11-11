@echo off
rem ============================================================
rem  Sync this project's outputs downstream:
rem    rm protocol  platform\RM\api    -> mctrl\...\RPCModule\include\rm   (A3)
rem    ReData       MainProject\ReData -> mctrl\...\RecipeModule\ReData    (A4)
rem    ReData                          -> gui\...\MCRecipeModule\ReData    (A5)
rem
rem  The engine lives at the top level (1927project\scripts\private_scripts\_sync_ops.bat) so
rem  all cross-project writes follow one table; this is only a forward.
rem  Identical content is skipped WITHOUT touching the target mtime, so
rem  downstream regeneration/compilation is not needlessly retriggered.
rem  Directory edges mirror: files that no longer exist here are deleted there.
rem
rem  Extra arguments are passed through, e.g.:
rem      sync.bat --dry-run      report only, write nothing
rem
rem  ASCII-only on purpose: see the note in build-release.bat.
rem ============================================================
setlocal
set "HERE=%~dp0"
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "TOP=%ROOT%\..\..\engineering\scripts"

if not exist "%TOP%\private_scripts\_sync_ops.bat" (
    echo [SKIP] top-level sync engine not found: %TOP%\private_scripts\_sync_ops.bat
    echo        Nothing was synced. Build the top level, or run private_scripts\_sync_ops.bat there.
    exit /b 0
)

call "%TOP%\private_scripts\_sync_ops.bat" proj re %*
set "RC=%ERRORLEVEL%"
call "%HERE%private_scripts\_lib.bat" summary %RC% ^
    "%ROOT%\..\..\mctrl\MainProject\RPCModule\include\rm\rm_controller.h" ^
    "%ROOT%\..\..\mctrl\MainProject\RecipeModule\ReData\S_Recipe.h" ^
    "--" ^
    "%ROOT%\..\..\gui\MainProject\MCRecipeModule\ReData\S_Recipe.h"
exit /b %RC%
