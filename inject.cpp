#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#define PIPE_NAME R"(\\.\pipe\MyHookPipe)"
using namespace std;

atomic<bool> running(true); 

DWORD GetProcessIDByName(char* processName) {
    wstring wstr(processName, processName + strlen(processName));
    DWORD processID = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (wstr == pe32.szExeFile) {
                    processID = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    return processID;
}

void SendCommand(HANDLE hPipe, const char* command) {
    DWORD written;
    if (WriteFile(hPipe, command, strlen(command) + 1, &written, NULL)) {

    }
}
void ReceiveCommand(HANDLE hPipe) {
    DWORD bytesAvailable = 0;
    while (running) {
        if (hPipe != INVALID_HANDLE_VALUE) {
            char buffer[256] = { 0 };
            DWORD bytesRead;
            if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0)
            {
                ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
            }

            if (strlen(buffer) > 0) {
                cout << "Ответ от DLL: " << buffer << endl;
            }
        }
        Sleep(1000);
    }
}

bool InjectDLL(HANDLE hProcess, const char* dllPath) {
    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!loadLibraryAddr) {
        cerr << "Не удалось найти LoadLibraryA!" << endl;
        return false;
    }

    LPVOID remoteMemory = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMemory) {
        cerr << "Не удалось выделить память в целевом процессе!" << endl;
        return false;
    }

    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath, strlen(dllPath) + 1, NULL)) {
        cerr << "Не удалось записать память в целевой процесс!" << endl;
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMemory, 0, NULL);
    if (!hThread) {
        cerr << "Не удалось создать удалённый поток!" << endl;
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);

    return true;
}

void ShowUsage() {
    cout << "Usage: \n";
    cout << "  -pid <PID>        : Target process by PID\n";
    cout << "  -name <process_name> : Target process by name\n";
    cout << "  -func <function_name> : Set the function to hook\n";
    cout << "  -hide <file_name> : Hide a file\n";
    cout << "Type 'stop' to exit the program.\n";
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");
    const char* dllPath = "S:\\work\\labs\\trpo\\lab1\\LAB1\\makedll\\Dll1\\x64\\Debug\\Dll1.dll";

    if (argc < 5) {
        ShowUsage();
        return 1;
    }

    string name;
    DWORD processID = 0;
    if (string(argv[1]) == "-pid")
        processID = stoi(argv[2]);
    else if (string(argv[1]) == "-name")
        processID = GetProcessIDByName(argv[2]);
    else {
        ShowUsage();
        return 1;
    }
    cout << processID<<endl;
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (!hProcess) {
        cerr << "Не удалось открыть целевой процесс! Ошибка: " << GetLastError() << endl;
        return 1;
    }

    if (InjectDLL(hProcess, dllPath)) {
        cout << "DLL успешно инжектирована!" << endl;
    }
    else {
        cerr << "Ошибка при инжектировании DLL!" << endl;
    }

    HANDLE hPipe;
    while (true) {
        hPipe = CreateFileA(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) break;
        cerr << "Ожидание подключения к серверу...\n";
        Sleep(1000);
    }


    if (string(argv[3]) == "-func") {
        name = argv[4];
        name = "FUNC " + name;
        SendCommand(hPipe,name.c_str());
    }
    else if (string(argv[3]) == "-hide") {
        name = argv[4];
        name = "FILE " + name;
        SendCommand(hPipe, name.c_str());
    }
    else {
        ShowUsage();
        return 1;
    }
    cout << "Введите команду (или 'stop' для выхода)\n";

    thread responseThread(ReceiveCommand,hPipe);

    string input;
    
    while (true) {
        getline(cin, input);
        SendCommand(hPipe, input.c_str());
        if (input == "stop") {
            running = false;
            break;
        }
      
    }

    responseThread.join();
    CloseHandle(hPipe);
    CloseHandle(hProcess);
    return 0;
}
