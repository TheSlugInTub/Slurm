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
    consoles[newIndex].size = newSizeNew;

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
    consoles[newIndex].size = newSizeNew;

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
}

void StartProgram()
{
    // Initialize ncurses
    initscr();

    // Check if colors are supported
    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_BLUE, COLOR_BLACK);
        init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(6, COLOR_CYAN, COLOR_BLACK);
        init_pair(7, COLOR_WHITE, COLOR_BLACK);
        init_pair(8, COLOR_BLACK, COLOR_BLACK);
    }

    // Configure ncurses
    keypad(stdscr, TRUE);   // Enable special keys
    noecho();               // Don't echo input automatically
    cbreak();               // Disable line buffering
    nodelay(stdscr, FALSE); // Make getch() blocking
    setlocale(LC_ALL, "");

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

enum ANSIType
{
    ANSIType_CSI,
    ANSIType_OSC,
    ANSIType_DCS,
    ANSIType_Single,
};

// And here comes the ANSI parser
void PrintString(char* str, DWORD len, WINDOW* win)
{
    for (int i = 0; i < (int)len; ++i)
    {
        char ch = str[i];

        if (ch == '\x1b' && i + 1 < (int)len)
        {
            ANSIType ansiType;
            int      startPos = i;
            i++; // Move past ESC
            char ch1 = str[i];

            if (ch1 == '[')
            {
                ansiType = ANSIType_CSI;
                i++; // Move past '['

                // Parse CSI sequence
                char params[64] = {0};
                int  paramIdx = 0;

                // Collect parameter characters (digits, semicolons,
                // spaces)
                while (i < (int)len && paramIdx < 63)
                {
                    char c = str[i];
                    if ((c >= '0' && c <= '9') || c == ';' ||
                        c == ' ' || c == '?')
                    {
                        params[paramIdx++] = c;
                        i++;
                    }
                    else
                    {
                        break;
                    }
                }

                // Get the final command character
                char command = (i < (int)len) ? str[i] : 0;

                // Handle common CSI sequences
                switch (command)
                {
                    case 'm': // SGR - Select Graphic Rendition
                              // (colors, styles)
                    {
                        // Parse color/style parameters
                        if (paramIdx == 0 ||
                            (paramIdx == 1 && params[0] == '0'))
                        {
                            // Reset all attributes
                            wattrset(win, A_NORMAL);
                        }
                        else
                        {
                            // Parse semicolon-separated parameters
                            char* param = strtok(params, ";");
                            while (param != NULL)
                            {
                                int code = atoi(param);
                                switch (code)
                                {
                                    case 0:
                                        wattrset(win, A_NORMAL);
                                        break;
                                    case 1:
                                        wattron(win, A_BOLD);
                                        break;
                                    case 2:
                                        wattron(win, A_DIM);
                                        break;
                                    case 4:
                                        wattron(win, A_UNDERLINE);
                                        break;
                                    case 5:
                                        wattron(win, A_BLINK);
                                        break;
                                    case 7:
                                        wattron(win, A_REVERSE);
                                        break;
                                    case 22:
                                        wattroff(win, A_BOLD | A_DIM);
                                        break;
                                    case 24:
                                        wattroff(win, A_UNDERLINE);
                                        break;
                                    case 25:
                                        wattroff(win, A_BLINK);
                                        break;
                                    case 27:
                                        wattroff(win, A_REVERSE);
                                        break;
                                    // Foreground colors (30-37,
                                    // 90-97)
                                    case 30:
                                    case 31:
                                    case 32:
                                    case 33:
                                    case 34:
                                    case 35:
                                    case 36:
                                    case 37:
                                        if (has_colors())
                                        {
                                            int colorPair =
                                                (code - 30) + 1;
                                            if (colorPair <=
                                                COLOR_PAIRS)
                                                wattron(
                                                    win,
                                                    COLOR_PAIR(
                                                        colorPair));
                                        }
                                        break;
                                    case 90:
                                    case 91:
                                    case 92:
                                    case 93:
                                    case 94:
                                    case 95:
                                    case 96:
                                    case 97:
                                        // Bright colors - treat as
                                        // normal colors for now
                                        if (has_colors())
                                        {
                                            int colorPair =
                                                (code - 90) + 1;
                                            if (colorPair <=
                                                COLOR_PAIRS)
                                                wattron(
                                                    win,
                                                    COLOR_PAIR(
                                                        colorPair));
                                        }
                                        break;
                                    case 39: // Default foreground
                                        if (has_colors())
                                            wattroff(win, A_COLOR);
                                        break;
                                }
                                param = strtok(NULL, ";");
                            }
                        }
                        break;
                    }
                    case 'A': // Cursor Up
                    {
                        int n = (paramIdx > 0) ? atoi(params) : 1;
                        int y, x;
                        getyx(win, y, x);
                        wmove(win, max(0, y - n), x);
                        break;
                    }
                    case 'B': // Cursor Down
                    {
                        int n = (paramIdx > 0) ? atoi(params) : 1;
                        int y, x;
                        getyx(win, y, x);
                        wmove(win, y + n, x);
                        break;
                    }
                    case 'C': // Cursor Forward (Right)
                    {
                        int n = (paramIdx > 0) ? atoi(params) : 1;
                        int y, x;
                        getyx(win, y, x);
                        wmove(win, y, x + n);
                        break;
                    }
                    case 'D': // Cursor Backward (Left)
                    {
                        int n = (paramIdx > 0) ? atoi(params) : 1;
                        int y, x;
                        getyx(win, y, x);
                        wmove(win, y, max(0, x - n));
                        break;
                    }
                    case 'H': // Cursor Position
                    case 'f': // Horizontal and Vertical Position
                    {
                        int row = 1, col = 1;
                        if (paramIdx > 0)
                        {
                            char* rowStr = strtok(params, ";");
                            char* colStr = strtok(NULL, ";");
                            if (rowStr)
                                row = atoi(rowStr);
                            if (colStr)
                                col = atoi(colStr);
                        }
                        wmove(win, row - 1,
                              col - 1); // Convert to 0-based
                        break;
                    }
                    case 'J': // Erase in Display
                    {
                        int n = (paramIdx > 0) ? atoi(params) : 0;
                        switch (n)
                        {
                            case 0:
                                wclrtobot(win);
                                break; // Clear from cursor to end
                            case 1: // Clear from start to cursor (not
                                    // directly supported)
                                break;
                            case 2:
                                wclear(win);
                                break; // Clear entire screen
                        }
                        break;
                    }
                    case 'K': // Erase in Line
                    {
                        int n = (paramIdx > 0) ? atoi(params) : 0;
                        switch (n)
                        {
                            case 0:
                                wclrtoeol(win);
                                break; // Clear from cursor to end of
                                       // line
                            case 1:    // Clear from start of line to
                                    // cursor (not directly supported)
                                break;
                            case 2: // Clear entire line
                            {
                                int y, x;
                                getyx(win, y, x);
                                wmove(win, y, 0);
                                wclrtoeol(win);
                                wmove(win, y, x);
                                break;
                            }
                        }
                        break;
                    }
                    case 'S': // Scroll Up
                    {
                        int n = (paramIdx > 0) ? atoi(params) : 1;
                        for (int j = 0; j < n; j++) wscrl(win, 1);
                        break;
                    }
                    case 'T': // Scroll Down
                    {
                        int n = (paramIdx > 0) ? atoi(params) : 1;
                        for (int j = 0; j < n; j++) wscrl(win, -1);
                        break;
                    }
                    case 'h': // Set Mode
                    case 'l': // Reset Mode
                    {
                        char* next_token = nullptr;
                        char* token =
                            strtok_s(params, ";", &next_token);
                        while (token != nullptr)
                        {
                            if (strcmp(token, "?25") ==
                                0) // Cursor visibility
                            {
                                if (command == 'h')
                                    curs_set(1); // Show cursor
                                else
                                    curs_set(0); // Hide cursor
                            }
                            // else if (strcmp(token, "?1049") ==
                            //          0) // Alternate screen buffer
                            // {
                            //     if (command == 'h')
                            //     {
                            //         // Switch to alternate screen
                            //         wclear(win);
                            //     }
                            //     else
                            //     {
                            //     }
                            // }
                            token =
                                strtok_s(nullptr, ";", &next_token);
                        }
                        break;
                    }
                    case 's': // Save cursor position
                        getyx(win, consoles[activeConsole].savedCursorPosition.y,
                              consoles[activeConsole].savedCursorPosition.x);
                        break;
                    case 'u': // Restore cursor position
                        wmove(win, consoles[activeConsole].savedCursorPosition.y,
                              consoles[activeConsole].savedCursorPosition.x);
                        break;
                    default:
                        // Unknown CSI sequence - ignore
                        break;
                }
            }
            else if (ch1 == ']')
            {
                ansiType = ANSIType_OSC;
                i++; // Move past ']'

                // OSC sequences end with BEL (0x07) or ST (ESC \)
                while (i < (int)len)
                {
                    if (str[i] == 0x07) // BEL
                    {
                        break;
                    }
                    else if (str[i] == '\x1b' && i + 1 < (int)len &&
                             str[i + 1] == '\\')
                    {
                        i++; // Skip the backslash too
                        break;
                    }
                    i++;
                }
                // OSC sequences typically handle window titles, etc.
                // For a terminal multiplexer, we might want to ignore
                // these or handle specific ones like setting window
                // titles
            }
            else if (ch1 == 'P')
            {
                ansiType = ANSIType_DCS;
                i++; // Move past 'P'

                // DCS sequences end with ST (ESC \)
                while (i < (int)len)
                {
                    if (str[i] == '\x1b' && i + 1 < (int)len &&
                        str[i + 1] == '\\')
                    {
                        i++; // Skip the backslash too
                        break;
                    }
                    i++;
                }
                // DCS sequences are device control strings
                // For basic terminal emulation, we can ignore these
            }
            else
            {
                ansiType = ANSIType_Single;

                // Handle single-character escape sequences
                switch (ch1)
                {
                    case 'D': // Index (move cursor down one line,
                              // scroll if at bottom)
                    {
                        int y, x;
                        getyx(win, y, x);
                        int maxy, maxx;
                        getmaxyx(win, maxy, maxx);
                        if (y >= maxy - 1)
                            wscrl(win, 1);
                        else
                            wmove(win, y + 1, x);
                        break;
                    }
                    case 'E': // Next Line (move to beginning of next
                              // line)
                    {
                        int y, x;
                        getyx(win, y, x);
                        wmove(win, y + 1, 0);
                        break;
                    }
                    case 'M': // Reverse Index (move cursor up one
                              // line, scroll if at top)
                    {
                        int y, x;
                        getyx(win, y, x);
                        if (y <= 0)
                            wscrl(win, -1);
                        else
                            wmove(win, y - 1, x);
                        break;
                    }
                    case 'c': // Reset terminal
                        wclear(win);
                        wmove(win, 0, 0);
                        wattrset(win, A_NORMAL);
                        break;
                    case '7': // Save cursor position
                        // Would need to store position somewhere
                        break;
                    case '8': // Restore cursor position
                        // Would need to restore stored position
                        break;
                    default:
                        // Unknown single escape sequence - ignore
                        break;
                }
            }
        }
        else if (ch == '\r')
        {
            // Carriage return - move to beginning of current line
            int y, x;
            getyx(win, y, x);
            wmove(win, y, 0);
        }
        else if (ch == '\n')
        {
            // Line feed - move to next line
            int y, x;
            getyx(win, y, x);
            wmove(win, y + 1, x);
        }
        else if (ch == '\b')
        {
            // Backspace - move cursor left
            int y, x;
            getyx(win, y, x);
            if (x > 0)
                wmove(win, y, x - 1);
        }
        else if (ch == '\t')
        {
            // Tab - move to next tab stop (typically every 8
            // characters)
            int y, x;
            getyx(win, y, x);
            int nextTab = ((x / 8) + 1) * 8;
            wmove(win, y, nextTab);
        }
        else if (ch >= 32 || ch < 0) // Printable characters
                                     // (including extended ASCII)
        {
            waddch(win, (unsigned char)ch);
        }
        // Control characters < 32 (except those handled above) are
        // ignored
    }
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
            PrintString(szBuffer, dwBytesRead,
                        consoles[consoleIndex].window);
            wrefresh(consoles[consoleIndex].window);

            LeaveCriticalSection(&consoleMutex);
        }

    } while (fRead && dwBytesRead > 0);
}
