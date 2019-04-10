/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

 /* {todo}
  * - Currently this uses a single mutex to reserve quite a lot; if this sees more
  *   action, that will becom a bottleneck.
  * - Property use move semantics
  * - Detect circular dependancies (hub files can link in themselves!)
  * - Timeout for error files could increase upon repeated errors (?)
  */

#include <time.h>

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Sys Thread.h"
#include "BlackRoot/Pubc/Sys Path.h"
#include "BlackRoot/Pubc/Files.h"
#include "BlackRoot/Pubc/JSON.h"
#include "BlackRoot/Pubc/Stringstream.h"

#include "HephaestusBase/Pubc/File Change Monitor.h"

using namespace Hephaestus::Pipeline;
namespace fs = std::experimental::filesystem;

    //  Setup
    // --------------------

FileChangeMonitor::FileChangeMonitor()
{
    this->NextID        = 0;
    this->CurrentState  = State::Stopped;

    this->OriginalHubDependancy = this->GetNewID();

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
        std::unique_lock<std::mutex> lock(this->MutexAccessFiles);
        this->UpdateSuspectPaths();
        this->UpdateDirtyHubs();
        this->UpdateDirtyPipes();

        bool anyActiveDirty  = this->GetActiveDirtyHubCount() > 0 ||
                               this->GetActiveDirtyPipeCount() > 0;
        bool anyActivity = anyActiveDirty;

        if (!anyActiveDirty) {
            this->UpdatePipeOutbox();
            this->UpdatePipeInbox();
        }

        if (!anyActivity) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}

void FileChangeMonitor::UpdateSuspectPaths()
{
    this->SuspectPaths.insert(this->SuspectPaths.end(), this->FutureSuspectPaths.begin(), this->FutureSuspectPaths.end());
    this->FutureSuspectPaths.resize(0);

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
    this->DirtyHubs.insert(this->DirtyHubs.end(), this->FutureDirtyHubs.begin(), this->FutureDirtyHubs.end());
    this->FutureDirtyHubs.resize(0);

    while (this->DirtyHubs.size() > 0) {
        if (this->ShouldInterrupt())
            break;

            // Select a single hub from our suspect list and try to update it
            // If anything fails the update function will put it in the list again
        auto id = *this->DirtyHubs.begin();
        this->DirtyHubs.erase(std::remove(this->DirtyHubs.begin(), this->DirtyHubs.end(), id), this->DirtyHubs.end());
        this->UpdateDirtyHub(id);
    }
}

void FileChangeMonitor::UpdateDirtyPipes()
{
    this->DirtyPipes.insert(this->DirtyPipes.end(), this->FutureDirtyPipes.begin(), this->FutureDirtyPipes.end());
    this->FutureDirtyPipes.resize(0);

    while (this->DirtyPipes.size() > 0) {
        if (this->ShouldInterrupt())
            break;

            // Select a single pipe from our suspect list and try to send it
            // If anything fails the update function will put it in the list again
        auto id = *this->DirtyPipes.begin();
        this->DirtyPipes.erase(std::remove(this->DirtyPipes.begin(), this->DirtyPipes.end(), id), this->DirtyPipes.end());
        this->UpdateDirtyPipe(id);
    }
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
        this->FutureSuspectPaths.push_back(id);
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
    cout{} << "Changed: " << this->SimpleFormatPath(prop.Path) << " (" << formatted << ")" << std::endl;

        // Update path meta
    prop.LastUpdate = fileWriteTime;
}

    //  Update hubs
    // --------------------

