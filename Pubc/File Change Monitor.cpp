/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <time.h>

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Sys Thread.h"
#include "BlackRoot/Pubc/Sys Path.h"
#include "BlackRoot/Pubc/Files.h"
#include "BlackRoot/Pubc/JSON.h"

#include "HephaestusBase/Pubc/File Change Monitor.h"

using namespace Hephaestus::Pipeline;

    //  Setup
    // --------------------

FileChangeMonitor::FileChangeMonitor()
{
    this->NextID        = 0;
    this->CurrentState  = State::Stopped;

    this->FileSource.reset(new BlackRoot::IO::BaseFileSource(""));
}

FileChangeMonitor::~FileChangeMonitor()
{
    using cout = BlackRoot::Util::Cout;

    if (this->CurrentState != State::Stopped) {
        cout{} << "!!Change monitor was not stopped before being destructed!";
    }
}

    //  Update cycle
    // --------------------

void FileChangeMonitor::InternalUpdateCycle()
{
    while (this->TargetState == State::Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        std::unique_lock<std::mutex> lock(this->MutexAccessFiles);
        this->InternalUpdateDirtyFiles();
    }
}

void FileChangeMonitor::InternalUpdateDirtyFiles()
{
    int maxItt = 999;

    while (--maxItt > 0 && this->DirtyHubFiles.size() > 0) {
        if (this->InternalShouldInterrupt())
            break;

        auto id = *this->DirtyHubFiles.begin();
        this->DirtyHubFiles.erase(this->DirtyHubFiles.begin());

        this->InternalUpdateHubFile(id);
    }

    for (auto it : this->HubFiles) {
        if (std::find(this->DirtyHubFiles.begin(), this->DirtyHubFiles.end(), it.first) == this->DirtyHubFiles.end()) {
            this->DirtyHubFiles.push_back(it.first);
        }
    }
}

    //  Hub files
    // --------------------

void FileChangeMonitor::InternalUpdateHubFile(InternalID id)
{
    namespace IO = BlackRoot::IO;
    using cout = BlackRoot::Util::Cout;

    Monitor::TimePoint currentTime = std::chrono::system_clock::now();

    auto it = this->HubFiles.find(id);
    if (it == this->HubFiles.end())
        return;

    auto & hub = it->second;

    if (hub.Timeout > currentTime) {
        this->DirtyHubFiles.push_back(id);
        return;
    }

    if (!this->FileSource->FileExists(hub.BasePath)) {
        cout{} << "File does not exist: " << hub.BasePath << std::endl;
        hub.Timeout = currentTime + std::chrono::seconds(4);
        this->DirtyHubFiles.push_back(id);
        return;
    }

    auto fileWriteTime = this->FileSource->LastWriteTime(hub.BasePath);
    
    if (hub.LastChange >= fileWriteTime) {
        return;
    }

    std::time_t rawtime = decltype(fileWriteTime)::clock::to_time_t(fileWriteTime);
    auto timePassed = std::time(nullptr) - rawtime;
    
    BlackRoot::IO::BaseFileSource::FCont contents;
    BlackRoot::Format::JSON jsonCont;

    try {
        contents = this->FileSource->ReadFile(hub.BasePath, IO::FileMode::OpenInstr{}.Default().Share(IO::FileMode::Share::Read));
        jsonCont = BlackRoot::Format::JSON::parse(contents);
    }
    catch (BlackRoot::Debug::Exception * e) {
        cout{} << "File reading error: " << hub.BasePath << std::endl;
        hub.Timeout = currentTime + std::chrono::seconds(4);
        this->DirtyHubFiles.push_back(id);
        delete e;
        return;
    }
    catch (...) {
        cout{} << "File reading error: " << hub.BasePath << std::endl;
        hub.Timeout = currentTime + std::chrono::seconds(4);
        this->DirtyHubFiles.push_back(id);
        return;
    }

    auto formatted = this->SimpleFormatDuration(timePassed);
    cout{} << "* " << hub.BasePath << " (" << formatted << ")" << std::endl;

    hub.LastChange = fileWriteTime;
}

    //  Debug
    // --------------------

void FileChangeMonitor::InternalHandleThreadException(BlackRoot::Debug::Exception * e)
{
    // TODO
    throw e;
}

    //  Util
    // --------------------

void FileChangeMonitor::InternalRemoveAllHubFiles()
{
    this->HubFiles.clear();
    this->DirtyHubFiles.resize(0);
}

FileChangeMonitor::InternalID FileChangeMonitor::InternalAddHubFile(const BlackRoot::IO::FilePath path)
{
    std::unique_lock<std::mutex> lock(this->MutexAccessFiles);

    InternalID id = ++this->NextID;

    MonHubFile hub;
    hub.SetDefault();
    hub.BasePath = path;

    this->HubFiles[id] = hub;
    this->DirtyHubFiles.push_back(id);

    return id;
}

std::string FileChangeMonitor::SimpleFormatDuration(long long t)
{
    std::stringstream ss;

    if (t < 100) {
        ss << t << "s";
        return ss.str();
    }
    t /= 6;
    if (t < 600) {
        if (t < 100) {
            ss << (t / 10) << "." << (t % 10) << "m";
        }
        else {
            ss << (t / 10) << "m";
        }
        return ss.str();
    }
    t /= 60;
    if (t < 240) {
        if (t < 100) {
            ss << (t / 10) << "." << (t % 10) << "h";
        }
        else {
            ss << (t / 10) << "h";
        }
        return ss.str();
    }
    t /= 24;
    if (t < 100) {
        ss << (t / 10) << "." << (t % 10) << "d";
    }
    else {
        ss << (t / 10) << "d";
    }
    return ss.str();
}

bool FileChangeMonitor::InternalShouldInterrupt()
{
    return this->TargetState != State::Running;
}

    //  Control
    // --------------------

void FileChangeMonitor::Begin()
{
    using cout = BlackRoot::Util::Cout;

    auto * This = this;

    DbAssert(this->CurrentState == State::Stopped);
    
    this->TargetState  = State::Running;
    this->CurrentState = State::Starting;

    this->UpdateThread = std::thread([&] {
        BlackRoot::System::SetCurrentThreadPriority(BlackRoot::System::ThreadPriority::Lowest);

        cout{} << "Starting ChangeMontor thread." << std::endl;

        this->CurrentState = State::Running;
        try {
            this->InternalUpdateCycle();
        }
        catch (BlackRoot::Debug::Exception * e) {
            this->InternalHandleThreadException(e);
        }

        cout{} << "ChangeMontor thread ended." << std::endl;

        this->CurrentState = State::Stopped;
    });
}

void FileChangeMonitor::EndAndWait()
{
    this->TargetState = State::Stopped;
    this->UpdateThread.join();
}

void FileChangeMonitor::UpdateBaseHubFile(const BlackRoot::IO::FilePath path)
{
    State::Type state = this->TargetState;

    if (state == State::Running) {
        this->EndAndWait();
    }

    this->InternalRemoveAllHubFiles();
    this->InternalAddHubFile(path);

    if (state == State::Running) {
        this->Begin();
    }
}

    //  Items
    // --------------------

void Monitor::IMonitoredItem::SetDefault()
{
    this->ExistentialDependanciesHub.resize(0);
    this->LastChange    = std::chrono::time_point<std::chrono::system_clock>{};
    this->Timeout       = std::chrono::system_clock::now();
}