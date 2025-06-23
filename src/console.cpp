#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <curses.h>
#include "../include/ptymanager.hpp"

#ifdef _WIN32
#    include <windows.h>
#    include <io.h>
#    define isatty _isatty
#    define fileno _fileno
#else
#    include <unistd.h>
#    include <sys/ioctl.h>
#    include <termios.h>
#endif

struct Vec2
{
    int x = 0.0f, y = 0.0f;
};

Vec2 terminalSize;

std::filesystem::path currentPath;

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

std::string GetInput(const std::string& prompt)
{
    printw("%s", prompt.c_str());
    refresh();

    std::string input;
    int         ch;
    int         startY, startX;
    getyx(stdscr, startY, startX);

    while ((ch = getch()) != '\n' && ch != '\r')
    {
        if (ch == ERR)
        {
            // No input available, continue
            continue;
        }
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
        {
            // Handle backspace
            if (!input.empty())
            {
                input.pop_back();
                int y, x;
                getyx(stdscr, y, x);
                if (x > startX)
                {
                    mvaddch(y, x - 1, ' ');
                    move(y, x - 1);
                }
                refresh();
            }
        }
        else if (ch >= 32 && ch <= 126)
        {
            // Printable character
            input += static_cast<char>(ch);
            addch(ch);
            refresh();
        }
    }

    addch('\n');
    refresh();
    return input;
}

#define BUFF_SIZE 0x200

DWORD WINAPI PipeListener(HANDLE hPipeIn)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    char   szBuffer[BUFF_SIZE];
    DWORD  dwBytesWritten, dwBytesRead;

    while (ReadFile(hPipeIn, szBuffer, BUFF_SIZE, &dwBytesRead, NULL))
    {
        if (dwBytesRead == 0)
            break;
        WriteFile(hConsole, szBuffer, dwBytesRead, &dwBytesWritten,
                  NULL);
    }
    return TRUE;
}

int main(int argc, char** argv)
{
    currentPath = std::filesystem::current_path();
    std::string input;

    Terminal terminal;

    StartProgram();

    // while (true)
    // {

    //     // Update terminal size in case window was resized
    //     getmaxyx(stdscr, terminalSize.y, terminalSize.x);
    // }

    BOOL bRes;

    HANDLE hPipeIn = NULL, hPipeOut;
    HANDLE hPipePTYIn, hPipePTYOut;

    /* Create the pipes to which the ConPTY will connect */
    if (CreatePipe(&hPipePTYIn, &hPipeOut, NULL, 0) &&
        CreatePipe(&hPipeIn, &hPipePTYOut, NULL, 0))
    {
        /* Create the Pseudo Console attached to the PTY-end of the
         * pipes */
        COORD                      consoleSize = {0};
        CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
        if (GetConsoleScreenBufferInfo(
                GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        {
            consoleSize.X =
                csbi.srWindow.Right - csbi.srWindow.Left + 1;
            consoleSize.Y =
                csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }

        terminal.InitializeConsole(consoleSize, hPipePTYIn,
                                   hPipePTYOut, 0);

        assert(bRes == 0);

        CloseHandle(hPipePTYOut);
        CloseHandle(hPipePTYIn);
    }

    /* Create & start thread to write */
    HANDLE hThread =
        CreateThread(NULL, 0, PipeListener, hPipeIn, 0, NULL);
    assert(hThread != INVALID_HANDLE_VALUE);

    /* Initialize thread attribute */
    size_t                       AttrSize;
    LPPROC_THREAD_ATTRIBUTE_LIST AttrList = NULL;
    InitializeProcThreadAttributeList(NULL, 1, 0, &AttrSize);
    AttrList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, AttrSize);

    InitializeProcThreadAttributeList(AttrList, 1, 0, &AttrSize);

    bRes = UpdateProcThreadAttribute(
        AttrList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, /* 0x20016u */
        terminal.consoleHandle, sizeof terminal.consoleHandle, NULL, NULL);
    assert(bRes != 0);

    /* Initialize startup info struct */
    PROCESS_INFORMATION ProcInfo;
    memset(&ProcInfo, 0, sizeof ProcInfo);
    STARTUPINFOEXW SInfoEx;
    memset(&SInfoEx, 0, sizeof SInfoEx);
    SInfoEx.StartupInfo.cb = sizeof SInfoEx;
    SInfoEx.lpAttributeList = AttrList;

    wchar_t Command[] = L"cmd.exe";

    bRes = CreateProcessW(NULL, Command, NULL, NULL, FALSE,
                          EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                          &SInfoEx.StartupInfo, &ProcInfo);
    assert(bRes != 0);

    if (bRes)
        WaitForSingleObject(ProcInfo.hThread, INFINITE);

    /* Cleanup */
    CloseHandle(ProcInfo.hThread);
    CloseHandle(ProcInfo.hProcess);
    HeapFree(GetProcessHeap(), 0, AttrList);
    terminal.CloseConsole();
    CloseHandle(hPipeOut);
    CloseHandle(hPipeIn);

    return 0;
}
