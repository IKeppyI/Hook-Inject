
typedef void (*HookCallback)();

using namespace std;
extern HANDLE hPipe;




class HookPatch {
public:
    LPVOID targetFunction;             
    std::vector<BYTE> originalBytes; 
    bool isHooked;        
    HookPatch(const char* moduleName, const char* functionName, HookCallback callback,bool m);
    ~HookPatch();
    bool InstallHook();
    bool RemoveHook();
    void* tramp = nullptr;

private:

    bool mode;
    HookCallback hookfunc;
    void SaveOriginalBytes();
    void RestoreOriginalBytes();
    LPVOID CreateTrampoline();
};

