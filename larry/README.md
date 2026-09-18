# Larry lldb-dap for Windows

This branch pins upstream LLVM `llvmorg-23.1.1` and adds one attach field:

```json
{
  "pid": 1234,
  "pdbModuleAllowlist": ["UnrealEditor-MyGame-Win64-DebugGame.dll"]
}
```

An empty list preserves upstream behavior. A non-empty list matches Windows
module basenames case-insensitively. Every module remains visible and mapped,
but denied PE/COFF modules cannot locate, download, or open PDBs. DAP module
serialization also avoids `GetNumCompileUnits()` for denied modules.

## Build

Run from a Visual Studio 2022 installation with MSVC 14.38 and Windows SDK:

```bat
larry\build-windows.cmd
powershell -ExecutionPolicy Bypass -File larry\package-windows.ps1
```

The build is Release, x64 host/X86 target only, `/MD`, without Python, DIA,
Lua, libxml2, curses, editline, LZMA, zlib, or zstd. Artifacts are written to
`artifacts/` with a SHA-256 sidecar. The archive contains the adapter, LLDB
runtime/server, Clang resource headers, `VERSION.json`, and the LLVM license.

LLVM compilation is intentionally separate from Larry Agent application CI.
Publish the zip and checksum under tag `23.1.1-larry.1`.
