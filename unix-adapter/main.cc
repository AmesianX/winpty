// Copyright (c) 2011-2012 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <winpty.h>
#include "../shared/DebugClient.h"
#include <string>


static int signalWriteFd;
static volatile bool ioHandlerDied;


// Put the input terminal into non-blocking non-canonical mode.
static termios setRawTerminalMode()
{
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "input is not a tty\n");
        exit(1);
    }
    if (!isatty(STDOUT_FILENO)) {
        fprintf(stderr, "output is not a tty\n");
        exit(1);
    }

    termios buf;
    if (tcgetattr(STDIN_FILENO, &buf) < 0) {
        perror("tcgetattr failed");
        exit(1);
    }
    termios saved = buf;
    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    buf.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    buf.c_cflag &= ~(CSIZE | PARENB);
    buf.c_cflag |= CS8;
    buf.c_oflag &= ~OPOST;
    buf.c_cc[VMIN] = 1;  // blocking read
    buf.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0) {
        fprintf(stderr, "tcsetattr failed\n");
        exit(1);
    }
    return saved;
}

static void restoreTerminalMode(termios original)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) < 0) {
        perror("error restoring terminal mode");
        exit(1);
    }
}

static void writeToSignalFd()
{
    char dummy = 0;
    write(signalWriteFd, &dummy, 1);
}

static void terminalResized(int signo)
{
    writeToSignalFd();
}

// Create a manual reset, initially unset event.
static HANDLE createEvent()
{
    return CreateEvent(NULL, TRUE, FALSE, NULL);
}


// Connect winpty overlapped I/O to Cygwin blocking STDOUT_FILENO.
class OutputHandler {
public:
    OutputHandler(HANDLE winpty);
    pthread_t getThread() { return thread; }
private:
    static void *threadProc(void *pvthis);
    HANDLE winpty;
    pthread_t thread;
};

OutputHandler::OutputHandler(HANDLE winpty) : winpty(winpty)
{
    pthread_create(&thread, NULL, OutputHandler::threadProc, this);
}

// TODO: See whether we can make the pipe non-overlapped if we still use
// an OVERLAPPED structure in the ReadFile/WriteFile calls.
void *OutputHandler::threadProc(void *pvthis)
{
    OutputHandler *pthis = (OutputHandler*)pvthis;
    HANDLE event = createEvent();
    OVERLAPPED over;
    const int bufferSize = 4096;
    char *buffer = new char[bufferSize];
    while (true) {
        DWORD amount;
        memset(&over, 0, sizeof(over));
        over.hEvent = event;
        BOOL ret = ReadFile(pthis->winpty,
                            buffer, bufferSize,
                            &amount,
                            &over);
        if (!ret && GetLastError() == ERROR_IO_PENDING)
            ret = GetOverlappedResult(pthis->winpty, &over, &amount, TRUE);
        if (!ret || amount == 0)
            break;
        // TODO: partial writes?
        // I don't know if this write can be interrupted or not, but handle it
        // just in case.
        int written;
        do {
            written = write(STDOUT_FILENO, buffer, amount);
        } while (written == -1 && errno == EINTR);
        if (written != (int)amount)
            break;
    }
    delete [] buffer;
    CloseHandle(event);
    ioHandlerDied = true;
    writeToSignalFd();
    return NULL;
}


// Connect Cygwin non-blocking STDIN_FILENO to winpty overlapped I/O.
class InputHandler {
public:
    InputHandler(HANDLE winpty);
    pthread_t getThread() { return thread; }
private:
    static void *threadProc(void *pvthis);
    HANDLE winpty;
    pthread_t thread;
};

InputHandler::InputHandler(HANDLE winpty) : winpty(winpty)
{
    pthread_create(&thread, NULL, InputHandler::threadProc, this);
}

void *InputHandler::threadProc(void *pvthis)
{
    InputHandler *pthis = (InputHandler*)pvthis;
    HANDLE event = createEvent();
    const int bufferSize = 4096;
    char *buffer = new char[bufferSize];
    while (true) {
        int amount = read(STDIN_FILENO, buffer, bufferSize);
        if (amount == -1 && errno == EINTR) {
            // Apparently, this read is interrupted on Cygwin 1.7 by a SIGWINCH
            // signal even though I set the SA_RESTART flag on the handler.
            continue;
        }
        if (amount <= 0)
            break;
        DWORD written;
        OVERLAPPED over;
        memset(&over, 0, sizeof(over));
        over.hEvent = event;
        BOOL ret = WriteFile(pthis->winpty,
                             buffer, amount,
                             &written,
                             &over);
        if (!ret && GetLastError() == ERROR_IO_PENDING)
            ret = GetOverlappedResult(pthis->winpty, &over, &written, TRUE);
        // TODO: partial writes?
        if (!ret || (int)written != amount)
            break;
    }
    delete [] buffer;
    CloseHandle(event);
    ioHandlerDied = true;
    writeToSignalFd();
    return NULL;
}

