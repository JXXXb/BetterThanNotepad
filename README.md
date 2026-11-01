# RepoManager

RepoManager is a Windows command-line data manager backed by `std::vector<string>`. It supports repository files, CRUD operations, search, automatic saving, operation logging, and crash recovery.

## Features

- Create, load, list, delete, and switch `.stxt` repositories
- Add, delete by index or matching text, modify, and list entries
- Find the first entry containing a substring
- Show repository and numeric statistics
- Auto-save every 20 seconds
- Write an operation queue that `logger.exe` copies to `logger.log`
- Recover the most recently saved in-memory state after an abnormal exit
- Request administrator elevation when starting `main.exe`

## Project structure

- `main.cpp` - Interactive program, persistence, recovery, and watchdog management
- `logger.cpp` - Watchdog and operation logger
- `main_manifest.rc` - UAC manifest resource (embedded into `main.exe` by CMake; embedded into both executables by the GitHub Actions release)
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

## Menus and commands

When no repository is selected, use the numeric repository menu:

- `1` - Load a repository
- `2` - Create and switch to a repository
- `3` - List repositories
- `4` - Delete a repository
- `0` - Exit

After selecting or creating a repository, enter one of these commands at the prompt:

| Command | Action |
| --- | --- |
| `a` | Add entries continuously; enter `exit` to finish |
| `d` | Delete by zero-based index or matching text; enter `exit` to finish |
| `m` | Modify an entry by its zero-based index |
| `f` | Find the first entry containing the supplied text |
| `r` | Load and switch to a repository |
| `n` | Create and switch to a repository |
| `list-data` | List all entries |
| `list-repos` | List repositories |
| `stats` | Show repository and numeric statistics |
| `w` | Write the current repository to disk |
| `q` | Save and exit |

Commands are entered exactly as shown. When deleting by text, matching entries are displayed for confirmation or selection. Repository names are used as entered, with `.stxt` appended automatically.

## Storage format

Repository files are plain text, with one entry per line:

```text
0@~Apple
1@~Banana
2@~Orange
```

## Crash recovery

After a repository is selected, `main.exe` creates `.mainerror` as the abnormal-exit marker and writes a state snapshot to `.repo_state.tmp`. The repository and state files are written through temporary files and atomically replaced, so a crash cannot truncate the previous complete snapshot. The marker is created before the snapshot is committed, so a crash cannot leave a snapshot behind without the marker; a marker left behind without a usable snapshot is removed on the next launch without retrying every time.

On a normal exit, `main.exe` saves the repository, deletes `.repo_state.tmp`, stops `logger.exe`, then deletes `.mainerror`. After an abnormal exit, the marker and last complete snapshot remain. The next `main.exe` launch restores the snapshot before displaying the main menu. `logger.exe` also records the event and copies the snapshot to `crash_recovery.txt` (it does not write `.mainerror` itself; that file is owned by `main.exe`).

Recovery does not depend on the logger polling interval, so it also works when the program is relaunched immediately after a crash.

| File | Purpose |
| --- | --- |
| `.repo_state.tmp` | Current repository name followed by in-memory entries |
| `.mainerror` | Marker written by `main.exe` before each snapshot commit; left behind after an abnormal exit and removed on normal exit |
| `crash_recovery.txt` | Backup made by `logger.exe` after it observes a crash |
| `logger.log` | Logger startup, heartbeat, crash, and operation records |
| `.opqueue.tmp` | Operation queue written by `main.exe` |

## Numeric statistics

The statistics command extracts signed decimal integers from entries. Values outside the `long long` range are skipped and reported. If the total cannot fit in `long long`, the program still reports the minimum and maximum values but explicitly reports that total and average are unavailable.

## Operation logging

`main.exe` appends data and repository events to `.opqueue.tmp`. `logger.exe` periodically moves the queue to `logger.log` and prefixes those records with `[MAIN]`. Search and list operations are not logged.
