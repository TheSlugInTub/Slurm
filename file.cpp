#include <windows.h>
#include <cstdio>
#include <string>
#include <fcntl.h>
#include <consoleapi.h>
#include <io.h>
#include <process.h>
#include <iostream>

struct Vec2
{
    int x, y;

    Vec2(int x, int y) : x(x), y(y) {}
    Vec2() : x(0.0f), y(0.0f) {}
};

Vec2 GetTerminalSize(HANDLE hConsole)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi {};
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    return Vec2(csbi.srWindow.Right - csbi.srWindow.Left + 1,
                csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
}

int main()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);

    Vec2 termSize = GetTerminalSize(hConsole);

    DWORD dwBytesWritten;

    // Set console to UTF-8 for proper Unicode support
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Enable UTF-8 mode for C++ streams
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);

    // Enable Console VT Processing
    DWORD consoleMode {};
    GetConsoleMode(hConsole, &consoleMode);
    SetConsoleMode(hConsole, consoleMode |
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

    // Move to bottom line
    char positionBuffer[32];
    sprintf_s(positionBuffer, "\x1b[%d;1H", termSize.y);
    WriteFile(hConsole, positionBuffer,
              static_cast<DWORD>(strlen(positionBuffer)),
              &dwBytesWritten, NULL);

    // std::cout << "\033[" << termSize.y << ";1H";

    // Set colors
    const char* colorCode = "\x1b[30;43m";
    WriteFile(hConsole, colorCode,
              static_cast<DWORD>(strlen(colorCode)), &dwBytesWritten,
              NULL);

    const char* text = "Hey chuck!";

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
    const char* resetCode = "\x1b[0m";
    WriteFile(hConsole, resetCode,
              static_cast<DWORD>(strlen(resetCode)), &dwBytesWritten,
              NULL);

    while (1) {}

    return 0;
}
