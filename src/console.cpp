#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <curses.h>
#include <mutex>
#include <thread>
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
    int x = 0, y = 0;
};

Vec2 terminalSize;

std::filesystem::path currentPath;

static std::mutex  outMutex;
static std::string outputBuffer;
std::atomic<bool>  done {false};

HANDLE hPipeIn = NULL, hPipeOut;
HANDLE hPipePTYIn, hPipePTYOut;

Terminal terminal;

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
        init_pair(4, COLOR_WHITE, COLOR_BLACK);  // For errors
    }

    // Configure ncurses
    keypad(stdscr, TRUE);   // Enable special keys
    noecho();               // Don't echo input automatically
    cbreak();               // Disable line buffering
    nodelay(stdscr, FALSE); // Make getch() blocking

    // Get terminal size
    getmaxyx(stdscr, terminalSize.y, terminalSize.x);

    // Display welcome message
    attron(COLOR_PAIR(1));
    printw("Slurm 1.0\n");
    attroff(COLOR_PAIR(1));

    refresh();
}

#define BUFF_SIZE 0x200

void PipeListener()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    char   szBuffer[BUFF_SIZE];
    DWORD  dwBytesWritten, dwBytesRead, dwAvailable;

    int loopCount = 0;
    while (true)
    {
        loopCount++;

        // Check if data is available to read
        if (PeekNamedPipe(hPipeIn, NULL, 0, NULL, &dwAvailable, NULL))
        {
            if (loopCount % 100 == 0)
            { 
                printw("Loop %d: PeekNamedPipe succeeded, "
                       "dwAvailable: %lu\n",
                       loopCount, dwAvailable);
                refresh();
            }

            if (dwAvailable > 0)
            {
                printw("first.\n");
                refresh();

                if (ReadFile(hPipeIn, szBuffer, BUFF_SIZE - 1,
                             &dwBytesRead, NULL))
                {
                    printw("second\n");
                    refresh();

                    if (dwBytesRead == 0)
                        break;

                    szBuffer[dwBytesRead] =
                        '\0';

                    printw(szBuffer);
                    refresh();

                    WriteFile(hConsole, szBuffer, dwBytesRead,
                              &dwBytesWritten, NULL);
                }
                else
                {
                    // ReadFile failed
                    DWORD error = GetLastError();
                    if (error == ERROR_BROKEN_PIPE ||
                        error == ERROR_PIPE_NOT_CONNECTED)
                        break;
                }
            }
            else
            {
                // No data available, sleep briefly to avoid busy
                // waiting
                Sleep(10);
            }
        }
        else
        {
            printw("ERROR: DATA IS UNAVAILABLE TO READ FROM PIPE.");
            refresh();

            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE ||
                error == ERROR_PIPE_NOT_CONNECTED)
            {
                printw("FATAL ERROR: BROKEN PIPE OR PIPE NOT "
                       "CONNECTED. SHUTTING DOWN LISTENERS. PREPARE "
                       "FOR MELTDOWN.\n");
                refresh();
                break;
            }

            Sleep(10);
        }
    }
}

int main(int argc, char** argv)
{
    currentPath = std::filesystem::current_path();
    std::string input;

    StartProgram();

    BOOL bRes;

    SECURITY_ATTRIBUTES attributes = {};
    attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes.lpSecurityDescriptor = NULL;
    attributes.bInheritHandle = TRUE;  // This makes the pipe handles inheritable

    /* Create the pipes to which the ConPTY will connect */
    if (CreatePipe(&hPipePTYIn, &hPipeOut, &attributes, 0) &&
        CreatePipe(&hPipeIn, &hPipePTYOut, &attributes, 0))
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

        terminal.InitializeConsole(consoleSize, hPipeOut,
                                   hPipePTYOut, 0);

    }

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
        terminal.consoleHandle, sizeof terminal.consoleHandle, NULL,
        NULL);
    assert(bRes != 0);

    /* Initialize startup info struct */
    PROCESS_INFORMATION ProcInfo = {};
    STARTUPINFOEXW SInfoEx;
    memset(&SInfoEx, 0, sizeof SInfoEx);
    SInfoEx.StartupInfo.cb = sizeof SInfoEx;
    SInfoEx.lpAttributeList = AttrList;

    wchar_t Command[] = L"cmd /c echo Hello World";

    bRes = CreateProcessW(NULL, Command, NULL, NULL, FALSE,
                          EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                          &SInfoEx.StartupInfo, &ProcInfo);
    assert(bRes != 0);
    
    std::thread listenerThread(PipeListener);

    while (1) {}

    /* Cleanup */
    CloseHandle(ProcInfo.hThread);
    CloseHandle(ProcInfo.hProcess);
    HeapFree(GetProcessHeap(), 0, AttrList);
    terminal.CloseConsole();
    CloseHandle(hPipeOut);
    CloseHandle(hPipeIn);
    CloseHandle(hPipePTYOut);
    CloseHandle(hPipePTYIn);
    listenerThread.join();

    return 0;
}
