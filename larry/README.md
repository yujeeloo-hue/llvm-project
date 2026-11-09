# Larry lldb-dap for Windows

This branch pins upstream LLVM `llvmorg-23.1.1` (`6dfe1677ab8d`) and ships
adapter `23.1.1-larry.3`.

## Capabilities

- `pdbModuleAllowlist` — attach field. An empty list preserves upstream
  behavior. A non-empty list matches Windows module basenames
  case-insensitively. Every module remains visible and mapped, but denied
  PE/COFF modules cannot locate, download, or open PDBs. DAP module
  serialization also avoids `GetNumCompileUnits()` for denied modules.
- `loadModuleSymbols` — DAP request. Widens the allowlist, then runs
  `target symbols add` for one already-loaded DLL.
- `deferredDebugEvents` — non-allowlisted DLL loads and `OutputDebugString`
  events are handled off the debugger thread. Allowlisted modules stay
  synchronous. The queue is flushed before a breakpoint, a single-step, or a
  second-chance stop.
- `LARRY_LLDB_EVENT_STATS` — when this environment variable names a file, the
  adapter appends a timing summary of Windows debug events. Unset, it writes
  nothing.

## Build

`larry\env-windows.cmd` finds Visual Studio with `vswhere` (VS 2022 or VS 2026).
It uses the MSVC 14.38 toolset when that toolset is installed, otherwise the
installation default. Overrides: `LARRY_VS_ROOT`, `LARRY_MSVC_VER`,
`LARRY_BUILD_DIR`, `LARRY_PYTHON` (CMake's Python 3 interpreter).

```bat
larry\build-windows.cmd
powershell -ExecutionPolicy Bypass -File larry\package-windows.ps1
```

The build is Release, x64 host / X86 target only, `/MD`, `/utf-8`, without
Python, DIA, Lua, libxml2, curses, editline, LZMA, zlib, or zstd. Artifacts
are written to `artifacts/` with a SHA-256 sidecar. The archive contains the
adapter, LLDB runtime/server, Clang resource headers, `VERSION.json`, and the
LLVM license. It does not bundle the VC++ runtime DLLs.

`build*` and `artifacts/` stay untracked. LLVM compilation is separate from
Larry Agent application CI. The zip consumed by Larry Agent is published from
tag `23.1.1-larry.3`.
