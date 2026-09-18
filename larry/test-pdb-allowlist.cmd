@echo off
setlocal
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.38 || exit /b 1

set "OUT=%~dp0..\build-release-14.38\larry-test"
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /Zi /Od /EHsc /Fo:"%OUT%\game.obj" /Fd:"%OUT%\game-compiler.pdb" /Fe:"%OUT%\game.exe" "%~dp0..\lldb\test\Shell\SymbolFile\NativePDB\pdb-module-allowlist.cpp" /link /PDB:"%OUT%\game.pdb" || exit /b 1
cl /nologo /Zi /Od /EHsc /Fo:"%OUT%\attach-target.obj" /Fd:"%OUT%\attach-target-compiler.pdb" /Fe:"%OUT%\attach-target.exe" "%~dp0attach-target.cpp" /link /PDB:"%OUT%\attach-target.pdb" || exit /b 1

set "LLDB=%~dp0..\build-release-14.38\bin\lldb.exe"
"%LLDB%" -O "settings set target.pdb-module-allowlist other.dll" -f "%OUT%\game.exe" -o "image lookup -n main" -o quit > "%OUT%\denied.txt" || exit /b 1
findstr /C:"Summary:" "%OUT%\denied.txt" >nul && (
  type "%OUT%\denied.txt"
  exit /b 1
)

"%LLDB%" -O "settings set target.pdb-module-allowlist GAME.EXE" -f "%OUT%\game.exe" -o "image lookup -n main" -o quit > "%OUT%\allowed.txt" || exit /b 1
findstr /C:"1 match found" "%OUT%\allowed.txt" >nul || (
  type "%OUT%\allowed.txt"
  exit /b 1
)

"%LLDB%" -f "%OUT%\game.exe" -o "image lookup -n main" -o quit > "%OUT%\empty.txt" || exit /b 1
findstr /C:"1 match found" "%OUT%\empty.txt" >nul || (
  type "%OUT%\empty.txt"
  exit /b 1
)

echo PDB allowlist denied, case-insensitive allowed, and empty-list compatibility passed.
