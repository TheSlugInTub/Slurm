#include <iostream>
#include <cstdio>
#include <regex>
#include <curses.h>

#ifdef _WIN32
#    include <windows.h>
#    include <io.h>
#    include <tchar.h>
#    include <process.h>
#    include <fcntl.h>
#    define isatty _isatty
#    define fileno _fileno
#else
#    include <unistd.h>
#    include <sys/ioctl.h>
#    include <termios.h>
#endif

struct Vec2
{
    int x, y;
};

Vec2 terminalSize;

void StartProgram()
{
    // Initialize ncurses
    initscr();

    // Check if colors are supported
    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);  // For prompt
        init_pair(2, COLOR_YELLOW, COLOR_BLACK); // For directory
        init_pair(3, COLOR_RED, COLOR_BLACK);    // For errors
    }

    // Configure ncurses
    keypad(stdscr, TRUE);   // Enable special keys
    noecho();               // Don't echo input automatically
    cbreak();               // Disable line buffering
    nodelay(stdscr, FALSE); // Make getch() blocking

    // Get terminal size
    getmaxyx(stdscr, terminalSize.y, terminalSize.x);

    // Display welcome message
    if (has_colors())
    {
        attron(COLOR_PAIR(1));
        printw("Slurm 1.0\n");
        attroff(COLOR_PAIR(1));
    }
    else
    {
        printw("Slurm 1.0\n");
    }

    refresh();
}

HRESULT CreatePseudoConsoleAndPipes(HPCON*, HANDLE*, HANDLE*);
HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEXW*,
                                                     HPCON);
void __cdecl PipeListener(LPVOID);
void __cdecl InputSender(LPVOID);

int main()
{
    // StartProgram();

    // Set console to UTF-8 for proper Unicode support
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Enable UTF-8 mode for C++ streams
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);

    wchar_t szCommand[] {L"cmd.exe"};
    HRESULT hr {E_UNEXPECTED};
    HANDLE  hConsole = {GetStdHandle(STD_OUTPUT_HANDLE)};
    HANDLE  hConsoleIn = {GetStdHandle(STD_INPUT_HANDLE)};

    // Enable Console VT Processing
    DWORD consoleMode {};
    GetConsoleMode(hConsole, &consoleMode);
    hr = SetConsoleMode(hConsole,
                        consoleMode |
                            ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                            ENABLE_PROCESSED_INPUT)
             ? S_OK
             : GetLastError();

    // Enable raw input processing to capture all keys
    DWORD inputMode {};
    GetConsoleMode(hConsoleIn, &inputMode);
    SetConsoleMode(
        hConsoleIn,
        inputMode | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT |
            ENABLE_MOUSE_INPUT);

    if (S_OK == hr)
    {
        HPCON hPC {INVALID_HANDLE_VALUE};

        //  Create the Pseudo Console and pipes to it
        HANDLE hPipeIn {INVALID_HANDLE_VALUE};
        HANDLE hPipeOut {INVALID_HANDLE_VALUE};
        hr = CreatePseudoConsoleAndPipes(&hPC, &hPipeIn, &hPipeOut);
        if (S_OK == hr)
        {
            // Create & start thread to listen to the incoming pipe
            // Note: Using CRT-safe _beginthread() rather than
            // CreateThread()
            HANDLE hPipeListenerThread {reinterpret_cast<HANDLE>(
                _beginthread(PipeListener, 0, hPipeIn))};
            HANDLE hInputSenderThread {reinterpret_cast<HANDLE>(
                _beginthread(InputSender, 0, hPipeOut))};

            // Initialize the necessary startup info struct
            STARTUPINFOEXW startupInfo {};

            if (S_OK == InitializeStartupInfoAttachedToPseudoConsole(
                            &startupInfo, hPC))
            {
                // Launch ping to emit some text back via the pipe
                PROCESS_INFORMATION piClient {};
                hr = CreateProcessW(
                         NULL, // No module name - use Command Line
                         szCommand, // Command Line
                         NULL,      // Process handle not inheritable
                         NULL,      // Thread handle not inheritable
                         FALSE,     // Inherit handles
                         EXTENDED_STARTUPINFO_PRESENT, // Creation
                                                       // flags
                         NULL, // Use parent's environment block
                         NULL, // Use parent's starting directory
                         &startupInfo
                              .StartupInfo, // Pointer to STARTUPINFO
                         &piClient) // Pointer to PROCESS_INFORMATION
                         ? S_OK
                         : GetLastError();

                if (S_OK == hr)
                {
                    // Wait up to 10s for ping process to complete
                    WaitForSingleObject(piClient.hThread, INFINITE);
                }

                // --- CLOSEDOWN ---

                // Now safe to clean-up client app's process-info &
                // thread
                CloseHandle(piClient.hThread);
                CloseHandle(piClient.hProcess);

                // Cleanup attribute list
                DeleteProcThreadAttributeList(
                    startupInfo.lpAttributeList);
                free(startupInfo.lpAttributeList);
            }

            // Close ConPTY - this will terminate client process if
            // running
            ClosePseudoConsole(hPC);

            // Clean-up the pipes
            if (INVALID_HANDLE_VALUE != hPipeOut)
                CloseHandle(hPipeOut);
            if (INVALID_HANDLE_VALUE != hPipeIn)
                CloseHandle(hPipeIn);
        }
    }

    while (1) {}

    return S_OK == hr ? EXIT_SUCCESS : EXIT_FAILURE;
}

