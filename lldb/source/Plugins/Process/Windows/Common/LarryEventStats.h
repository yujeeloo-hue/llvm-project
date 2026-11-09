//===-- LarryEventStats.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_Plugins_Process_Windows_Common_LarryEventStats_H_
#define liblldb_Plugins_Process_Windows_Common_LarryEventStats_H_

#include "lldb/Host/windows/windows.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

namespace lldb_private {
namespace larry {

/// Debug-event timing summaries are written only when the environment
/// variable LARRY_LLDB_EVENT_STATS names a file to append to.
inline const char *EventStatsPath() {
  static const char *path = [] {
    const char *p = std::getenv("LARRY_LLDB_EVENT_STATS");
    return (p && *p) ? p : nullptr;
  }();
  return path;
}

inline bool EventStatsEnabled() { return EventStatsPath() != nullptr; }

inline void AppendEventStatsLine(const std::string &line) {
  const char *path = EventStatsPath();
  if (!path)
    return;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  FILE *file = std::fopen(path, "a");
  if (!file)
    return;
  SYSTEMTIME now;
  ::GetLocalTime(&now);
  std::fprintf(file, "%02u:%02u:%02u.%03u %s\n", now.wHour, now.wMinute,
               now.wSecond, now.wMilliseconds, line.c_str());
  std::fclose(file);
}

} // namespace larry
} // namespace lldb_private

#endif // liblldb_Plugins_Process_Windows_Common_LarryEventStats_H_
