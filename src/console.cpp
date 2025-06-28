#include <iostream>
#include <cstdio>
#include <vector>
#include <thread>
#include <curses.h>
#include <pseudoconsole.hpp>
#include <fstream>

struct Vec2
{
    int x, y;

    Vec2(int x, int y) : x(x), y(y) {}
    Vec2() : x(0.0f), y(0.0f) {}
};

Vec2 terminalSize;

Vec2 GetTerminalSize(HANDLE hConsole)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi {};
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    return Vec2(csbi.srWindow.Right - csbi.srWindow.Left + 1,
                csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
}

void __cdecl PipeListener(HANDLE hPipe, int consoleIndex);
void __cdecl InputSender(int* consoleIndex);

void DrawStatusLine(HANDLE hConsole, const Vec2& termSize,
                    const std::string& text);

std::vector<Pseudoconsole> consoles;

CRITICAL_SECTION consoleMutex;

int activeConsole = 0;

const std::string statusText =
    "SLURM 3.161.746 ------------ MADE BY "
    "SLUGARIUS "
    "------------ PROPERTY OF THE OPENWELL "
    "FOUNDATION";

wchar_t szCommand[] {L"cmd.exe"};
    
std::ofstream file;

// Fixed horizontal split function
void CreateHorizontalSplit(int index)
{
    IVec2 originalPos = consoles[index].position;
    COORD originalSize = consoles[index].size;

    // Resize original console to half width
    COORD newSizeOriginal = {(SHORT)(originalSize.X / 2),
                             originalSize.Y};
    consoles[index].Resize(newSizeOriginal);

    // Create new console
    consoles.push_back({});
    int newIndex = consoles.size() - 1;

    // Calculate new console size
    COORD newSizeNew = {(SHORT)(originalSize.X - newSizeOriginal.X),
                        originalSize.Y};

    // Set new console position
    IVec2 newPos = {originalPos.x + newSizeOriginal.X, originalPos.y};

    // Initialize and position new console
    consoles[newIndex].Initialize(szCommand);
    consoles[newIndex].Resize(newSizeNew);
    consoles[newIndex].position = newPos;

    activeConsole = newIndex;

    // Update ncurses windows
    wresize(consoles[index].window, newSizeOriginal.Y,
            newSizeOriginal.X);
    mvwin(consoles[index].window, originalPos.y, originalPos.x);

    consoles[newIndex].window =
        newwin(newSizeNew.Y, newSizeNew.X, newPos.y, newPos.x);

    // Start pipe listener for new console
    std::thread(PipeListener, consoles[newIndex].hPipeIn, newIndex)
        .detach();

    // Refresh windows
    wrefresh(consoles[index].window);
    wrefresh(consoles[newIndex].window);
    
    file << "newPos: " << newPos.x << ", " << newPos.y << '\n';
    file << "newSizeNew: " << newSizeNew.X << ", " << newSizeNew.Y << '\n';
    file << "newSizeOriginal: " << newSizeOriginal.X << ", " << newSizeOriginal.Y << '\n';
}

// Fixed vertical split function
void CreateVerticalSplit(int index)
{
    IVec2 originalPos = consoles[index].position;
    COORD originalSize = consoles[index].size;

    // Resize original console to half height
    COORD newSizeOriginal = {originalSize.X,
                             (SHORT)(originalSize.Y / 2)};
    consoles[index].Resize(newSizeOriginal);

    // Create new console
    consoles.push_back({});
    int newIndex = consoles.size() - 1;

    // Calculate new console size
    COORD newSizeNew = {originalSize.X,
                        (SHORT)(originalSize.Y - newSizeOriginal.Y)};

    // Set new console position (below original)
    IVec2 newPos = {originalPos.x, originalPos.y + newSizeOriginal.Y};

    // Initialize and position new console
    consoles[newIndex].Initialize(szCommand);
    consoles[newIndex].Resize(newSizeNew);
    consoles[newIndex].position = newPos;

    activeConsole = newIndex;

    // Update ncurses windows
    wresize(consoles[index].window, newSizeOriginal.Y,
            newSizeOriginal.X);
    mvwin(consoles[index].window, originalPos.y, originalPos.x);

    consoles[newIndex].window =
        newwin(newSizeNew.Y, newSizeNew.X, newPos.y, newPos.x);

    // Start pipe listener for new console
    std::thread(PipeListener, consoles[newIndex].hPipeIn, newIndex)
        .detach();

    // Refresh windows
    wrefresh(consoles[index].window);
    wrefresh(consoles[newIndex].window);

    file << "newPos: " << newPos.x << ", " << newPos.y << '\n';
    file << "newSizeNew: " << newSizeNew.X << ", " << newSizeNew.Y << '\n';
    file << "newSizeOriginal: " << newSizeOriginal.X << ", " << newSizeOriginal.Y << '\n';
}

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

    refresh();
}

