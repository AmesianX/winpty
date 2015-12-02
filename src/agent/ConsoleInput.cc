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

#include "ConsoleInput.h"

#include <stdio.h>
#include <string.h>

#include <string>

#include "DefaultInputMap.h"
#include "DsrSender.h"
#include "Win32Console.h"
#include "../shared/DebugClient.h"
#include "../shared/UnixCtrlChars.h"

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

const int kIncompleteEscapeTimeoutMs = 1000;

ConsoleInput::ConsoleInput(DsrSender *dsrSender) :
    m_console(new Win32Console),
    m_dsrSender(dsrSender),
    m_dsrSent(false),
    lastWriteTick(0)
{
    addDefaultEntriesToInputMap(m_inputMap);
}

ConsoleInput::~ConsoleInput()
{
    delete m_console;
}

void ConsoleInput::writeInput(const std::string &input)
{
    if (input.size() == 0) {
        return;
    }

    if (isTracingEnabled()) {
        static bool debugInput = hasDebugFlag("input");
        if (debugInput) {
            std::string dumpString;
            for (size_t i = 0; i < input.size(); ++i) {
                const char ch = input[i];
                const char ctrl = decodeUnixCtrlChar(ch);
                if (ctrl != '\0') {
                    dumpString += '^';
                    dumpString += ctrl;
                } else {
                    dumpString += ch;
                }
            }
            dumpString += " (";
            for (size_t i = 0; i < input.size(); ++i) {
                if (i > 0) {
                    dumpString += ' ';
                }
                const unsigned char uch = input[i];
                char buf[32];
                sprintf(buf, "%02X", uch);
                dumpString += buf;
            }
            dumpString += ')';
            trace("input chars: %s", dumpString.c_str());
        }
    }

    m_byteQueue.append(input);
    doWrite(false);
    if (!m_byteQueue.empty() && !m_dsrSent) {
        trace("send DSR");
        m_dsrSender->sendDsr();
        m_dsrSent = true;
    }
    lastWriteTick = GetTickCount();
}

void ConsoleInput::flushIncompleteEscapeCode()
{
    if (!m_byteQueue.empty() &&
            (int)(GetTickCount() - lastWriteTick) > kIncompleteEscapeTimeoutMs) {
        doWrite(true);
        m_byteQueue.clear();
    }
}

void ConsoleInput::doWrite(bool isEof)
{
    const char *data = m_byteQueue.c_str();
    std::vector<INPUT_RECORD> records;
    size_t idx = 0;
    while (idx < m_byteQueue.size()) {
        int charSize = scanKeyPress(records, &data[idx], m_byteQueue.size() - idx, isEof);
        if (charSize == -1)
            break;
        idx += charSize;
    }
    m_byteQueue.erase(0, idx);
    m_console->writeInput(records.data(), records.size());
}

int ConsoleInput::scanKeyPress(std::vector<INPUT_RECORD> &records,
                               const char *input,
                               int inputSize,
                               bool isEof)
{
    //trace("scanKeyPress: %d bytes", inputSize);

    // Ctrl-C.
    if (input[0] == '\x03' && m_console->processedInputMode()) {
        trace("Ctrl-C");
        BOOL ret = GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        trace("GenerateConsoleCtrlEvent: %d", ret);
        return 1;
    }

    // Attempt to match the Device Status Report (DSR) reply.
    int dsrLen = matchDsr(input, inputSize);
    if (dsrLen > 0) {
        trace("Received a DSR reply");
        m_dsrSent = false;
        return dsrLen;
    } else if (!isEof && dsrLen == -1) {
        // Incomplete DSR match.
        trace("Incomplete DSR match");
        return -1;
    }

    // Search the input map.
    bool incomplete;
    int matchLen;
    const InputMap::Key *match =
        lookupKey(input, inputSize, incomplete, matchLen);
    if (!isEof && incomplete) {
        // Incomplete match -- need more characters (or wait for a
        // timeout to signify flushed input).
        trace("Incomplete escape sequence");
        return -1;
    } else if (match != NULL) {
        appendKeyPress(records,
                       match->virtualKey,
                       match->unicodeChar,
                       match->keyState);
        return matchLen;
    }

    // Recognize Alt-<character>.
    //
    // This code doesn't match Alt-ESC, which is encoded as `ESC ESC`, but
    // maybe it should.  I was concerned that pressing ESC rapidly enough could
    // accidentally trigger Alt-ESC.  (e.g. The user would have to be faster
    // than the DSR flushing mechanism or use a decrepit terminal.  The user
    // might be on a slow network connection.)
    if (input[0] == '\x1B' && inputSize >= 2 && input[1] != '\x1B') {
        int len = utf8CharLength(input[1]);
        if (1 + len > inputSize) {
            // Incomplete character.
            trace("Incomplete UTF-8 character in Alt-<Char>");
            return -1;
        }
        appendUtf8Char(records, &input[1], len, LEFT_ALT_PRESSED);
        return 1 + len;
    }

    // A UTF-8 character.
    int len = utf8CharLength(input[0]);
    if (len > inputSize) {
        // Incomplete character.
        trace("Incomplete UTF-8 character");
        return -1;
    }
    appendUtf8Char(records, &input[0], len, 0);
    return len;
}

