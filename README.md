# RepoManager - Command-Line Data Tool (with Watchdog Crash Recovery)

A lightweight command-line data management tool built on `std::vector<string>`,
supporting CRUD operations and file persistence. An external watchdog process
(logger.exe) monitors main.exe and automatically recovers unsaved data on crash.

## Features

- CRUD operations (Create, Read, Update, Delete)
- Substring search (first match) and fuzzy search (all matches)
- File persistence with auto-save (every 20s)
- Multiple repository switching
- Interactive CLI interface
- External watchdog crash recovery (logger.exe monitors main.exe)
- Operation logging (data changes and repository events written to logger.log)
- UAC administrator elevation on launch

## Project Structure

- `main.cpp` — Main program: data management, user interaction
- `logger.cpp` — Watchdog process: monitors main.exe, generates logs
- `main.manifest` / `main_manifest.rc` — UAC manifest (requireAdministrator), embedded into main.exe
- `CMakeLists.txt` — CMake build script
- `build.bat` — One-click build helper (CMake + Ninja)
- `README.md` — This file

## Build

Two build paths are used:

### Local build (CMake)

Requires CMake and a Developer Command Prompt (or vcvars64.bat in PATH):

```bat
build.bat
```

Or manually:

```bat
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binaries are written to `build/bin/Release/` (`main.exe` and `logger.exe`).
The Visual Studio solution (`BetterThanNotepad.sln`) can also be used.

- Runtime library: `/MD` (`MultiThreadedDLL`)
- Optimization: `/O2` (Release only)
- UAC: only main.exe embeds the `requireAdministrator` manifest
  (`main_manifest.rc`); logger.exe inherits elevation from main.exe.

### Release build (GitHub Actions, `.github/workflows/release.yml`)

Compiles directly with `cl.exe` (MSVC x64) and embeds `main_manifest.rc`
into both executables:

```bat
rc /nologo /fo main.res main_manifest.rc
rc /nologo /fo logger.res main_manifest.rc
cl /nologo /W3 /O2 /EHsc /utf-8 /MT /DWIN32 /DNDEBUG /D_CONSOLE /DUNICODE /D_UNICODE main.cpp main.res /Fe:main.exe /link /SUBSYSTEM:CONSOLE /MANIFEST:NO
cl /nologo /W3 /O2 /EHsc /utf-8 /MT /DWIN32 /DNDEBUG /D_CONSOLE /DUNICODE /D_UNICODE logger.cpp logger.res /Fe:logger.exe /link /SUBSYSTEM:CONSOLE /MANIFEST:NO
```

- Runtime library: `/MT` (static CRT)
- UAC: both main.exe and logger.exe embed the `requireAdministrator` manifest.

Additionally, main.exe re-launches itself elevated with `ShellExecuteW("runas")`
if it detects it is not elevated (fallback for both build paths).

## Run

```bash
build\bin\Release\main.exe
```

On launch, Windows will show a UAC prompt requesting administrator privileges.
Confirm to run. The program automatically launches logger.exe (also elevated).

## Auto-Save

After entering a repository, data is automatically saved to the .txt file every
20 seconds. No manual save needed.

## Crash Recovery Mechanism

### Architecture (logger.exe monitors main.exe)

```text
main.exe ──launches──> logger.exe <main-PID>
    │                       │
    │  on each data change: │  every 2s: check if main.exe is alive
    │  writes state to      │  writes heartbeat to logger.log
    │  .repo_state.tmp      │
    │                       │
    ├─ normal exit ──────────> deletes .repo_state.tmp (no crash marker left)
    │
    └─ crash ───────────────> .repo_state.tmp remains on disk
                              → logger detects crash
                              → creates empty .mainerror marker
                              → saves backup copy to crash_recovery.txt