HRESULT CreatePseudoConsoleAndPipes(HPCON* phPC, HANDLE* phPipeIn,
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
                csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }

        // Create the Pseudo Console of the required size, attached to
        // the PTY-end of the pipes
        hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut,
                                 0, phPC);

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
HRESULT InitializeStartupInfoAttachedToPseudoConsole(
    STARTUPINFOEXW* pStartupInfo, HPCON hPC)
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

std::string StripAnsi(const char* input, DWORD len)
{
    std::string output = {};

    for (int i = 0; i < len; i++)
    {
        if (isalnum(input[i]) ||
            (input[i] == '\n' || input[i] == ' '))
        {
            output.append(1, input[i]);
        }
    }

    return output;
}

void __cdecl PipeListener(LPVOID pipe)
{
    HANDLE hPipe {pipe};
    HANDLE hConsole {GetStdHandle(STD_OUTPUT_HANDLE)};

    const DWORD BUFF_SIZE {2048};
    char        szBuffer[BUFF_SIZE] {};

    DWORD dwBytesWritten {};
    DWORD dwBytesRead {};
    BOOL  fRead {FALSE};
    do {
        // Read from the pipe
        fRead =
            ReadFile(hPipe, szBuffer, BUFF_SIZE, &dwBytesRead, NULL);

        // Write received text to the Console
        // Note: Write to the Console using WriteFile(hConsole...),
        // not printf()/puts() to prevent partially-read VT sequences
        // from corrupting output

        WriteFile(hConsole, szBuffer, dwBytesRead, &dwBytesWritten,
                  NULL);

        // std::string cleanString = StripAnsi(szBuffer, dwBytesRead);
        // printw("%s", cleanString.c_str());
        // refresh();

    } while (fRead && dwBytesRead >= 0);
}

