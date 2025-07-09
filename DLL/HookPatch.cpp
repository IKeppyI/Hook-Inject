#include "pch.h"
#include "HookPatch.h"


extern "C" void* trampoline = nullptr;

extern "C"
{
    void saver();
}

extern "C" HookCallback callback = nullptr;





HookPatch::HookPatch(const char* moduleName, const char* functionName, HookCallback call,bool m):isHooked(false),hookfunc(call),mode(m)
{
    HMODULE hModule = GetModuleHandleA(moduleName);
    if (!hModule) {
        std::cerr << "Ошибка: Не удалось получить модуль " << moduleName << std::endl;
        targetFunction = nullptr;
        return;
    }
    callback = call;
    targetFunction = (LPVOID)GetProcAddress(hModule, functionName);
    if (!targetFunction) {
        std::cerr << "Ошибка: Не удалось найти функцию " << functionName << std::endl;
        return;
    }

}

HookPatch::~HookPatch() {
    if (isHooked) {
        RemoveHook();
    }
}

void HookPatch::SaveOriginalBytes() {
    originalBytes.resize(14);
    memcpy(originalBytes.data(), targetFunction, 14);
}

void HookPatch::RestoreOriginalBytes() {
    if (!targetFunction || originalBytes.empty()) return;

    DWORD oldProtect;
    VirtualProtect(targetFunction, 14, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(targetFunction, originalBytes.data(), 14);
    VirtualProtect(targetFunction, 14, oldProtect, &oldProtect);
}

LPVOID HookPatch::CreateTrampoline() {
    LPVOID trampolineMem = VirtualAlloc(nullptr, 50, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampolineMem) {
        std::cerr << "Ошибка: Не удалось выделить память под трамплин!" << std::endl;
        return nullptr;
    }

    BYTE* pTrampoline = (BYTE*)trampolineMem;
    size_t originalSize = originalBytes.size();
    size_t i = 0;
    size_t j = 0;
    while (i < originalSize) {
        if (i + 6 <= originalSize && originalBytes[i] == 0xFF && originalBytes[i + 1] == 0x25) {
            int32_t disp32 = *(int32_t*)&originalBytes[i + 2];
            uintptr_t absAddr = *(uintptr_t*)((BYTE*)targetFunction + i + 6 + disp32);

            pTrampoline[j] = 0x48;
            pTrampoline[j + 1] = 0xB8;
            *(uintptr_t*)&pTrampoline[j + 2] = absAddr;
            pTrampoline[j + 10] = 0xFF;
            pTrampoline[j + 11] = 0xE0;
            i += 6;
            j += 12;
        }
        else {
            pTrampoline[j] = originalBytes[i];
            j++;
            i++;
        }
    }

    pTrampoline[j] = 0x48;
    pTrampoline[j+1] = 0xB8;
    *(uintptr_t*)&pTrampoline[j + 2] = (uintptr_t)((BYTE*)targetFunction + originalSize);
    pTrampoline[j+10] = 0xFF;
    pTrampoline[j+11] = 0xE0;

    return trampolineMem;
}




bool HookPatch::InstallHook() {
    if (!targetFunction || isHooked) return false;

    SaveOriginalBytes();
    trampoline = CreateTrampoline();
    tramp = trampoline;
    if (!trampoline) return false;

    DWORD oldProtect;
    VirtualProtect(targetFunction, 14, PAGE_EXECUTE_READWRITE, &oldProtect);

    *(BYTE*)targetFunction = 0x48;
    *(BYTE*)((BYTE*)targetFunction + 1) = 0xB8;
   if(!mode)*(uintptr_t*)((BYTE*)targetFunction + 2) = (uintptr_t)saver; 
    else  *(uintptr_t*)((BYTE*)targetFunction + 2) = (uintptr_t)hookfunc; 
    *(BYTE*)((BYTE*)targetFunction + 10) = 0xFF;
    *(BYTE*)((BYTE*)targetFunction + 11) = 0xE0;

    VirtualProtect(targetFunction, 14, oldProtect, &oldProtect);

    isHooked = true;
    return true;
}


bool HookPatch::RemoveHook() {
    if (!targetFunction || !isHooked) return false;

    RestoreOriginalBytes();
    VirtualFree(trampoline, 0, MEM_RELEASE);

    isHooked = false;
    return true;
}



