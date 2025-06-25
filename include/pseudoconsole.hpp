#ifdef _WIN32
#    include <windows.h>
#    include <consoleapi.h>
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

class Pseudoconsole
{
  public:
    HPCON               hPC;
    HANDLE              hPipeIn;
    HANDLE              hPipeOut;
    PROCESS_INFORMATION piClient {};
    STARTUPINFOEXW      startupInfo {};

    void Initialize(wchar_t* command);
    void Close();

  private:
    HRESULT InitializeStartupInfoAttachedToPseudoConsole(
        STARTUPINFOEXW* pStartupInfo);
    HRESULT CreatePseudoConsoleAndPipes(HANDLE* phPipeIn,
                                        HANDLE* phPipeOut);
};
