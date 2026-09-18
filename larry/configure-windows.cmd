@echo off
setlocal

set "VS_VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VS_VCVARS%" (
  echo Visual Studio 2022 Community C++ tools were not found. 1>&2
  exit /b 1
)
call "%VS_VCVARS%" -vcvars_ver=14.38 || exit /b 1

set "NINJA=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%NINJA%" (
  echo Visual Studio Ninja was not found. 1>&2
  exit /b 1
)

cmake -S llvm -B build-release-14.38 -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DLLVM_ENABLE_PROJECTS="clang;lldb" ^
  -DLLVM_TARGETS_TO_BUILD=X86 ^
  -DLLVM_INCLUDE_TESTS=ON ^
  -DLLVM_INCLUDE_EXAMPLES=OFF ^
  -DLLVM_INCLUDE_BENCHMARKS=OFF ^
  -DLLVM_ENABLE_BINDINGS=OFF ^
  -DLLVM_ENABLE_ZLIB=OFF ^
  -DLLVM_ENABLE_ZSTD=OFF ^
  -DLLVM_ENABLE_LIBXML2=OFF ^
  -DLLVM_ENABLE_DIA_SDK=OFF ^
  -DLLVM_ENABLE_TERMINFO=OFF ^
  -DLLDB_ENABLE_PYTHON=OFF ^
  -DLLDB_ENABLE_LUA=OFF ^
  -DLLDB_ENABLE_LIBXML2=OFF ^
  -DLLDB_ENABLE_CURSES=OFF ^
  -DLLDB_ENABLE_LZMA=OFF ^
  -DLLDB_ENABLE_SWIG=OFF ^
  -DLLDB_ENABLE_LIBEDIT=OFF
exit /b %ERRORLEVEL%