void ConsoleInput::appendUtf8Char(std::vector<INPUT_RECORD> &records,
                                  const char *charBuffer,
                                  const int charLen,
                                  const int keyState)
{
    WCHAR wideInput[2];
    int wideLen = MultiByteToWideChar(CP_UTF8,
                                      0,
                                      charBuffer,
                                      charLen,
                                      wideInput,
                                      sizeof(wideInput) / sizeof(wideInput[0]));
    for (int i = 0; i < wideLen; ++i) {
        short charScan = VkKeyScan(wideInput[i]);
        int virtualKey = 0;
        int charKeyState = keyState;
        if (charScan != -1) {
            virtualKey = charScan & 0xFF;
            if (charScan & 0x100)
                charKeyState |= SHIFT_PRESSED;
            else if (charScan & 0x200)
                charKeyState |= LEFT_CTRL_PRESSED;
            else if (charScan & 0x400)
                charKeyState |= LEFT_ALT_PRESSED;
        }
        appendKeyPress(records, virtualKey, wideInput[i], charKeyState);
    }
}

void ConsoleInput::appendKeyPress(std::vector<INPUT_RECORD> &records,
                                  int virtualKey,
                                  int unicodeChar,
                                  int keyState)
{
    const bool ctrl = keyState & LEFT_CTRL_PRESSED;
    const bool alt = keyState & LEFT_ALT_PRESSED;
    const bool shift = keyState & SHIFT_PRESSED;

    if (isTracingEnabled()) {
        static bool debugInput = hasDebugFlag("input");
        if (debugInput) {
            InputMap::Key key = { virtualKey, unicodeChar, keyState };
            trace("keypress: %s", key.toString().c_str());
        }
    }

    int stepKeyState = 0;
    if (ctrl) {
        stepKeyState |= LEFT_CTRL_PRESSED;
        appendInputRecord(records, TRUE, VK_CONTROL, 0, stepKeyState);
    }
    if (alt) {
        stepKeyState |= LEFT_ALT_PRESSED;
        appendInputRecord(records, TRUE, VK_MENU, 0, stepKeyState);
    }
    if (shift) {
        stepKeyState |= SHIFT_PRESSED;
        appendInputRecord(records, TRUE, VK_SHIFT, 0, stepKeyState);
    }
    if (ctrl && alt) {
        // This behavior seems arbitrary, but it's what I see in the Windows 7
        // console.
        unicodeChar = 0;
    }
    appendInputRecord(records, TRUE, virtualKey, unicodeChar, stepKeyState);
    if (alt) {
        // This behavior seems arbitrary, but it's what I see in the Windows 7
        // console.
        unicodeChar = 0;
    }
    appendInputRecord(records, FALSE, virtualKey, unicodeChar, stepKeyState);
    if (shift) {
        stepKeyState &= ~SHIFT_PRESSED;
        appendInputRecord(records, FALSE, VK_SHIFT, 0, stepKeyState);
    }
    if (alt) {
        stepKeyState &= ~LEFT_ALT_PRESSED;
        appendInputRecord(records, FALSE, VK_MENU, 0, stepKeyState);
    }
    if (ctrl) {
        stepKeyState &= ~LEFT_CTRL_PRESSED;
        appendInputRecord(records, FALSE, VK_CONTROL, 0, stepKeyState);
    }
}