static void setFdNonBlock(int fd)
{
    int status = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, status | O_NONBLOCK);
}

// Convert argc/argv into a Win32 command-line following the escaping convention
// documented on MSDN.  (e.g. see CommandLineToArgvW documentation)
static std::string argvToCommandLine(int argc, char *argv[])
{
    std::string result;
    for (int argIndex = 0; argIndex < argc; ++argIndex) {
        if (argIndex > 0)
            result.push_back(' ');
        const char *arg = argv[argIndex];
        bool quote = strchr(arg, ' ') != NULL || strchr(arg, '\"') != NULL;
        if (quote)
            result.push_back('\"');
        int bsCount = 0;
        for (const char *p = arg; *p != '\0'; ++p) {
            if (*p == '\\') {
                bsCount++;
            } else if (*p == '\"') {
                result.append(bsCount * 2 + 1, '\\');
                result.push_back('\"');
                bsCount = 0;
            } else {
                result.append(bsCount, '\\');
                bsCount = 0;
                result.push_back(*p);
            }
        }
        if (quote) {
            result.append(bsCount * 2, '\\');
            result.push_back('\"');
        } else {
            result.append(bsCount, '\\');
        }
    }
    return result;
}

static wchar_t *heapMbsToWcs(const char *text)
{
    // Calling mbstowcs with a NULL first argument seems to be broken on MSYS.
    // Instead of returning the size of the converted string, it returns 0
    // instead.  Using strlen(text) * 2 is probably big enough.
    size_t maxLen = strlen(text) * 2 + 1;
    wchar_t *ret = new wchar_t[maxLen];
    size_t len = mbstowcs(ret, text, maxLen);
    assert(len != (size_t)-1 && len < maxLen);
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("Usage: %s program [args]\n",
               argv[0]);
        return 0;
    }

    {
        // Copy the WINPTYDBG environment variable from the Cygwin environment
        // to the Win32 environment so the agent will inherit it.
        const char *dbgvar = getenv("WINPTYDBG");
        if (dbgvar != NULL) {
            SetEnvironmentVariableW(L"WINPTYDBG", L"1");
        }
    }

    winsize sz;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &sz);

    winpty_t *winpty = winpty_open(sz.ws_col, sz.ws_row);
    if (winpty == NULL) {
        fprintf(stderr, "Error creating winpty.\n");
        exit(1);
    }

    {
        // Start the child process under the console.
        std::string cmdLine = argvToCommandLine(argc - 1, &argv[1]);
        wchar_t *cmdLineW = heapMbsToWcs(cmdLine.c_str());
        int ret = winpty_start_process(winpty,
                                         NULL,
                                         cmdLineW,
                                         NULL,
                                         NULL);
        if (ret != 0) {
            fprintf(stderr,
                    "Error %#x starting %s\n",
                    (unsigned int)ret,
                    cmdLine.c_str());
            exit(1);
        }
        delete [] cmdLineW;
    }

    {
        struct sigaction resizeSigAct;
        memset(&resizeSigAct, 0, sizeof(resizeSigAct));
        resizeSigAct.sa_handler = terminalResized;
        resizeSigAct.sa_flags = SA_RESTART;
        sigaction(SIGWINCH, &resizeSigAct, NULL);
    }

    termios mode = setRawTerminalMode();
    int signalReadFd;

    {
        int pipeFd[2];
        if (pipe(pipeFd) != 0) {
            perror("Could not create pipe");
            exit(1);
        }
        setFdNonBlock(pipeFd[0]);
        setFdNonBlock(pipeFd[1]);
        signalReadFd = pipeFd[0];
        signalWriteFd = pipeFd[1];
    }

    OutputHandler outputHandler(winpty_get_data_pipe(winpty));
    InputHandler inputHandler(winpty_get_data_pipe(winpty));

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(signalReadFd, &readfds);
        if (select(signalReadFd + 1, &readfds, NULL, NULL, NULL) < 0 &&
                errno != EINTR) {
            perror("select failed");
            exit(1);
        }

        // Discard any data in the signal pipe.
        {
            char tmpBuf[256];
            int amount = read(signalReadFd, tmpBuf, sizeof(tmpBuf));
            if (amount == 0 || (amount < 0 && errno != EAGAIN)) {
                perror("error reading internal signal fd");
                exit(1);
            }
        }

        // Check for terminal resize.
        {
            winsize sz2;
            ioctl(STDIN_FILENO, TIOCGWINSZ, &sz2);
            if (memcmp(&sz, &sz2, sizeof(sz)) != 0) {
                sz = sz2;
                winpty_set_size(winpty, sz.ws_col, sz.ws_row);
            }
        }

        // Check for an I/O handler shutting down (possibly indicating that the
        // child process has exited).
        if (ioHandlerDied)
            break;
    }

    int exitCode = winpty_get_exit_code(winpty);

    restoreTerminalMode(mode);
    // TODO: Call winpty_close?  Shut down one or both I/O threads?
    return exitCode;
}
