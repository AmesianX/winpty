//
// Test half-width vs full-width characters.
//

#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "TestUtil.cc"
#include "../shared/DebugClient.cc"

static void writeChars(const wchar_t *text) {
    wcslen(text);
    const int len = wcslen(text);
    DWORD actual = 0;
    BOOL ret = WriteConsoleW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        text, len, &actual, NULL);
    assert(ret && actual == len);
}

static void dumpChars(int x, int y, int w, int h) {
    BOOL ret;
    const COORD bufSize = {w, h};
    const COORD bufCoord = {0, 0};
    const SMALL_RECT topLeft = {x, y, x + w - 1, y + h - 1};
    CHAR_INFO mbcsData[w * h];
    CHAR_INFO unicodeData[w * h];
    SMALL_RECT readRegion;
    readRegion = topLeft;
    ret = ReadConsoleOutputW(GetStdHandle(STD_OUTPUT_HANDLE), unicodeData,
                             bufSize, bufCoord, &readRegion);
    assert(ret);
    readRegion = topLeft;
    ret = ReadConsoleOutputA(GetStdHandle(STD_OUTPUT_HANDLE), mbcsData,
                             bufSize, bufCoord, &readRegion);
    assert(ret);

    printf("\n");
    for (int i = 0; i < w * h; ++i) {
        printf("CHAR: 0x%04x 0x%04x -- 0x%02x 0x%04x\n",
            (unsigned short)unicodeData[i].Char.UnicodeChar,
            (unsigned short)unicodeData[i].Attributes,
            (unsigned char)mbcsData[i].Char.AsciiChar,
            (unsigned short)mbcsData[i].Attributes);
    }
}

int main(int argc, char *argv[]) {
    system("cls");
    setWindowPos(0, 0, 1, 1);
    setBufferSize(80, 20);
    setWindowPos(0, 0, 80, 20);

    // Write text.
    const wchar_t text1[] = {
        0x3044, // U+3044 (HIRAGANA LETTER I)
        0xFF2D, // U+FF2D (FULLWIDTH LATIN CAPITAL LETTER M)
        0x0033, // U+0030 (DIGIT THREE)
        0x005C, // U+005C (REVERSE SOLIDUS)
        0
    };
    setCursorPos(0, 0);
    writeChars(text1);

    setCursorPos(78, 1);
    writeChars(L"<>");

    const wchar_t text2[] = {
        0x0032, // U+3032 (DIGIT TWO)
        0x3044, // U+3044 (HIRAGANA LETTER I)
        0,
    };
    setCursorPos(78, 1);
    writeChars(text2);

    dumpChars(0, 0, 6, 1);
    dumpChars(78, 1, 2, 1);
    dumpChars(0, 2, 2, 1);

    return 0;
}
