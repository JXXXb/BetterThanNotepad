#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#define NOMINMAX
#include<windows.h>
#include<ctime>
#include<mutex>
#include<thread>
#include<atomic>
#include<chrono>
#include<limits>

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

using namespace std;

vector<string> Data;

// === 看门狗/ 崩溃恢复系统（必须在函数定义之前）===
std::mutex dataMutex;
const string MAINERROR_FILE = ".mainerror";

// 前置声明：将操作事件写入操作队列（logger.exe 会读取并记录）
void logOp(const string &action, const string &detail);

//增加项函数
bool add(string str)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    Data.push_back(str);
    // 记录操作日志：新增项，记录新索引和当前总数
    try {
        logOp("ADD", "index=" + to_string((int)Data.size() - 1) + " total=" + to_string((int)Data.size()));
    } catch(...) {}
    return true;
}

//删除项函数
bool pop(int idx)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    if(idx >= 0 && idx < Data.size())
    {
        Data.erase(Data.begin() + idx);
        // 记录操作日志：删除项，记录被删除的索引和删除后的总数
        try {
            logOp("DELETE", "index=" + to_string(idx) + " total=" + to_string((int)Data.size()));
        } catch(...) {}
        return true;
    }
    return false;
}

//修改项函数
bool mod(int idx, string str)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    if(idx >= 0 && idx < Data.size())
    {
        Data[idx] = str;
        // 记录操作日志：修改项，记录索引与新值的简要信息
        try {
            logOp("MODIFY", "index=" + to_string(idx) + " new=" + str);
        } catch(...) {}
        return true;
    }
    return false;
}

//查找项函数
int find(string str)
{
    for(int i = 0; i < Data.size(); i++)
    {
        if (Data[i].find(str) != std::string::npos)//Data[i]包含str
        {
            return i;
        }
    }
    return -1;
}

//模糊查找项函数
int find_fuzzy(string str)
{
    for(int i = 0; i < Data.size(); i++)
    {
        if (Data[i].find(str) != std::string::npos)//Data[i]包含str
        {
            cout << "索引为: " << i << " 原文为:" << Data[i] << endl;
        }
    }
    return -1;
}

string currentRepoName = "";

//  看门狗运行状态
HANDLE hLoggerProcess = NULL;
const string STATE_FILE = ".repo_state.tmp";

//  自动保存（前置声明 write）
bool write();

// 前置声明，供 watchLoggerFunc 使用
bool startLoggerProcess();

std::thread autoSaveThread;
std::atomic<bool> autoSaveRunning(false);

// main 监控 logger 是否存活，挂了就自动重启
std::thread loggerWatchThread;
std::atomic<bool> loggerWatchRunning(false);

void autoSaveFunc()
{
    while (autoSaveRunning.load())
    {
        // 每1秒检查一次退出标志，最多等20秒后执行保存
        for (int i = 0; i < 20 && autoSaveRunning.load(); i++)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!autoSaveRunning.load()) break;
        std::lock_guard<std::mutex> lock(dataMutex);
        write();
    }
}

// main 端监控 logger.exe，挂了就重启
void watchLoggerFunc()
{
    while (loggerWatchRunning.load())
    {
        // 每3秒检查一次
        for (int i = 0; i < 3 && loggerWatchRunning.load(); i++)
            std::this_thread::sleep_for(std::chrono::seconds(3));
        if (!loggerWatchRunning.load()) break;

        if (hLoggerProcess == NULL) continue;

        DWORD exitCode;
        if (GetExitCodeProcess(hLoggerProcess, &exitCode) && exitCode != STILL_ACTIVE)
        {
            CloseHandle(hLoggerProcess);
            hLoggerProcess = NULL;

            cout << endl <<  "[看门狗] logger.exe 已终止，正在重启..." << endl << "请继续输入：";
            if (startLoggerProcess())
                cout << endl << "[看门狗] logger.exe 已重新启动" << endl << "请继续输入：";
            else
                cout << endl << "[看门狗] logger.exe 重启失败" << endl << "请继续输入：";
        }
    }
}

