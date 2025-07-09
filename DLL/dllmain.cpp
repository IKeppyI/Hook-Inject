#include "pch.h"
#include <fstream>
#include <unordered_map>
#include "HookPatch.h"
#define PIPE_NAME R"(\\.\pipe\MyHookPipe)"

using namespace std;

HookPatch* hook = nullptr;
HookPatch* hook2 = nullptr;
HookPatch* hook3 = nullptr;
string fname;
HANDLE hPipe;



std::string GetCurrentTimeString() {
    SYSTEMTIME st;
    GetSystemTime(&st);

    char timeStr[100];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
         (st.wHour+3)%24, st.wMinute, st.wSecond);
    return std::string(timeStr);
}






HANDLE HideCreateFileW(LPCWSTR name, DWORD access, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES atr, DWORD thing1,
    DWORD thing2, HANDLE templatefile)
{
    DWORD bytesWritten;
    std::wstring wName(name);
    std::wstring wFname = std::wstring(fname.begin(), fname.end());

    if (wName == wFname )
    {
        return INVALID_HANDLE_VALUE;
    }

    return ((HANDLE(*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE))hook->tramp)
        (name, access, dwShareMode, atr, thing1, thing2, templatefile);
}


HANDLE  HideFindFirstFileW(LPCWSTR first, LPWIN32_FIND_DATAA sec) {
    std::wstring wFirstName(first);
    std::wstring wFname = std::wstring(fname.begin(), fname.end());

    if (wFirstName==wFname) {
        return INVALID_HANDLE_VALUE;
    }

    return ((HANDLE(*)(LPCWSTR, LPWIN32_FIND_DATAA))hook3->tramp)(first, sec);
}



BOOL HideFindNextFileW(HANDLE first, LPWIN32_FIND_DATAA sec) {
    std::string wFirstName(sec->cFileName);
    std::string wFname = std::string(fname.begin(), fname.end());

    if (wFirstName == wFname) {
      
        return FALSE; 
    }

    return ((BOOL(*)(HANDLE, LPWIN32_FIND_DATAA))hook2->tramp)(first, sec);
}





HookCallback Hooked_CreateFileW() {
    DWORD bytesWritten;
    std::string timeStr = GetCurrentTimeString();
    std::string logMessage = "CreateFileW called at " + timeStr;
    WriteFile(hPipe, logMessage.c_str(), logMessage.length() + 1, &bytesWritten, NULL);

    return 0;
}

HookCallback Hooked_FindNextFileW() {
    DWORD bytesWritten;
    std::string timeStr = GetCurrentTimeString();
    std::string logMessage = "FindNextFileA called at " + timeStr;
    WriteFile(hPipe, logMessage.c_str(), logMessage.length() + 1, &bytesWritten, NULL);
    return 0;
}

HookCallback Hooked_FindFirstFileW() {
    DWORD bytesWritten;
    std::string timeStr = GetCurrentTimeString();
    std::string logMessage = "FindFirstFileA called at " + timeStr;
    WriteFile(hPipe, logMessage.c_str(), logMessage.length() + 1, &bytesWritten, NULL);
    return 0;
}



HookCallback Hooked_ReadFile() {
    DWORD bytesWritten;
    std::string timeStr = GetCurrentTimeString();
    std::string logMessage = "ReadFile called at " + timeStr;
    WriteFile(hPipe, logMessage.c_str(), logMessage.length() + 1, &bytesWritten, NULL);
    return 0;
}

HookCallback Hooked_CloseHandle()
{
    DWORD bytesWritten;
    std::string timeStr = GetCurrentTimeString();
    std::string logMessage = "CloseHandle called at " + timeStr;
    WriteFile(hPipe, logMessage.c_str(), logMessage.length() + 1, &bytesWritten, NULL);
    return 0;
}





