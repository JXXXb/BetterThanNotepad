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
#include<filesystem>
#include<iomanip>
#include<charconv>
#include<cctype>

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

using std::cin;
using std::cout;
using std::string;
using std::vector;
using std::to_string;
using std::endl;
using std::wstring;
using std::wcout;
using std::to_wstring;
using std::ofstream;
using std::ios;
using std::ifstream;
namespace fs = std::filesystem;

vector<string> Data;

struct mwnum
{
    long long num;
    long long index;
};


// === 看门狗/ 崩溃恢复系统（必须在函数定义之前）===
std::mutex dataMutex;
const string MAINERROR_FILE = ".mainerror";

// 前置声明：将操作事件写入操作队列（logger.exe 会读取并记录）
bool logOp(const string &action, const string &detail = "");

// 前置声明：刷新崩溃恢复状态文件（add/pop/mod 每次变更后调用）
bool saveState();

//增加项函数：数据变更后同步刷新操作队列与崩溃恢复状态（均为尽力而为）
bool add(string str)
{
    int idx = 0;
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        Data.push_back(str);
        idx = (int)Data.size() - 1;
    }
    logOp("ADD", "index=" + to_string(idx) + " total=" + to_string((int)Data.size()));
    saveState();
    return true;
}

//删除项函数
bool pop(int idx)
{
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        if (idx < 0 || idx >= (int)Data.size())
            return false;
        Data.erase(Data.begin() + idx);
    }
    logOp("DELETE", "index=" + to_string(idx) + " total=" + to_string((int)Data.size()));
    saveState();
    return true;
}

//修改项函数
bool mod(int idx, string str)
{
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        if (idx < 0 || idx >= (int)Data.size())
            return false;
        Data[idx] = str;
    }
    logOp("MODIFY", "index=" + to_string(idx) + " new=" + str);
    saveState();
    return true;
}

//查找项函数：返回第一个包含关键字的项的下标，未找到返回 -1（加锁）
int find(string str)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    for (int i = 0; i < (int)Data.size(); i++)
    {
        if (Data[i].find(str) != std::string::npos)//Data[i]包含str
        {
            return i;
        }
    }
    return -1;
}

//模糊查找：返回所有包含关键字的下标（加锁、无副作用、按索引升序）。
// 修复：旧 find_fuzzy 只打印第一条就 return，无法展示全部匹配。
std::vector<int> fuzzyMatches(const string &key)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    std::vector<int> hits;
    for (int i = 0; i < (int)Data.size(); i++)
    {
        if (Data[i].find(key) != std::string::npos)
            hits.push_back(i);
    }
    return hits;
}

//打印匹配列表：带 1..N 序号，便于后续按序号选择删除
void printMatchList(const std::vector<int> &hits)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    for (size_t i = 0; i < hits.size(); i++)
    {
        cout << "  匹配 " << (i + 1) << ": 索引 " << hits[i]
             << "  原文: " << Data[hits[i]] << endl;
    }
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

// 将用户输入解析为索引：只接受十进制数字（允许首尾空白）。
// 负数/非数字一律返回 false（不再发生 “-1 → erase(begin()-1)” 这类越界崩溃）。
bool tryParseInt(const string &s, int &out)
{
    if (s.empty()) return false;
    size_t b = s.find_first_not_of(" \t");
    if (b == string::npos) return false;
    size_t e = s.find_last_not_of(" \t");
    string t = s.substr(b, e - b + 1);
    if (t.empty()) return false;

    size_t i = 0;
    if (t[0] == '+')
    {
        if (t.size() == 1) return false;
        i = 1;
    }
    else if (t[0] == '-')
    {
        return false;   // 索引不允许为负
    }

    long long v = 0;
    for (; i < t.size(); i++)
    {
        if (t[i] < '0' || t[i] > '9') return false;
        v = v * 10 + (t[i] - '0');
        if (v > (long long)std::numeric_limits<int>::max()) return false; // 防溢出
    }
    out = (int)v;
    return true;
}

// 加锁保存到当前存储库 .stxt（write 的加锁版本）。
// 注意：调用方不得再持有 dataMutex，否则会死锁。
bool saveRepo()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return write();
}

