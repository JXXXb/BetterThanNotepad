============================================
  RepoManager - Command-Line Data Tool
  (with Watchdog Crash Recovery)
============================================

A lightweight command-line data management tool built on
std::vector<string>, supporting CRUD operations and file persistence.
Includes a watchdog process that monitors for crashes and automatically
recovers unsaved data.

【Features】
  - CRUD operations (Create, Read, Update, Delete)
  - Exact search and fuzzy search
  - File persistence (auto-save/load)
  - Multiple repository switching
  - Interactive CLI interface
  - Watchdog crash recovery (logger.exe monitors & auto-recovers)

【Project Structure】
  main.cpp   - Main program: data management & user interaction
  logger.cpp - Logger process: heartbeat logging & crash detection target
  README.md  - This file

【Build】

  # 1. Build the logger process first
  g++ -o logger.exe logger.cpp -std=c++11

  # 2. Then build the main program
  g++ -o main.exe  main.cpp  -std=c++11

  # Both must be placed in the same directory

【Run】
  main.exe

  The program automatically launches logger.exe on startup.

【Crash Recovery Mechanism】

  Architecture:
    main.exe ──launches──> logger.exe (writes heartbeat to logger.log every 2s)
        │                       │
        └── watchdog thread ────┘
               │
               └─ logger.exe exits unexpectedly
                    → saves unsaved data to crash_recovery.txt
                    → auto-recovers on next launch

  Recovery flow:
    1. logger.exe crashes → watchdog detects process exit
    2. watchdog writes currentRepoName + all Data into crash_recovery.txt
    3. User restarts main.exe
    4. Program detects crash_recovery.txt → loads data → calls write()
       → deletes recovery file → enters main menu directly

【How to Test Crash Recovery】

  1. Launch main.exe, load or create a repository
  2. Add a few test entries from the main menu
  3. Open Task Manager (Ctrl+Shift+Esc), find logger.exe, right-click → End task
  4. The watchdog detects the crash; crash_recovery.txt is generated
  5. Close main.exe (or forcibly terminate it)
  6. Relaunch main.exe
  7. Observe the prompt "⚠ Detected abnormal exit, recovering data..."
  8. Data has been automatically restored to the repository file

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
  - Auto-save on exit — no manual save required
  - logger.exe must reside in the same directory as main.exe
  - On normal exit, logger.exe is terminated cleanly — no crash recovery triggered

【Tech Stack】
  - C++11 Standard Library
  - File streams (fstream)
  - Standard containers (vector, string)
  - Multithreading (thread, mutex, atomic)
  - Windows API (CreateProcess, WaitForSingleObject, TerminateProcess)