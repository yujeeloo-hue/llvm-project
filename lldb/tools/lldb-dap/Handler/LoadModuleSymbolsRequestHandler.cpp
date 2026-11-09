//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DAP.h"
#include "DAPError.h"
#include "RequestHandler.h"
#include "lldb/API/SBCommandInterpreter.h"
#include "lldb/API/SBCommandReturnObject.h"
#include "lldb/API/SBFileSpec.h"
#include "lldb/API/SBFrame.h"
#include "lldb/API/SBModule.h"
#include "lldb/API/SBModuleSpec.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBThread.h"
#include "lldb/Utility/UUID.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <algorithm>
#include <string>
#include <vector>

using namespace lldb_dap::protocol;
namespace lldb_dap {

namespace {

std::string PathOf(const lldb::SBFileSpec &spec) {
  if (!spec.IsValid())
    return {};
  std::vector<char> buffer(4096);
  const uint32_t length = spec.GetPath(buffer.data(), buffer.size());
  return std::string(buffer.data(), std::min<size_t>(length, buffer.size()));
}

/// LLDB command-line quoting: backslashes and quotes are escapes inside "".
std::string Quote(llvm::StringRef value) {
  std::string quoted = "\"";
  for (char c : value) {
    if (c == '\\' || c == '"')
      quoted += '\\';
    quoted += c;
  }
  quoted += '"';
  return quoted;
}

bool RunCommand(DAP &dap, const std::string &command, std::string &output) {
  lldb::SBCommandReturnObject result;
  dap.debugger.GetCommandInterpreter().HandleCommand(command.c_str(), result);
  const char *text = result.Succeeded() ? result.GetOutput() : result.GetError();
  output = text ? llvm::StringRef(text).trim().str() : std::string();
  return result.Succeeded();
}

lldb::SBModule FindModuleByName(lldb::SBTarget &target, llvm::StringRef name) {
  const llvm::StringRef wanted = llvm::sys::path::filename(name);
  for (uint32_t i = 0, n = target.GetNumModules(); i < n; ++i) {
    lldb::SBModule module = target.GetModuleAtIndex(i);
    const char *filename = module.GetFileSpec().GetFilename();
    if (!filename)
      continue;
    const llvm::StringRef file(filename);
    if (file.equals_insensitive(wanted) ||
        llvm::sys::path::stem(file).equals_insensitive(wanted))
      return module;
  }
  return lldb::SBModule();
}

bool IsPDBPath(llvm::StringRef path) {
  return path.ends_with_insensitive(".pdb");
}

} // namespace

/// Larry extension. Attach-time `pdbModuleAllowlist` keeps engine PDBs closed;
/// a call-stack click asks for exactly one more module. The module is added
/// to the global allowlist and its adjacent PDB attached. A module that
/// already has its PDB is left alone: LLDB keeps every replaced symbol file
/// alive, so reloading would only leak.
llvm::Expected<LoadModuleSymbolsResponseBody>
LoadModuleSymbolsRequestHandler::Run(
    const LoadModuleSymbolsArguments &args) const {
  LoadModuleSymbolsResponseBody body;

  lldb::SBFrame frame;
  lldb::SBModule module;
  if (args.frameId) {
    frame = dap.GetLLDBFrame(*args.frameId);
    if (frame.IsValid())
      module = frame.GetModule();
  }
  if (!module.IsValid() && !args.moduleId.empty()) {
    llvm::SmallVector<uint8_t, 20> uuid_bytes;
    if (lldb_private::UUID::DecodeUUIDBytesFromString(args.moduleId,
                                                      uuid_bytes)
            .empty()) {
      lldb::SBModuleSpec spec;
      spec.SetUUIDBytes(uuid_bytes.data(), uuid_bytes.size());
      module = dap.target.FindModule(spec);
    }
  }
  if (!module.IsValid() && !args.moduleName.empty())
    module = FindModuleByName(dap.target, args.moduleName);
  if (!module.IsValid())
    return llvm::make_error<DAPError>("module not found");

  const char *filename = module.GetFileSpec().GetFilename();
  if (!filename || !*filename)
    return llvm::make_error<DAPError>("module has no file name");
  body.moduleName = filename;

  if (dap.IsPDBModuleAllowed(module)) {
    const std::string symbol_file = PathOf(module.GetSymbolFileSpec());
    if (IsPDBPath(symbol_file)) {
      body.alreadyLoaded = true;
      body.symbolStatus = "Symbols loaded.";
      body.symbolFilePath = symbol_file;
      return body;
    }
  } else {
    // Only reachable with a non-empty allowlist, so widening it never turns
    // an unrestricted session into a restricted one.
    dap.pdb_module_allowlist.insert(llvm::StringRef(body.moduleName).lower());
    std::string command = "settings set -g target.pdb-module-allowlist";
    for (const auto &entry : dap.pdb_module_allowlist)
      command += " " + Quote(entry.getKey());
    std::string output;
    if (!RunCommand(dap, command, output))
      return llvm::make_error<DAPError>(
          "failed to widen pdb-module-allowlist: " + output);
  }

  const std::string module_path = PathOf(module.GetFileSpec());
  llvm::SmallString<256> pdb_path(module_path);
  llvm::sys::path::replace_extension(pdb_path, ".pdb");

  std::string output;
  bool added = false;
  if (!module_path.empty() && llvm::sys::fs::exists(pdb_path)) {
    added = RunCommand(dap,
                       "target symbols add --shlib " + Quote(module_path) +
                           " " + Quote(pdb_path),
                       output);
  } else {
    output = "adjacent PDB not found (" + pdb_path.str().str() + ")";
  }
  if (!added && frame.IsValid()) {
    // `--frame` acts on the selected frame and lets the symbol locators find
    // a PDB that is not next to the module.
    lldb::SBThread thread = frame.GetThread();
    thread.GetProcess().SetSelectedThread(thread);
    thread.SetSelectedFrame(frame.GetFrameID());
    std::string frame_output;
    added = RunCommand(dap, "target symbols add --frame", frame_output);
    if (!added && !frame_output.empty())
      output += "; " + frame_output;
  }

  const std::string symbol_file = PathOf(module.GetSymbolFileSpec());
  if (added && IsPDBPath(symbol_file)) {
    body.loaded = true;
    body.symbolStatus = "Symbols loaded.";
    body.symbolFilePath = symbol_file;
  } else {
    if (output.empty())
      output = "Could not load symbols for " + std::string(body.moduleName);
    body.symbolStatus = output;
  }
  return body;
}

} // namespace lldb_dap