//列出所有项
bool listItems()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    if (Data.empty())
    {
        cout << "存储库为空" << endl;
        return false;
    }
    for (int i = 0; i < Data.size(); i++)
    {
        cout << i << "  " << Data[i] << endl;
    }
    cout << "共" << Data.size() << "条数据" << endl;
    return true;
}

//写入
bool write()
{
    if(currentRepoName.empty())
        return false;

    ofstream out(currentRepoName + ".txt");
    if(!out.is_open())
    {
        return false;
    }
    for(int i = 0; i < Data.size(); i++)
    {
        out << i << "@~" << Data[i] << endl;
    }
    out.close();
    return true;
}

//创建文件
bool repositoryExists(const string &filename)
{
    ifstream in(filename + ".txt");
    return in.is_open();
}

//读取存储库
bool loadRepo()
{
    string filename;
    cout << "输入要读取的存储库名：";
    getline(cin, filename);
    ifstream in(filename + ".txt");
    if(!in.is_open())
        return false;

    std::lock_guard<std::mutex> lock(dataMutex);
    Data.clear();
    currentRepoName = filename;

    string line;
    while(getline(in, line))
    {
        if(line.empty())
            continue;
        size_t pos = line.find("@~");
        string value;
        if(pos != string::npos)
            value = line.substr(pos + 2);
        else
            value = line;
        Data.push_back(value);
    }
    in.close();
    return true;
}

//创建存储库
bool createRepo()
{
    string filename;
    while(true)
    {
        cout << "输入新建存储库名：";
        getline(cin, filename);
        if(filename.empty())
            return false;
        if(repositoryExists(filename))
        {
            char confirm;
            cout << "存储库已存在，是否覆盖现有文件？(y/n):";
            cin >> confirm;
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if(confirm == 'y' || confirm == 'Y')
                break;
            continue;
        }
        break;
    }

    std::lock_guard<std::mutex> lock(dataMutex);
    vector<string>().swap(Data); //清空Data
    currentRepoName = filename;
    write();
    return true;
}

//=== 状态文件：用于崩溃恢复 ===
void saveState()
{
    if (currentRepoName.empty()) return;
    std::lock_guard<std::mutex> lock(dataMutex);
    ofstream state(STATE_FILE);
    if (!state.is_open()) return;
    state << currentRepoName << endl;
    for (const auto& item : Data)
        state << item << endl;
    state.close();
}

void deleteState()
{
    DeleteFileA(STATE_FILE.c_str());
}

// === 将操作事件写入操作队列（logger.exe 会读取并记录）===
void logOp(const string &action, const string &detail = "")
{
    ofstream q(".opqueue.tmp", ios::app);
    if (!q.is_open()) return;
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    q << "[" << buf << "] " << action;
    if (!detail.empty())
        q << " | " << detail;
    q << endl;
    q.close();
}

