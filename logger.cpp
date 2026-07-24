#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <string>
#include <windows.h>

using namespace std;

int main()
{
    string logFileName = "logger.log";
    ofstream logFile(logFileName, ios::app);
    if (!logFile.is_open())
        return 1;

    int heartbeatCount = 0;
    while (true)
    {
        time_t now = time(nullptr);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));

        logFile << "[" << timeStr << "] Heartbeat #" << ++heartbeatCount << endl;
        logFile.flush();

        this_thread::sleep_for(chrono::seconds(2));
    }

    return 0;
}
