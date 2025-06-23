#include <string>
#include <thread>
#include <atomic>
#include <curses.h>
#include <queue>
#include <cassert>
#include <iostream>

#ifdef _WIN32
#    include <windows.h>
#    include <io.h>
#    include <winternl.h>
#    define isatty _isatty
#    define fileno _fileno
#else
#    include <unistd.h>
#    include <sys/ioctl.h>
#    include <termios.h>
#    include <fcntl.h>
#    include <sys/types.h>
#    include <sys/wait.h>
#endif

#ifdef _WIN32

#    define NtCurrentProcess()           ((HANDLE)(LONG_PTR) - 1)
#    define RESIZE_CONHOST_SIGNAL_BUFFER 8
#    ifndef PSEUDOCONSOLE_INHERIT_CURSOR
#        define PSEUDOCONSOLE_INHERIT_CURSOR 1
#    endif

typedef struct _HPCON_INTERNAL
{
    HANDLE hWritePipe;
    HANDLE hConDrvReference;
    HANDLE hConHostProcess;
} HPCON_INTERNAL;

typedef struct _RESIZE_PSEUDO_CONSOLE_BUFFER
{
    SHORT Flags;
    SHORT SizeX;
    SHORT SizeY;
} RESIZE_PSEUDO_CONSOLE_BUFFER;

inline NTSTATUS WINAPI CreateHandle(PHANDLE FileHandle, PWSTR Buffer,
                                    ACCESS_MASK DesiredAccess,
                                    HANDLE      RootDirectory,
                                    BOOLEAN     IsInheritable,
                                    ULONG       OpenOptions)
{
    IO_STATUS_BLOCK IoStatusBlock;
    ULONG           Attributes = OBJ_CASE_INSENSITIVE;
    if (IsInheritable)
        Attributes |= OBJ_INHERIT;

    UNICODE_STRING ObjectName;
    memset(&ObjectName, 0, sizeof ObjectName);
    RtlInitUnicodeString(&ObjectName, Buffer);

    OBJECT_ATTRIBUTES ObjectAttributes;
    memset(&ObjectAttributes, 0, sizeof ObjectAttributes);
    ObjectAttributes.RootDirectory = RootDirectory;
    ObjectAttributes.Length = sizeof ObjectAttributes;
    ObjectAttributes.Attributes = Attributes;
    ObjectAttributes.ObjectName = &ObjectName;

    return NtOpenFile(
        FileHandle, DesiredAccess, &ObjectAttributes, &IoStatusBlock,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        OpenOptions);
}

class Terminal
{
  public:
    HPCON consoleHandle;