// === 启动日志进程（看门狗，传入 main.exe 的 PID）===
bool startLoggerProcess()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != wstring::npos)
        dir = dir.substr(0, pos + 1);
    wstring loggerPath = dir + L"logger.exe";

    // 把当前进程 PID 传给 logger.exe
    // 使用 lpApplicationName + 可修改的命令行缓冲区以避免 CreateProcessW 在某些环境下失败
    wstring cmdLine = L"\"" + loggerPath + L"\" " + to_wstring(GetCurrentProcessId());
    // CreateProcessW 可能会修改命令行缓冲区，传递可写的 wchar_t*
    vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { 0 };
    // 诊断：将即将执行的命令行与路径写入调试日志，便于在 VS 环境下追踪
    ofstream dbglog(".start_logger_debug.log", ios::app);
    if (dbglog.is_open()) {
        time_t now = time(nullptr);
        char tbuf[64];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        // 将 wstring 转为 UTF-8
        int n1 = WideCharToMultiByte(CP_UTF8, 0, loggerPath.c_str(), -1, NULL, 0, NULL, NULL);
        string loggerPathUtf8(n1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, loggerPath.c_str(), -1, &loggerPathUtf8[0], n1, NULL, NULL);
        int n2 = WideCharToMultiByte(CP_UTF8, 0, cmdLine.c_str(), -1, NULL, 0, NULL, NULL);
        string cmdLineUtf8(n2, '\0');
        WideCharToMultiByte(CP_UTF8, 0, cmdLine.c_str(), -1, &cmdLineUtf8[0], n2, NULL, NULL);
        dbglog << "[" << tbuf << "] loggerPath=" << loggerPathUtf8 << " cmdLine=" << cmdLineUtf8 << endl;
        dbglog.close();
    }

    // DETACHED_PROCESS: logger 不属于本控制台，Ctrl+C/关窗口不会杀死它
    if (!CreateProcessW(loggerPath.c_str(), cmdBuf.data(), NULL, NULL, FALSE,
                        DETACHED_PROCESS, NULL, dir.c_str(), &si, &pi))
    {
        DWORD err = GetLastError();

        // 尝试获取系统错误描述并写入诊断文件，便于在 VS 调试器中分析
        LPWSTR msgBuf = NULL;
        DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
        FormatMessageW(flags, NULL, err, lang, (LPWSTR)&msgBuf, 0, NULL);

        // 将错误信息转换为 UTF-8 narrow string
        string errMsg = "";
        if (msgBuf)
        {
            int needed = WideCharToMultiByte(CP_UTF8, 0, msgBuf, -1, NULL, 0, NULL, NULL);
            if (needed > 0)
            {
                vector<char> buf(needed);
                WideCharToMultiByte(CP_UTF8, 0, msgBuf, -1, buf.data(), needed, NULL, NULL);
                errMsg = string(buf.data());
            }
            LocalFree(msgBuf);
        }

        // 写入诊断日志
        ofstream errlog(".start_logger_error.log", ios::app);
        if (errlog.is_open())
        {
            time_t now = time(nullptr);
            char tbuf[64];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            errlog << "[" << tbuf << "] CreateProcessW failed, GetLastError=" << err;
            if (!errMsg.empty()) errlog << " | " << errMsg;
            errlog << endl;
            errlog.close();
        }

        // 控制台也输出简单信息
        wcout << L"[看门狗] CreateProcessW 启动 logger.exe 失败，GetLastError=" << err << endl;
        return false;
    }
    hLoggerProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    return true;
}

// === 正常退出：停止日志进程并清理状态文件 ===
void stopLogger()
{
    // 停止自动保存
    autoSaveRunning.store(false);
    if (autoSaveThread.joinable())
        autoSaveThread.join();

    // 停止 logger 存活监控
    loggerWatchRunning.store(false);
    if (loggerWatchThread.joinable())
        loggerWatchThread.join();

    deleteState();  // 先删状态文件，这样 logger 判定为正常退出
    if (hLoggerProcess != NULL)
    {
        TerminateProcess(hLoggerProcess, 0);
        CloseHandle(hLoggerProcess);
        hLoggerProcess = NULL;
    }
    // 清理可能残留的崩溃标记文件
    DeleteFileA(MAINERROR_FILE.c_str());
}

