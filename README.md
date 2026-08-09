# RepoManager - Command-Line Data Tool (with Watchdog Crash Recovery)

A lightweight command-line data management tool built on `std::vector<string>`,
supporting CRUD operations and file persistence. An external watchdog process
(logger.exe) monitors main.exe and automatically recovers unsaved data on crash.

## Features

- CRUD operations (Create, Read, Update, Delete)
- Exact search and fuzzy search
- File persistence with auto-save (every 20s)
- Multiple repository switching
- Interactive CLI interface
- External watchdog crash recovery (logger.exe monitors main.exe)
- Operation logging (all actions written to logger.log)
- UAC administrator elevation on launch

## Project Structure

- `main.cpp` — Main program: data management, user interaction
- `main.exe.manifest` — UAC manifest (requireAdministrator)
- `main.rc` — Resource script for embedding manifest
- `logger.cpp` — Watchdog process: monitors main.exe, generates logs
- `logger.exe.manifest` — UAC manifest (requireAdministrator)
- `logger.rc` — Resource script for embedding manifest
- `README.md` — This file

## Build

```bash
# 0. Compile resource files (UAC manifest) for both executables
windres main.rc   -o main_res.o
windres logger.rc -o logger_res.o

# 1. Build the logger (watchdog) process
g++ -o logger.exe logger.cpp logger_res.o -std=c++11 -static

# 2. Build the main program
g++ -o main.exe  main.cpp  main_res.o  -std=c++11 -static
```

Both .exe will request administrator privileges on launch (UAC prompt).

## Run

```bash
main.exe
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
    ├─ normal exit ──────────> deletes .repo_state.tmp
    │                         → logger detects clean exit, does nothing
    │
    └─ crash ───────────────> .repo_state.tmp remains on disk
                              → logger detects crash
                              → creates empty .mainerror marker
```

### Recovery Flow

1. main.exe crashes → logger.exe detects process death
2. logger.exe finds .repo_state.tmp → creates empty .mainerror marker
3. User restarts main.exe
4. Program detects .mainerror → checks logger.log exists
   → reads state from .repo_state.tmp → calls write()
   → deletes .mainerror → enters main menu directly
5. If .mainerror exists but logger.log is missing → prints notice, skips recovery

## How to Test Crash Recovery

1. Launch main.exe, load or create a repository
2. Add a few test entries from the main menu
3. Open Task Manager (Ctrl+Shift+Esc), find main.exe → End task
4. logger.exe detects main.exe is gone + .repo_state.tmp exists
   → empty .mainerror file is created as crash marker
5. Relaunch main.exe
6. Observe the prompt "检测到上次异常退出（.mainerror）。"
7. Data has been automatically restored from .repo_state.tmp to the repository file

## Operation Logging

Both main.exe and logger.exe write to logger.log:

- `[MAIN] ADD | total=5`
- `[MAIN] DELETE | index=2`
- `[MAIN] MODIFY | index=0 new=hello`
- `[MAIN] LOAD_REPO | mydata`
- `[MAIN] CREATE_REPO | newrepo`
- `[MAIN] EXIT | saving 5 items to mydata`
- Heartbeat #N | monitoring PID=12345 (from logger.exe)
- CRASH DETECTED! ... (from logger.exe)

## Workflow

1. **Startup**: Choose operation mode
   - 1 — Load existing repository
   - 2 — Create new repository
2. **Main menu**: Perform data management operations
3. **Exit**: Auto-save data (watchdog & logger cleanly terminated)

## Main Menu Options

- **1** — Add items (continuous addition, type "exit" to quit)
- **2** — Delete item (by index)
- **3** — Modify item (by index)
- **4** — Search item (exact match, returns first result)
- **5** — Fuzzy search (displays all entries containing the keyword)
- **6** — Load repository (switch to another repository file)
- **7** — New repository (clear data and create a new file)
- **0** — Exit (auto-save and quit)

## Storage Format

- File format: .txt plain text
- Data structure: one record per line, format "index data_content"
- Example:

```text
0 Apple
1 Banana
2 Orange
```

## Crash Recovery Files

| File | Purpose |
|------|---------|
| `.mainerror` | Empty marker — logger creates this on crash |
| `logger.log` | Operation log — confirms crash was monitored |
| `.repo_state.tmp` | State snapshot — first line repo name, rest are data entries |

Example `.repo_state.tmp`:

```text
mydata
Apple
Banana
Orange
```

## Usage Notes

- Repository names must not contain spaces; .txt suffix is added automatically
- Index is zero-based
- If the repository already exists, the program prompts for overwrite
- Auto-save every 20 seconds — no manual save required
- logger.exe must reside in the same directory as main.exe
- On normal exit, .repo_state.tmp is deleted → logger knows it was clean
- On crash, logger creates .mainerror → next launch auto-recovers from .repo_state.tmp

## Tech Stack

- C++11 Standard Library
- File streams (fstream)
- Standard containers (vector, string)
- Multithreading (thread, mutex, atomic)
- Windows API (CreateProcess, OpenProcess, TerminateProcess, WaitForSingleObject)
