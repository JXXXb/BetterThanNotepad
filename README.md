# RepoManager

RepoManager is a Windows command-line data manager backed by `std::vector<string>`. It supports repository files, CRUD operations, search, automatic saving, operation logging, and crash recovery.

## Features

- Create, load, list, delete, and switch `.stxt` repositories
- Add, delete, modify, list, substring-search, and fuzzy-search entries
- Show repository and numeric statistics
- Auto-save every 20 seconds
- Write an operation queue that `logger.exe` copies to `logger.log`
- Recover the most recently saved in-memory state after an abnormal exit
- Request administrator elevation when starting `main.exe`

## Project structure

- `main.cpp` - Interactive program, persistence, recovery, and watchdog management
- `logger.cpp` - Watchdog and operation logger
- `main_manifest.rc` - UAC manifest resource for `main.exe`
- `CMakeLists.txt` - CMake build configuration
- `build.bat` - One-command MSVC and Ninja build helper

## Build

Install Visual Studio with the Desktop C++ workload, CMake, and Ninja. From a normal Command Prompt or PowerShell window, run:

```bat
build.bat
```

`build.bat` locates Visual Studio, loads the x64 MSVC environment, configures a Release Ninja build, and builds both executables.

To configure manually from a Visual Studio Developer Command Prompt:

```bat
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Release binaries are written to `build/bin/Release/`. Use `-DCMAKE_BUILD_TYPE=Debug` to create `build/bin/Debug/` binaries. The Visual Studio solution, `BetterThanNotepad.sln`, is also supported.

- C++ standard: C++23
- Local CMake runtime library: `/MD`
- Release optimization: `/O2` with link-time optimization
- GitHub Actions release runtime library: `/MT`

## Run

```bat
build\bin\Release\main.exe
```

Keep `logger.exe` in the same directory as `main.exe`. The program starts it automatically.

## Main menu

- `1` - Add entries continuously, enter `exit` to finish
- `2` - Delete by index or matching text
- `3` - Modify an entry by index
- `4` - Find the first substring match
- `5` - Show all substring matches
- `6` - Load and switch repository
- `7` - Create and switch repository
- `8` - List all entries
- `9` - List repositories
- `10` - Show repository and numeric statistics
- `0` - Save and exit

Indexes are zero-based. Repository names are used as entered, with `.stxt` appended automatically.

## Storage format

Repository files are plain text, with one entry per line:

```text
0@~Apple
1@~Banana
2@~Orange
```

## Crash recovery

After a repository is selected, `main.exe` saves a state snapshot to `.repo_state.tmp` and creates `.mainerror` to indicate that the process has not exited normally. Both state and repository files are written through temporary files and atomically replaced, so a crash cannot truncate the previous complete snapshot.

On a normal exit, `main.exe` saves the repository, deletes `.repo_state.tmp` and `.mainerror`, then stops `logger.exe`. After an abnormal exit, the marker and last complete snapshot remain. The next `main.exe` launch restores the snapshot before displaying the main menu. `logger.exe` also records the event and copies the snapshot to `crash_recovery.txt`.

Recovery does not depend on the logger polling interval, so it also works when the program is relaunched immediately after a crash.

| File | Purpose |
| --- | --- |
| `.repo_state.tmp` | Current repository name followed by in-memory entries |
| `.mainerror` | Marker that the previous run did not exit normally |
| `crash_recovery.txt` | Backup made by `logger.exe` after it observes a crash |
| `logger.log` | Logger startup, heartbeat, crash, and operation records |
| `.opqueue.tmp` | Operation queue written by `main.exe` |

## Numeric statistics

The statistics command extracts signed decimal integers from entries. Values outside the `long long` range are skipped and reported. If the total cannot fit in `long long`, the program still reports the minimum and maximum values but explicitly reports that total and average are unavailable.

## Operation logging

`main.exe` appends data and repository events to `.opqueue.tmp`. `logger.exe` periodically moves the queue to `logger.log` and prefixes those records with `[MAIN]`. Search and list operations are not logged.
