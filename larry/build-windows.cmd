@echo off
setlocal
call "%~dp0configure-windows.cmd" || exit /b 1
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.38 || exit /b 1
cmake --build build-release-14.38 --target lldb-dap lldb-server --config Release --parallel
exit /b %ERRORLEVEL%
