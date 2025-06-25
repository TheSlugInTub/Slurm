#include <iostream>
#include <cstdio>
#include <curses.h>
#include <pseudoconsole.hpp>

struct Vec2
{
    int x, y;

    Vec2(int x, int y) : x(x), y(y) {}
    Vec2() : x(0.0f), y(0.0f) {}
};

Vec2 terminalSize;

// ANSI escape code functions
void ClearScreen()
{
    printf("\033[2J\x1b[H");
}

void MoveCursor(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

void SetColor(int fg, int bg = 40)
{
    printf("\033[%d;%dm", fg, bg);
}

void ResetColor()
{
    printf("\033[0m");
}

Vec2 GetTerminalSize(HANDLE hConsole)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi {};
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    return Vec2(csbi.srWindow.Right - csbi.srWindow.Left + 1,
                csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
}

void __cdecl PipeListener(LPVOID);
void __cdecl InputSender(LPVOID);
void __cdecl StatusDrawer(LPVOID);

Pseudoconsole console;
// Pseudoconsole console2;

int activeConsole = 0;

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

    terminalSize = GetTerminalSize(hConsole);

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
    // console2.Initialize(szCommand);

    HANDLE hPipeListenerThread {reinterpret_cast<HANDLE>(
        _beginthread(PipeListener, 0, console.hPipeIn))};
    HANDLE hInputSenderThread {reinterpret_cast<HANDLE>(
        _beginthread(InputSender, 0, console.hPipeOut))};
    HANDLE hStatusThraead {reinterpret_cast<HANDLE>(
        _beginthread(StatusDrawer, 0, console.hPipeOut))};

    if (S_OK == hr)
    {
        // Wait up to 10s for ping process to complete
        WaitForSingleObject(console.piClient.hThread, INFINITE);
    }

    return S_OK == hr ? EXIT_SUCCESS : EXIT_FAILURE;
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
            strcpy_s(escapeSeq, "\033[A");
            break;
        case VK_DOWN:
            strcpy_s(escapeSeq, "\033[B");
            break;
        case VK_RIGHT:
            strcpy_s(escapeSeq, "\033[C");
            break;
        case VK_LEFT:
            strcpy_s(escapeSeq, "\033[D");
            break;
        case VK_HOME:
            strcpy_s(escapeSeq, "\033[H");
            break;
        case VK_END:
            strcpy_s(escapeSeq, "\033[F");
            break;
        case VK_PRIOR: // Page Up
            strcpy_s(escapeSeq, "\033[5~");
            break;
        case VK_NEXT: // Page Down
            strcpy_s(escapeSeq, "\033[6~");
            break;
        case VK_INSERT:
            strcpy_s(escapeSeq, "\033[2~");
            break;
        case VK_DELETE:
            strcpy_s(escapeSeq, "\033[3~");
            break;
        case VK_F1:
            strcpy_s(escapeSeq, "\033OP");
            break;
        case VK_F2:
            strcpy_s(escapeSeq, "\033OQ");
            break;
        case VK_F3:
            strcpy_s(escapeSeq, "\033OR");
            break;
        case VK_F4:
            strcpy_s(escapeSeq, "\033OS");
            break;
        case VK_F5:
            strcpy_s(escapeSeq, "\033[15~");
            break;
        case VK_F6:
            strcpy_s(escapeSeq, "\033[17~");
            break;
        case VK_F7:
            strcpy_s(escapeSeq, "\033[18~");
            break;
        case VK_F8:
            strcpy_s(escapeSeq, "\033[19~");
            break;
        case VK_F9:
            strcpy_s(escapeSeq, "\033[20~");
            break;
        case VK_F10:
            strcpy_s(escapeSeq, "\033[21~");
            break;
        case VK_F11:
            strcpy_s(escapeSeq, "\033[23~");
            break;
        case VK_F12:
            strcpy_s(escapeSeq, "\033[24~");
            break;
        case VK_ESCAPE:
            strcpy_s(escapeSeq, "\033");
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
    } while (fRead && dwBytesRead >= 0);
}

void DrawStatusLine(HANDLE hConsole, const Vec2& termSize,
                    const std::string& text = "")
{
    DWORD dwBytesWritten;

    // Move to bottom line
    char positionBuffer[32];
    sprintf_s(positionBuffer, "\033[%d;1H", termSize.y);
    WriteFile(hConsole, positionBuffer,
              static_cast<DWORD>(strlen(positionBuffer)),
              &dwBytesWritten, NULL);

    // Set colors
    const char* colorCode = "\033[30;43m";
    WriteFile(hConsole, colorCode,
              static_cast<DWORD>(strlen(colorCode)), &dwBytesWritten,
              NULL);

    // Create status line content
    std::string statusLine = text;
    if (statusLine.length() > termSize.x)
    {
        statusLine =
            statusLine.substr(0, termSize.x); // Truncate if too long
    }
    else
    {
        statusLine.resize(termSize.x,
                          ' '); // Pad with spaces to fill line
    }

    WriteFile(hConsole, statusLine.c_str(),
              static_cast<DWORD>(statusLine.length()),
              &dwBytesWritten, NULL);

    // Reset colors
    const char* resetCode = "\033[0m";
    WriteFile(hConsole, resetCode,
              static_cast<DWORD>(strlen(resetCode)), &dwBytesWritten,
              NULL);
}

void __cdecl StatusDrawer(LPVOID lpvoid)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // Draw initial status line
    DrawStatusLine(hConsole, terminalSize, "Status: Ready");

    while (1) { Sleep(100); }
}
