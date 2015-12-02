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

#include "InputMap.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

namespace {

static const char *getVirtualKeyString(int virtualKey)
{
    switch (virtualKey) {
#define WINPTY_GVKS_KEY(x) case VK_##x: return #x;
        WINPTY_GVKS_KEY(RBUTTON)    WINPTY_GVKS_KEY(F9)
        WINPTY_GVKS_KEY(CANCEL)     WINPTY_GVKS_KEY(F10)
        WINPTY_GVKS_KEY(MBUTTON)    WINPTY_GVKS_KEY(F11)
        WINPTY_GVKS_KEY(XBUTTON1)   WINPTY_GVKS_KEY(F12)
        WINPTY_GVKS_KEY(XBUTTON2)   WINPTY_GVKS_KEY(F13)
        WINPTY_GVKS_KEY(BACK)       WINPTY_GVKS_KEY(F14)
        WINPTY_GVKS_KEY(TAB)        WINPTY_GVKS_KEY(F15)
        WINPTY_GVKS_KEY(CLEAR)      WINPTY_GVKS_KEY(F16)
        WINPTY_GVKS_KEY(RETURN)     WINPTY_GVKS_KEY(F17)
        WINPTY_GVKS_KEY(SHIFT)      WINPTY_GVKS_KEY(F18)
        WINPTY_GVKS_KEY(CONTROL)    WINPTY_GVKS_KEY(F19)
        WINPTY_GVKS_KEY(MENU)       WINPTY_GVKS_KEY(F20)
        WINPTY_GVKS_KEY(PAUSE)      WINPTY_GVKS_KEY(F21)
        WINPTY_GVKS_KEY(CAPITAL)    WINPTY_GVKS_KEY(F22)
        WINPTY_GVKS_KEY(HANGUL)     WINPTY_GVKS_KEY(F23)
        WINPTY_GVKS_KEY(JUNJA)      WINPTY_GVKS_KEY(F24)
        WINPTY_GVKS_KEY(FINAL)      WINPTY_GVKS_KEY(NUMLOCK)
        WINPTY_GVKS_KEY(KANJI)      WINPTY_GVKS_KEY(SCROLL)
        WINPTY_GVKS_KEY(ESCAPE)     WINPTY_GVKS_KEY(LSHIFT)
        WINPTY_GVKS_KEY(CONVERT)    WINPTY_GVKS_KEY(RSHIFT)
        WINPTY_GVKS_KEY(NONCONVERT) WINPTY_GVKS_KEY(LCONTROL)
        WINPTY_GVKS_KEY(ACCEPT)     WINPTY_GVKS_KEY(RCONTROL)
        WINPTY_GVKS_KEY(MODECHANGE) WINPTY_GVKS_KEY(LMENU)
        WINPTY_GVKS_KEY(SPACE)      WINPTY_GVKS_KEY(RMENU)
        WINPTY_GVKS_KEY(PRIOR)      WINPTY_GVKS_KEY(BROWSER_BACK)
        WINPTY_GVKS_KEY(NEXT)       WINPTY_GVKS_KEY(BROWSER_FORWARD)
        WINPTY_GVKS_KEY(END)        WINPTY_GVKS_KEY(BROWSER_REFRESH)
        WINPTY_GVKS_KEY(HOME)       WINPTY_GVKS_KEY(BROWSER_STOP)
        WINPTY_GVKS_KEY(LEFT)       WINPTY_GVKS_KEY(BROWSER_SEARCH)
        WINPTY_GVKS_KEY(UP)         WINPTY_GVKS_KEY(BROWSER_FAVORITES)
        WINPTY_GVKS_KEY(RIGHT)      WINPTY_GVKS_KEY(BROWSER_HOME)
        WINPTY_GVKS_KEY(DOWN)       WINPTY_GVKS_KEY(VOLUME_MUTE)
        WINPTY_GVKS_KEY(SELECT)     WINPTY_GVKS_KEY(VOLUME_DOWN)
        WINPTY_GVKS_KEY(PRINT)      WINPTY_GVKS_KEY(VOLUME_UP)
        WINPTY_GVKS_KEY(EXECUTE)    WINPTY_GVKS_KEY(MEDIA_NEXT_TRACK)
        WINPTY_GVKS_KEY(SNAPSHOT)   WINPTY_GVKS_KEY(MEDIA_PREV_TRACK)
        WINPTY_GVKS_KEY(INSERT)     WINPTY_GVKS_KEY(MEDIA_STOP)
        WINPTY_GVKS_KEY(DELETE)     WINPTY_GVKS_KEY(MEDIA_PLAY_PAUSE)
        WINPTY_GVKS_KEY(HELP)       WINPTY_GVKS_KEY(LAUNCH_MAIL)
        WINPTY_GVKS_KEY(LWIN)       WINPTY_GVKS_KEY(LAUNCH_MEDIA_SELECT)
        WINPTY_GVKS_KEY(RWIN)       WINPTY_GVKS_KEY(LAUNCH_APP1)
        WINPTY_GVKS_KEY(APPS)       WINPTY_GVKS_KEY(LAUNCH_APP2)
        WINPTY_GVKS_KEY(SLEEP)      WINPTY_GVKS_KEY(OEM_1)
        WINPTY_GVKS_KEY(NUMPAD0)    WINPTY_GVKS_KEY(OEM_PLUS)
        WINPTY_GVKS_KEY(NUMPAD1)    WINPTY_GVKS_KEY(OEM_COMMA)
        WINPTY_GVKS_KEY(NUMPAD2)    WINPTY_GVKS_KEY(OEM_MINUS)
        WINPTY_GVKS_KEY(NUMPAD3)    WINPTY_GVKS_KEY(OEM_PERIOD)
        WINPTY_GVKS_KEY(NUMPAD4)    WINPTY_GVKS_KEY(OEM_2)
        WINPTY_GVKS_KEY(NUMPAD5)    WINPTY_GVKS_KEY(OEM_3)
        WINPTY_GVKS_KEY(NUMPAD6)    WINPTY_GVKS_KEY(OEM_4)
        WINPTY_GVKS_KEY(NUMPAD7)    WINPTY_GVKS_KEY(OEM_5)
        WINPTY_GVKS_KEY(NUMPAD8)    WINPTY_GVKS_KEY(OEM_6)
        WINPTY_GVKS_KEY(NUMPAD9)    WINPTY_GVKS_KEY(OEM_7)
        WINPTY_GVKS_KEY(MULTIPLY)   WINPTY_GVKS_KEY(OEM_8)
        WINPTY_GVKS_KEY(ADD)        WINPTY_GVKS_KEY(OEM_102)
        WINPTY_GVKS_KEY(SEPARATOR)  WINPTY_GVKS_KEY(PROCESSKEY)
        WINPTY_GVKS_KEY(SUBTRACT)   WINPTY_GVKS_KEY(PACKET)
        WINPTY_GVKS_KEY(DECIMAL)    WINPTY_GVKS_KEY(ATTN)
        WINPTY_GVKS_KEY(DIVIDE)     WINPTY_GVKS_KEY(CRSEL)
        WINPTY_GVKS_KEY(F1)         WINPTY_GVKS_KEY(EXSEL)
        WINPTY_GVKS_KEY(F2)         WINPTY_GVKS_KEY(EREOF)
        WINPTY_GVKS_KEY(F3)         WINPTY_GVKS_KEY(PLAY)
        WINPTY_GVKS_KEY(F4)         WINPTY_GVKS_KEY(ZOOM)
        WINPTY_GVKS_KEY(F5)         WINPTY_GVKS_KEY(NONAME)
        WINPTY_GVKS_KEY(F6)         WINPTY_GVKS_KEY(PA1)
        WINPTY_GVKS_KEY(F7)         WINPTY_GVKS_KEY(OEM_CLEAR)
        WINPTY_GVKS_KEY(F8)
#undef WINPTY_GVKS_KEY
        default:                        return NULL;
    }
}

} // anonymous namespace