unordered_map<string, void*> hookTable = {
    {"CreateFileW", Hooked_CreateFileW},
    {"FindNextFileA", Hooked_FindNextFileW},
    {"FindFirstFileA", Hooked_FindFirstFileW},
    {"ReadFile", Hooked_ReadFile},
    {"CloseHandle", Hooked_CloseHandle}
};

DWORD WINAPI  PipeServer(LPVOID lpParam) {
    ofstream kek("S:\\work\\labs\\trpo\\lab1\\LAB1\\inject\\out.txt", ios::app);

    hPipe = CreateNamedPipeA(PIPE_NAME, PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, 256, 256, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        kek << "channel creation error\n";
        return 1;
    }

    kek << "client waiting...\n";

    if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
        return 1;
    }

    DWORD bytesAvailable = 0;
    while (true) {
        char buffer[256] = { 0 };
        DWORD bytesRead;
        if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
            if (!ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                kek << "answ error\n";
                DisconnectNamedPipe(hPipe);
                break;
            }
            kek << "got command: " << buffer << endl;

            string command(buffer);
            string response;

            if (command.substr(0, 4) == "stop") {
                if (hook) {
                    hook->RemoveHook();
                    delete hook;
                    hook = nullptr;
                }
                if (hook2) {
                    hook2->RemoveHook();
                    delete hook2;
                    hook2 = nullptr;
                }
                if (hook3) {
                    hook3->RemoveHook();
                    delete hook3;
                    hook3 = nullptr;
                }
                kek << "stop received\n";
                DisconnectNamedPipe(hPipe);
                break;
            }
            else if (command.substr(0, 4) == "FUNC") {
                string funcName = command.substr(5);
                if (hookTable.find(funcName) != hookTable.end()) {
                    if (!hook) {
                        hook = new HookPatch("kernel32.dll", funcName.c_str(), (HookCallback)hookTable[funcName],0);
                        if (hook->InstallHook()) {
                            response = "HOOK_SET";
                            kek << "hook on " << funcName << " set\n";
                        }
                        else {
                            response = "HOOK_FAILED";
                            kek << "hook on " << funcName << " failed\n";
                        }
                    }
                    else {
                        response = "HOOK_ALREADY_SET";
                    }
                }
                else {
                    response = "UNKNOWN_FUNCTION";
                }
            }
            else if (command.substr(0, 4) == "FILE") {
                fname = command.substr(5);
                    if (!hook) {
                        hook = new HookPatch("kernel32.dll", "CreateFileW", (HookCallback)HideCreateFileW,1);
                        hook2 = new HookPatch("kernel32.dll", "FindNextFileW", (HookCallback)HideFindNextFileW,1);
                        hook3 = new HookPatch("kernel32.dll", "FindFirstFileW", (HookCallback)HideFindFirstFileW,1);
                        if (hook->InstallHook() && hook2->InstallHook() && hook3->InstallHook())
                        {
                            response = "HOOKS_SET";
                            kek << "hook on " << fname << " set\n";
                        }
                        else {
                            response = "HOOK_FAILED";
                            kek << "hook on " << fname << " failed\n";
                        }
                    }
                    else {
                        response = "HOOK_ALREADY_SET";
                    }
            }
            else {
                response = "UNKNOWN_COMMAND";
            }

            kek << "sent: " << response << endl;

            DWORD bytesWritten;
            WriteFile(hPipe, response.c_str(), response.size() + 1, &bytesWritten, NULL);
        }
        Sleep(100);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    ofstream kek("S:\\work\\labs\\trpo\\lab1\\LAB1\\inject\\out.txt", ios::app);

    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        kek << "DLL attached\n";
        kek.close();
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, PipeServer, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        kek << "DLL detached\n";
        kek.close();
        if (hook) {
            hook->RemoveHook();
            delete hook;
            hook = nullptr;
        }
        if (hook2) {
            hook2->RemoveHook();
            delete hook2;
            hook2 = nullptr;
        }
        if (hook3) {
            hook3->RemoveHook();
            delete hook3;
            hook3 = nullptr;
        }
        break;
    }
    return TRUE;
}