void FileChangeMonitor::UpdateDirtyHub(InternalID id)
{
    namespace IO = BlackRoot::IO;
    using cout = BlackRoot::Util::Cout;

        // Use the current time as a reference for file changes
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();

        // Find the hub properties
    auto itProp = this->HubProperties.find(id);
    if (itProp == this->HubProperties.end())
        return;
    auto & prop = itProp->second;

        // If we are an orphan we shouldn't update, so add us to
        // a special list to keep track of us
    if (prop.HubDependency == Monitor::InternalIDNone) {
        this->OrphanedDirtyHubs.push_back(id);
        return;
    }

        // A timeout prevents a file from updating;
        // if we are timed out just put us on the dirty list
    if (prop.Timeout > currentTime) {
        this->FutureDirtyHubs.push_back(id);
        return;
    }

        // Make dependants orphan before we even complete the hub process
    this->MakeDependantsOnHubOrphan(id);
    
    cout{} << std::endl << "Hub:" << this->SimpleFormatHub(prop) << std::endl;

    BlackRoot::IO::BaseFileSource::FCont contents;
    BlackRoot::Format::JSON jsonCont;

        // See if we can load the file contents as JSON
    try {
        contents = this->FileSource->ReadFile(prop.Path, IO::FileMode::OpenInstr{}.Default().Share(IO::FileMode::Share::Read));
        jsonCont = BlackRoot::Format::JSON::parse(contents);
        
            // As hub files can have nested properties, we simply process the main file as a group
        this->ProcessHubGroup(id, prop.InputProcessProp, jsonCont);
    }
    catch (BlackRoot::Debug::Exception * e) {
        this->HandleHubFileError(id, e);
        return;
    }
    catch (...) {
        this->HandleHubFileError(id, nullptr);
        return;
    }
    
    cout{} << "Done." << std::endl;
}

void FileChangeMonitor::ProcessHubGroup(InternalID id, const Monitor::ProcessProperties prop, Monitor::JSON group)
{
    Monitor::ProcessProperties subProp = prop;

        // Adapt our variables with potential changes based on the hub
    auto & vars = group.find("vars");
    if (vars != group.end()) {
        DbAssertMsgFatal(vars.value().is_array(), "Var list must be an array");
        subProp.AdaptVariables(vars.value());
    }

        // Find or spawn hubs listed by this hub group
    auto & subGroups = group.find("groups");
    if (subGroups != group.end()) {
        DbAssertMsgFatal(subGroups.value().is_array(), "Subgroup list must be an array");
        
            // Simply send all subgroups to this function, recursively
        for (auto & elem : subGroups.value()) {
            this->ProcessHubGroup(id, subProp, elem);
        }
    }

        // Find or spawn hubs listed by this hub group
    auto & hubs = group.find("hubs");
    if (hubs != group.end()) {
        DbAssertMsgFatal(hubs.value().is_array(), "Hub list must be an array");

        for (auto & elem : hubs.value()) {
                // First we update our variables; the path may depend on them
            Monitor::ProcessProperties uniqueProp = subProp;

            auto & list = elem.find("vars");
            if (list != elem.end()) {
                DbAssertMsgFatal(list.value().is_array(), "Var list must be array");
                uniqueProp.AdaptVariables(list.value());
            }

                // Find and process path
            auto & path = elem.find("path");
            DbAssertMsgFatal(path.value().is_string(), "Hub Path must be a string");
            std::string hubPath = uniqueProp.StringVariables["cur-dir"];
            hubPath += BlackRoot::System::DirSeperator;
            hubPath += path.value().get<std::string>();
            hubPath = uniqueProp.ProcessString(hubPath);
        
                // Directly set the directory variable to the child's input path
            uniqueProp.StringVariables["cur-dir"] = fs::canonical(Monitor::Path(hubPath).parent_path()).string();

                // Paths may contain wildcards. A wildcard-containing path spaws its
                // own objects (TODO) to monitor changes to folder structures which
                // may change the processing, variables, etc
            if (this->PathContainsWildcards(hubPath)) {
                    // TODO
                DbAssertMsgFatal(0, "Wildcards are not yet supported.");
                continue;
            }
            
                // Create a reference for finding an orphaned hub; or creating a new one
            HubProp hub;
            hub.SetDefault();
            hub.HubDependency    = id;
            hub.Path             = hubPath;
            hub.InputProcessProp = uniqueProp;

            this->FindOrAddHub(hub);
        }
    }

        // Find or spawn pipes listed by this hub group
    auto & pipes = group.find("pipes");
    if (pipes != group.end()) {
        DbAssertMsgFatal(pipes.value().is_array(), "Pipe list must be an array");
        
        for (auto & elem : pipes.value()) {
                // First we update our variables
            Monitor::ProcessProperties uniqueProp = subProp;
            
            std::string tool  = elem["tool"].get<std::string>();
            DbAssertMsgFatal(tool.length() > 0, "Pipe must have 'tool' value");

            auto & list = elem.find("vars");
            if (list != elem.end()) {
                DbAssertMsgFatal(list.value().is_array(), "Var list must be array");
                uniqueProp.AdaptVariables(list.value());
            }

                // Blanket replace this entire pipe
            uniqueProp.ProcessJSONRecursively(&elem);

                // Obtain settings
            auto & settings = elem["settings"];

                // Paths are all { "in" : "pairs" }, we create unique items
                // for all of them
            auto & pathList = elem.find("paths");
            if (pathList != elem.end()) {
                DbAssertMsgFatal(pathList.value().is_array(), "Pipe path list must be array");
                
                for (auto & item : pathList.value()) {
                    std::string pathIn  = item["in"];
                    std::string pathOut = item["out"];

                    DbAssertMsgFatal(pathIn.length() > 0, "Pipe path must have 'in' value");
                    DbAssertMsgFatal(pathOut.length() > 0, "Pipe path must have 'out' value");

                        // Paths may contain wildcards. A wildcard-containing path spaws its
                        // own objects (TODO) to monitor changes to folder structures which
                        // may change the processing, variables, etc
                    if (this->PathContainsWildcards(pathIn)) {
                            // TODO
                        DbAssertMsgFatal(0, "Wildcards are not yet supported.");
                        continue;
                    }

                        // Create a reference for finding an orphaned pipe; or creating a new one
                    PipeProp pipe;
                    pipe.SetDefault();
                    pipe.HubDependency = id;
                    pipe.Tool          = tool;
                    pipe.BasePathIn    = fs::canonical(Monitor::Path(pathIn));
                    pipe.BasePathOut   = fs::canonical(Monitor::Path(pathOut));

                    this->FindOrAddPipe(pipe);
                }
            }
        }
    }
}

