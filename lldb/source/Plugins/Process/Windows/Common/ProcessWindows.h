//===-- ProcessWindows.h ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_Plugins_Process_Windows_Common_ProcessWindows_H_
#define liblldb_Plugins_Process_Windows_Common_ProcessWindows_H_

#include "lldb/Host/windows/PseudoConsole.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Status.h"
#include "lldb/lldb-forward.h"

#include "Plugins/DynamicLoader/Windows-DYLD/DynamicLoaderWindowsDYLD.h"
#include "ProcessDebugger.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace lldb_private {

class HostProcess;

class ProcessWindows : public Process, public ProcessDebugger {
public:
  // Static functions.
  static lldb::ProcessSP CreateInstance(lldb::TargetSP target_sp,
                                        lldb::ListenerSP listener_sp,
                                        const FileSpec *,
                                        bool can_connect);

  static void Initialize();

  static void Terminate();

  static llvm::StringRef GetPluginNameStatic() { return "windows"; }

  static llvm::StringRef GetPluginDescriptionStatic();

  ~ProcessWindows();

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  Status EnableBreakpointSite(BreakpointSite *bp_site) override;
  Status DisableBreakpointSite(BreakpointSite *bp_site) override;

  bool ShouldUseDelayedBreakpoints() const override { return false; }

  Status DoDetach(bool keep_stopped) override;
  Status DoLaunch(Module *exe_module, ProcessLaunchInfo &launch_info) override;
  Status DoAttachToProcessWithID(
      lldb::pid_t pid,
      const lldb_private::ProcessAttachInfo &attach_info) override;
  Status DoResume(lldb::RunDirection direction) override;
  Status DoDestroy() override;
  Status DoHalt(bool &caused_stop) override;

  void DidLaunch() override;
  void DidAttach(lldb_private::ArchSpec &arch_spec) override;

  void RefreshStateAfterStop() override;

  bool CanDebug(lldb::TargetSP target_sp,
                bool plugin_specified_by_name) override;
  bool DestroyRequiresHalt() override { return false; }
  bool DoUpdateThreadList(ThreadList &old_thread_list,
                          ThreadList &new_thread_list) override;
  bool IsAlive() override;

  ArchSpec GetSystemArchitecture() override;

  size_t DoReadMemory(lldb::addr_t vm_addr, void *buf, size_t size,
                      Status &error) override;
  size_t DoWriteMemory(lldb::addr_t vm_addr, const void *buf, size_t size,
                       Status &error) override;
  lldb::addr_t DoAllocateMemory(size_t size, uint32_t permissions,
                                Status &error) override;
  Status DoDeallocateMemory(lldb::addr_t ptr) override;

  lldb::addr_t GetImageInfoAddress() override;

  DynamicLoaderWindowsDYLD *GetDynamicLoader() override;

  // IDebugDelegate overrides.
  void OnExitProcess(uint32_t exit_code) override;
  void OnDebuggerConnected(lldb::addr_t image_base) override;
  ExceptionResult OnDebugException(bool first_chance,
                                   const ExceptionRecord &record) override;
  void OnCreateThread(const HostThread &thread) override;
  void OnExitThread(lldb::tid_t thread_id, uint32_t exit_code) override;
  DllEventAction OnLoadDll(const ModuleSpec &module_spec,
                           lldb::addr_t module_addr,
                           lldb::tid_t thread_id) override;
  DllEventAction OnUnloadDll(lldb::addr_t module_addr,
                             lldb::tid_t thread_id) override;
  void OnDebugString(lldb::addr_t debug_string_addr, bool is_unicode,
                     uint16_t length_lower_word) override;
  void OnDebuggerError(const Status &error, uint32_t type) override;

  std::optional<uint32_t> GetWatchpointSlotCount() override;

  /// Returns the exception code of the active (current) debug exception,
  /// or std::nullopt if there is no active exception.
  std::optional<DWORD> GetActiveExceptionCode() const;

  Status EnableWatchpoint(lldb::WatchpointSP wp_sp,
                          bool notify = true) override;
  Status DisableWatchpoint(lldb::WatchpointSP wp_sp,
                           bool notify = true) override;

  void SetPseudoConsoleHandle() override;

protected:
  /// Block until the stdio read thread has surfaced everything currently
  /// buffered in the ConPTY/pipe to the process's STDOUT cache.
  void DrainProcessStdout();

  size_t PutSTDIN(const char *src, size_t src_len, Status &error) override;

  ProcessWindows(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp);

  Status DoGetMemoryRegionInfo(lldb::addr_t vm_addr,
                               MemoryRegionInfo &info) override;

private:
  /// Runs debug-event bookkeeping that does not need the inferior suspended
  /// (module loads/unloads outside the PDB allowlist, debug-string output) on
  /// a worker thread, so the debugger thread can ContinueDebugEvent right
  /// away. Items run in order, one at a time.
  class DeferredWork {
  public:
    ~DeferredWork() { Stop(/*run_pending=*/false); }

    /// Runs `work` inline once the queue has been stopped.
    void Enqueue(std::function<void()> work);
    /// Blocks until every queued item has run. Must not be called while
    /// holding a lock that queued work may take (e.g. the process m_mutex).
    void Flush();
    void Stop(bool run_pending);

  private:
    void Run();

    std::mutex m_mutex;
    std::condition_variable m_work_cv;
    std::condition_variable m_idle_cv;
    std::deque<std::function<void()>> m_queue;
    bool m_busy = false;
    bool m_stopped = false;
    std::thread::id m_worker_id;
    std::mutex m_join_mutex;
    std::thread m_thread;
  };

  bool ShouldDeferModuleLoad(const ModuleSpec &module_spec) const;

  DeferredWork m_deferred;

  struct WatchpointInfo {
    uint32_t slot_id;
    lldb::addr_t address;
    uint32_t size;
    bool read;
    bool write;
  };
  std::map<lldb::break_id_t, WatchpointInfo> m_watchpoints;
  std::vector<lldb::break_id_t> m_watchpoint_ids;
  std::shared_ptr<PTY> m_pty;
  bool m_pending_halt = false;
  bool m_expecting_loader_int3 = false;
};
} // namespace lldb_private

#endif // liblldb_Plugins_Process_Windows_Common_ProcessWindows_H_
