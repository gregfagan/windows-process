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
// #include <psapi.h>

#define SE_DEBUG_PRIVILEGE 20

//  Exported Function Declarations:
//
NAN_METHOD(ReadFromProcess);

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
    // Check the number of arguments passed.
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
            auto halt = Nan::MakeCallback(Nan::GetCurrentContext()->Global(), callback, 0, 0);
            if (halt->IsTrue()) {
                break;
            }
        }
        
    } while (Process32Next(snap, &pe32));
    CloseHandle(snap);
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