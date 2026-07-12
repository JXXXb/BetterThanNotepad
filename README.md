========================================
  RepoManager - Command-Line Data Tool
========================================

A lightweight command-line data management tool built with 
std::vector<string>, supporting CRUD operations and file persistence.

【Features】
  - CRUD operations (Create, Read, Update, Delete)
  - Exact search and fuzzy search
  - File persistence (auto-save/load)
  - Multiple repository switching
  - Interactive CLI interface

【Build & Run】
  g++ -o repo main.cpp -std=c++11
  ./repo

【Workflow】
  1. Launch: Choose operation mode
     - 1 - Load existing repository
     - 2 - Create new repository
  
  2. Main Menu: Perform data management operations
  
  3. Exit: Auto-save data on exit

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
      Displays all items containing keyword
  
  6 - Load repository
      Switch to another repository file
  
  7 - New repository
      Clear data and create new file
  
  8 - Exit
      Auto-save and quit

【Storage Format】
  - File format: .txt plain text
  - Data structure: One record per line, format "index data_content"
  - Example:
    0 Apple
    1 Banana
    2 Orange

【Usage Notes】
  - Repository name must not contain spaces; .txt suffix added automatically
  - Index starts from 0
  - If repository already exists, program will prompt for overwrite
  - Auto-save on exit - no manual save required

【Tech Stack】
  - C++11 Standard Library
  - File streams (fstream)
  - Standard containers (vector, string)