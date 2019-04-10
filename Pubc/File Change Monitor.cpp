/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

 /* {todo}
  * - Currently this uses a single mutex to reserve quite a lot; if this sees more
  *   action, that will becom a bottleneck.
  * - Property use move semantics
  * - Detect circular dependancies
  */

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

void FileChangeMonitor::UpdateCycle()
{
    while (this->TargetState == State::Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        std::unique_lock<std::mutex> lock(this->MutexAccessFiles);
        this->UpdateSuspectPaths();
    }
}

void FileChangeMonitor::UpdateSuspectPaths()
{
    while (this->SuspectPaths.size() > 0) {
        if (this->ShouldInterrupt())
            break;

            // Select a single path from our suspect list and try to update it
            // If anything fails the update function will put it in the list again
        auto id = *this->SuspectPaths.begin();
        this->SuspectPaths.erase(std::remove(this->SuspectPaths.begin(), this->SuspectPaths.end(), id), this->SuspectPaths.end());
        this->UpdateSuspectPath(id);
    }

        // Debug; for now just make all paths suspect to check functionality
    for (auto it : this->MonitoredPaths) {
        this->SuspectPaths.push_back(it.first);
    }
}

void FileChangeMonitor::UpdateDirtyHubs()
{
    //while (this->DirtyHubs.size() > 0) {
    //    if (this->ShouldInterrupt())
    //        break;

    //        // Select a single hub from our suspect list and try to update it
    //        // If anything fails the update function will put it in the list again
    //    auto id = *this->DirtyHubs.begin();
    //    this->DirtyHubs.erase(this->DirtyHubs.begin());
    //    this->UpdateDirtyHub(id);
    //}
}

    //  Update paths
    // --------------------

void FileChangeMonitor::UpdateSuspectPath(InternalID id)
{
    namespace IO = BlackRoot::IO;
    using cout = BlackRoot::Util::Cout;

        // Use the current time as a reference for file changes
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();

        // Find the path properties
    auto itProp = this->MonitoredPaths.find(id);
    if (itProp == this->MonitoredPaths.end())
        return;
    auto & prop = itProp->second;

        // A timeout prevents a file from updating;
        // if we are timed out just put us on the dirty list
    if (prop.Timeout > currentTime) {
        this->SuspectPaths.push_back(id);
        return;
    }

    IO::FileTime fileWriteTime;
    
        // Safety try in case any files are changed while we are doing this
    try {
            // If the file does not exist we keep trying until it does; if the
            // file is no longer referenced the monitored path will be removed
        if (!this->FileSource->FileExists(prop.Path)) {
            this->HandleMonitoredPathMissing(id);
            return;
        }

            // Check if file was actually updated since last we remember
            // We check whether the update _equals_ to ensure shenanigans so
            // that changed file times (renaming, creative version control) to
            // have a minimum impact
        fileWriteTime = this->FileSource->LastWriteTime(prop.Path);
        if (prop.LastUpdate == fileWriteTime) {
            return;
        }
    }
    catch (BlackRoot::Debug::Exception * e) {
        this->HandleMonitoredPathError(id, e);
        return;
    }
    catch (...) {
        this->HandleMonitoredPathError(id, nullptr);
        return;
    }
    
        // If the file was updated after our last update we consider _all_ things
        // based on this path dirty and in need of an update
    this->MakeUsersOfPathDirty(id);

        // Simple output of time
    std::time_t rawtime = decltype(fileWriteTime)::clock::to_time_t(fileWriteTime);
    auto timePassed = std::time(nullptr) - rawtime;

    auto formatted = this->SimpleFormatDuration(timePassed);
    cout{} << "Changed: " << prop.Path << " (" << formatted << ")" << std::endl;

        // Update path meta
    prop.LastUpdate = fileWriteTime;
}

    //  Update hubs
    // --------------------