void FileChangeMonitor::CleanupOrphanedHubs()
{
    // TODO
}

    //  Update pipes
    // --------------------

void FileChangeMonitor::UpdateDirtyPipe(InternalID id)
{
    namespace IO = BlackRoot::IO;
    using cout = BlackRoot::Util::Cout;

        // Use the current time as a reference for file changes
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();

        // Find the hub properties
    auto itProp = this->PipeProperties.find(id);
    if (itProp == this->PipeProperties.end())
        return;
    auto & prop = itProp->second;

        // If we are an orphan we shouldn't update, so add us to
        // a special list to keep track of us
    if (prop.HubDependency == Monitor::InternalIDNone) {
        this->OrphanedDirtyPipes.push_back(id);
        return;
    }

        // A timeout prevents a file from updating;
        // if we are timed out just put us on the dirty list
    if (prop.Timeout > currentTime) {
        this->FutureDirtyPipes.push_back(id);
        return;
    }

        // Make dependants orphan before we even complete the hub process
    this->MakeDependantsOnHubOrphan(id);
    
    cout{} << std::endl << "Pipe: " << this->SimpleFormatPipe(prop) << std::endl;

        // We remove all path dependencies; we 'send off' this pipe and
        // we do not care about it changing while it is already pending
        // (with the exception of it being removed, of course)
    prop.PathDependencies.resize(0);

        // Put it in the outbox
    this->OutboxPipes.push_back(id);
}