// Convert virtual key code to ANSI escape sequence or character
void SendKeyToPipe(HANDLE hPipe, const KEY_EVENT_RECORD& keyEvent)
{
    DWORD dwBytesWritten {};
    char  escapeSeq[32] {};
    
    // Only process key down events (not key up)
    if (!keyEvent.bKeyDown) return;
    
    // Handle special keys that generate escape sequences
    switch (keyEvent.wVirtualKeyCode)
    {
        case VK_UP:
            strcpy_s(escapeSeq, "\x1b[A");
            break;
        case VK_DOWN:
            strcpy_s(escapeSeq, "\x1b[B");
            break;
        case VK_RIGHT:
            strcpy_s(escapeSeq, "\x1b[C");
            break;
        case VK_LEFT:
            strcpy_s(escapeSeq, "\x1b[D");
            break;
        case VK_HOME:
            strcpy_s(escapeSeq, "\x1b[H");
            break;
        case VK_END:
            strcpy_s(escapeSeq, "\x1b[F");
            break;
        case VK_PRIOR: // Page Up
            strcpy_s(escapeSeq, "\x1b[5~");
            break;
        case VK_NEXT: // Page Down
            strcpy_s(escapeSeq, "\x1b[6~");
            break;
        case VK_INSERT:
            strcpy_s(escapeSeq, "\x1b[2~");
            break;
        case VK_DELETE:
            strcpy_s(escapeSeq, "\x1b[3~");
            break;
        case VK_F1:
            strcpy_s(escapeSeq, "\x1bOP");
            break;
        case VK_F2:
            strcpy_s(escapeSeq, "\x1bOQ");
            break;
        case VK_F3:
            strcpy_s(escapeSeq, "\x1bOR");
            break;
        case VK_F4:
            strcpy_s(escapeSeq, "\x1bOS");
            break;
        case VK_F5:
            strcpy_s(escapeSeq, "\x1b[15~");
            break;
        case VK_F6:
            strcpy_s(escapeSeq, "\x1b[17~");
            break;
        case VK_F7:
            strcpy_s(escapeSeq, "\x1b[18~");
            break;
        case VK_F8:
            strcpy_s(escapeSeq, "\x1b[19~");
            break;
        case VK_F9:
            strcpy_s(escapeSeq, "\x1b[20~");
            break;
        case VK_F10:
            strcpy_s(escapeSeq, "\x1b[21~");
            break;
        case VK_F11:
            strcpy_s(escapeSeq, "\x1b[23~");
            break;
        case VK_F12:
            strcpy_s(escapeSeq, "\x1b[24~");
            break;
        case VK_ESCAPE:
            strcpy_s(escapeSeq, "\x1b");
            break;
        case VK_TAB:
            strcpy_s(escapeSeq, "\t");
            break;
        case VK_BACK:
            strcpy_s(escapeSeq, "\b");
            break;
        case VK_RETURN:
            strcpy_s(escapeSeq, "\r");
            break;
        default:
            // Handle regular characters and modified keys
            if (keyEvent.uChar.AsciiChar != 0)
            {
                // Handle Ctrl+key combinations
                if (keyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                {
                    char ctrlChar = keyEvent.uChar.AsciiChar;
                    WriteFile(hPipe, &ctrlChar, 1, &dwBytesWritten, NULL);
                }
                else
                {
                    // Regular character
                    char ch = keyEvent.uChar.AsciiChar;
                    WriteFile(hPipe, &ch, 1, &dwBytesWritten, NULL);
                }
                return;
            }
            else
            {
                // Keys that don't produce characters (like Caps Lock, etc.) - ignore
                return;
            }
    }
    
    // Send the escape sequence if we have one
    if (escapeSeq[0] != 0)
    {
        WriteFile(hPipe, escapeSeq, static_cast<DWORD>(strlen(escapeSeq)), &dwBytesWritten, NULL);
    }
}

// Thread function to read input from our console and send to pseudoconsole
void __cdecl InputSender(LPVOID pipe)
{
    HANDLE hPipe {reinterpret_cast<HANDLE>(pipe)};
    HANDLE hStdIn {GetStdHandle(STD_INPUT_HANDLE)};
    
    INPUT_RECORD inputBuffer[128];
    DWORD eventsRead {};
    
    do {
        // Read console input events
        if (ReadConsoleInput(hStdIn, inputBuffer, 128, &eventsRead))
        {
            for (DWORD i = 0; i < eventsRead; i++)
            {
                if (inputBuffer[i].EventType == KEY_EVENT)
                {
                    SendKeyToPipe(hPipe, inputBuffer[i].Event.KeyEvent);
                }
                // Ignore other event types (MOUSE_EVENT, WINDOW_BUFFER_SIZE_EVENT, etc.)
            }
            
            FlushFileBuffers(hPipe);
        }
        else
        {
            // Error reading input
            break;
        }
        
    } while (1);
}
