#include <pseudoconsole.hpp>

void Pseudoconsole::Initialize(wchar_t* command)
{
    HRESULT hr;
    hr = CreatePseudoConsoleAndPipes(&hPipeIn, &hPipeOut);
    if (S_OK == hr)
    {
        // Initialize the necessary startup info struct

        if (S_OK == InitializeStartupInfoAttachedToPseudoConsole(
                        &startupInfo))
        {
            hr = CreateProcessW(
                     NULL,    // No module name - use Command Line
                     command, // Command Line
                     NULL,    // Process handle not inheritable
                     NULL,    // Thread handle not inheritable
                     FALSE,   // Inherit handles
                     EXTENDED_STARTUPINFO_PRESENT, // Creation
                                                   // flags
                     NULL, // Use parent's environment block
                     NULL, // Use parent's starting directory
                     &startupInfo
                          .StartupInfo, // Pointer to STARTUPINFO
                     &piClient) // Pointer to PROCESS_INFORMATION
                     ? S_OK
                     : GetLastError();
        }
    }
}

void Pseudoconsole::Close()
{
    CloseHandle(piClient.hThread);
    CloseHandle(piClient.hProcess);

    // Cleanup attribute list
    DeleteProcThreadAttributeList(startupInfo.lpAttributeList);
    free(startupInfo.lpAttributeList);

    // Close ConPTY - this will terminate client process if
    // running
    ClosePseudoConsole(hPC);

    // Clean-up the pipes
    if (INVALID_HANDLE_VALUE != hPipeOut)
        CloseHandle(hPipeOut);
    if (INVALID_HANDLE_VALUE != hPipeIn)
        CloseHandle(hPipeIn);
}
    
void Pseudoconsole::Resize(COORD newSize)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HRESULT hr = ResizePseudoConsole(hPC, newSize);
    GetConsoleScreenBufferInfo(hPC, &csbi);
    size = csbi.dwSize;
}

HRESULT Pseudoconsole::CreatePseudoConsoleAndPipes(HANDLE* phPipeIn,
                                                   HANDLE* phPipeOut)
{
    HRESULT hr {E_UNEXPECTED};
    HANDLE  hPipePTYIn {INVALID_HANDLE_VALUE};
    HANDLE  hPipePTYOut {INVALID_HANDLE_VALUE};

    // Create the pipes to which the ConPTY will connect
    if (CreatePipe(&hPipePTYIn, phPipeOut, NULL, 0) &&
        CreatePipe(phPipeIn, &hPipePTYOut, NULL, 0))
    {
        // Determine required size of Pseudo Console
        COORD                      consoleSize {};
        CONSOLE_SCREEN_BUFFER_INFO csbi {};
        HANDLE hConsole {GetStdHandle(STD_OUTPUT_HANDLE)};
        if (GetConsoleScreenBufferInfo(hConsole, &csbi))
        {
            consoleSize.X =
                csbi.srWindow.Right - csbi.srWindow.Left + 1;
            consoleSize.Y =
                csbi.srWindow.Bottom - csbi.srWindow.Top;
            size = consoleSize;
        }

        // Create the Pseudo Console of the required size, attached to
        // the PTY-end of the pipes
        hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut,
                                 0, &hPC);

        // Note: We can close the handles to the PTY-end of the pipes
        // here because the handles are dup'ed into the ConHost and
        // will be released when the ConPTY is destroyed.
        if (INVALID_HANDLE_VALUE != hPipePTYOut)
            CloseHandle(hPipePTYOut);
        if (INVALID_HANDLE_VALUE != hPipePTYIn)
            CloseHandle(hPipePTYIn);
    }

    return hr;
}

// Initializes the specified startup info struct with the required
// properties and updates its thread attribute list with the specified
// ConPTY handle
HRESULT Pseudoconsole::InitializeStartupInfoAttachedToPseudoConsole(
    STARTUPINFOEXW* pStartupInfo)
{
    HRESULT hr {E_UNEXPECTED};

    if (pStartupInfo)
    {
        size_t attrListSize {};

        pStartupInfo->StartupInfo.cb = sizeof(STARTUPINFOEXW);

        // Get the size of the thread attribute list.
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);

        // Allocate a thread attribute list of the correct size
        pStartupInfo->lpAttributeList =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
                malloc(attrListSize));

        // Initialize thread attribute list
        if (pStartupInfo->lpAttributeList &&
            InitializeProcThreadAttributeList(
                pStartupInfo->lpAttributeList, 1, 0, &attrListSize))
        {
            // Set Pseudo Console attribute
            hr = UpdateProcThreadAttribute(
                     pStartupInfo->lpAttributeList, 0,
                     PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC,
                     sizeof(HPCON), NULL, NULL)
                     ? S_OK
                     : HRESULT_FROM_WIN32(GetLastError());
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return hr;
}