std::string InputMap::Key::toString() {
    std::string ret;
    if (keyState & SHIFT_PRESSED) {
        ret += "Shift-";
    }
    if (keyState & LEFT_CTRL_PRESSED) {
        ret += "Ctrl-";
    }
    if (keyState & LEFT_ALT_PRESSED) {
        ret += "Alt-";
    }
    char buf[256];
    const char *vkString = getVirtualKeyString(virtualKey);
    if (vkString != NULL) {
        ret += vkString;
    } else if ((virtualKey >= 'A' && virtualKey <= 'Z') ||
               (virtualKey >= '0' && virtualKey <= '9')) {
        ret += static_cast<char>(virtualKey);
    } else {
        sprintf(buf, "0x%x", virtualKey);
        ret += buf;
    }
    if (unicodeChar >= 32 && unicodeChar <= 126) {
        sprintf(buf, " ch='%c'", unicodeChar);
    } else {
        sprintf(buf, " ch=%#x", unicodeChar);
    }
    ret += buf;
    return ret;
}

InputMap::InputMap() : m_key(NULL), m_children(NULL) {
}

InputMap::~InputMap() {
    delete m_key;
    if (m_children != NULL) {
        for (int i = 0; i < 256; ++i) {
            delete (*m_children)[i];
        }
    }
    delete [] m_children;
}

void InputMap::set(const char *encoding, const Key &key) {
    unsigned char ch = encoding[0];
    if (ch == '\0') {
        setKey(key);
    } else {
        getOrCreateChild(ch)->set(encoding + 1, key);
    }
}

void InputMap::setKey(const Key &key) {
    delete m_key;
    m_key = new Key(key);
}

InputMap *InputMap::getOrCreateChild(unsigned char ch) {
    if (m_children == NULL) {
        m_children = reinterpret_cast<InputMap*(*)[256]>(new InputMap*[256]);
        memset(m_children, 0, sizeof(InputMap*) * 256);
    }
    if ((*m_children)[ch] == NULL) {
        (*m_children)[ch] = new InputMap;
    }
    return (*m_children)[ch];
}