```

### Recovery Flow

1. main.exe crashes → logger.exe detects process death
2. logger.exe finds .repo_state.tmp → creates empty .mainerror marker
   (and saves a backup copy to crash_recovery.txt)
3. User restarts main.exe
4. Program detects .mainerror → deletes the marker → checks logger.log exists
   → reads state from .repo_state.tmp → calls write()
   → enters main menu directly (skips repository selection)
5. If .mainerror exists but logger.log is missing → prints notice, skips recovery
6. If .mainerror exists but .repo_state.tmp is missing → prints notice, skips recovery

## How to Test Crash Recovery

1. Launch main.exe, load or create a repository
2. Add a few test entries from the main menu
3. Open Task Manager (Ctrl+Shift+Esc), find main.exe → End task
4. logger.exe detects main.exe is gone + .repo_state.tmp exists
   → empty .mainerror file is created as crash marker
   → .repo_state.tmp is copied to crash_recovery.txt
5. Relaunch main.exe
6. Observe the prompt "检测到上次异常退出（.mainerror）。"
7. Data has been automatically restored from .repo_state.tmp to the repository file

## Operation Logging

main.exe appends events to the `.opqueue.tmp` queue; logger.exe periodically
moves the queue into `logger.log`, prefixing each line with `[MAIN]`.
Only data changes and repository events are logged — search and list
operations are not.

- `[MAIN] ADD | index=0 total=5`
- `[MAIN] DELETE | index=2 total=4`
- `[MAIN] MODIFY | index=0 new=hello`
- `[MAIN] LOAD_REPO | mydata`
- `[MAIN] CREATE_REPO | newrepo`
- `[MAIN] EXIT | saving 5 items to mydata`
- `[LOGGER] Heartbeat #N | monitoring PID=12345`
- `[LOGGER] CRASH DETECTED! ...`

## Workflow

1. **Startup**: Choose operation mode
   - 1 — Load existing repository
   - 2 — Create new repository
2. **Main menu**: Perform data management operations
3. **Exit**: Auto-save data, delete `.repo_state.tmp`, then terminate logger.exe

## Main Menu Options

- **1** — Add items (continuous addition, type "exit" to quit)
- **2** — Delete items (continuous, by index, type "exit" to quit)
- **3** — Modify item (by index)
- **4** — Search item (substring match, returns first result)
- **5** — Fuzzy search (displays all entries containing the keyword)
- **6** — Load repository (switch to another repository file)
- **7** — New repository (clear data and create a new file)
- **8** — List all items
- **0** — Exit (auto-save and quit)

## Storage Format

- File format: .txt plain text
- Data structure: one record per line, format `index@~data_content`
- Example:

```text
0@~Apple
1@~Banana
2@~Orange
```

## Crash Recovery Files

| File | Purpose |
| ------ | --------- |
| `.mainerror` | Empty marker — logger creates this on crash; triggers auto-recovery on next launch |
| `crash_recovery.txt` | Backup copy of `.repo_state.tmp` made by logger on crash |
| `logger.log` | Operation log — confirms crash was monitored |
| `.repo_state.tmp` | State snapshot — first line repo name, rest are data entries |

Example `.repo_state.tmp`:

```text
mydata
Apple
Banana
Orange
```

Transient / diagnostic files:

| File | Purpose |
| ------ | --------- |
| `.opqueue.tmp` | Operation queue written by main.exe |
| `.opqueue.proc` | Queue copy being processed by logger.exe (deleted afterwards) |
| `logger_started.pid` | Logger startup diagnostics |
| `.start_logger_debug.log` / `.start_logger_error.log` | CreateProcessW diagnostics |

## Usage Notes

- Repository names are used as typed; the .txt suffix is added automatically
- Index is zero-based
- If the repository already exists, the program prompts for overwrite
- Auto-save every 20 seconds — no manual save required
- logger.exe must reside in the same directory as main.exe
- On normal exit, main.exe deletes .repo_state.tmp and then terminates logger.exe
- On crash, logger creates .mainerror → next launch auto-recovers from .repo_state.tmp

## Tech Stack

- C++11 Standard Library
- File streams (fstream)
- Standard containers (vector, string)
- Multithreading (thread, mutex, atomic, chrono)
- Windows API (CreateProcessW, OpenProcess, TerminateProcess, WaitForSingleObject, ShellExecuteW, GetExitCodeProcess, MoveFileExA)