// === 统计当前存储库信息（主菜单 10，新功能）===
void showStats()
{
    long long datanumsum = 0;
    mwnum datanummax{};
    mwnum datanummin{};
    size_t numberCount = 0;
    size_t invalidNumberCount = 0;
    bool hasNumber = false;
    bool sumOverflow = false;
    std::lock_guard<std::mutex> lock(dataMutex);
    if (Data.empty())
    {
        cout << "当前存储库为空，暂无统计信息" << endl;
        return;
    }
    size_t totalChars = 0;
    size_t longest = 0, shortest = 0;
    for (size_t i = 0; i < Data.size(); i++)
    {
        totalChars += Data[i].size();
        if (Data[i].size() > Data[longest].size())  longest  = i;
        if (Data[i].size() < Data[shortest].size()) shortest = i;
    }
    double avg = (double)totalChars / (double)Data.size();
    cout << "存储库: " << currentRepoName << endl;
    cout << "条目总数: " << Data.size() << endl;
    cout << "字符总数(不含索引前缀): " << totalChars << endl;
    cout << std::fixed << std::setprecision(1) << "平均每条字符数: " << avg << std::setprecision(0) << endl;
    cout << "最长条目: 索引 " << longest  << " (" << Data[longest].size()  << " 字符) " << Data[longest]  << endl;
    cout << "最短条目: 索引 " << shortest << " (" << Data[shortest].size() << " 字符) " << Data[shortest] << endl;

    // 提取每条数据中的有符号十进制整数。超出 long long 范围的数字会被跳过，
    // 以免统计功能因一条用户输入而抛出异常或发生未定义行为。
    for (size_t i = 0; i < Data.size(); i++)
    {
        const std::string& str = Data[i];
        for (size_t j = 0; j < str.size(); )
        {
            const bool negative = str[j] == '-' && j + 1 < str.size()
                && std::isdigit(static_cast<unsigned char>(str[j + 1]));
            if (!negative && !std::isdigit(static_cast<unsigned char>(str[j])))
            {
                ++j;
                continue;
            }

            const size_t begin = j;
            if (negative) ++j;
            while (j < str.size() && std::isdigit(static_cast<unsigned char>(str[j])))
                ++j;

            long long value = 0;
            const char* first = str.data() + begin;
            const char* last = str.data() + j;
            const auto [ptr, ec] = std::from_chars(first, last, value);
            if (ec != std::errc() || ptr != last)
            {
                ++invalidNumberCount;
                continue;
            }

            if (!hasNumber || value > datanummax.num)
                datanummax = { value, static_cast<long long>(i) };
            if (!hasNumber || value < datanummin.num)
                datanummin = { value, static_cast<long long>(i) };
            hasNumber = true;
            ++numberCount;

            if ((value > 0 && datanumsum > std::numeric_limits<long long>::max() - value)
                || (value < 0 && datanumsum < std::numeric_limits<long long>::min() - value))
                sumOverflow = true;
            else if (!sumOverflow)
                datanumsum += value;
        }
    }

    if (!hasNumber)
    {
        cout << "未找到可统计的 long long 范围内数字" << endl;
        return;
    }
    if (sumOverflow)
        cout << "数字总和超出 long long 范围，无法计算总和和平均值" << endl;
    else
    {
        cout << "数字总和为 " << datanumsum << endl;
        cout << std::fixed << std::setprecision(1)
             << "数字平均值为 " << static_cast<double>(datanumsum) / numberCount
             << std::setprecision(0) << endl;
    }
    cout << "数字最大值为 " << datanummax.num << "，索引为 " << datanummax.index
         << "，条目为 " << Data[datanummax.index] << endl;
    cout << "数字最小值为 " << datanummin.num << "，索引为 " << datanummin.index
         << "，条目为 " << Data[datanummin.index] << endl;
    if (invalidNumberCount != 0)
        cout << "已跳过 " << invalidNumberCount << " 个超出 long long 范围的数字" << endl;
}

bool autoSaveFunc()
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
        if (!write())
            return false;
    }
    return true;
}