    HRESULT
    WINAPI
    InitializeConsole(COORD size, HANDLE hInput, HANDLE hOutput,
                      DWORD dwFlags)
    {
        NTSTATUS Status;
        BOOL     bRes;

        HANDLE InputHandle, OutputHandle;
        HANDLE hConServer, hConReference;
        HANDLE hProc = NtCurrentProcess();

        bRes = DuplicateHandle(hProc, hInput, hProc, &InputHandle, 0,
                               TRUE, DUPLICATE_SAME_ACCESS);
        assert(bRes != 0);

        bRes = DuplicateHandle(hProc, hOutput, hProc, &OutputHandle,
                               0, TRUE, DUPLICATE_SAME_ACCESS);
        assert(bRes != 0);

        Status = CreateHandle(&hConServer,
                              (LPWSTR)L"\\Device\\ConDrv\\Server",
                              GENERIC_ALL, NULL, TRUE, 0);

        std::cout << "Status: " << Status << '\n';

        assert(Status == 0);

        HANDLE              ReadPipeHandle, WritePipeHandle;
        SECURITY_ATTRIBUTES PipeAttributes;
        memset(&PipeAttributes, 0, sizeof PipeAttributes);
        PipeAttributes.bInheritHandle = FALSE;
        PipeAttributes.lpSecurityDescriptor = NULL;
        PipeAttributes.nLength = sizeof PipeAttributes;

        bRes = CreatePipe(&ReadPipeHandle, &WritePipeHandle,
                          &PipeAttributes, 0);
        assert(bRes != 0);

        bRes = SetHandleInformation(
            ReadPipeHandle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        assert(bRes != 0);

        PWSTR InheritCursor =
            (LPWSTR)L"--inheritcursor "; /* Requires one space */
        if (!(dwFlags & PSEUDOCONSOLE_INHERIT_CURSOR))
            InheritCursor = (LPWSTR)L"";

        wchar_t ConHostCommand[MAX_PATH];

        /* ConHost command format strings */
#    define COMMAND_FORMAT                                          \
        L"\\\\?\\%s\\system32\\conhost.exe %s--width %hu --height " \
        L"%hu --signal 0x%x --server 0x%x"

        wchar_t winDir[MAX_PATH];
        GetWindowsDirectoryW(winDir, MAX_PATH);
        swprintf_s(ConHostCommand, MAX_PATH,
                   L"\"%s\\system32\\conhost.exe\" %s--width %hu "
                   L"--height %hu --signal 0x%x --server 0x%x",
                   winDir, InheritCursor, size.X, size.Y,
                   HandleToULong(ReadPipeHandle),
                   HandleToULong(hConServer));

        /* Initialize thread attribute list */
        HANDLE Values[4] = {hConServer, InputHandle, OutputHandle,
                            ReadPipeHandle};
        size_t AttrSize;
        LPPROC_THREAD_ATTRIBUTE_LIST AttrList = NULL;
        InitializeProcThreadAttributeList(NULL, 1, 0, &AttrSize);
        AttrList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(
            GetProcessHeap(), 0, AttrSize);
        InitializeProcThreadAttributeList(AttrList, 1, 0, &AttrSize);
        bRes = UpdateProcThreadAttribute(
            AttrList, 0,
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST, /* 0x20002u */
            Values, sizeof Values, NULL, NULL);
        assert(bRes != 0);

        /* Assign members of STARTUPINFOEXW */
        PROCESS_INFORMATION ProcInfo;
        memset(&ProcInfo, 0, sizeof ProcInfo);
        STARTUPINFOEXW SInfoEx;
        memset(&SInfoEx, 0, sizeof SInfoEx);
        SInfoEx.StartupInfo.cb = sizeof SInfoEx;
        SInfoEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
        SInfoEx.StartupInfo.hStdInput = InputHandle;
        SInfoEx.StartupInfo.hStdOutput = OutputHandle;
        SInfoEx.StartupInfo.hStdError = OutputHandle;
        SInfoEx.lpAttributeList = AttrList;

        bRes = CreateProcessW(
            NULL, ConHostCommand, NULL, NULL,
            TRUE, /* All handles are inheritable by ConHost PTY */
            EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
            &SInfoEx.StartupInfo, &ProcInfo);
        assert(bRes != 0);

        Status = CreateHandle(
            &hConReference, (LPWSTR)L"\\Reference",
            GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, hConServer,
            FALSE, FILE_SYNCHRONOUS_IO_NONALERT);
        assert(Status == 0);

        /* Return handles to caller */
        HPCON_INTERNAL* consoleHandleInt = (HPCON_INTERNAL*)HeapAlloc(
            GetProcessHeap(), 0, sizeof(*consoleHandleInt));
        consoleHandleInt->hWritePipe = WritePipeHandle;
        consoleHandleInt->hConDrvReference = hConReference;
        consoleHandleInt->hConHostProcess = ProcInfo.hProcess;

        consoleHandle = consoleHandleInt;

        /* Cleanup */
        HeapFree(GetProcessHeap(), 0, AttrList);
        CloseHandle(InputHandle);
        CloseHandle(OutputHandle);
        CloseHandle(hConServer);
        CloseHandle(ProcInfo.hThread);
        CloseHandle(ProcInfo.hProcess);

        return 0;
    }

    HRESULT WINAPI ResizeConsole(COORD size)
    {
        BOOL bRes;

        RESIZE_PSEUDO_CONSOLE_BUFFER ResizeBuffer;
        memset(&ResizeBuffer, 0, sizeof ResizeBuffer);
        ResizeBuffer.Flags = RESIZE_CONHOST_SIGNAL_BUFFER;
        ResizeBuffer.SizeX = size.X;
        ResizeBuffer.SizeY = size.Y;

        HPCON_INTERNAL* consoleHandle =
            (HPCON_INTERNAL*)consoleHandle;

        bRes = WriteFile(consoleHandle->hWritePipe, &ResizeBuffer,
                         sizeof ResizeBuffer, NULL, NULL);
        assert(bRes != 0);
        return 0;
    }

    void WINAPI CloseConsole()
    {
        DWORD ExitCode;

        HPCON_INTERNAL* consoleHandle =
            (HPCON_INTERNAL*)consoleHandle;

        CloseHandle(consoleHandle->hWritePipe);

        if (GetExitCodeProcess(consoleHandle->hConHostProcess,
                               &ExitCode) &&
            ExitCode == STILL_ACTIVE)
        {
            WaitForSingleObject(consoleHandle->hConHostProcess,
                                INFINITE);
        }

        TerminateProcess(consoleHandle->hConHostProcess, 0);

        CloseHandle(consoleHandle->hConDrvReference);
        HeapFree(GetProcessHeap(), 0, consoleHandle);
    }
};

#else

// TODO
class Terminal
{
  public:
};

#endif
