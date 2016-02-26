// WindowsProcess module
//
// Functions for accessing the memory of a running process on Windows
// exposed to JavaScript as a native NodeJS add-on.
//
// Portions of this code are derived from r57shell's
// d2dumpitems project:
//   http://www.blizzhackers.cc/viewtopic.php?f=182&t=499536&start=0

#pragma warning ( push )
#pragma warning ( disable: 4244 ) // uintptr_t <-> v8::Number should be safe
#include <nan.h>
#pragma warning ( pop )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdint>
#include <string>
#include <sstream>

#define SE_DEBUG_PRIVILEGE 20

//  Forward Declarations:
//
NAN_METHOD(WithProcess);
NAN_METHOD(getModuleBase);
NAN_METHOD(readMemory);
DWORD(__stdcall * RtlAdjustPrivilege)(DWORD, DWORD, DWORD, PVOID); // from ntdll
bool AdjustPrivilege();
struct ci_char_traits;
typedef std::basic_string<char, ci_char_traits> ci_string;

// Definitions:
//
//   WithProcess(name: string, callback: function) -> nothing
//
// Searches for processes of the given name.
//
// When found, `callback` is invoked with a `process` object that
// offers further methods to interact with the process.
//
//   callback(process: object) -> shouldHalt: boolean
//
//   process {
//     getModuleBase: (name: string) -> number
//     readMemory: (address: number, countInBytes:number) -> buffer
//   }
//
// These functions are described in more detail below.
//
// When finsihed working with the `process` object, return `true`
// to halt the search, or `false` to continue looking for more
// processes with the same `name`.
//
// This two-step API encapsulates the process resource HANDLE,
// providing a guaranteed, scoped lifetime that won't leak.
//
NAN_METHOD(WithProcess) {
    if (info.Length() < 2) {
        Nan::ThrowError("Expects 2 arguments.");
        return;
    }
    
    auto processName = Nan::To<v8::String>(info[0]);
    if (processName.IsEmpty()) {
        Nan::ThrowError("Must supply a process name as the first argument.");
        return;
    }
    
    char szProcessName[MAX_PATH] = {0};
    Nan::DecodeWrite(szProcessName, MAX_PATH, processName.ToLocalChecked(), Nan::Encoding::ASCII);
    
    if (!info[1]->IsFunction()) {
        Nan::ThrowError("Must supply a callback function as the second argument.");
        return;
    }
    
    auto callback = info[1].As<v8::Function>();
    
    if (!AdjustPrivilege()) {
        Nan::ThrowError("Access denied. Please run as administrator.");
        return;
    }
    
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        Nan::ThrowError("Can't create process snapshot.");
        return;
    }
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(snap, &pe32))
    {
        CloseHandle(snap);
        Nan::ThrowError("Can't get first process.");
        return;
    }
    do
    {
        if (strncmp(pe32.szExeFile, szProcessName, MAX_PATH) == 0) {
            //
            // Open process resource
            HANDLE hProcess = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                0,
                pe32.th32ProcessID
            );
            
            if (!hProcess) continue;
            
            // Build process API object
            //
            // Each function is bound to hProcess via its Data* field.
            auto process = Nan::New<v8::Object>();
            
            auto key_getModuleBase = Nan::New("getModuleBase").ToLocalChecked();
            auto value_getModuleBase = Nan::GetFunction(Nan::New<v8::FunctionTemplate>(
                getModuleBase, 
                Nan::New<v8::External>(hProcess)
            )).ToLocalChecked();
            Nan::Set(process, key_getModuleBase, value_getModuleBase);
            
            auto key_readMemory = Nan::New("readMemory").ToLocalChecked();
            auto value_readMemory = Nan::GetFunction(Nan::New<v8::FunctionTemplate>(
                readMemory, 
                Nan::New<v8::External>(hProcess)
            )).ToLocalChecked();
            Nan::Set(process, key_readMemory, value_readMemory);
            
            // Call callback
            v8::Local<v8::Value> argv[1] = { process };
            auto halt = Nan::MakeCallback(Nan::GetCurrentContext()->Global(), callback, 1, argv);
            
            // Delete out of process object so references to HMODULE won't leak
            Nan::Delete(process, key_getModuleBase);
            Nan::Delete(process, key_readMemory);

            CloseHandle(hProcess);
            
            if (halt->IsTrue()) break;
        }
    } while (Process32Next(snap, &pe32));
    CloseHandle(snap);
}

