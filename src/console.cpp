#include <iostream>
#include <cstdio>
#include <curses.h>
#include <pseudoconsole.hpp>

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
void __cdecl PipeListener(LPVOID);
void __cdecl InputSender(LPVOID);

Pseudoconsole console;

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
    SetConsoleMode(hConsoleIn, inputMode | ENABLE_EXTENDED_FLAGS |
                                   ENABLE_WINDOW_INPUT |
                                   ENABLE_MOUSE_INPUT);

    console.Initialize(szCommand);

    HANDLE hPipeListenerThread {reinterpret_cast<HANDLE>(
        _beginthread(PipeListener, 0, console.hPipeIn))};
    HANDLE hInputSenderThread {reinterpret_cast<HANDLE>(
        _beginthread(InputSender, 0, console.hPipeOut))};

    if (S_OK == hr)
    {
        // Wait up to 10s for ping process to complete
        WaitForSingleObject(console.piClient.hThread, INFINITE);
    }

    return S_OK == hr ? EXIT_SUCCESS : EXIT_FAILURE;
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
    if (!keyEvent.bKeyDown)
        return;

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
                if (keyEvent.dwControlKeyState &
                    (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                {
                    char ctrlChar = keyEvent.uChar.AsciiChar;
                    WriteFile(hPipe, &ctrlChar, 1, &dwBytesWritten,
                              NULL);
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
                // Keys that don't produce characters (like Caps Lock,
                // etc.) - ignore
                return;
            }
    }

    // Send the escape sequence if we have one
    if (escapeSeq[0] != 0)
    {
        WriteFile(hPipe, escapeSeq,
                  static_cast<DWORD>(strlen(escapeSeq)),
                  &dwBytesWritten, NULL);
    }
}

// Thread function to read input from our console and send to
// pseudoconsole
void __cdecl InputSender(LPVOID pipe)
{
    HANDLE hPipe {reinterpret_cast<HANDLE>(pipe)};
    HANDLE hStdIn {GetStdHandle(STD_INPUT_HANDLE)};

    INPUT_RECORD inputBuffer[128];
    DWORD        eventsRead {};

    do {
        // Read console input events
        if (ReadConsoleInput(hStdIn, inputBuffer, 128, &eventsRead))
        {
            for (DWORD i = 0; i < eventsRead; i++)
            {
                if (inputBuffer[i].EventType == KEY_EVENT)
                {
                    SendKeyToPipe(hPipe,
                                  inputBuffer[i].Event.KeyEvent);
                }
                // Ignore other event types (MOUSE_EVENT,
                // WINDOW_BUFFER_SIZE_EVENT, etc.)
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
