#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <string>
#include <windows.h>

using namespace std;

const string STATE_FILE   = ".repo_state.tmp";
const string CRASH_FILE   = "crash_recovery.txt";
const string LOG_FILE     = "logger.log";
const string OPQ_FILE     = ".opqueue.tmp";      // main.exe 写入的操作队列
const string OPQ_PROC     = ".opqueue.proc";     // 处理中的队列副本

string nowStr()
{
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

void logMsg(ofstream &log, const string &msg)
{
    log << "[" << nowStr() << "] " << msg << endl;
    log.flush();
}

// 处理操作队列：main.exe 写入 .opqueue.tmp，我们负责写进 logger.log
void processOpQueue(ofstream &log)
{
    // 先检查队列文件是否存在
    ifstream test(OPQ_FILE);
    if (!test.is_open()) return;
    test.close();

    // 原子重命名（避免 main.exe 同时写入时冲突）
    if (!MoveFileExA(OPQ_FILE.c_str(), OPQ_PROC.c_str(), MOVEFILE_REPLACE_EXISTING))
        return;

    ifstream proc(OPQ_PROC);
    if (!proc.is_open()) return;

    string line;
    while (getline(proc, line))
    {
        if (!line.empty())
            logMsg(log, "[MAIN] " + line);
    }
    proc.close();
    DeleteFileA(OPQ_PROC.c_str());
}

int main(int argc, char* argv[])
{
    ofstream logFile(LOG_FILE, ios::app);
    if (!logFile.is_open()) return 1;

    // 从命令行获取 main.exe 的 PID
    DWORD mainPid = 0;
    if (argc >= 2)
        mainPid = stoul(argv[1]);

    if (mainPid == 0)
    {
        logMsg(logFile, "[LOGGER] ERROR: No main PID provided, exiting.");
        return 1;
    }

    logMsg(logFile, "[LOGGER] ======== Logger started, monitoring PID="
           + to_string(mainPid) + " ========");

    //打开 main.exe 进程句柄用于监控
    HANDLE hMain = OpenProcess(SYNCHRONIZE, FALSE, mainPid);
    if (hMain == NULL)
    {
        logMsg(logFile, "[LOGGER] ERROR: Cannot open main process (PID="
               + to_string(mainPid) + "), code=" + to_string(GetLastError()));
        return 1;
    }

    int heartbeat = 0;
    while (true)
    {
        this_thread::sleep_for(chrono::seconds(2));

        // 1. 处理 main.exe 发来的操作队列
        processOpQueue(logFile);

        // 2. 写心跳
        logMsg(logFile, "[LOGGER] Heartbeat #" + to_string(++heartbeat)
               + " | monitoring PID=" + to_string(mainPid));

        // 3. 检查 main.exe 是否还活着
        DWORD exitCode;
        if (!GetExitCodeProcess(hMain, &exitCode))
        {
            logMsg(logFile, "[LOGGER] WARN: GetExitCodeProcess failed");
            break;
        }

        if (exitCode != STILL_ACTIVE)
        {
            // main.exe 已退出，处理最后的操作队列
            processOpQueue(logFile);

            logMsg(logFile, "[LOGGER] Main process exited (code="
                   + to_string(exitCode) + ")");

            // 检查状态文件 → 存在说明是崩溃
            ifstream stateFile(STATE_FILE);
            if (stateFile.is_open())
            {
                logMsg(logFile, "[LOGGER] CRASH DETECTED! State file found, saving recovery...");

                ofstream crash(CRASH_FILE);
                if (crash.is_open())
                {
                    string line;
                    while (getline(stateFile, line))
                        crash << line << endl;
                    crash.close();
                    logMsg(logFile, "[LOGGER] Crash recovery saved to " + CRASH_FILE);
                }
                else
                {
                    logMsg(logFile, "[LOGGER] ERROR: Cannot write " + CRASH_FILE);
                }
                stateFile.close();
            }
            else
            {
                logMsg(logFile, "[LOGGER] Normal exit (no state file). No recovery needed.");
            }

            break;
        }
    }

    CloseHandle(hMain);
    logMsg(logFile, "[LOGGER] Logger exiting.");
    return 0;
}