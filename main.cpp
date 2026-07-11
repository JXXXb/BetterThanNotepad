#include<iostream>
#include<fstream>
#include<string>
#include<vector>
using namespace std;

vector<string> Data;

//增加项函数
bool add(string str)
{
    Data.push_back(str);
    return true;
}

//删除项函数
bool pop(int idx)
{
    if(idx >= 0 && idx < Data.size())
    {
        Data.erase(Data.begin() + idx);
        return true;
    }
    return false;
}

//修改项函数
bool mod(int idx, string str)
{
    if(idx >= 0 && idx < Data.size())
    {
        Data[idx] = str;
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

int find_fuzzy(string str)
{
    for(int i = 0; i < Data.size(); i++)
    {
        if (Data[i].find(str) != std::string::npos)//Data[i]包含str
        {
            cout << "索引为: " << i << "原文为:" << Data[i] << endl;
        }
    }
    return -1;
}

string currentRepoName = "";

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
        out << i << " " << Data[i] << endl;
    }
    out.close();
    return true;
}

bool repositoryExists(const string &filename)
{
    ifstream in(filename + ".txt");
    return in.is_open();
}

bool loadRepo()
{
    string filename;
    cout << "输入要读取的存储库名：";
    cin >> filename;
    ifstream in(filename + ".txt");
    if(!in.is_open())
        return false;

    Data.clear();
    currentRepoName = filename;

    string line;
    while(getline(in, line))
    {
        if(line.empty())
            continue;
        size_t pos = line.find(' ');
        string value;
        if(pos != string::npos)
            value = line.substr(pos + 1);
        else
            value = line;
        Data.push_back(value);
    }
    in.close();
    return true;
}

bool createRepo()
{
    string filename;
    while(true)
    {
        cout << "输入新建存储库名：";
        cin >> filename;
        if(filename.empty())
            return false;
        if(repositoryExists(filename))
        {
            char confirm;
            cout << "存储库已存在，是否覆盖现有文件？(y/n):";
            cin >> confirm;
            if(confirm == 'y' || confirm == 'Y')
                break;
            continue;
        }
        break;
    }

    vector<string>().swap(Data); //清空Data
    currentRepoName = filename;
    return true;
}

int main()
{
    int operation = -1;//控制指令
    string tmpaddcin = "";//临时存储添加项/修改项
    string str = "";//临时存储查找项
    int idx = 0;//临时存储索引
    while(operation != 0)
    {
        switch(operation)
        {
            case -1:
                cout << "选择操作:1.读取存储库, 2.新建存储库" << endl;
                cin >> operation;
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

    operation = -1;
    tmpaddcin = "";

    while(operation != 0)
    {
        switch(operation)
        {
            case -1:
                cout << "选择操作:1.增加项, 2.删除项, 3.修改项, 4.查找项, 5.模糊查找, 6.读取存储库, 7.新建存储库, 0.退出" << endl;
                cin >> operation;
                break;
            case 1:
                cout << "连续写入,输入exit退出" << endl;
                while(true)
                {
                    cout << "添加数据:";
                    cin >> tmpaddcin;
                    if(tmpaddcin == "exit")
                        break;
                    add(tmpaddcin);
                }
                operation = -1;
                break;
            case 2:
                cout << "删除项的索引:";
                cin >> idx;
                pop(idx);
                operation = -1;
                break;
            case 3:
                cout << "修改项索引:";
                cin >> idx;
                cout << "新的数据:";
                cin >> tmpaddcin;
                mod(idx, tmpaddcin);
                operation = -1;
                break;
            case 4:
                cout << "查找数据索引:";
                cin >> str;
                idx = find(str);
                if(idx != -1)
                    cout << "原文为:" << Data[idx] << endl;
                else
                    cout << "未找到" << endl;
                operation = -1;
                break;
            case 5:
                cout << "模糊查找数据:";
                cin >> str;
                find_fuzzy(str);
                operation = -1;
                break;
            case 6:
                if(loadRepo())
                {
                    cout << "读取成功" << endl;
                    cout << "当前存储库为: " << currentRepoName << endl;
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
                }
                else
                {
                    cout << "新建存储库失败" << endl;
                }
                operation = -1;
                break;
            case 0:
                while(true)
                {
                    bool result = write();
                    if(result)
                    {
                        cout << Data.size() << "  已写入" << endl;
                        return 0;
                    }
                    else
                        cout << "写入文件失败，正在重写" << endl;
                    continue;
                }
                break;
            default:
                operation = -1;
                break;
        }
    }
}