void FileChangeMonitor::CleanupOrphanedPipes()
{
    // TODO
}

    //  Update outbox / inbox
    // --------------------

void FileChangeMonitor::UpdatePipeOutbox()
{
    using cout = BlackRoot::Util::Cout;

    if (this->OutboxPipes.size() == 0)
        return;
    
    cout{} << std::endl  << "Sending off " << this->OutboxPipes.size() << " pipes." << std::endl;

    // TODO: no code for this, just, uh, just, uh, throw the whole box away
    this->OutboxPipes.resize(0);
}

void FileChangeMonitor::UpdatePipeInbox()
{
    // TODO
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

FileChangeMonitor::InternalID FileChangeMonitor::FindOrAddHub(HubProp hub)
{
    for (auto it : this->HubProperties) {
        if (!it.second.EqualsAbstractly(hub))
            continue;
        
        if (hub.HubDependency != Monitor::InternalIDNone) {
            it.second.HubDependency = hub.HubDependency;
            
                // If we were orphaned, check if we are on the orphaned dirty list and
                // if so, put us on the proper dirty hub list
            auto found = std::find(this->OrphanedDirtyHubs.begin(), this->OrphanedDirtyHubs.end(), it.first);
            if (found != this->OrphanedDirtyHubs.end()) {
                this->OrphanedDirtyHubs.erase(found);
                this->DirtyHubs.push_back(it.first);
            }
        }

        return it.first;
    }
    
    hub.PathDependencies.resize(0);
    hub.PathDependencies.push_back(this->FindOrAddMonitoredPath(hub.Path, nullptr));

    auto id = this->GetNewID();
    this->HubProperties[id] = hub;

    this->FutureDirtyHubs.push_back(id);

    return id;
}

FileChangeMonitor::InternalID FileChangeMonitor::FindOrAddPipe(PipeProp pipe)
{
    for (auto it : this->PipeProperties) {
        if (!it.second.EqualsAbstractly(pipe))
            continue;
        
        if (pipe.HubDependency != Monitor::InternalIDNone) {
            it.second.HubDependency = pipe.HubDependency;

                // If we were orphaned, check if we are on the orphaned dirty list and
                // if so, put us on the proper dirty pipe list
            auto found = std::find(this->OrphanedDirtyPipes.begin(), this->OrphanedDirtyPipes.end(), it.first);
            if (found != this->OrphanedDirtyPipes.end()) {
                this->OrphanedDirtyPipes.erase(found);
                this->DirtyPipes.push_back(it.first);
            }
        }

        return it.first;
    }
    
        // Note, we do not add any path dependencies. As a new pipe we by default are dirty,
        // and by default we'll be sent off; there is no use to tracking anything
    pipe.PathDependencies.resize(0);

    auto id = this->GetNewID();
    this->PipeProperties[id] = pipe;

    this->FutureDirtyPipes.push_back(id);

    return id;
}

void FileChangeMonitor::MakeUsersOfPathDirty(InternalID id)
{
        // Check hubs
    for (auto it : this->HubProperties) {
        auto & dep = it.second.PathDependencies;
        if (std::find(dep.begin(), dep.end(), id) == dep.end())
            continue;
        this->FutureDirtyHubs.push_back(it.first);
    }

        // Check pipes
    for (auto it : this->PipeProperties) {
        auto & dep = it.second.PathDependencies;
        if (std::find(dep.begin(), dep.end(), id) == dep.end())
            continue;
        this->FutureDirtyPipes.push_back(it.first);
    }
}

void FileChangeMonitor::MakeDependantsOnHubOrphan(InternalID id)
{
        // Check hubs
    for (auto & it : this->HubProperties) {
        if (id != it.second.HubDependency)
            continue;
        it.second.HubDependency = Monitor::InternalIDNone;
    }
    
        // Check pipes
    for (auto & it : this->PipeProperties) {
        if (id != it.second.HubDependency)
            continue;
        it.second.HubDependency = Monitor::InternalIDNone;
    }
}

    //  Util
    // --------------------

FileChangeMonitor::Count FileChangeMonitor::GetActiveDirtyHubCount()
{
    return this->DirtyHubs.size() + this->FutureDirtyHubs.size();
}

FileChangeMonitor::Count FileChangeMonitor::GetActiveDirtyPipeCount()
{
    return this->DirtyPipes.size() + this->FutureDirtyPipes.size();
}

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

Monitor::Path FileChangeMonitor::SimpleFormatPath(Monitor::Path path)
{
    // TODO: C++17 isn't that hot yet; we don't have relative path

    std::string refPath = this->InfoReferenceDirectory.string();
    std::string curPath = path.string();

    size_t diffIndex = 0;
    while (refPath[diffIndex] == curPath[diffIndex]) {
        if (++diffIndex == refPath.length())
            break;
    }

    curPath.erase(0, diffIndex+1);

    return curPath;
}

bool FileChangeMonitor::PathContainsWildcards(const std::string path)
{
        // All wildcarts use a *, so just return whether that is found
    return path.find("*") != std::string::npos;
}

std::string FileChangeMonitor::SimpleFormatHub(HubProp prop)
{
    std::stringstream ss;

    ss << this->SimpleFormatPath(prop.Path);

    for (auto & elem : prop.InputProcessProp.StringVariables) {
        if (elem.first.find("-dir") != elem.first.npos) {
            ss << std::endl << " " << elem.first << " : " << this->SimpleFormatPath(elem.second);
        }
        else {
            ss << std::endl << " " << elem.first << " : " << elem.second;
        }
    }

    return ss.str();
}

std::string FileChangeMonitor::SimpleFormatPipe(PipeProp prop)
{
    std::stringstream ss;

    ss << this->SimpleFormatPath(prop.BasePathIn) << std::endl;
    ss << " Out: " << this->SimpleFormatPath(prop.BasePathOut) << std::endl;
    ss << " Settings: " << prop.Settings.dump(2);

    return ss.str();
}

void FileChangeMonitor::HandleMonitoredPathMissing(InternalID id)
{
    using cout = BlackRoot::Util::Cout;

    auto it = this->MonitoredPaths.find(id);
    if (it == this->MonitoredPaths.end())
        return;
    auto & prop = it->second;
    
    cout{} << "File missing: " << this->SimpleFormatPath(prop.Path) << std::endl;
    
        // The file might 'come back' by another file being renamed; this
        // might change the contents of the file without changing the time
        // To counter this we just reset to the beginning of time
    prop.LastUpdate = std::chrono::time_point<std::chrono::system_clock>{};

        // Set the timeout to a few seconds from now to prevent a file
        // being constantly updated
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();
    prop.Timeout = currentTime + std::chrono::seconds(4);

    this->FutureSuspectPaths.push_back(id);
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

    this->FutureSuspectPaths.push_back(id);

    delete e;
}

void FileChangeMonitor::HandleHubFileError(InternalID id, BlackRoot::Debug::Exception *e)
{
    using cout = BlackRoot::Util::Cout;

    auto it = this->HubProperties.find(id);
    if (it == this->HubProperties.end())
        return;
    auto & prop = it->second;
    
    if (e) {
        cout{} << "Hub error: " << this->SimpleFormatPath(prop.Path.string()) << std::endl << " " << e->GetPrettyDescription() << std::endl;
    }
    else {
        cout{} << "Unknown hub error: " << this->SimpleFormatPath(prop.Path.string()) << std::endl;
    }
    
        // Set the timeout to a few seconds from now to prevent a file
        // being constantly updated
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();
    prop.Timeout = currentTime + std::chrono::seconds(4);

        // As a precaution make all paths used by this hub suspicious
    for (auto & pid : prop.PathDependencies) {
        this->SuspectPaths.push_back(pid);
    }

        // Push the hub back on the dirty hub stack
    this->DirtyHubs.push_back(id);

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
    hub.HubDependency = this->OriginalHubDependancy;
    hub.Path = path;

        // We always set a variable "cur-dir" to be the parent directory of
        // the file. Hubs themselves then use this variable as a relative path.
    hub.InputProcessProp.StringVariables["cur-dir"] = hub.Path.parent_path().string();

    this->FindOrAddHub(hub);
}

void FileChangeMonitor::SetReferenceDirectory(const BlackRoot::IO::FilePath path)
{
    this->InfoReferenceDirectory = fs::canonical(path);
}

    //  Process
    // --------------------

void Monitor::ProcessProperties::SetDefault()
{
    this->StringVariables.clear();
}

void Monitor::ProcessProperties::AdaptVariables(const JSON json)
{
    DbAssert(json.is_array());

    for (auto & elem : json) {
        for (auto & kv : elem.items()) {
            this->StringVariables[this->ProcessString(kv.key())] = this->ProcessString(kv.value());
        }
    }
}

std::string Monitor::ProcessProperties::ProcessString(std::string str)
{
    size_t start;

        // Define a max itteration just in case something manages to
        // get a circular replacement going
    int maxItt = 1 << 4;

        // We look by last so that nested processing can happen
        // (which is a terrible idea, and it shouldn't happen, but
        // it no doubt comes in handy sometime, one time, one day)
    while (std::string::npos != (start = str.find_last_of("{"))) {
        size_t end = str.find_first_of("}", start+1);
        DbAssertMsgFatal(end != std::string::npos, "Malformed string");

        auto key = str.substr(start+1, end-start-1);
        auto & it = this->StringVariables.find(key);
        DbAssertMsgFatal(it != this->StringVariables.end(), (std::stringstream{} << "Unknown key '" << key << "'").str());

        str.replace(str.begin() + start, str.begin() + end + 1, it->second);

        DbAssertMsgFatal(--maxItt >= 0, "Maximum of iterations exceeded.");
    }

    return str;
}

void Monitor::ProcessProperties::ProcessJSONRecursively(JSON * json)
{
    for (auto & elem : (*json)) {
        if (elem.is_string()) {
            elem = this->ProcessString(elem.get<std::string>());
            continue;
        }
        if (elem.is_array() || elem.is_object()) {
            this->ProcessJSONRecursively(&elem);
        }
    }
}

bool Monitor::ProcessProperties::Equals(const ProcessProperties rh)
{
    if (this->StringVariables != rh.StringVariables)
        return false;
    return true;
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
    this->HubDependency = Monitor::InternalIDNone;

    this->Timeout    = std::chrono::system_clock::now();

    this->InputProcessProp.SetDefault();
}

bool Monitor::HubProperties::EqualsAbstractly(const HubProperties rh)
{
    if (this->Path != rh.Path)
        return false;
    if (this->HubDependency != Monitor::InternalIDNone &&
        this->HubDependency != rh.HubDependency)
        return false;
    if (!this->InputProcessProp.Equals(rh.InputProcessProp))
        return false;
    return true;
}

void Monitor::PipeProperties::SetDefault()
{
    this->PathDependencies.resize(0);

    this->BasePathIn    = "";
    this->BasePathOut   = "";

    this->HubDependency = Monitor::InternalIDNone;
    this->Timeout       = std::chrono::system_clock::now();

    this->Settings      = {};
}

bool Monitor::PipeProperties::EqualsAbstractly(const PipeProperties rh)
{
    if (0 != this->Tool.compare(rh.Tool))
        return false;
    if (this->BasePathIn != rh.BasePathIn)
        return false;
    if (this->BasePathOut != rh.BasePathOut)
        return false;
    if (this->HubDependency != Monitor::InternalIDNone &&
        this->HubDependency != rh.HubDependency)
        return false;
    if (!(this->Settings == rh.Settings))
        return false;
    return true;
}