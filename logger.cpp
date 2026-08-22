#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <string>
#define NOMINMAX
#include <windows.h>

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

using namespace std;

const string STATE_FILE   = ".repo_state.tmp";
const string CRASH_FILE   = "crash_recovery.txt";
const string LOG_FILE     = "logger.log";
const string OPQ_FILE     = ".opqueue.tmp";      // main.exe 写入的操作队列
const string OPQ_PROC     = ".opqueue.proc";     // 处理中的队列副本
const string MAINERROR_FILE = ".mainerror";      // 崩溃标记（供 main.exe 下次启动时触发恢复）

//生成日志
string nowStr()
{
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

//写日志到文件
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

    // 诊断：在 logger 启动时写入启动指示文件，帮助确定正在运行的二进制
    ofstream started("logger_started.pid", ios::app);
    if (started.is_open()) {
        started << nowStr() << " PID=" << GetCurrentProcessId() << endl;
        started.close();
    }

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
    // 需要同时请求查询退出码的权限（PROCESS_QUERY_LIMITED_INFORMATION）和同步权限
    // 以便 GetExitCodeProcess 能成功返回主进程状态
#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif
    HANDLE hMain = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, mainPid);
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

        // 3. 检查 main.exe 是否还活着，优先使用 WaitForSingleObject 避免对查询权限的依赖
        DWORD waitRes = WaitForSingleObject(hMain, 0);
        if (waitRes == WAIT_FAILED)
        {
            DWORD err = GetLastError();
            LPSTR msgBuf = NULL;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msgBuf, 0, NULL);
            string errMsg = "";
            if (msgBuf)
            {
                errMsg = string(msgBuf);
                LocalFree(msgBuf);
            }
            logMsg(logFile, string("[LOGGER] WARN: WaitForSingleObject failed, code=") + to_string(err) + " msg=" + errMsg);
            break;
        }

        if (waitRes == WAIT_OBJECT_0)
        {
            // main.exe 已退出，处理最后的操作队列
            processOpQueue(logFile);

            // 尝试读取退出码（若权限允许）
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hMain, &exitCode))
            {
                logMsg(logFile, string("[LOGGER] Main process exited (code=") + to_string(exitCode) + ")");
            }
            else
            {
                DWORD err = GetLastError();
                logMsg(logFile, string("[LOGGER] Main process exited (exit code unknown, GetExitCodeProcess failed code=") + to_string(err) + ")");
            }

            // 检查状态文件 → 存在说明是崩溃
            ifstream stateFile(STATE_FILE);
            if (stateFile.is_open())
            {
                logMsg(logFile, "[LOGGER] CRASH DETECTED! State file found, saving recovery...");

                // 创建崩溃标记 .mainerror，供 main.exe 下次启动时触发自动恢复
                ofstream marker(MAINERROR_FILE);
                if (marker.is_open())
                {
                    marker.close();
                    logMsg(logFile, "[LOGGER] Crash marker " + MAINERROR_FILE + " created");
                }
                else
                {
                    logMsg(logFile, "[LOGGER] ERROR: Cannot create " + MAINERROR_FILE + " marker");
                }

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