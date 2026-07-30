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
        logMsg(logFile, "ERROR: No main PID provided, exiting.");
        return 1;
    }

    logMsg(logFile, "======== Logger started, monitoring PID=" + to_string(mainPid) + " ========");

    // 打开 main.exe 进程句柄用于监控
    HANDLE hMain = OpenProcess(SYNCHRONIZE, FALSE, mainPid);
    if (hMain == NULL)
    {
        logMsg(logFile, "ERROR: Cannot open main process (PID="
               + to_string(mainPid) + "), code=" + to_string(GetLastError()));
        return 1;
    }

    int heartbeat = 0;
    while (true)
    {
        this_thread::sleep_for(chrono::seconds(2));

        logMsg(logFile, "Heartbeat #" + to_string(++heartbeat)
               + " | monitoring PID=" + to_string(mainPid));

        // 检查 main.exe 是否还活着
        DWORD exitCode;
        if (!GetExitCodeProcess(hMain, &exitCode))
        {
            logMsg(logFile, "WARN: GetExitCodeProcess failed");
            break;
        }

        if (exitCode != STILL_ACTIVE)
        {
            // main.exe 已退出
            logMsg(logFile, "Main process exited (code=" + to_string(exitCode) + ")");

            // 检查状态文件是否存在 → 存在说明是崩溃
            ifstream stateFile(STATE_FILE);
            if (stateFile.is_open())
            {
                logMsg(logFile, "CRASH DETECTED! State file found, saving crash recovery...");

                ofstream crash(CRASH_FILE);
                if (crash.is_open())
                {
                    string line;
                    while (getline(stateFile, line))
                        crash << line << endl;
                    crash.close();
                    logMsg(logFile, "Crash recovery saved to " + CRASH_FILE);
                }
                else
                {
                    logMsg(logFile, "ERROR: Cannot write " + CRASH_FILE);
                }
                stateFile.close();
            }
            else
            {
                logMsg(logFile, "Normal exit (no state file). No recovery needed.");
            }

            break;
        }
    }

    CloseHandle(hMain);
    logMsg(logFile, "Logger exiting.");
    return 0;
}