int main()
{
    file.open("debug.txst");

    InitializeCriticalSection(&consoleMutex);

    StartProgram();

    HRESULT hr {E_UNEXPECTED};
    HANDLE  hConsole = {GetStdHandle(STD_OUTPUT_HANDLE)};
    HANDLE  hConsoleIn = {GetStdHandle(STD_INPUT_HANDLE)};

    terminalSize = GetTerminalSize(hConsole);

    int cIndex = 0;

    consoles.push_back({});
    consoles[cIndex].Initialize(szCommand);

    COORD newSize = consoles[cIndex].size;
    IVec2 newPos = consoles[cIndex].position;
    consoles[cIndex].window =
        newwin(newSize.Y, newSize.X, newPos.y, newPos.x);

    std::thread hPipeListenerThread(
        PipeListener, consoles[activeConsole].hPipeIn, 0);

    std::thread hInputSenderThread(InputSender, &activeConsole);

    // EnterCriticalSection(&consoleMutex);
    // DrawStatusLine(hConsole, terminalSize, statusText);
    // LeaveCriticalSection(&consoleMutex);

    while (1) {}

    file.close();

    return S_OK == hr ? EXIT_SUCCESS : EXIT_FAILURE;
}

void SendKeyToPipe(HANDLE hPipe, int key)
{
    DWORD dwBytesWritten {};

    switch (key)
    {
        case KEY_ENTER:
        case '\r':
        case '\n':
        {
            char enter[] = "\r\n";
            WriteFile(hPipe, enter, 2, &dwBytesWritten, NULL);
            break;
        }
        case KEY_BACKSPACE:
        case '\b':
        case 127:
        {
            char backspace = '\b';
            WriteFile(hPipe, &backspace, 1, &dwBytesWritten, NULL);
            break;
        }
        case KEY_UP:
        {
            // char up[] = "\x1b[A";
            // WriteFile(hPipe, up, 3, &dwBytesWritten, NULL);
            CreateHorizontalSplit(activeConsole);
            break;
        }
        case KEY_DOWN:
        {
            // char down[] = "\x1b[B";
            // WriteFile(hPipe, down, 3, &dwBytesWritten, NULL);
            CreateVerticalSplit(activeConsole);
            break;
        }
        case KEY_RIGHT:
        {
            char right[] = "\x1b[C";
            WriteFile(hPipe, right, 3, &dwBytesWritten, NULL);
            break;
        }
        case KEY_LEFT:
        {
            char left[] = "\x1b[D";
            WriteFile(hPipe, left, 3, &dwBytesWritten, NULL);
            break;
        }
        case '\t':
        {
            char tab = '\t';
            WriteFile(hPipe, &tab, 1, &dwBytesWritten, NULL);
            break;
        }
        default:
        {
            if (key >= 32 && key <= 126)
            { // Printable ASCII
                char ch = (char)key;
                WriteFile(hPipe, &ch, 1, &dwBytesWritten, NULL);
            }
            break;
        }
    }

    FlushFileBuffers(hPipe);
}

// Thread function to read input from ncurses and send to
// pseudoconsole
void __cdecl InputSender(int* consoleIndexP)
{
    int key;

    do {
        int consoleIndex = (*consoleIndexP);

        key = getch();

        // Handle special application keys
        if (key == 27)
        { // ESC key - could be start of escape sequence or standalone
          // ESC
            nodelay(stdscr, TRUE); // Make next getch non-blocking
            int next = getch();
            nodelay(stdscr, FALSE); // Restore blocking

            if (next == ERR)
            {
                // Standalone ESC key
                char  esc = 27;
                DWORD dwBytesWritten;
                WriteFile(consoles[consoleIndex].hPipeOut, &esc, 1,
                          &dwBytesWritten, NULL);
                FlushFileBuffers(consoles[consoleIndex].hPipeOut);
            }
            else
            {
                // Put the character back and let normal processing
                // handle it
                ungetch(next);
                ungetch(key);
                continue;
            }
        }
        else
        {
            SendKeyToPipe(consoles[consoleIndex].hPipeOut, key);
        }

    } while (key != 'q' && key != 'Q'); // Add quit condition
}

std::string CleanString(char* string, int len)
{
    std::string str {};
    for (int i = 0; i < len; i++)
    {
        if (string[i] != '\r')
        {
            str.append(1, string[i]);
        }
    }

    return str;
}

void __cdecl PipeListener(HANDLE hPipe, int consoleIndex)
{
    const DWORD BUFF_SIZE {2048};
    char        szBuffer[BUFF_SIZE];

    DWORD dwBytesRead {};
    BOOL  fRead {FALSE};

    do {
        // Clear buffer
        memset(szBuffer, 0, BUFF_SIZE);

        // Read from the pipe
        fRead = ReadFile(hPipe, szBuffer, BUFF_SIZE - 1, &dwBytesRead,
                         NULL);

        if (fRead && dwBytesRead > 0)
        {
            // Ensure null termination
            szBuffer[dwBytesRead] = '\0';

            // Write to ncurses screen
            EnterCriticalSection(&consoleMutex);

            // Add the text without additional newlines
            move(consoles[consoleIndex].position.y,
                 consoles[consoleIndex].position.x);
            waddstr(consoles[consoleIndex].window,
                    CleanString(szBuffer, dwBytesRead).c_str());
            wrefresh(consoles[consoleIndex].window);

            LeaveCriticalSection(&consoleMutex);
        }

    } while (fRead && dwBytesRead > 0);
}
