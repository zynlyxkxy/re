@echo off
rem ============================================================
rem  Internal helper: build the platform submodule for one config.
rem  Not a command of its own - called by build-release.bat / build-debug.bat
rem  (and directly by the top-level orchestration).
rem
rem  build-platform.bat <Debug|Release> [--control]
rem      --control : configure + build only (no install, no deploy_product).
rem                  That is exactly what the control layer (CTL_TL_RM) needs;
rem                  it has no install rules of its own and is always Release.
rem
rem  The platform is a SEPARATE CMake project: own presets, own install tree
rem  (platform\output), own deploy_product.bat. We only consume its products,
rem  so it is built here and not from our CMakeLists.
rem
rem  Order matters (identical to the old build.bat):
rem    configure -> build -> install -> deploy_product.bat
rem  deploy_product.bat flattens platform\output\<cfg>\X86_64\Windows into
rem  platform\deploy\<cfg>, which is what the runtime staging and the
rem  output\gui_* packages copy.
rem
rem  ASCII-only on purpose: see the note in build-release.bat.
rem ============================================================
setlocal
set "HERE=%~dp0"
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"

set "CFG=%~1"
set "CONTROL=0"
if /i "%~2"=="--control" set "CONTROL=1"

if /i "%CFG%"=="" (
    echo [ERROR] build-platform.bat: missing config ^(Debug^|Release^)
    exit /b 1
)
if /i not "%CFG%"=="Debug" if /i not "%CFG%"=="Release" (
    echo [ERROR] build-platform.bat: unknown config "%CFG%"
    exit /b 1
)

call :qt_prefix || exit /b 1
call :main || exit /b 1
exit /b 0

:main
rem The platform preset reads CMAKE_PREFIX_PATH from the environment
rem ("CMAKE_PREFIX_PATH": "$env{CMAKE_PREFIX_PATH}"), so export it here.
set "CMAKE_PREFIX_PATH=%QT_PREFIX%"

pushd "%ROOT%\platform" || exit /b 1

echo.
echo --- platform %CFG%: configure ---
rem Always configure: the platform's RM/CMakeLists.txt regenerates RM/code/** and
rem RM/code/CMakeLists.txt at configure time, and that generated CMakeLists.txt is
rem a CMake input. Skipping configure breaks the equilibrium (ZERO_CHECK would
rem re-run configure in the middle of the build, and the generator can crash), so
rem do NOT try to skip it here.
cmake --preset %CFG%_X86_64_Windows || (
    echo [ERROR] platform %CFG% configure failed
    popd
    exit /b 1
)

echo --- platform %CFG%: build ---
cmake --build --preset %CFG%_X86_64_Windows --config %CFG% --parallel 8 || (
    echo [ERROR] platform %CFG% build failed
    popd
    exit /b 1
)

if "%CONTROL%"=="1" (
    echo [OK] platform %CFG% control layer built ^(no install^)
    popd
    exit /b 0
)

echo --- platform %CFG%: install ---
cmake --build --preset %CFG%_X86_64_Windows --config %CFG% --target install --parallel 8 || (
    echo [ERROR] platform %CFG% install failed
    popd
    exit /b 1
)
popd || exit /b 1

echo --- platform %CFG%: deploy_product ---
call "%ROOT%\platform\deploy_product.bat" %CFG% || (
    echo [ERROR] platform %CFG% deploy failed
    exit /b 1
)
echo [OK] platform %CFG% built + installed + deployed
exit /b 0

rem ============================================================
rem  Qt prefix. QTDIR wins, otherwise probe the usual install locations.
rem  This mirrors the probe list in cmake\istar_common.cmake - the platform's
rem  preset cannot use our CMake-side probe, it needs the environment variable.
rem ============================================================
:qt_prefix
set "QT_PREFIX="
if defined QTDIR if exist "%QTDIR%\lib\cmake\Qt5" set "QT_PREFIX=%QTDIR%"
if not defined QT_PREFIX if exist "D:\Qt\Qt5.12.12\5.12.12\msvc2017_64\lib\cmake\Qt5" set "QT_PREFIX=D:\Qt\Qt5.12.12\5.12.12\msvc2017_64"
if not defined QT_PREFIX if exist "C:\Qt\Qt5.12.12\5.12.12\msvc2017_64\lib\cmake\Qt5" set "QT_PREFIX=C:\Qt\Qt5.12.12\5.12.12\msvc2017_64"
if not defined QT_PREFIX if exist "D:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5" set "QT_PREFIX=D:\Qt\5.15.2\msvc2019_64"
if not defined QT_PREFIX if exist "C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5" set "QT_PREFIX=C:\Qt\5.15.2\msvc2019_64"
if not defined QT_PREFIX (
    echo [ERROR] Qt5 not found - the platform cannot be built without it.
    echo         set QTDIR to your Qt prefix, e.g.
    echo         setx QTDIR "D:\Qt\Qt5.12.12\5.12.12\msvc2017_64"
    exit /b 1
)
echo [INFO] Qt prefix: %QT_PREFIX%
exit /b 0
