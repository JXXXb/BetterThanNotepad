============================================
  RepoManager - Command-Line Data Tool
  (with Watchdog Crash Recovery)
============================================

A lightweight command-line data management tool built on
std::vector<string>, supporting CRUD operations and file persistence.
An external watchdog process (logger.exe) monitors main.exe and
automatically recovers unsaved data on crash.

【Features】
  - CRUD operations (Create, Read, Update, Delete)
  - Exact search and fuzzy search
  - File persistence with auto-save (every 20s)
  - Multiple repository switching
  - Interactive CLI interface
  - External watchdog crash recovery (logger.exe monitors main.exe)
  - Operation logging (all actions written to logger.log)

【Project Structure】
  main.cpp   - Main program: data management, user interaction, operation logging
  logger.cpp - Watchdog process: monitors main.exe, writes heartbeats,
               detects crashes and saves crash recovery
  README.md  - This file

【Build】

  # 1. Build the logger (watchdog) process first
  g++ -o logger.exe logger.cpp -std=c++11

  # 2. Then build the main program
  g++ -o main.exe  main.cpp  -std=c++11

  # Both must be placed in the same directory

【Run】
  main.exe

  The program automatically launches logger.exe (watchdog) on startup.

【Auto-Save】
  After entering a repository, data is automatically saved to the
  .txt file every 20 seconds. No manual save needed.

【Crash Recovery Mechanism】

  Architecture (logger.exe monitors main.exe):
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
                                  → logger detects crash → reads state
                                  → writes crash_recovery.txt

  Recovery flow:
    1. main.exe crashes → logger.exe detects process death
    2. logger.exe finds .repo_state.tmp → reads it → writes crash_recovery.txt
    3. User restarts main.exe
    4. Program detects crash_recovery.txt → loads data → calls write()
       → deletes recovery file → enters main menu directly

【How to Test Crash Recovery】

  1. Launch main.exe, load or create a repository
  2. Add a few test entries from the main menu
  3. Open Task Manager (Ctrl+Shift+Esc), find main.exe → End task
  4. logger.exe detects main.exe is gone + .repo_state.tmp exists
     → crash_recovery.txt is generated automatically
  5. Relaunch main.exe
  6. Observe the prompt "Detected abnormal exit, recovering data..."
  7. Data has been automatically restored to the repository file

【Operation Logging】

  Both main.exe and logger.exe write to logger.log:
    - [MAIN] ADD | total=5
    - [MAIN] DELETE | index=2
    - [MAIN] MODIFY | index=0 new=hello
    - [MAIN] LOAD_REPO | mydata
    - [MAIN] CREATE_REPO | newrepo
    - [MAIN] EXIT | saving 5 items to mydata
    - Heartbeat #N | monitoring PID=12345    (from logger.exe)
    - CRASH DETECTED! ...                     (from logger.exe)

【Workflow】
  1. Startup: Choose operation mode
     - 1 - Load existing repository
     - 2 - Create new repository

  2. Main menu: Perform data management operations

  3. Exit: Auto-save data (watchdog & logger cleanly terminated)

【Main Menu Options】
  1 - Add items
      Continuous addition, type "exit" to quit

  2 - Delete item
      Delete by index

  3 - Modify item
      Modify by index

  4 - Search item
      Exact match, returns first result

  5 - Fuzzy search
      Displays all entries containing the keyword

  6 - Load repository
      Switch to another repository file

  7 - New repository
      Clear data and create a new file

  0 - Exit
      Auto-save and quit

【Storage Format】
  - File format: .txt plain text
  - Data structure: One record per line, format "index data_content"
  - Example:
    0 Apple
    1 Banana
    2 Orange

【Crash Recovery File Format】
  - crash_recovery.txt: first line = repo name, then one data entry per line
  - Example:
    mydata
    Apple
    Banana
    Orange

【Usage Notes】
  - Repository names must not contain spaces; .txt suffix is added automatically
  - Index is zero-based
  - If the repository already exists, the program prompts for overwrite
  - Auto-save every 20 seconds — no manual save required
  - logger.exe must reside in the same directory as main.exe
  - On normal exit, .repo_state.tmp is deleted → logger knows it was clean

【Tech Stack】
  - C++11 Standard Library
  - File streams (fstream)
  - Standard containers (vector, string)
  - Multithreading (thread, mutex, atomic)
  - Windows API (CreateProcess, OpenProcess, TerminateProcess)
  - Multithreading (thread, mutex, atomic)
  - Windows API (CreateProcess, WaitForSingleObject, TerminateProcess)