// main 端监控 logger.exe，挂了就重启
bool watchLoggerFunc()
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

            cout << endl <<  "[logger.exe] logger.exe 已终止，正在重启..." << endl << "请继续输入：";
            if (startLoggerProcess())
                cout << endl << "[logger.exe] logger.exe 已重新启动" << endl << "请继续输入：";
            else
                cout << endl << "[logger.exe] logger.exe 重启失败" << endl << "请继续输入：";
        }
    }
    return true;
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

    const string target = currentRepoName + ".stxt";
    const string temporary = target + ".tmp";
    std::ofstream out(temporary, ios::trunc);
    if(!out.is_open())
    {
        return false;
    }
    for(int i = 0; i < Data.size(); i++)
    {
        out << i << "@~" << Data[i] << endl;
    }
    out.close();
    if (!out.good())
    {
        DeleteFileA(temporary.c_str());
        return false;
    }
    if (!MoveFileExA(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileA(temporary.c_str());
        return false;
    }
    return true;
}

//创建文件
bool repositoryExists(const string &filename)
{
    std::ifstream in(filename + ".stxt");
    return in.is_open();
}

//列出存储库
string listRepo()
{
    string repoList;
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("*.stxt", &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return repoList;
    do
    {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        if (!repoList.empty())
            repoList += "\n";
        repoList += string(findData.cFileName);
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
    return repoList;
}

//读取存储库
bool loadRepo()
{
    string filename;
    cout << "输入要读取的存储库名：";
    std::getline(cin, filename);
    if (filename.empty())
    {
        cout << "名称为空，未读取任何存储库" << endl;
        return false;
    }
    std::ifstream in(filename + ".stxt");
    if(!in.is_open())
        return false;

    std::lock_guard<std::mutex> lock(dataMutex);
    Data.clear();
    currentRepoName = filename;

    string line;
    while(std::getline(in, line))
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
    return logOp("LOAD_REPO", currentRepoName);
}

//创建存储库
bool createRepo()
{
    string filename;
    while(true)
    {
        cout << "输入新建存储库名：";
        std::getline(cin, filename);
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
    return write() && logOp("CREATE_REPO", currentRepoName);
}

//删除存储库
bool deleteRepo(string filename)
{
    string filepath = filename + ".stxt";
    if (fs::exists(filepath))
    {
        if (fs::remove(filepath))
        {
                std::cout << "文件删除成功！" << std::endl;
                return 1;
        }
        else
        {
            std::cout << "文件存在，但删除失败（可能被占用或无权限）" << std::endl;
            return 0;
        }
    }
    else
    {
        std::cout << "文件不存在" << std::endl;
        return 0;
    }
}

//=== 状态文件：用于崩溃恢复 ===
bool saveState()
{
    if (currentRepoName.empty()) return false;
    std::lock_guard<std::mutex> lock(dataMutex);

    // 先创建/刷新 .mainerror 作为“尚未正常退出”的提交点：只要标记存在，
    // 下次启动就会尝试用最近一次完整快照恢复；正常退出时 stopLogger 会删除它。
    // 这样即使进程在“提交快照”的瞬间崩溃，也不会出现“快照有、标记没有”的漏恢复窗口。
    ofstream marker(MAINERROR_FILE, ios::trunc);
    if (!marker.is_open())
        return false;
    marker.close();

    const string temporary = STATE_FILE + ".new";
    ofstream state(temporary, ios::trunc);
    if (!state.is_open())
    {
        DeleteFileA(temporary.c_str());
        return false;
    }
    state << currentRepoName << endl;
    for (const auto& item : Data)
        state << item << endl;
    state.close();
    if (!state.good())
    {
        DeleteFileA(temporary.c_str());
        return false;
    }
    if (!MoveFileExA(temporary.c_str(), STATE_FILE.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileA(temporary.c_str());
        return false;
    }
    return true;
}

bool deleteState()
{
    if (DeleteFileA(STATE_FILE.c_str()))
        return true;
    return GetLastError() == ERROR_FILE_NOT_FOUND;
}

// 格式化为 "YYYY-MM-DD HH:MM:SS"(MSVC 下使用安全的 localtime_s)
bool formatNow(char *buf, size_t size)
{
    if (buf == nullptr || size == 0)
        return false;
    time_t now = time(nullptr);
#ifdef _MSC_VER
    struct tm tmbuf;
    if (localtime_s(&tmbuf, &now) != 0)
        return false;
    return strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tmbuf) != 0;
#else
    struct tm *localTime = localtime(&now);
    return localTime != nullptr && strftime(buf, size, "%Y-%m-%d %H:%M:%S", localTime) != 0;
#endif
}

// === 将操作事件写入操作队列（logger.exe 会读取并记录）===
bool logOp(const string &action, const string &detail)
{
    std::ofstream q(".opqueue.tmp", ios::app);
    if (!q.is_open()) return false;
    char buf[64];
    if (!formatNow(buf, sizeof(buf))) return false;
    q << "[" << buf << "] " << action;
    if (!detail.empty())
        q << " | " << detail;
    q << endl;
    q.close();
    return q.good();
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
        char tbuf[64];
        formatNow(tbuf, sizeof(tbuf));
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
    // 继承 main 的当前工作目录。main 和 logger 都使用相对路径访问
    // 状态、队列和日志文件，不能把 logger 的工作目录改成 exe 所在目录。
    if (!CreateProcessW(loggerPath.c_str(), cmdBuf.data(), NULL, NULL, FALSE,
                        DETACHED_PROCESS, NULL, NULL, &si, &pi))
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
            char tbuf[64];
            formatNow(tbuf, sizeof(tbuf));
            errlog << "[" << tbuf << "] CreateProcessW failed, GetLastError=" << err;
            if (!errMsg.empty()) errlog << " | " << errMsg;
            errlog << endl;
            errlog.close();
        }

        // 控制台也输出简单信息
        wcout << L"[logger.exe] CreateProcessW 启动 logger.exe 失败，GetLastError=" << err << endl;
        return false;
    }
    hLoggerProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    return true;
}

// === 正常退出：停止日志进程并清理状态文件 ===
bool stopLogger()
{
    bool success = true;
    // 停止自动保存
    autoSaveRunning.store(false);
    if (autoSaveThread.joinable())
        autoSaveThread.join();

    // 停止 logger 存活监控
    loggerWatchRunning.store(false);
    if (loggerWatchThread.joinable())
        loggerWatchThread.join();

    success = deleteState();  // 先删状态文件，这样 logger 判定为正常退出
    if (hLoggerProcess != NULL)
    {
        TerminateProcess(hLoggerProcess, 0);
        CloseHandle(hLoggerProcess);
        hLoggerProcess = NULL;
    }
    // 清理可能残留的崩溃标记文件
    if (!DeleteFileA(MAINERROR_FILE.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
        success = false;
    return success;
}

// 退出逻辑已内联到主菜单 case 0（带失败重试与用户选择，避免死循环）。
// bool exitProgram() 已移除，见 case 0。

// 返回 true 表示已恢复，可跳过存储库选择
bool checkAndRecoverCrash()
{
    // 检查 .mainerror 崩溃标记是否存在
    ifstream mainerrorFile(MAINERROR_FILE);
    if (!mainerrorFile.is_open())
        return false;
    mainerrorFile.close();

    cout << "检测到上次异常退出（.mainerror）。" << endl;

    cout << "正在从状态文件恢复数据..." << endl;

    ifstream stateFile(STATE_FILE);
    if (!stateFile.is_open())
    {
        cout << "未找到状态文件，无法恢复数据。" << endl;
        DeleteFileA(MAINERROR_FILE.c_str());
        return false;
    }

    string repoName;
    if (!std::getline(stateFile, repoName) || repoName.empty())
    {
        cout << "状态文件损坏（无存储库名），无法恢复。" << endl;
        stateFile.close();
        DeleteFileA(MAINERROR_FILE.c_str());
        return false;
    }

    cout << "存储库: " << repoName << endl;

    Data.clear();
    currentRepoName = repoName;

    string line;
    while (std::getline(stateFile, line))
    {
        if (!line.empty())
            Data.push_back(line);
    }
    stateFile.close();

    // 立即执行 write 将恢复的数据写入正常存储文件（此刻线程未启动，加锁无冲突）
    if (saveRepo())
        cout << "已恢复 " << Data.size() << " 条数据到存储库" << endl;
    else
        cout << "恢复写入失败，数据保留在内存中" << endl;

    return true;
}

// 检查当前进程是否已以管理员权限运行
bool isElevatedProcess()
{
    BOOL elevated = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        TOKEN_ELEVATION te = {0};
        DWORD ret = 0;
        if (GetTokenInformation(token, TokenElevation, &te, sizeof(te), &ret))
            elevated = te.TokenIsElevated;
        CloseHandle(token);
    }
    return elevated == TRUE;
}

// 以管理员权限重新启动自身（触发 UAC）
// 返回 true 表示已成功启动新的提升实例，调用方应立即退出本（非提升）实例，
// 否则本实例会与提升实例同时运行，导致出现两个进程、两个控制台。
bool relaunchAsAdmin()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    HINSTANCE h = ShellExecuteW(NULL, L"runas", exePath, NULL, NULL, SW_SHOWNORMAL);
    // ShellExecuteW 返回值 > 32 表示成功启动（SE_ERR_* 错误码均 <= 32）
    return (INT_PTR)h > 32;
}

int main()
{
    //强制命令行字符为UTF_8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    //UAC提升权限：确保以管理员权限运行。
    // 定义 BTN_TEST_MODE 编译宏（仅自动化测试用）时跳过 UAC，方便无人值守运行；
    // 默认生产构建不受影响，仍会请求管理员权限。
#ifndef BTN_TEST_MODE
    while (true)
    {
        if (isElevatedProcess())
            break;          // 已提升，本实例继续运行

        if (relaunchAsAdmin())
            return 0;       // 已成功启动新的提升实例，本实例立即退出（修复两个控制台/两个进程）

        // 提升失败或用户取消了 UAC：询问是否重试
        cout << "UAC申请失败, 是否重试?(T/F)" << endl;
        char isagain = '\0';
        cin >> isagain;
        if (isagain == 'T' || isagain == 't')
        {
            cout << "正在重试...";
            continue;
        }
        else if (isagain == 'F' || isagain == 'f')
        {
            cout << "已放弃UAC";
            break;
        }
        else
        {
            cout << "输入非法字符, 默认重试...";
            continue;
        }
    }
#else
    cout << "[测试模式] BTN_TEST_MODE 已定义，跳过 UAC 提权" << endl;
#endif

    //启动时检查崩溃恢复
    bool recovered = checkAndRecoverCrash();

    string operation;//控制指令
    string tmpaddcin = "";//临时存储添加项/修改项
    string str = "";//临时存储查找项
    int idx = 0;//临时存储索引
    auto readOperation = [&]()
    {
        string input;
        if (!(cin >> input))
        {
            cin.clear();
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            cout << "无效输入，请重新输入" << endl;
            operation.clear();
            return;
        }
        cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        operation = input;
    };

    //如果已从崩溃恢复，跳过存储库选择
    if (recovered)
    {
        operation = "0";
        cout << "当前存储库为: " << currentRepoName << endl;
    }

    while(operation != "0")
    {
        if (operation.empty())
        {
            cout << "选择操作:1.读取存储库, 2.新建存储库, 3.列出存储库, 4.删除存储库, 0.退出" << endl << ">";
            readOperation();
        }
        else if (operation == "1")
        {
            if(loadRepo())
            {
                cout << "读取成功" << endl;
                cout << "当前存储库为: " << currentRepoName << endl;
                operation = "0";
            }
            else
            {
                cout << "读取失败" << endl;
                operation.clear();
            }
        }
        else if (operation == "2")
        {
            if(createRepo())
            {
                cout << "已创建并切换到存储库: " << currentRepoName << endl;
                operation = "0";
            }
            else
            {
                cout << "新建存储库失败" << endl;
                operation.clear();
            }
        }
        else if (operation == "3")
        {
            cout << "存储库列表:" << endl;
            cout << listRepo() << endl;
            operation.clear();
        }
        else if (operation == "4")
        {
            // 修复：旧逻辑删除失败后卡在 “是否重试” 无限循环，且完成后无法返回菜单。
            // 现在一次只处理一个名字，完成后总是回到菜单。
            cout << "请输入要删除的存储库名称（输入 exit 或直接回车返回菜单）:" << endl << ">";
            string deleteReponame = "";
            std::getline(cin, deleteReponame);
            if (deleteReponame.empty() || deleteReponame == "exit")
            {
                cout << "已取消删除" << endl;
            }
            else
            {
                deleteRepo(deleteReponame);   // 内部会打印成功/失败原因
            }
            operation.clear();
        }
        else
        {
            operation.clear();
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
        cout << "[logger.exe] 日志进程已启动，崩溃保护已激活" << endl;
        if (!saveState())
            cout << "[logger.exe] 初始状态保存失败" << endl;
    }
    else
    {
        cout << "[logger.exe] 警告：无法启动日志进程，崩溃保护未激活" << endl;
    }

    // 启动自动保存（每20秒）
    autoSaveRunning.store(true);
    autoSaveThread = std::thread(autoSaveFunc);
    cout << "[自动保存] 已开启, 每20秒自动保存一次" << endl;

    // 启动 logger 存活监控（main 端双向检验）
    loggerWatchRunning.store(true);
    loggerWatchThread = std::thread(watchLoggerFunc);

    operation.clear();
    tmpaddcin = "";

    string operation_list[11] = {"a", "d", "m", "f", "r", "n", "list-data", "list-repos", "stats", "q", "w"};
    cout << "选择操作:a.增加项, d.删除项, m.修改项, f.查找项, r.读取存储库, n.新建存储库, list-data.列出所有项, list-repos.列出存储库, stats.统计信息, q.退出, w.写入" << endl;
    while(true)
    {
        if (operation.empty())
        {
            cout << ">";
            getline(cin, operation);
        }
        else if (operation == operation_list[0])
        {
            cout << "连续写入, 输入 exit 结束" << endl;
            while(true)
            {
                cout << "添加数据:";
                std::getline(cin, tmpaddcin);
                if(tmpaddcin == "exit")
                    break;
                if(tmpaddcin.empty())
                {
                    continue;
                }
                if (!add(tmpaddcin))
                    cout << "添加失败" << endl;
            }
            operation.clear();
        }
        else if (operation == operation_list[1])
        {
            // 修复：旧实现只取首字符当索引（“12”会删第 1 条）、负号/越界会触发
            // Data.erase(begin()±n) 越界（未定义行为/崩溃），且直接改 Data 绕过了
            // pop() 的加锁与操作日志。现在：纯数字按完整索引删除；否则按内容
            // 查找，列出全部匹配后由用户确认，绝不删除未匹配到的项。
            cout << "连续删除：输入索引或内容，输入 exit 结束" << endl;
            while(true)
            {
                cout << "删除:";
                string input;
                std::getline(cin, input);
                if(input == "exit")
                    break;
                if(input.empty())
                    continue;

                int didx = -1;
                if (tryParseInt(input, didx))
                {
                    if (pop(didx))
                        cout << "已删除索引 " << didx << " 的项，剩余 " << Data.size() << " 条" << endl;
                    else
                        cout << "索引 " << didx << " 超出范围（当前共 " << Data.size()
                             << " 条，索引 0.." << (Data.empty() ? 0 : (int)Data.size() - 1) << "）" << endl;
                    continue;
                }

                // 不是合法索引 → 按内容查找，确认后删除
                vector<int> hits = fuzzyMatches(input);
                if (hits.empty())
                {
                    cout << "未找到包含 \"" << input << "\" 的项，未删除任何数据" << endl;
                    continue;
                }
                printMatchList(hits);
                if (hits.size() == 1)
                {
                    while(true)
                    {
                        cout << "是否删除以上匹配项?" << "T确认" << endl;
                        string ans;
                        std::getline(cin, ans);
                        if (ans == "T" || ans == "t" || ans == "y" || ans == "Y")
                        {
                            pop(hits[0]);
                            cout << "已删除索引 " << hits[0] << " 的项，剩余 " << Data.size() << " 条" << endl;
                            break;
                        }
                        else
                        {
                            cout << "已取消删除" << endl;
                            break;
                        }
                    }
                }
                else
                {
                    while(true)
                    {
                        cout << "有多个匹配，输入要删除的匹配序号(1.." << hits.size()
                             << ")，或输入 F 取消:";
                        string ans;
                        std::getline(cin, ans);
                        int sel = -1;
                        if (tryParseInt(ans, sel) && sel >= 1 && sel <= (int)hits.size())
                        {
                            int target = hits[sel - 1];
                            pop(target);
                            cout << "已删除索引 " << target << " 的项，剩余 " << Data.size() << " 条" << endl;
                            break;
                        }
                        if (ans == "F" || ans == "f" || ans == "n" || ans == "N")
                        {
                            cout << "已取消删除" << endl;
                            break;
                        }
                        cout << "无效输入，请输入 1.." << hits.size() << " 或 F" << endl;
                    }
                }
            }
            operation.clear();
        }
        else if (operation == operation_list[2])
        {
            cout << "修改项索引:";
            if (!(cin >> idx))
            {
                cin.clear();
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                // 旧的回退分支条件恒为假（死代码），且会用残留的 idx 越界访问 Data[idx]，
                // 空存储库时直接未定义行为。这里直接拒绝非法索引。
                cout << "无效的索引，请输入数字" << endl;
                operation.clear();
                continue;
            }
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (idx < 0)
            {
                cout << "索引不能为负数" << endl;
                operation.clear();
                continue;
            }
            cout << "新的数据:";
            std::getline(cin, tmpaddcin);
            if (tmpaddcin.empty())
            {
                cout << "内容为空，未修改" << endl;
                operation.clear();
                continue;
            }
            if(!mod(idx, tmpaddcin))
                cout << "修改失败：索引 " << idx << " 超出范围（当前共 " << Data.size() << " 条）" << endl;
            else
                cout << "已修改索引 " << idx << " 的项" << endl;
            operation.clear();
        }
        else if (operation == operation_list[3])
        {
            cout << "输入要查找的内容:";
            std::getline(cin, str);
            if (str.empty())
            {
                cout << "内容为空，未查找" << endl;
                operation.clear();
                continue;
            }
            idx = find(str);
            if(idx != -1)
                cout << "索引: " << idx << "  原文: " << Data[idx] << endl;
            else
                cout << "未找到包含 \"" << str << "\" 的项" << endl;
            operation.clear();
        }
        else if (operation == operation_list[4])
        {
            if (!saveRepo())
            {
                cout << "当前存储库写入失败，无法读取" << endl;
                operation.clear();
                continue;
            }
            if(loadRepo())
            {
                cout << "读取成功" << endl;
                cout << "当前存储库为: " << currentRepoName << endl;
                if (!saveState())
                    cout << "存储库已读取，但状态或日志记录失败" << endl;
                operation.clear();
            }
            else
            {
                cout << "读取失败" << endl;
                cout << "当前存储库为: " << currentRepoName << endl;
                operation.clear();
            }
        }
        else if (operation == operation_list[5])
        {
            if (!saveRepo())
            {
                cout << "当前存储库写入失败，无法新建" << endl;
                operation.clear();
                continue;
            }
            if(createRepo())
            {
                cout << "已创建并切换到存储库: " << currentRepoName << endl;
                if (!saveState())
                    cout << "存储库已创建，但状态或日志记录失败" << endl;
            }
            else
            {
                cout << "新建存储库失败" << endl;
            }
            operation.clear();
        }
        else if (operation == operation_list[6])
        {
            listItems();
            operation.clear();
        }
        else if (operation == operation_list[7])
        {
            cout << "存储库列表:" << endl;
            cout << listRepo() << endl;
            operation.clear();
        }
        else if (operation == operation_list[8])
        {
            showStats();
            operation.clear();
        }
        if(operation == operation_list[9])
        {
            if(saveRepo())
            {
                cout << Data.size() << "条     " << sizeof(Data) <<" 字节已写入   " << endl;
                logOp("EXIT", "saving " + to_string(Data.size()) + " items to " + currentRepoName);
                stopLogger();
                cout << "[logger.exe]已停止" << endl << "[自动保存]已停止";
                return 0;
            }
            else
            {
                cout << "失败，是否重试(T确认)" << endl;
                char tryagain = 'F';
                if(tryagain == 'T' && tryagain == 't' && tryagain == 'y' && tryagain == 'Y')
                {
                    operation = operation_list[9];
                    break;
                }
            }
        }
        if(operation == operation_list[10])
        {
            if(saveRepo())
            {
                cout << Data.size() << "条     " << sizeof(Data) <<" 字节已写入   " << endl;
            }
            else
            {
                cout << "失败，是否重试(T确认)" << endl;
                char tryagain = 'F';
                if(tryagain == 'T' && tryagain == 't' && tryagain == 'y' && tryagain == 'Y')
                {
                    operation = operation_list[10];
                    break;
                }
            }
        }
        else
        {
            cout << "无效输入，请重新选择菜单操作" << endl;
            operation.clear();
        }
    }
}