// 返回 true 表示已恢复，可跳过存储库选择
bool checkAndRecoverCrash()
{
    // 检查 .mainerror 崩溃标记是否存在
    ifstream mainerrorFile(MAINERROR_FILE);
    if (!mainerrorFile.is_open())
        return false;
    mainerrorFile.close();

    // 先删除崩溃标记文件
    DeleteFileA(MAINERROR_FILE.c_str());

    cout << "检测到上次异常退出（.mainerror）。" << endl;

    // 检查日志文件是否存在
    ifstream logFile("logger.log");
    if (!logFile.is_open())
    {
        cout << "未找到日志文件，无法恢复数据。" << endl;
        return false;
    }
    logFile.close();

    // 日志存在 → 从状态文件恢复内容
    cout << "正在从状态文件恢复数据..." << endl;

    ifstream stateFile(STATE_FILE);
    if (!stateFile.is_open())
    {
        cout << "未找到状态文件，无法恢复数据。" << endl;
        return false;
    }

    string repoName;
    if (!getline(stateFile, repoName) || repoName.empty())
    {
        cout << "状态文件损坏（无存储库名），无法恢复。" << endl;
        stateFile.close();
        return false;
    }

    cout << "存储库: " << repoName << endl;

    Data.clear();
    currentRepoName = repoName;

    string line;
    while (getline(stateFile, line))
    {
        if (!line.empty())
            Data.push_back(line);
    }
    stateFile.close();

    // 立即执行 write 将恢复的数据写入正常存储文件
    if (write())
        cout << "已恢复 " << Data.size() << " 条数据到存储库" << endl;
    else
        cout << "恢复写入失败，数据保留在内存中" << endl;

    return true;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 确保程序以管理员权限运行；如果当前未提升，则通过 ShellExecuteW 的 "runas" 重新以管理员身份启动以触发 UAC。
    // This guarantees a UAC prompt when double-clicking the exe or launching from Explorer.
    auto isElevated = []() -> bool {
        BOOL elevated = FALSE;
        HANDLE token = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            TOKEN_ELEVATION te = {0};
            DWORD ret = 0;
            if (GetTokenInformation(token, TokenElevation, &te, sizeof(te), &ret)) {
                elevated = te.TokenIsElevated;
            }
            CloseHandle(token);
        }
        return elevated == TRUE;
    };

    if (!isElevated()) {
        // 尝试以管理员权限重新启动自身
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        // Preserve current working directory and command-line arguments are not required here
        HINSTANCE h = ShellExecuteW(NULL, L"runas", exePath, NULL, NULL, SW_SHOWNORMAL);
        // 如果用户同意提升，新的提升进程会接替运行；当前非提升实例退出。
        if ((INT_PTR)h > 32) {
            return 0;
        }
        // 如果用户取消或提升失败，则继续以非管理员权限运行（避免死循环）。
    }

    //启动时检查崩溃恢复
    bool recovered = checkAndRecoverCrash();

    int operation = -1;//控制指令
    string tmpaddcin = "";//临时存储添加项/修改项
    string str = "";//临时存储查找项
    int idx = 0;//临时存储索引

    //如果已从崩溃恢复，跳过存储库选择
    if (recovered)
    {
        operation = 0;
        cout << "当前存储库为: " << currentRepoName << endl;
    }

    while(operation != 0)
    {
        switch(operation)
        {
            case -1:
                cout << "选择操作:1.读取存储库, 2.新建存储库, 0.退出" << endl;
                if (!(cin >> operation))
                {
                    cin.clear();
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    cout << "无效输入，请输入数字" << endl;
                    operation = -1;
                }
                else
                {
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                }
                break;
            case 1:
                if(loadRepo())
                {
                    cout << "读取成功" << endl;
                    cout << "当前存储库为: " << currentRepoName << endl;
                    operation = 0;
                }
                else
                {
                    cout << "读取失败" << endl;
                    operation = -1;
                }
                break;
            case 2:
                if(createRepo())
                {
                    cout << "已创建并切换到存储库: " << currentRepoName << endl;
                    operation = 0;
                }
                else
                {
                    cout << "新建存储库失败" << endl;
                    operation = -1;
                }
                break;
            default:
                operation = -1;
                break;
        }
    }

    if(currentRepoName.empty())
    {
        cout << "Exit program" << endl;
        return 0;
    }

    //启动日志进程（看门狗） 
    if (startLoggerProcess())
    {
        cout << "[看门狗] 日志进程已启动，崩溃保护已激活" << endl;
        saveState();  // 写入初始状态
    }
    else
    {
        cout << "[看门狗] 警告：无法启动日志进程，崩溃保护未激活" << endl;
    }

    // 启动自动保存（每20秒）
    autoSaveRunning.store(true);
    autoSaveThread = std::thread(autoSaveFunc);
    cout << "[自动保存] 已开启, 每20秒自动保存一次" << endl;

    // 启动 logger 存活监控（main 端双向检验）
    loggerWatchRunning.store(true);
    loggerWatchThread = std::thread(watchLoggerFunc);

    operation = -1;
    tmpaddcin = "";

    while(true)
    {
        switch(operation)
        {
            case -1:
                cout << "选择操作:1.增加项, 2.删除项, 3.修改项, 4.查找项, 5.模糊查找, 6.读取存储库, 7.新建存储库, 0.退出, 8.列出所有项" << endl;
                if (!(cin >> operation))
                {
                    cin.clear();
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    cout << "无效输入，请输入数字" << endl;
                    operation = -1;
                }
                else
                {
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                }
                break;
            case 1:
                cout << "连续写入,输入exit退出" << endl;
                while(true)
                {
                    cout << "添加数据:";
                    getline(cin, tmpaddcin);
                    if(tmpaddcin == "exit")
                        break;
                    add(tmpaddcin);
                }
                // 在 add() 内部已记录每次添加的日志，这里统一保存状态
                saveState();
                operation = -1;
                break;
            case 2:
                cout << "连续删除, 输入exit退出" << endl;
                while(true)
                {
                    cout << "删除项的索引:";
                    string input;
                    getline(cin, input);
                    if(input == "exit")
                        break;
                    if(input.empty())
                        continue;
                    try {
                        int didx = stoi(input);
                        if(!pop(didx))
                            cout << "索引超出范围" << endl;
                        else
                            ; // pop() 已在内部记录日志
                    } catch(...) {
                        cout << "无效的索引" << endl;
                    }
                }
                // 删除操作已在 pop() 内部记录日志，统一保存状态
                saveState();
                operation = -1;
                break;
            case 3:
                cout << "修改项索引:";
                if (!(cin >> idx))
                {
                    cin.clear();
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    cout << "无效的索引" << endl;
                    operation = -1;
                    break;
                }
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                cout << "新的数据:";
                getline(cin, tmpaddcin);
                // mod() 内部会记录日志
                if(!mod(idx, tmpaddcin))
                    cout << "索引超出范围" << endl;
                saveState();
                operation = -1;
                break;
            case 4:
                cout << "查找数据索引:";
                getline(cin, str);
                idx = find(str);
                if(idx != -1)
                    cout << "原文为:" << Data[idx] << endl;
                else
                    cout << "未找到" << endl;
                operation = -1;
                break;
            case 5:
                cout << "模糊查找数据:";
                getline(cin, str);
                find_fuzzy(str);
                operation = -1;
                break;
            case 6:
                if(loadRepo())
                {
                    cout << "读取成功" << endl;
                    cout << "当前存储库为: " << currentRepoName << endl;
                    saveState();
                    logOp("LOAD_REPO", currentRepoName);
                    operation = -1;
                }
                else
                {
                    cout << "读取失败" << endl;
                    cout << "当前存储库为: " << currentRepoName << endl;
                    operation = -1;
                }
                break;
            case 7:
                if(createRepo())
                {
                    cout << "已创建并切换到存储库: " << currentRepoName << endl;
                    saveState();
                    logOp("CREATE_REPO", currentRepoName);
                }
                else
                {
                    cout << "新建存储库失败" << endl;
                }
                operation = -1;
                break;
            case 8:
                listItems();
                operation = -1;
                break;
            case 0:
                logOp("EXIT", "saving " + to_string(Data.size()) + " items to " + currentRepoName);
                //正常写入并退出 
                while(true)
                {
                    bool result = write();
                    if(result)
                    {
                        cout << Data.size() << "  已写入" << endl;
                        //正常退出：停止日志进程
                        stopLogger();
                        return 0;
                    }
                    else
                    {
                        cout << "写入文件失败，正在重写" << endl;
                    }
                    continue;
                }
                break;
            default:
                operation = -1;
                break;
        }
    }
}