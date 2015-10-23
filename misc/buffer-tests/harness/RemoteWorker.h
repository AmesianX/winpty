#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "Command.h"
#include "Event.h"
#include "RemoteHandle.h"
#include "ShmemParcel.h"
#include "Spawn.h"
#include "UnicodeConversions.h"

class RemoteWorker {
    friend class RemoteHandle;

private:
    RemoteWorker(const std::string &name);
public:
    RemoteWorker() : RemoteWorker(SpawnParams { false, CREATE_NEW_CONSOLE }) {}
    RemoteWorker(SpawnParams params);
    RemoteWorker child(SpawnParams params={});
    RemoteWorker tryChild(SpawnParams params={}, DWORD *errCode=nullptr);
    ~RemoteWorker() { cleanup(); }
    bool valid() { return m_valid; }
    void exit();
private:
    void cleanup() { if (m_valid) { exit(); } }
public:

    // basic worker info
    HANDLE processHandle() { return m_process; }
    DWORD pid() { return GetProcessId(m_process); }

    // allow moving
    RemoteWorker(RemoteWorker &&other) :
        m_valid(std::move(other.m_valid)),
        m_name(std::move(other.m_name)),
        m_parcel(std::move(other.m_parcel)),
        m_startEvent(std::move(other.m_startEvent)),
        m_finishEvent(std::move(other.m_finishEvent)),
        m_process(std::move(other.m_process))
    {
        other.m_valid = false;
        other.m_process = nullptr;
    }
    RemoteWorker &operator=(RemoteWorker &&other) {
        cleanup();
        m_valid = std::move(other.m_valid);
        m_name = std::move(other.m_name);
        m_parcel = std::move(other.m_parcel);
        m_startEvent = std::move(other.m_startEvent);
        m_finishEvent = std::move(other.m_finishEvent);
        m_process = std::move(other.m_process);
        other.m_valid = false;
        other.m_process = nullptr;
        return *this;
    }

    // Commands
    RemoteHandle getStdin()             { rpc(Command::GetStdin); return RemoteHandle(cmd().handle, *this); }
    RemoteHandle getStdout()            { rpc(Command::GetStdout); return RemoteHandle(cmd().handle, *this); }
    RemoteHandle getStderr()            { rpc(Command::GetStderr); return RemoteHandle(cmd().handle, *this); }
    bool detach()                       { rpc(Command::FreeConsole); return cmd().success; }
    bool attach(RemoteWorker &worker)   { cmd().dword = GetProcessId(worker.m_process); rpc(Command::AttachConsole); return cmd().success; }
    bool alloc()                        { rpc(Command::AllocConsole); return cmd().success; }
    void dumpStandardHandles()          { rpc(Command::DumpStandardHandles); }
    int system(const std::string &arg)  { cmd().u.systemText = arg; rpc(Command::System); return cmd().dword; }
    HWND consoleWindow()                { rpc(Command::GetConsoleWindow); return cmd().hwnd; }

    CONSOLE_SELECTION_INFO selectionInfo();
    void dumpConsoleHandles(BOOL writeToEach=FALSE);
    std::vector<RemoteHandle> scanForConsoleHandles();
    void setTitle(const std::string &str)       { auto b = setTitleInternal(widenString(str)); ASSERT(b && "setTitle failed"); }
    bool setTitleInternal(const std::wstring &str);
    std::string title();
    DWORD titleInternal(std::array<wchar_t, 1024> &buf, DWORD bufSize);
    std::vector<DWORD> consoleProcessList();

    RemoteHandle openConin(BOOL bInheritHandle=FALSE) {
        cmd().bInheritHandle = bInheritHandle;
        rpc(Command::OpenConin);
        return RemoteHandle(cmd().handle, *this);
    }

    RemoteHandle openConout(BOOL bInheritHandle=FALSE) {
        cmd().bInheritHandle = bInheritHandle;
        rpc(Command::OpenConout);
        return RemoteHandle(cmd().handle, *this);
    }

    RemoteHandle newBuffer(BOOL bInheritHandle=FALSE, wchar_t firstChar=L'\0') {
        cmd().bInheritHandle = bInheritHandle;
        rpc(Command::NewBuffer);
        auto h = RemoteHandle(cmd().handle, *this);
        if (firstChar != L'\0') {
            h.setFirstChar(firstChar);
        }
        return h;
    }

private:
    Command &cmd() { return m_parcel.value(); }
    void rpc(Command::Kind kind);
    void rpcAsync(Command::Kind kind);
    void rpcImpl(Command::Kind kind);

private:
    bool m_valid = false;
    std::string m_name;
    ShmemParcelTyped<Command> m_parcel;
    Event m_startEvent;
    Event m_finishEvent;
    HANDLE m_process = NULL;
};
