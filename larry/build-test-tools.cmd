@echo off
setlocal
call "%~dp0env-windows.cmd" || exit /b 1
if not exist "%LARRY_BUILD_DIR%\build.ninja" (
  call "%~dp0configure-windows.cmd" || exit /b 1
)
cmake --build "%LARRY_BUILD_DIR%" --target lldb --config Release --parallel
exit /b %ERRORLEVEL%
