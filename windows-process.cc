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
NAN_METHOD(GetRunningProcesses);

//  Internal Function Declarations:
//
DWORD(__stdcall * RtlAdjustPrivilege)(DWORD, DWORD, DWORD, PVOID); // from ntdll
bool AdjustPrivilege();

// Definitions:
//
// returns an array of all running process names
NAN_METHOD(GetRunningProcesses) {
    v8::Local<v8::Array> result = Nan::New<v8::Array>();
    
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
    
    int i = 0;
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
        Nan::Set(result, i++, Nan::New(pe32.szExeFile).ToLocalChecked());
    } while (Process32Next(snap, &pe32));
    CloseHandle(snap);

    info.GetReturnValue().Set(result);
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
    Nan::Set(target, Nan::New("GetRunningProcesses").ToLocalChecked(),
      Nan::GetFunction(Nan::New<v8::FunctionTemplate>(GetRunningProcesses)).ToLocalChecked());
}
NODE_MODULE(WindowsProcess, InitAll);