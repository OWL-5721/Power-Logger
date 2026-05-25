# Power Logger

A lightweight Windows Service that logs system power events — sleep, resume, lock, unlock, startup, and shutdown — to a plain text file.

Built because I leave my laptop unattended a lot, and I wanted a simple, honest record of what it was doing while I wasn't around.

---

## Why This Exists

I often leave my laptop with friends, or running builds and downloads for hours at a time. At some point I started wondering: *when exactly did it sleep? Did someone open it while I was away? Did it actually shut down cleanly?*

My first answer was a batch file hooked into Windows Task Scheduler:

```bat
@echo off
set logFile=%USERPROFILE%\system_event_log.txt
echo %date% %time% - %1 >> "%logFile%"
```

Triggered by:

- **Startup** — direct trigger
- **Sleep** — Event ID 42
- **Resume** — Event ID 1

It worked well for months. Then Windows update **KB5072033** (December 2025) quietly changed power management behavior. Sleep and resume events became unreliable. Scheduler triggers started misfiring during power transitions. The logs turned into noise.

After digging through Event Viewer, Windows power docs, and Modern Standby (S0) behavior, the root cause became clear: **Task Scheduler is not designed to survive modern power-state transitions reliably.** It wasn't my event IDs — it was the wrong tool for the job.

So I rebuilt it properly as a Windows Service in C++.

---

## How It Works

The service runs a hidden message-only window that listens for two types of system events:

- **`WM_POWERBROADCAST`** — catches actual hardware suspend and resume signals
- **`WM_WTSSESSION_CHANGE`** — catches session lock and unlock via WTS notifications

It also captures shutdown via `SERVICE_CONTROL_SHUTDOWN` in the service control handler.

All events are written with a timestamp to:

```
C:\ProgramData\PowerLogger\system_event_log.txt
```

Example log output:

```
2026-04-23 15:23:57 - Service Started
2026-04-23 15:24:13 - System Sleeping (Lock)
2026-04-23 15:24:20 - System Resumed (Unlock)
2026-04-23 15:49:03 - System Suspend
2026-04-23 15:51:30 - System Resume
2026-04-23 22:14:00 - System Shutdown
```

---

## Requirements

- Windows 10 or 11
- g++ (MinGW) or MSVC
- Administrator privileges (required for service installation)

---

## Build

```bash
g++ logger.cpp -o PowerLoggerService.exe -lwtsapi32
```

Or with MSVC:

```bash
cl logger.cpp /link Wtsapi32.lib
```

---

## Installation

**1. Install the service:**

```bash
sc create PowerLoggerService binPath= "C:\full\path\to\PowerLoggerService.exe" start= auto
```

> Note: The space after `binPath=` and `start=` is required by `sc`.

**2. Start the service:**

```bash
sc start PowerLoggerService
```

**3. Verify it's running:**

```bash
sc query PowerLoggerService
```

---

## Uninstall

```bash
sc stop PowerLoggerService
sc delete PowerLoggerService
```

---

## Log Location

```
C:\ProgramData\PowerLogger\system_event_log.txt
```

The directory is created automatically on first run. You can open the file with any text editor.

---

## What Gets Logged

| Event | Trigger |
|---|---|
| Service Started | Service starts successfully |
| System Sleeping (Lock) | Session lock detected |
| System Resumed (Unlock) | Session unlock detected |
| System Suspend | Hardware suspend signal |
| System Resume | Hardware resume signal |
| System Shutdown | Windows shutdown initiated |
| Service Stop Requested | Manual `sc stop` |

---

## Future Plans

This project was originally about solving a practical problem. But it's also pointed me toward something more interesting: building a custom activity logger that tracks foreground apps, background runtime, active usage duration, and idle time — not because Windows doesn't have tools for this, but because I want to understand how these systems work from the inside.

That's the next thing I want to build.

---

## Project Background

- **Started:** 4th semester of college — batch file + Task Scheduler version
- **Rebuilt:** After Windows update KB5072033 broke the original approach
- **Language:** C++
- **Architecture:** Windows Service (Win32 API + WTS Session Notifications)

---

## License

MIT
