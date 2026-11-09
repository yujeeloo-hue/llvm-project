@echo off
rem Sets up the MSVC x64 environment, NINJA and LARRY_BUILD_DIR for the other
rem larry\*.cmd scripts. Call it without setlocal so the variables persist.
rem
rem Optional overrides:
rem   LARRY_VS_ROOT    Visual Studio installation (default: vswhere -latest)
rem   LARRY_MSVC_VER   MSVC toolset, e.g. 14.38 (default: 14.38 when installed,
rem                    else the installation's default toolset)
rem   LARRY_BUILD_DIR  build directory (default: build-release-<toolset> or
rem                    build-release)
rem   LARRY_PYTHON     Python 3 interpreter passed to CMake

if not defined LARRY_VS_ROOT (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "LARRY_VS_ROOT=%%i"
)
if not defined LARRY_VS_ROOT (
  echo Visual Studio with the C++ x64 tools was not found. 1>&2
  exit /b 1
)

set "VS_VCVARS=%LARRY_VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VS_VCVARS%" (
  echo vcvars64.bat was not found under %LARRY_VS_ROOT%. 1>&2
  exit /b 1
)

if not defined LARRY_MSVC_VER (
  if exist "%LARRY_VS_ROOT%\VC\Tools\MSVC\14.38*" set "LARRY_MSVC_VER=14.38"
)

if defined LARRY_MSVC_VER (
  call "%VS_VCVARS%" -vcvars_ver=%LARRY_MSVC_VER% || exit /b 1
) else (
  call "%VS_VCVARS%" || exit /b 1
)

set "NINJA=%LARRY_VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%NINJA%" (
  echo Visual Studio Ninja was not found. 1>&2
  exit /b 1
)

if not defined LARRY_BUILD_DIR (
  if defined LARRY_MSVC_VER (
    set "LARRY_BUILD_DIR=build-release-%LARRY_MSVC_VER%"
  ) else (
    set "LARRY_BUILD_DIR=build-release"
  )
)
exit /b 0