void FileChangeMonitor::UpdateDirtyHub(InternalID id)
{
    //namespace IO = BlackRoot::IO;
    //using cout = BlackRoot::Util::Cout;

    //    // Find the hub properties
    //auto itProp = this->HubProperties.find(id);
    //if (itProp == this->HubProperties.end())
    //    return;
    //auto & prop = itProp->second;
    //
    //    // Find the path properties
    //auto pathProp = this->SinglePathProperties[prop.SinglePathDepID];
    //
    //cout{} << "Hub: " << pathProp.Path << std::endl;

    //BlackRoot::IO::BaseFileSource::FCont contents;
    //BlackRoot::Format::JSON jsonCont;

    //    // See if we can load the file contents as JSON
    //try {
    //    contents = this->FileSource->ReadFile(pathProp.Path, IO::FileMode::OpenInstr{}.Default().Share(IO::FileMode::Share::Read));
    //    jsonCont = BlackRoot::Format::JSON::parse(contents);
    //}
    //catch (BlackRoot::Debug::Exception * e) {
    //    this->HandleSingleFileError(prop.SinglePathDepID, e);
    //    return;
    //}
    //catch (...) {
    //    this->HandleSingleFileError(prop.SinglePathDepID, nullptr);
    //    return;
    //}

    //    // TODO: Actually use this
    //
    //cout{} << "Done." << std::endl;
}

    //  Debug
    // --------------------

void FileChangeMonitor::HandleThreadException(BlackRoot::Debug::Exception * e)
{
    // TODO
    throw e;
}

    //  Manipulation
    // --------------------

FileChangeMonitor::InternalID FileChangeMonitor::GetNewID()
{
    // {Paranoia Note} This ID could overflow
    DbAssert(this->NextID < Monitor::InternalIDNone);
    return this->NextID++;
}

FileChangeMonitor::InternalID FileChangeMonitor::FindOrAddMonitoredPath(Monitor::Path path, Monitor::TimePoint * outPrevTimePoint)
{
    for (auto it : this->MonitoredPaths) {
        if (it.second.Path != path)
            continue;

        if (outPrevTimePoint) {
            *outPrevTimePoint = it.second.LastUpdate;
        }
        return it.first;
    }

    MonPath monPath;
    monPath.SetDefault();
    monPath.Path = path;
    if (outPrevTimePoint) {
        *outPrevTimePoint = monPath.LastUpdate;
    }

    auto id = this->GetNewID();
    this->MonitoredPaths[id] = monPath;

    this->SuspectPaths.push_back(id);

    return id;
}

FileChangeMonitor::InternalID FileChangeMonitor::FindOrAddHub(HubProp hub, Monitor::TimePoint * outPrevTimePoint)
{
    for (auto it : this->HubProperties) {
        if (!it.second.EqualsAbstractly(hub))
            continue;
        
        if (outPrevTimePoint) {
            *outPrevTimePoint = it.second.LastUpdate;
        }
        return it.first;
    }
    
    if (outPrevTimePoint) {
        *outPrevTimePoint = hub.LastUpdate;
    }
    
    hub.PathDependencies.resize(0);
    hub.PathDependencies.push_back(this->FindOrAddMonitoredPath(hub.Path, nullptr));

    auto id = this->GetNewID();
    this->HubProperties[id] = hub;

    this->DirtyHubs.push_back(id);

    return id;
}

void FileChangeMonitor::MakeUsersOfPathDirty(InternalID id)
{
    for (auto it : this->HubProperties) {
        auto & dep = it.second.PathDependencies;
        if (std::find(dep.begin(), dep.end(), id) != dep.end())
            continue;
        this->DirtyHubs.push_back(it.first);
    }
}

    //  Util
    // --------------------

std::string FileChangeMonitor::SimpleFormatDuration(long long t)
{
    std::stringstream ss;

        // Report in seconds
    if (t < 100) {
        ss << t << "s";
        return ss.str();
    }

        // Report in minutes
        // Dividing by 6 (and not 60) to maintain a significant digit
    t /= 6;
    if (t < 100) {
        ss << (t / 10) << "." << (t % 10) << "m";
        return ss.str();
    }
    if (t < 1000) {
        ss << (t / 10) << "m";
        return ss.str();
    }

        // Report in hours
    t /= 60;
    if (t < 100) {
        ss << (t / 10) << "." << (t % 10) << "h";
        return ss.str();
    }
    if (t < 1000) {
        ss << (t / 10) << "h";
        return ss.str();
    }

        // Report in days
    t /= 24;
    if (t < 100) {
        ss << (t / 10) << "." << (t % 10) << "d";
    }

    ss << (t / 10) << "d";
    return ss.str();
}