void ConsoleInput::appendInputRecord(std::vector<INPUT_RECORD> &records,
                                     BOOL keyDown,
                                     int virtualKey,
                                     int unicodeChar,
                                     int keyState)
{
    INPUT_RECORD ir;
    memset(&ir, 0, sizeof(ir));
    ir.EventType = KEY_EVENT;
    ir.Event.KeyEvent.bKeyDown = keyDown;
    ir.Event.KeyEvent.wRepeatCount = 1;
    ir.Event.KeyEvent.wVirtualKeyCode = virtualKey;
    ir.Event.KeyEvent.wVirtualScanCode =
            MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);
    ir.Event.KeyEvent.uChar.UnicodeChar = unicodeChar;
    ir.Event.KeyEvent.dwControlKeyState = keyState;
    records.push_back(ir);
}

// Return the byte size of a UTF-8 character using the value of the first
// byte.
int ConsoleInput::utf8CharLength(char firstByte)
{
    // This code would probably be faster if it used __builtin_clz.
    if ((firstByte & 0x80) == 0) {
        return 1;
    } else if ((firstByte & 0xE0) == 0xC0) {
        return 2;
    } else if ((firstByte & 0xF0) == 0xE0) {
        return 3;
    } else if ((firstByte & 0xF8) == 0xF0) {
        return 4;
    } else if ((firstByte & 0xFC) == 0xF8) {
        return 5;
    } else if ((firstByte & 0xFE) == 0xFC) {
        return 6;
    } else {
        // Malformed UTF-8.
        return 1;
    }
}

// Find the longest matching key and node.
const InputMap::Key *
ConsoleInput::lookupKey(const char *input, int inputSize,
                        bool &incompleteOut, int &matchLenOut)
{
    incompleteOut = false;
    matchLenOut = 0;

    InputMap *node = &m_inputMap;
    const InputMap::Key *longestMatch = NULL;
    int longestMatchLen = 0;

    for (int i = 0; i < inputSize; ++i) {
        unsigned char ch = input[i];
        node = node->getChild(ch);
        //trace("ch: %d --> node:%p", ch, node);
        if (node == NULL) {
            matchLenOut = longestMatchLen;
            return longestMatch;
        } else if (node->getKey() != NULL) {
            longestMatchLen = i + 1;
            longestMatch = node->getKey();
        }
    }
    incompleteOut = node->hasChildren();
    matchLenOut = longestMatchLen;
    return longestMatch;
}

// Match the Device Status Report console input:  ESC [ nn ; mm R
// Returns:
// 0   no match
// >0  match, returns length of match
// -1  incomplete match
int ConsoleInput::matchDsr(const char *input, int inputSize)
{
    const char *pch = input;
    const char *stop = input + inputSize;

    if (pch == stop) { return -1; }

#define CHECK(cond) \
        do { \
            if (!(cond)) { return 0; } \
        } while(0)

#define ADVANCE() \
        do { \
            pch++; \
            if (pch == stop) { return -1; } \
        } while(0)

    CHECK(*pch == '\x1B');  ADVANCE();
    CHECK(*pch == '[');     ADVANCE();
    CHECK(isdigit(*pch));   ADVANCE();
    while (isdigit(*pch)) {
        ADVANCE();
    }
    CHECK(*pch == ';');     ADVANCE();
    CHECK(isdigit(*pch));   ADVANCE();
    while (isdigit(*pch)) {
        ADVANCE();
    }
    CHECK(*pch == 'R');
    return pch - input + 1;
#undef CHECK
#undef ADVANCE
}
