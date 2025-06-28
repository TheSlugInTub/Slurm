#ifdef _WIN32
#    include <Windows.h>
#    include <consoleapi.h>
#    include <io.h>
#    include <tchar.h>
#    include <process.h>
#    include <fcntl.h>
#    include <curses.h>
#    define isatty _isatty
#    define fileno _fileno
#else
#    include <unistd.h>
#    include <sys/ioctl.h>
#    include <termios.h>
#endif

struct IVec2
{
    int x = 0;
    int y = 0;

    IVec2(int x, int y)
        : x(x), y(y)
    {}
    
    IVec2()
        : x(0), y(0)
    {}
};

class Pseudoconsole
{
  public:
    HPCON               hPC;
    HANDLE              hPipeIn;
    HANDLE              hPipeOut;
    PROCESS_INFORMATION piClient {};
    STARTUPINFOEXW      startupInfo {};
    WINDOW* window;
    COORD size;
    IVec2 position;
    IVec2 cursorPosition;
    IVec2 savedCursorPosition;

    void Initialize(wchar_t* command);
    void Close();
    void Resize(COORD newSize);

  private:
    HRESULT InitializeStartupInfoAttachedToPseudoConsole(
        STARTUPINFOEXW* pStartupInfo);
    HRESULT CreatePseudoConsoleAndPipes(HANDLE* phPipeIn,
                                        HANDLE* phPipeOut);
};