void FileChangeMonitor::HandleMonitoredPathMissing(InternalID id)
{
    using cout = BlackRoot::Util::Cout;

    auto it = this->MonitoredPaths.find(id);
    if (it == this->MonitoredPaths.end())
        return;
    auto & prop = it->second;
    
    cout{} << "File missing: " << prop.Path << std::endl;
    
        // The file might 'come back' by another file being renamed; this
        // might change the contents of the file without changing the time
        // To counter this we just reset to the beginning of time
    prop.LastUpdate = std::chrono::time_point<std::chrono::system_clock>{};

        // Set the timeout to a few seconds from now to prevent a file
        // being constantly updated
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();
    prop.Timeout = currentTime + std::chrono::seconds(4);

    this->SuspectPaths.push_back(id);
}

void FileChangeMonitor::HandleMonitoredPathError(InternalID id, BlackRoot::Debug::Exception *e)
{
    using cout = BlackRoot::Util::Cout;

    auto it = this->MonitoredPaths.find(id);
    if (it == this->MonitoredPaths.end())
        return;
    auto & prop = it->second;
    
    cout{} << "File error: " << prop.Path << std::endl;
    
        // Set the timeout to a few seconds from now to prevent a file
        // being constantly updated
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();
    prop.Timeout = currentTime + std::chrono::seconds(4);

    this->SuspectPaths.push_back(id);

    delete e;
}

bool FileChangeMonitor::ShouldInterrupt()
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

        // Launch the low-priority update thread, which calls back into UpdateCycle
    this->UpdateThread = std::thread([&] {
        BlackRoot::System::SetCurrentThreadPriority(BlackRoot::System::ThreadPriority::Lowest);

        cout{} << "Starting FileChangeMontor thread." << std::endl;

        this->CurrentState = State::Running;
        try {
            this->UpdateCycle();
        }
        catch (BlackRoot::Debug::Exception * e) {
            this->HandleThreadException(e);
        }

        cout{} << "Ending FileChangeMontor thread." << std::endl;

        this->CurrentState = State::Stopped;
    });
}

void FileChangeMonitor::EndAndWait()
{
    this->TargetState = State::Stopped;
    this->UpdateThread.join();
}

void FileChangeMonitor::AddBaseHubFile(const BlackRoot::IO::FilePath path)
{
    std::unique_lock<std::mutex> lock(this->MutexAccessFiles);

    HubProp hub;
    hub.SetDefault();
    hub.Path = path;

    this->FindOrAddHub(hub);
}

    //  Items
    // --------------------

void Monitor::MonitoredPath::SetDefault()
{
    this->Path       = "";

    this->LastUpdate = std::chrono::time_point<std::chrono::system_clock>{};
    this->Timeout    = std::chrono::system_clock::now();
}

void Monitor::HubProperties::SetDefault()
{
    this->PathDependencies.resize(0);

    this->Path          = "";
    this->HubDependancy = Monitor::InternalIDNone;

    this->LastUpdate = std::chrono::time_point<std::chrono::system_clock>{};
    this->Timeout    = std::chrono::system_clock::now();

    this->ProcessProp.SetDefault();
}

bool Monitor::HubProperties::EqualsAbstractly(const HubProperties rh)
{
    if (this->Path != rh.Path)
        return false;
    if (rh.HubDependancy != Monitor::InternalIDNone &&
        this->HubDependancy != rh.HubDependancy)
        return false;
    if (!this->ProcessProp.Equals(rh.ProcessProp))
        return false;
    return true;
}

void Monitor::ProcessProperties::SetDefault()
{
    this->StringVariables.clear();
}

bool Monitor::ProcessProperties::Equals(const ProcessProperties rh)
{
    if (this->StringVariables != rh.StringVariables)
        return false;
    return true;
}