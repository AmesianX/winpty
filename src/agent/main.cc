// Copyright (c) 2011-2015 Ryan Prichard
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
#include <wchar.h>

#include "../shared/StringUtil.h"
#include "../shared/WindowsVersion.h"
#include "../shared/WinptyAssert.h"
#include "../shared/WinptyVersion.h"

#include "Agent.h"
#include "AgentCreateDesktop.h"
#include "DebugShowInput.h"

const char USAGE[] =
"Usage: %ls controlPipeName flags mouseMode cols rows\n"
"Usage: %ls controlPipeName --create-desktop\n"
"\n"
"Ordinarily, this program is launched by winpty.dll and is not directly\n"
"useful to winpty users.  However, it also has options intended for\n"
"debugging winpty.\n"
"\n"
"Usage: %ls [options]\n"
"\n"
"Options:\n"
"  --show-input     Dump INPUT_RECORDs from the console input buffer\n"
"  --show-input --with-mouse\n"
"                   Include MOUSE_INPUT_RECORDs in the dump output\n"
"  --version        Print the winpty version\n";

static uint64_t winpty_atoi64(const char *str) {
    return strtoll(str, NULL, 10);
}

int main() {
    dumpWindowsVersion();
    dumpVersionToTrace();

    // Technically, we should free the CommandLineToArgvW return value using
    // a single call to LocalFree, but the call will never actually happen in
    // the normal case.
    int argc = 0;
    wchar_t *cmdline = GetCommandLineW();
    ASSERT(cmdline != nullptr && "GetCommandLineW returned NULL");
    wchar_t **argv = CommandLineToArgvW(cmdline, &argc);
    ASSERT(argv != nullptr && "CommandLineToArgvW returned NULL");

    if (argc == 2 && !wcscmp(argv[1], L"--version")) {
        dumpVersionToStdout();
        return 0;
    }

    if (argc == 2 && !wcscmp(argv[1], L"--show-input")) {
        debugShowInput(false);
        return 0;
    } else if (argc == 3 &&
            !wcscmp(argv[1], L"--show-input") &&
            !wcscmp(argv[2], L"--with-mouse")) {
        debugShowInput(true);
        return 0;
    }

    if (argc == 3 && !wcscmp(argv[2], L"--create-desktop")) {
        handleCreateDesktop(argv[1]);
        return 0;
    }

    if (argc != 6) {
        fprintf(stderr, USAGE, argv[0], argv[0], argv[0]);
        return 1;
    }

    Agent agent(argv[1],
                winpty_atoi64(utf8FromWide(argv[2]).c_str()),
                atoi(utf8FromWide(argv[3]).c_str()),
                atoi(utf8FromWide(argv[4]).c_str()),
                atoi(utf8FromWide(argv[5]).c_str()));
    agent.run();

    // The Agent destructor shouldn't return, but if it does, exit
    // unsuccessfully.
    return 1;
}
