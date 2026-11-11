/**
 * Pause-stop classification smoke test for the Larry lldb-dap adapter.
 *
 * Launches a trivial always-running target (cmd.exe /c pause), sends a DAP
 * `pause` and prints how the resulting `stopped` event is classified.
 *
 * Expected after the fixes: reason === "pause" (interrupt stop, not
 * "exception") and the stopped thread is the process's initial (main)
 * thread — the injection's transient thread already exited and is never in
 * the threads list. The main thread shows up as "Thread 1" (index-based
 * fallback name) for a nameless target like ping.exe.
 *
 * Usage: node larry/test-pause-reason.mjs [path-to-lldb-dap.exe]
 */
import { spawn } from "node:child_process";
import { dirname, join } from "node:path";

const adapterPath =
  process.argv[2] ??
  join(import.meta.dirname, "..", "build-release", "bin", "lldb-dap.exe");

const log = (...args) => console.log("[test]", ...args);

const child = spawn(adapterPath, [], {
  cwd: dirname(adapterPath),
  stdio: ["pipe", "pipe", "pipe"],
  windowsHide: true,
});
let stderr = "";
child.stderr.on("data", (c) => { stderr += c; });

let seq = 0;
let buffer = Buffer.alloc(0);
const pending = new Map();
const waiters = [];

function send(command, arguments_) {
  const message = { type: "request", seq: ++seq, command };
  if (arguments_ !== undefined) message.arguments = arguments_;
  const json = Buffer.from(JSON.stringify(message), "utf8");
  child.stdin.write(
    Buffer.concat([
      Buffer.from(`Content-Length: ${json.length}\r\n\r\n`, "ascii"),
      json,
    ]),
  );
  return message.seq;
}

function request(command, arguments_, timeoutMs = 30_000) {
  const seqId = send(command, arguments_);
  return new Promise((resolve, reject) => {
    const timer = setTimeout(
      () => {
        pending.delete(seqId);
        reject(new Error(`DAP ${command} timed out after ${timeoutMs}ms`));
      },
      timeoutMs,
    );
    pending.set(seqId, { resolve, reject, timer, command });
  });
}

function waitForEvent(name, timeoutMs = 30_000) {
  return new Promise((resolve, reject) => {
    const waiter = {
      name,
      resolve: (body) => {
        clearTimeout(timer);
        waiters.splice(waiters.indexOf(waiter), 1);
        resolve(body);
      },
    };
    const timer = setTimeout(() => {
      waiters.splice(waiters.indexOf(waiter), 1);
      reject(new Error(`timed out waiting for event "${name}"`));
    }, timeoutMs);
    waiters.push(waiter);
  });
}

child.stdout.on("data", (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  for (;;) {
    const headerEnd = buffer.indexOf("\r\n\r\n", "ascii");
    if (headerEnd < 0) return;
    const header = buffer.subarray(0, headerEnd).toString("ascii");
    const match = /^Content-Length:\s*(\d+)/m.exec(header);
    if (!match) throw new Error(`bad DAP header: ${header}`);
    const length = Number(match[1]);
    if (buffer.length < headerEnd + 4 + length) return;
    const json = buffer.subarray(headerEnd + 4, headerEnd + 4 + length);
    buffer = buffer.subarray(headerEnd + 4 + length);
    const message = JSON.parse(json.toString("utf8"));
    if (message.type === "response") {
      const entry = pending.get(message.request_seq);
      if (entry) {
        pending.delete(message.request_seq);
        clearTimeout(entry.timer);
        if (message.success) entry.resolve(message.body ?? {});
        else entry.reject(new Error(`${entry.command} failed: ${message.message ?? JSON.stringify(message.body)}`));
      }
      continue;
    }
    if (message.type === "event") {
      for (const waiter of [...waiters]) {
        if (waiter.name === message.event) waiter.resolve(message.body ?? {});
      }
    }
  }
});

const exit = (code) => {
  for (const entry of pending.values()) {
    clearTimeout(entry.timer);
    entry.reject(new Error("adapter exited"));
  }
  child.kill();
  process.exit(code);
};

try {
  await request("initialize", {
    clientID: "larry-test",
    clientName: "Larry pause smoke test",
    adapterID: "lldb-dap",
    linesStartAt1: true,
    columnsStartAt1: true,
    pathFormat: "path",
  });
  const initialized = waitForEvent("initialized", 60_000);
  const launchDone = request("launch", {
    name: "pause-classification",
    program: join(process.env.SystemRoot ?? "C:\\Windows", "System32", "ping.exe"),
    // Each probe times out after -w ms: a wall-clock sleeper that never
    // touches stdin (unlike cmd /c pause, which exits on closed stdin).
    args: ["-n", "50", "-w", "60000", "10.255.255.1"],
    cwd: process.env.TEMP ?? ".",
    stopOnEntry: false,
    console: "internalConsole",
    disableSTDIO: true,
  }, 120_000);
  await initialized;
  await request("configurationDone", {}, 120_000);
  await launchDone;
  log("target launched; waiting 1.5s before pause…");
  await new Promise((r) => setTimeout(r, 1_500));

  const stoppedEvent = waitForEvent("stopped", 15_000);
  await request("pause", { threadId: 0 }, 10_000);
  log("pause accepted; waiting for the stopped event…");
  const stopped = await stoppedEvent;
  log("stopped event:", JSON.stringify(stopped));

  const { threads } = await request("threads", {}, 10_000);
  log(`threads (${threads?.length ?? 0}):`, JSON.stringify(threads?.map((t) => ({ id: t.id, name: t.name }))));

  await request("disconnect", { terminateDebuggee: true }, 10_000).catch(() => undefined);
  const reason = stopped.reason;
  if (reason !== "pause") {
    log(`FAIL — pause stop is classified "${reason}" (expected "pause")`);
    if (stderr.trim()) log("adapter stderr:", stderr.trim().slice(-2000));
    exit(1);
  }
  const stoppedThread = threads?.find((t) => t.id === stopped.threadId);
  if (!stoppedThread) {
    log(`FAIL — stopped threadId ${stopped.threadId} is not in the threads list (still the transient injection thread)`);
    exit(1);
  }
  const mainThread = threads?.find((t) => String(t.name ?? "").trim() === "Thread 1");
  if (mainThread && stoppedThread.id !== mainThread.id) {
    log(`FAIL — stop attributed to "${stoppedThread.name}" (id ${stoppedThread.id}); expected the main thread "${mainThread.name}" (id ${mainThread.id})`);
    exit(1);
  }
  log(`PASS — pause stop is classified "pause" on ${stoppedThread.name} (id ${stoppedThread.id})`);
  exit(0);
} catch (err) {
  log("ERROR:", err.message ?? err);
  if (stderr.trim()) log("adapter stderr:", stderr.trim().slice(-2000));
  exit(1);
}