//
//   getModuleBase: (name: string) -> number
//
// Returns a memory offset to the base of the module loaded into
// process memory. This is useful if the memory address you wish
// to read from is a known offset into a module's memory space.
//
NAN_METHOD(getModuleBase) {
    if (info.Length() < 1 || !info[0]->IsString()) {
        Nan::ThrowError("Must provide module name as an argument.");
        return;
    }
    
    Nan::Utf8String moduleName(info[0]);
    auto hProcess = (HANDLE)info.Data().As<v8::External>()->Value();
    
    HMODULE *hModules;
    TCHAR name[MAX_PATH];
    DWORD sizeD;
    if (EnumProcessModulesEx(hProcess, 0, 0, &sizeD, LIST_MODULES_ALL))
    {
        hModules = (HMODULE *)malloc(sizeD);
        if (hModules)
        {
            if (EnumProcessModulesEx(hProcess, hModules, sizeD, &sizeD, LIST_MODULES_ALL))
            {
                size_t count = sizeD / sizeof(HMODULE);
                for (int i = 0; i < count; i++)
                {
                    if (!GetModuleFileNameEx(hProcess, hModules[i], name, sizeof(name)))
                    {
                        // Can't get module filename, just skip it
                        continue;
                    }
                    
                    // Search the end of the enumerated process name for the argument.
                    ci_string searchString(name);
                    if (searchString.rfind(*moduleName) != ci_string::npos)
                    {
                        MODULEINFO modInfo;
                        if (GetModuleInformation(hProcess, hModules[i], &modInfo, sizeof(modInfo)))
                        {
                            auto ptr = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
                            
                            //
                            // Return the module pointer as a number. This cast from LPVOID
                            // to JavaScript number triggers a warning for a possible data
                            // loss if the sizes don't match up.
                            //
                            // A 64-bit JavaScript number should be suffice to hold any
                            // 32 or 64 bit memory address, which is all that is needed at
                            // this time.
                            //
                            info.GetReturnValue().Set(Nan::New<v8::Number>(ptr));
                        }
                    }
                }
            }
            else
            {
                Nan::ThrowError("Can't enum modules");
            }
            free(hModules);
        }
        else
        {
            Nan::ThrowError("Can't callocate memory for modules");
        }
    }
}

//
//   readMemory(address: number, countInBytes: number) -> buffer
//
// What else? Does exactly what it says on the tin.
//
NAN_METHOD(readMemory) {
    if (info.Length() < 2) {
        Nan::ThrowError("Expects 2 arguments.");
        return;
    }
    
    if (!info[0]->IsNumber()) {
        Nan::ThrowError("First argument must be a memory offset (number)");
        return;
    }
    
    if (!info[1]->IsNumber()) {
        Nan::ThrowError("Second argument must be a count of bytes (number)");
        return;
    }
    
    auto hProcess = (HANDLE)info.Data().As<v8::External>()->Value();
    auto addr = (LPVOID)static_cast<uintptr_t>(Nan::To<v8::Number>(info[0]).ToLocalChecked()->Value());
    auto count = Nan::To<uint32_t>(info[1]).FromMaybe(0);
    
    char *buffer = (char*)malloc(count);
    if (!buffer) {
        Nan::ThrowError("Could not allocate memory for read");
        return;
    }
    
    size_t countOfBytesRead = 0;
    if (!ReadProcessMemory(hProcess, addr, buffer, count, &countOfBytesRead)) {
        std::stringstream ss;
        ss << "Failed to read memory. Error code: " << GetLastError() << " bytes read " << countOfBytesRead;
        Nan::ThrowError(ss.str().c_str());
        free(buffer);
        return;
    }
    
    // node buffer assumes ownership of memory
    auto result = Nan::NewBuffer(buffer, (uint32_t)countOfBytesRead);
    if (result.IsEmpty()) {
        Nan::ThrowError("Failed to create node buffer");
        free(buffer);
        return;
    }
    
    info.GetReturnValue().Set(result.ToLocalChecked());
}

//
// (internal) Adjusts this process to SE_DEBUG_PRIVILEGE necessary
// to read memory from other processes.
//
// returns `true` for success and `false` for failure
//
bool AdjustPrivilege() {
    HINSTANCE nt = GetModuleHandle("ntdll.dll"); // always loaded
    if (!nt)
        return false;

    *(FARPROC *)&RtlAdjustPrivilege = GetProcAddress(nt, "RtlAdjustPrivilege");

    DWORD prevState;

    if (RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, 1, 0, &prevState))
        return false;

    return true;
}

// Case-insensitve string for comparison
//
// authors: wilhelmtell, Herb Sutter
// sources: http://stackoverflow.com/questions/11635/case-insensitive-string-comparison-in-c
//          http://www.gotw.ca/gotw/029.htm
//
struct ci_char_traits : public std::char_traits<char> {
    static bool eq(char c1, char c2) { return toupper(c1) == toupper(c2); }
    static bool ne(char c1, char c2) { return toupper(c1) != toupper(c2); }
    static bool lt(char c1, char c2) { return toupper(c1) <  toupper(c2); }
    static int compare(const char* s1, const char* s2, size_t n) {
        while( n-- != 0 ) {
            if( toupper(*s1) < toupper(*s2) ) return -1;
            if( toupper(*s1) > toupper(*s2) ) return 1;
            ++s1; ++s2;
        }
        return 0;
    }
    static const char* find(const char* s, int n, char a) {
        while( n-- > 0 && toupper(*s) != toupper(a) ) {
            ++s;
        }
        return s;
    }
};

// Module export
//
NAN_MODULE_INIT(InitAll) {
    Nan::Set(target, Nan::New("WithProcess").ToLocalChecked(),
      Nan::GetFunction(Nan::New<v8::FunctionTemplate>(WithProcess)).ToLocalChecked());
}
NODE_MODULE(WindowsProcess, InitAll);