#pragma once

#include <windows.h>

#include <iostream>
#include <string>
#include <tuple>

#include "RemoteHandle.h"

class RemoteWorker;

#define CHECK(cond) \
    do {                                                                      \
        if (!(cond)) {                                                        \
            trace("%s:%d: ERROR: check failed: " #cond, __FILE__, __LINE__);  \
            std::cout << __FILE__ << ":" << __LINE__                          \
                      << (": ERROR: check failed: " #cond)                    \
                      << std::endl;                                           \
        }                                                                     \
    } while(0)

#define CHECK_EQ(actual, expected) \
    do {                                                                      \
        auto a = (actual);                                                    \
        auto e = (expected);                                                  \
        if (a != e) {                                                         \
            trace("%s:%d: ERROR: check failed "                               \
                  "(" #actual " != " #expected ")", __FILE__, __LINE__);      \
            std::cout << __FILE__ << ":" << __LINE__                          \
                      << ": ERROR: check failed "                             \
                      << ("(" #actual " != " #expected "): ")                 \
                      << a << " != " << e                                     \
                      << std::endl;                                           \
        }                                                                     \
    } while(0)

std::tuple<RemoteHandle, RemoteHandle> newPipe(
        RemoteWorker &w, BOOL inheritable=FALSE);
void printTestName(const char *testName);
std::string windowText(HWND hwnd);
void *ntHandlePointer(RemoteHandle h);
bool compareObjectHandles(RemoteHandle h1, RemoteHandle h2);
