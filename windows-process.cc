// WindowsProcess module
//
// Functions for accessing the memory of a running process on Windows
// exposed to JavaScript as a native NodeJS add-on.
//
// Portions of this code are derived from r57shell's
// d2dumpitems project:
//   http://www.blizzhackers.cc/viewtopic.php?f=182&t=499536&start=0

#include <nan.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdint>
#include <string>

#define SE_DEBUG_PRIVILEGE 20

//  Exported Function Declarations:
//
NAN_METHOD(ReadFromProcess);
NAN_METHOD(GetModuleBase);

//  Internal Function Declarations:
//
DWORD(__stdcall * RtlAdjustPrivilege)(DWORD, DWORD, DWORD, PVOID); // from ntdll
bool AdjustPrivilege();

// Definitions:
//
// ReadFromProcess(name: string, callback: function) -> nothing
//
// Finds processes by the given name, and calls the callback for 
// each one. Once a process by that name is found, the search can
// be halted by returning `true` from the callback.
//
// Callback signature:
//
// callback() -> shouldHalt:boolean
//
NAN_METHOD(ReadFromProcess) {
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

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pe32.th32ProcessID);
            if (!hProcess)
            {
                continue;
            }
            
            // Build reader object
            auto reader = Nan::New<v8::Object>();

            auto key_GetModuleBase = Nan::New("GetModuleBase").ToLocalChecked();
            auto value_GetModuleBase = Nan::GetFunction(Nan::New<v8::FunctionTemplate>(
                GetModuleBase, 
                Nan::New<v8::External>(hProcess)
            )).ToLocalChecked();
            Nan::Set(reader, key_GetModuleBase, value_GetModuleBase);
            
            v8::Local<v8::Value> argv[1] = { reader };
            auto halt = Nan::MakeCallback(Nan::GetCurrentContext()->Global(), callback, 1, argv);
            
            Nan::Delete(reader, key_GetModuleBase);
            
            CloseHandle(hProcess);
            
            if (halt->IsTrue()) {
                break;
            }
        }
        
    } while (Process32Next(snap, &pe32));
    CloseHandle(snap);
}

NAN_METHOD(GetModuleBase) {
    if (info.Length() < 1 || !info[0]->IsString()) {
        Nan::ThrowError("Must provide module name as an argument.");
        return;
    }
    
    Nan::Utf8String moduleName(info[0]);
    auto hProcess = (HANDLE)info.Data().As<v8::External>()->Value();
    
    HMODULE *hModules;
    TCHAR name[MAX_PATH];
    DWORD sizeD;
    if (EnumProcessModules(hProcess, 0, 0, &sizeD))
    {
        hModules = (HMODULE *)malloc(sizeD);
        if (hModules)
        {
            if (EnumProcessModules(hProcess, hModules, sizeD, &sizeD))
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
                    std::string str_name(name);
                    if (str_name.rfind(*moduleName) != std::string::npos)
                    {
                        MODULEINFO modInfo;
                        if (GetModuleInformation(hProcess, hModules[i], &modInfo, sizeof(modInfo)))
                        {
                            info.GetReturnValue().Set(Nan::New<v8::Number>(reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll)));
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

// (internal) Adjusts this process to SE_DEBUG_PRIVILEGE necessary to read
// memory from other processes.
//
// returns `true` for success and `false` for failure
bool AdjustPrivilege()
{
    HINSTANCE nt = GetModuleHandle("ntdll.dll"); // always loaded
    if (!nt)
        return false;

    *(FARPROC *)&RtlAdjustPrivilege = GetProcAddress(nt, "RtlAdjustPrivilege");

    DWORD prevState;

    if (RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, 1, 0, &prevState))
        return false;

    return true;
}

// Module export
//
NAN_MODULE_INIT(InitAll) {
    Nan::Set(target, Nan::New("ReadFromProcess").ToLocalChecked(),
      Nan::GetFunction(Nan::New<v8::FunctionTemplate>(ReadFromProcess)).ToLocalChecked());
}
NODE_MODULE(WindowsProcess, InitAll);