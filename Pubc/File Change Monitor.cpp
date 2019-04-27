/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

 /* {todo}
  * - Currently this uses a single mutex to reserve quite a lot; if this sees more
  *   action, that will becom a bottleneck.
  * - Property use move semantics
  * - Detect circular dependancies (hub files can link in themselves!)
  * - Timeout for error files could increase upon repeated errors (?)
  * - If a file for a pipe does not exist, do not send it immediately
  * - Handle files being written to being deleted (monitoring only for file existence)
  * - Handle the situation in which a file is being written to for a longer
  *   duration; i.e., hide errors while it is clear a file is write-locked
  */

#include <time.h>

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Sys Thread.h"
#include "BlackRoot/Pubc/Sys Path.h"
#include "BlackRoot/Pubc/Files.h"
#include "BlackRoot/Pubc/JSON.h"
#include "BlackRoot/Pubc/Stringstream.h"
#include "BlackRoot/Pubc/File Wildcard.h"

#include "HephaestusBase/Pubc/File Change Monitor.h"

using namespace Hephaestus::Pipeline;
using namespace Hephaestus::Pipeline::Monitor;

namespace fs = std::experimental::filesystem;

    //  Setup
    // --------------------

FileChangeMonitor::FileChangeMonitor()
{
    this->NextID        = 0;
    this->CurrentState  = State::Stopped;
    this->TargetState   = State::Stopped;

    this->OriginalHubDependancy = this->GetNewID();

    this->WranglerResultCount = 0;

    this->PendingSaveChanges = true;

    this->FileSource = new BlackRoot::IO::BaseFileSource();
}

FileChangeMonitor::~FileChangeMonitor()
{
    using cout = BlackRoot::Util::Cout;

    if (this->CurrentState != State::Stopped) {
        cout{} << "!!Change monitor was not stopped before being destructed!";
    }

    delete this->FileSource;
}

    //  Update cycle
    // --------------------

void FileChangeMonitor::UpdateCycle()
{
    while (this->TargetState == State::Running) {
        std::unique_lock<std::mutex> lock(this->MutexAccessFiles);
        this->UpdateSuspectPaths();
        this->UpdateSuspectWildcards();

        bool anyActiveDirty = this->GetActiveDirtyHubCount() > 0 ||
                              this->GetActiveDirtyPipeCount() > 0;
        bool anyActivity = anyActiveDirty;

        this->UpdateDirtyHubs();
        this->CleanupOrphanedHubs();

        this->UpdateDirtyPipeWildcards();
        this->UpdateDirtyPipes();

        this->UpdatePipeOutbox();
        this->UpdatePipeInbox();

        if (this->PendingSaveChanges) {
            this->SaveToPersistent();
        }

        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void FileChangeMonitor::UpdateSuspectWildcards()
{
    this->SuspectWildcards.insert(this->SuspectWildcards.end(), this->FutureSuspectWildcards.begin(), this->FutureSuspectWildcards.end());
    this->FutureSuspectWildcards.resize(0);

    while (this->SuspectWildcards.size() > 0) {
        if (this->ShouldInterrupt())
            break;

            // Select a single path from our suspect list and try to update it
            // If anything fails the update function will put it in the list again
        auto id = *this->SuspectWildcards.begin();
        this->SuspectWildcards.erase(std::remove(this->SuspectWildcards.begin(), this->SuspectWildcards.end(), id), this->SuspectWildcards.end());
        this->UpdateSuspectWildcard(id);
    }

        // Debug; for now just make all wildcards suspect to check functionality
    for (auto it : this->MonitoredWildcards) {
        this->SuspectWildcards.push_back(it.first);
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

void FileChangeMonitor::UpdateDirtyPipeWildcards()
{
    this->DirtyPipeWildcards.insert(this->DirtyPipeWildcards.end(), this->FutureDirtyPipeWildcards.begin(), this->FutureDirtyPipeWildcards.end());
    this->FutureDirtyPipeWildcards.resize(0);

    while (this->DirtyPipeWildcards.size() > 0) {
        if (this->ShouldInterrupt())
            break;

            // Select a wildcard from our dirty list and try to update it
        auto id = *this->DirtyPipeWildcards.begin();
        this->DirtyPipeWildcards.erase(std::remove(this->DirtyPipeWildcards.begin(), this->DirtyPipeWildcards.end(), id), this->DirtyPipeWildcards.end());
        this->UpdateDirtyPipeWildcard(id);
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
            // We take a few ms margin to allow these times to be passed around
            // with millisecond precision (to facilitate non-STD dynlibs etc)
        fileWriteTime = this->FileSource->LastWriteTime(prop.Path);

        if (this->FileTimeEqualsWithEpsilon(prop.LastUpdate, fileWriteTime)) {
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
    
    this->PendingSaveChanges    = true;
    
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

    //  Update wildcards
    // --------------------

void FileChangeMonitor::UpdateSuspectWildcard(InternalID id)
{
    namespace IO = BlackRoot::IO;
    using cout = BlackRoot::Util::Cout;

        // Find the path properties
    auto itProp = this->MonitoredWildcards.find(id);
    if (itProp == this->MonitoredWildcards.end())
        return;
    auto & prop = itProp->second;

    if (prop.Check.Check(this->FileSource)) {
        cout{} << std::endl << "Update wildcard" << std::endl << " " << prop.Check.GetCheckPath() << std::endl;
        this->MakeUsersOfWildcardDirty(id);
    }
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
    
    this->PendingSaveChanges = true;

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
}

void FileChangeMonitor::ProcessHubGroup(InternalID id, const ProcessProperties prop, JSON group)
{ DbFunctionTry {
    using cout = BlackRoot::Util::Cout;

    ProcessProperties subProp = prop;

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
                    
                        // We only process the _in path; it may be a wildcard, which may
                        // make the processing of the _out_ path dependant on variables
                        // created by the wildcard
                    pathIn  = uniqueProp.ProcessString(pathIn);

                        // If we have a wildard, create a pipewildcard which will handle updates,
                        // or find an orphan with the right properties
                    if (this->PathContainsWildcards(pathIn)) {
                        PipeWild pipe;
                        pipe.SetDefault();
                        pipe.HubDependency      = id;
                        pipe.WildcardDependency = this->FindOrAddMonitoredWildcard(pathIn);
                        pipe.InputProcessProp   = uniqueProp;
                        pipe.Tool               = tool;
                        pipe.BasePathIn         = fs::canonical(Monitor::Path(pathIn));
                        pipe.BasePathOut        = Monitor::Path(pathOut);
                        pipe.Settings           = settings;

                        this->FindOrAddPipeWildcards(pipe);
                        continue;
                    }
                    
                        // With no wildcard, we are free to process the out path
                    pathOut = uniqueProp.ProcessString(pathOut);

                        // Do the base replacement for the settings; a wildcard can
                        // actually replace its settings, so we do this as a copy
                    auto uniqueSettings = settings;
                    uniqueProp.ProcessJSONRecursively(&uniqueSettings);

                        // Create a regular pipe, or find an orphan with the right properties
                    PipeProp pipe;
                    pipe.SetDefault();
                    pipe.HubDependency = id;
                    pipe.Tool          = tool;
                    pipe.BasePathIn    = fs::canonical(Monitor::Path(pathIn));
                    pipe.BasePathOut   = fs::canonical(Monitor::Path(pathOut));
                    pipe.Settings      = settings;

                    this->FindOrAddPipe(pipe);
                }
            }
        }
    }
} DbFunctionCatch;
}

void FileChangeMonitor::CleanupOrphanedHubs()
{
    using cout = BlackRoot::Util::Cout;
    
    for (auto & it : this->PotentiallyOrphanedHubs) {
            // Find the hub properties
        auto & itProp = this->HubProperties.find(it);
        if (itProp == this->HubProperties.end())
            return;
        auto & prop = itProp->second;
        
        this->PendingSaveChanges = true;

            // If the hub has a dependency now, it is not orphaned
            // and does not need to be erased
        if (prop.HubDependency != InternalIDNone)
            continue;

        cout{} << "Cleanup hub " << this->SimpleFormatHub(prop) << std::endl;

            // We make this hub's dependants orphaned, which at this
            // point makes this function recursively remove all hubs
        this->MakeDependantsOnHubOrphan(it);
        this->HubProperties.erase(it);
    }

    this->PotentiallyOrphanedHubs.resize(0);
}

    //  Update pipes
    // --------------------

void FileChangeMonitor::UpdateDirtyPipeWildcard(InternalID id)
{
    namespace IO = BlackRoot::IO;
    using cout = BlackRoot::Util::Cout;

        // Find the wildcard properties
    auto & itProp = this->PipeWildcards.find(id);
    if (itProp == this->PipeWildcards.end())
        return;
    auto & prop = itProp->second;

        // Check the paths
    WildcardCheck check;
    check.SetCheckPath(prop.BasePathIn);
    check.SetWildcard("*");
    check.SetDelimiters("~", "~");
    check.Check(this->FileSource);

        // For each found path, try to make a pipe
    for (auto & it : check.GetFound()) {

            // Every path gets its unique properties, with the
            // wildcard replacements as variables
        Monitor::ProcessProperties uniqueProp = prop.InputProcessProp;
        for (auto & r : it.Replacements) {
            uniqueProp.StringVariables[r.first] = r.second;
        }

        auto uniqueSettings = prop.Settings;
        uniqueProp.ProcessJSONRecursively(&uniqueSettings);
                    
            // Find the outpath
        std::string pathOut = uniqueProp.ProcessString(prop.BasePathOut.string());

            // Create a regular pipe, or find an orphan with the right properties
        PipeProp pipe;
        pipe.SetDefault();
        pipe.HubDependency      = prop.HubDependency;
        pipe.WildcardDependency = id;
        pipe.Tool               = prop.Tool;
        pipe.BasePathIn         = fs::canonical(it.FoundPath);
        pipe.BasePathOut        = fs::canonical(Monitor::Path(pathOut));
        pipe.Settings           = std::move(uniqueSettings);

        this->FindOrAddPipe(pipe);
    }
}

void FileChangeMonitor::UpdateDirtyPipe(InternalID id)
{
    namespace IO = BlackRoot::IO;
    using cout = BlackRoot::Util::Cout;

        // Use the current time as a reference for file changes
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();

        // Find the pipe properties
    auto & itProp = this->PipeProperties.find(id);
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
    
    this->PendingSaveChanges = true;
    
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

    //  Asynch
    // --------------------

void FileChangeMonitor::AsynchReceiveTaskResult(const WranglerTaskResult& result)
{
    using cout = BlackRoot::Util::Cout;

    std::unique_lock<std::mutex> lk(this->MxWranglerResults);
    this->WranglerResults.push_back(result);
    this->WranglerResultCount += 1;
}

JSON FileChangeMonitor::AsynchGetTrackedInformation()
{
    std::unique_lock<std::mutex> lock(this->MutexAccessFiles);
    
    JSON paths;
    for (auto & it : this->MonitoredPaths) {
        auto & prop = it.second;
        paths += {
            { "path", this->SimpleFormatPath(prop.Path).string() }
        };
    }

    JSON hubs;
    for (auto & it : this->HubProperties) {
        auto & prop = it.second;
        hubs += {
            { "path", this->SimpleFormatPath(prop.Path).string() }
        };
    }

    JSON wild;
    for (auto & it : this->PipeWildcards) {
        auto & prop = it.second;
        wild += {
            { "path", this->SimpleFormatPath(prop.BasePathIn).string() }
        };
    }

    return {
        { "paths" , paths },
        { "hubs" , hubs },
        { "wildcards" , wild }
    };
}

    //  Update outbox / inbox
    // --------------------

void FileChangeMonitor::UpdatePipeOutbox()
{
    using cout = BlackRoot::Util::Cout;

    if (this->OutboxPipes.size() == 0)
        return;

    this->PendingSaveChanges    = true;
    
    cout{} << std::endl  << "Sending off " << this->OutboxPipes.size() << " pipes." << std::endl;

    WranglerTaskList tasks(this->OutboxPipes.size());

    int outputCount = 0;
    for (auto id : this->OutboxPipes) {
        auto & itProp = this->PipeProperties.find(id);
        if (itProp == this->PipeProperties.end())
            return;
        auto & prop = itProp->second;

        WranglerTask task;
        task.Callback = [&](WranglerTaskResult &&r){ this->AsynchReceiveTaskResult(std::move(r)); };
        task.UniqueID = id;
        task.ToolName = prop.Tool;
        task.FileIn   = prop.BasePathIn;
        task.FileOut  = prop.BasePathOut;
        task.Settings = prop.Settings;

#ifdef _WIN32
        std::string outFile = task.FileOut.string();
        if (outFile.find(".exe") != outFile.npos && this->FileSource->FileExists(task.FileOut)) {
            try {
                auto tmpPath = this->PersistentDirectory / task.FileOut.filename().replace_extension("~old");
                this->FileSource->CreateDirectories(this->PersistentDirectory);
                if (this->FileSource->Exists(tmpPath)) {
                    this->FileSource->Remove(tmpPath);
                }
                this->FileSource->Rename(prop.BasePathOut, tmpPath);
                cout{} << "Renamed [" << this->SimpleFormatPath(outFile) << "]" << std::endl << std::endl;
            }
            catch (...) {
                cout{} << "Tried to rename [" << this->SimpleFormatPath(outFile) << "] but failed." << std::endl << std::endl;
            }
        }
#endif


        tasks[outputCount++] = std::move(task);
    }

    tasks.resize(outputCount);
    this->Wrangler->AsynchReceiveTasks(tasks);
    
    this->PendingPipes.insert(this->PendingPipes.end(), this->OutboxPipes.begin(), this->OutboxPipes.end());

    this->OutboxPipes.resize(0);
}

void FileChangeMonitor::UpdatePipeInbox()
{
    using cout = BlackRoot::Util::Cout;

    if (this->PendingPipes.size() == 0)
        return;
    if (this->WranglerResultCount == 0)
        return;
    
    while (true) {
            // See if we have wrangler results, and if so, get one
        std::unique_lock<std::mutex> lk(this->MxWranglerResults);

        auto it = this->WranglerResults.begin();
        if (it == this->WranglerResults.end()) {
            this->WranglerResultCount = 0;
            return;
        }

        auto val = std::move(*it);

        this->WranglerResults.erase(it);
        this->WranglerResultCount -= 1;

        lk.unlock();

        this->PendingSaveChanges    = true;

            // We gave the wrangler our unique pipe id as unique id
        auto id = val.UniqueID;
        
            // Get our original data. If we can't find it, the pipe may
            // have been removed before the wrangler could even return;
            // this is not a problem for anybody, so just forget about it.
        auto pipeit = this->PipeProperties.find(id);
        if (pipeit == this->PipeProperties.end())
            continue;
        auto & pipe = pipeit->second;

            // If there was an error give it to the handler; that probably
            // will schedule it for a timeout and a retry
        if (val.Exception) {
            this->HandleWrangledPipeError(id, val.Exception);
            continue;
        }

            // Add all read files as paths that this pipe depends on. If
            // any path has a previous time that is past when we read it,
            // we just dirty the pipe immediately again.
            // If the path is new, it will now have the last modified time
            // of when it was read and may be found to have changed.
        for (auto & fi : val.ReadFiles) {
            auto prevTime = fi.LastChange;
            auto pathId = this->FindOrAddMonitoredPath(fi.Path, &prevTime);
            pipe.PathDependencies.push_back(pathId);

            if (!this->FileTimeEqualsWithEpsilon(fi.LastChange, prevTime)) {
                this->DirtyPipes.push_back(id);
            }
        }
        
            // TODO: Update written-to files


            // This pipe is done!
        this->PendingPipes.erase(std::remove(this->PendingPipes.begin(), this->PendingPipes.end(), id), this->PendingPipes.end());
        
        cout{} << std::endl << "Pipe done: " << pipe.Tool << " (" << val.ProcessDuration.count() << "ms)" << std::endl
            << " " << this->SimpleFormatPath(pipe.BasePathIn.string()) << std::endl
            << " " << this->SimpleFormatPath(pipe.BasePathOut.string()) << std::endl;
    }
}

    //  Persistent
    // --------------------

void FileChangeMonitor::LoadFromPersistent()
{
    using cout = BlackRoot::Util::Cout;

    auto pathIn   = this->PersistentDirectory / "state.json";
    
    if (!this->FileSource->Exists(pathIn))
        return;
    
    BlackRoot::IO::BaseFileSource::FCont contents;
    BlackRoot::Format::JSON jsonCont;
    
    auto clock = std::chrono::system_clock::time_point{};

        // See if we can load the file
    try {
        contents = this->FileSource->ReadFile(pathIn, BlackRoot::IO::FileMode::OpenInstr{}.Default().Share(BlackRoot::IO::FileMode::Share::Read));
        jsonCont = BlackRoot::Format::JSON::parse(contents);
        
        for (auto & it : jsonCont["paths"]) {
            std::string path = it["path"];
            auto time = clock + std::chrono::milliseconds(it["changed"].get<long long>());
            this->FindOrAddMonitoredPath(it["path"].get<std::string>(), &time);
        }

        for (auto & it : jsonCont["pipes"]) {
            PipeProp pipe;
            pipe.SetDefault();
            pipe.HubDependency = InternalIDNone;
            pipe.Tool          = it["tool"].get<std::string>();
            pipe.BasePathIn    = it["pathIn"].get<std::string>();
            pipe.BasePathOut   = it["pathOut"].get<std::string>();
            pipe.Settings      = it["settings"];

            for (auto & pit : it["paths"]) {
                DbAssert(pit.is_string());
                pipe.PathDependencies.push_back(this->FindOrAddMonitoredPath(pit.get<std::string>(), nullptr));
            }

            this->FindOrAddPipe(pipe);
        }
    }
    catch (BlackRoot::Debug::Exception * e) {
        // TODO
        return;
    }
    catch (...) {
        // TODO
        return;
    }
}

void FileChangeMonitor::SaveToPersistent()
{
    using cout = BlackRoot::Util::Cout;

    JSON outData;

        // Create master JSON structure
    auto & pathData = outData["paths"];
    auto & pipeData = outData["pipes"];
    
    auto clock = std::chrono::system_clock::time_point{}; 

    for (auto & it : this->MonitoredPaths) {
        auto & prop = it.second;

        pathData += {
            { "path", prop.Path.string() },
            { "changed", std::chrono::duration_cast<std::chrono::milliseconds>(prop.LastUpdate - clock).count() }
        };
    }
    for (auto & it : this->PipeProperties) {
        auto & prop = it.second;

            // If it is orphaned, this is a good moment to forget about it
        if (prop.HubDependency == InternalIDNone) 
            continue;

            // If we are dirty or pending, the state of the pipe depends on the executable;
            // to reflect this we simply do not save this pipe.
        if (std::find(this->DirtyPipes.begin(), this->DirtyPipes.end(), it.first) != this->DirtyPipes.end())
            continue;
        if (std::find(this->FutureDirtyPipes.begin(), this->FutureDirtyPipes.end(), it.first) != this->FutureDirtyPipes.end())
            continue;
        if (std::find(this->PendingPipes.begin(), this->PendingPipes.end(), it.first) != this->PendingPipes.end())
            continue;

        JSON pathData;

        for (auto & pit : prop.PathDependencies) {
            auto & fpath = this->MonitoredPaths.find(pit);
            if (fpath == this->MonitoredPaths.end())
                continue;
            pathData += fpath->second.Path.string();
        }

        pipeData += {
            { "tool", prop.Tool },
            { "pathIn", prop.BasePathIn.string() },
            { "pathOut", prop.BasePathOut.string() },
            { "settings", prop.Settings },
            { "paths", pathData }
        };
    }

        // Write to file
    try {
        auto outPathWrite = this->PersistentDirectory / "~state.json";
        auto outPathFin   = this->PersistentDirectory / "state.json";

            // Ensure the directory and remove the old ~ file
        this->FileSource->CreateDirectories(this->PersistentDirectory);
        if (this->FileSource->Exists(outPathWrite)) {
            this->FileSource->Remove(outPathWrite);
        }

            // Open a stream and dump the json
        auto * stream = this->FileSource->OpenFile(this->PersistentDirectory / "~state.json", BlackRoot::IO::IFileSource::OpenInstr{}
                                                    .Creation(BlackRoot::IO::FileMode::Creation::CreateAlways)
                                                    .Access(BlackRoot::IO::FileMode::Access::Read | BlackRoot::IO::FileMode::Access::Write)
                                                    .Share(BlackRoot::IO::FileMode::Share::None) );

        auto str = outData.dump(4);
        stream->Write((void*)(str.c_str()), str.length());
        stream->CloseAndRelease();
        
            // Rename our ~ file to the final name
        if (this->FileSource->Exists(outPathFin)) {
            this->FileSource->Remove(outPathFin);
        }
        this->FileSource->Rename(outPathWrite, outPathFin);
    }
    catch (std::exception e) {
        // TODO
        return;
    }
    catch (...) {
        // TODO
        return;
    }

    cout{} << "Saved." << std::endl;

    this->PendingSaveChanges = false;
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
    DbAssert(this->NextID < InternalIDNone);
    return this->NextID++;
}

FileChangeMonitor::InternalID FileChangeMonitor::FindOrAddMonitoredPath(Path path, TimePoint * prevTimePoint)
{
    for (auto it : this->MonitoredPaths) {
        if (it.second.Path != path)
            continue;

        if (prevTimePoint) {
            *prevTimePoint = it.second.LastUpdate;
        }
        return it.first;
    }

    MonPath monPath;
    monPath.SetDefault();
    monPath.Path = path;
    if (prevTimePoint) {
        monPath.LastUpdate = *prevTimePoint;
    }

    auto id = this->GetNewID();
    this->MonitoredPaths[id] = monPath;

    this->SuspectPaths.push_back(id);

    return id;
}

FileChangeMonitor::InternalID FileChangeMonitor::FindOrAddMonitoredWildcard(Path path)
{
    for (auto it : this->MonitoredWildcards) {
        if (it.second.Check.GetCheckPath() != path)
            continue;
        return it.first;
    }

    MonWild monWild;
    monWild.SetDefault();
    monWild.Check.SetCheckPath(path);

    auto id = this->GetNewID();
    this->MonitoredWildcards[id] = monWild;

    this->SuspectWildcards.push_back(id);

    return id;
}

FileChangeMonitor::InternalID FileChangeMonitor::FindOrAddHub(HubProp hub)
{
    for (auto & it : this->HubProperties) {
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
    for (auto & it : this->PipeProperties) {
        if (!it.second.EqualsAbstractly(pipe))
            continue;
        
        if (pipe.HubDependency != Monitor::InternalIDNone) {
            it.second.HubDependency = pipe.HubDependency;

                // If we were orphaned, check if we are on the orphaned dirty list and
                // if so, put us on the proper dirty pipe list
            auto found = std::find(this->OrphanedDirtyPipes.begin(), this->OrphanedDirtyPipes.end(), it.first);
            if (found != this->OrphanedDirtyPipes.end()) {
                this->OrphanedDirtyPipes.erase(std::remove(this->OrphanedDirtyPipes.begin(), this->OrphanedDirtyPipes.end(), it.first), this->OrphanedDirtyPipes.end());
                this->DirtyPipes.push_back(it.first);
            }
        }

        return it.first;
    }

    auto id = this->GetNewID();
    this->PipeProperties[id] = pipe;
    
        // If we are created as an orphan we should just quietly exist; but
        // if we have a parent hub we should consider ourselves dirty.
    if (pipe.HubDependency != Monitor::InternalIDNone) {
        pipe.PathDependencies.resize(0);
        this->FutureDirtyPipes.push_back(id);
    }

    return id;
}

FileChangeMonitor::InternalID FileChangeMonitor::FindOrAddPipeWildcards(PipeWild wild)
{
    for (auto & it : this->PipeWildcards) {
        if (!it.second.EqualsAbstractly(wild))
            continue;
        return it.first;
    }

    auto id = this->GetNewID();
    this->PipeWildcards[id] = wild;

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

void FileChangeMonitor::MakeUsersOfWildcardDirty(InternalID id)
{
        // Check pipe wildcards
    for (auto it : this->PipeWildcards) {
        if (id != it.second.WildcardDependency)
            continue;
        this->FutureDirtyPipeWildcards.push_back(it.first);
    }
}

void FileChangeMonitor::MakeDependantsOnHubOrphan(InternalID id)
{
        // Check hubs
    for (auto & it : this->HubProperties) {
        if (id != it.second.HubDependency)
            continue;
        it.second.HubDependency = Monitor::InternalIDNone;
        this->PotentiallyOrphanedHubs.push_back(it.first);
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
    return (Count)(this->DirtyHubs.size() + this->FutureDirtyHubs.size());
}

FileChangeMonitor::Count FileChangeMonitor::GetActiveDirtyPipeCount()
{
    return (Count)(this->DirtyPipes.size() + this->FutureDirtyPipes.size());
}

bool FileChangeMonitor::FileTimeEqualsWithEpsilon(TimePoint lh, TimePoint rh)
{
    auto diff = lh - rh;
    return (diff < std::chrono::milliseconds(5) && diff > std::chrono::milliseconds(-5));
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

Path FileChangeMonitor::SimpleFormatPath(Path path)
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
    
    this->PendingSaveChanges = true;

    auto it = this->MonitoredPaths.find(id);
    if (it == this->MonitoredPaths.end())
        return;
    auto & prop = it->second;
    
        // We need to check if anything actually uses us
    auto check_used = [&] {
        for (auto & it : this->HubProperties) {
            for (auto & sub : it.second.PathDependencies) {
                if (id != sub)
                    continue;
                return true;
            }
        }
        for (auto & it : this->PipeProperties) {
            for (auto & sub : it.second.PathDependencies) {
                if (id != sub)
                    continue;
                return true;
            }
        }
        return false;
    };

        // If we were removed, just silently remove
        // the monitored file
    if (!check_used()) {
        this->MonitoredPaths.erase(id);
        return;
    }

    cout{} << "File missing: " << this->SimpleFormatPath(prop.Path) << std::endl << std::endl;
    
        // The file might 'come back' by another file being renamed; this
        // might change the contents of the file without changing the time
        // To counter this we just reset to the beginning of time
    prop.LastUpdate = std::chrono::time_point<std::chrono::system_clock>{};

        // Set the timeout to a few second from now to prevent a file
        // being constantly updated
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();
    prop.Timeout = currentTime + std::chrono::seconds(1);

    this->FutureSuspectPaths.push_back(id);
}

void FileChangeMonitor::HandleMonitoredPathError(InternalID id, BlackRoot::Debug::Exception *e)
{
    using cout = BlackRoot::Util::Cout;
    
    this->PendingSaveChanges = true;

    auto it = this->MonitoredPaths.find(id);
    if (it == this->MonitoredPaths.end())
        return;
    auto & prop = it->second;
    
    cout{} << "File error: " << prop.Path << std::endl;
    
        // Set the timeout to a second from now to prevent a file
        // being constantly updated
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();
    prop.Timeout = currentTime + std::chrono::seconds(1);

    this->FutureSuspectPaths.push_back(id);

    delete e;
}

void FileChangeMonitor::HandleHubFileError(InternalID id, BlackRoot::Debug::Exception *e)
{
    using cout = BlackRoot::Util::Cout;
    
    this->PendingSaveChanges = true;

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
    
        // Set the timeout to a few second from now to prevent a file
        // being constantly updated
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();
    prop.Timeout = currentTime + std::chrono::seconds(1);

        // As a precaution make all paths used by this hub suspicious
    for (auto & pid : prop.PathDependencies) {
        this->SuspectPaths.push_back(pid);
    }

        // Push the hub back on the dirty hub stack
    this->DirtyHubs.push_back(id);

    delete e;
}

void FileChangeMonitor::HandleWrangledPipeError(InternalID id, BlackRoot::Debug::Exception *e)
{
    using cout = BlackRoot::Util::Cout;
    
    this->PendingSaveChanges = true;

    auto it = this->PipeProperties.find(id);
    if (it == this->PipeProperties.end())
        return;
    auto & prop = it->second;
    
    if (e) {
        cout{} << "Pipe error: " << this->SimpleFormatPath(prop.BasePathIn.string()) << std::endl << " " << e->GetPrettyDescription() << std::endl;
    }
    else {
        cout{} << "Unknown hub error: " << this->SimpleFormatPath(prop.BasePathIn.string()) << std::endl;
    }
    
        // Set the timeout to a few seconds from now to prevent a file
        // being constantly updated
    Monitor::TimePoint currentTime = std::chrono::system_clock::now();
    prop.Timeout = currentTime + std::chrono::seconds(4);

        // As a precaution make all paths used by this pipe suspicious
    for (auto & pid : prop.PathDependencies) {
        this->SuspectPaths.push_back(pid);
    }

        // Push the pipe back on the dirty hub stack
    this->DirtyPipes.push_back(id);

    delete e;
}

bool FileChangeMonitor::ShouldInterrupt()
{
    return this->TargetState != State::Running;
}

bool FileChangeMonitor::IsStopped()
{
    return this->CurrentState == State::Stopped &&
            this->TargetState == State::Stopped;
}

    //  Control
    // --------------------

void FileChangeMonitor::Begin()
{
    using cout = BlackRoot::Util::Cout;

    DbAssertFatal(this->IsStopped());
    
    this->TargetState  = State::Running;
    this->CurrentState = State::Starting;

        // Launch the low-priority update thread, which calls back into UpdateCycle
    this->UpdateThread = std::thread([&] {
        BlackRoot::System::SetCurrentThreadPriority(BlackRoot::System::ThreadPriority::Lowest);

        cout{} << "Starting FileChangeMontor thread." << std::endl;
        
        try {
            std::unique_lock<std::mutex> lock(this->MutexAccessFiles);
            this->LoadFromPersistent();
        }
        catch (BlackRoot::Debug::Exception * e) {
            this->HandleThreadException(e);
        }

        this->CurrentState = State::Running;
        try {
            this->UpdateCycle();
        }
        catch (BlackRoot::Debug::Exception * e) {
            this->HandleThreadException(e);
        }

        cout{} << "Ending FileChangeMontor thread." << std::endl;
        
        try {
            std::unique_lock<std::mutex> lock(this->MutexAccessFiles);
            this->SaveToPersistent();
        }
        catch (BlackRoot::Debug::Exception * e) {
            this->HandleThreadException(e);
        }

        this->CurrentState = State::Stopped;
    });
}

void FileChangeMonitor::EndAndWait()
{
    this->TargetState = State::Stopped;
    this->UpdateThread.join();
}

void FileChangeMonitor::SetPersistentDirectory(const BlackRoot::IO::FilePath path)
{
    this->PersistentDirectory = fs::canonical(path);
}

void FileChangeMonitor::SetReferenceDirectory(const BlackRoot::IO::FilePath path)
{
    this->InfoReferenceDirectory = fs::canonical(path);
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

void FileChangeMonitor::SetWrangler(IWrangler * wrangler)
{
    DbAssert(this->IsStopped());

    this->Wrangler = wrangler;
}

    //  Process
    // --------------------

void ProcessProperties::SetDefault()
{
    this->StringVariables.clear();
}

void ProcessProperties::AdaptVariables(const JSON json)
{
    DbAssert(json.is_array());

    for (auto & elem : json) {
        for (auto & kv : elem.items()) {
            this->StringVariables[this->ProcessString(kv.key())] = this->ProcessString(kv.value());
        }
    }
}

std::string ProcessProperties::ProcessString(std::string str)
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

void ProcessProperties::ProcessJSONRecursively(JSON * json)
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

bool ProcessProperties::Equals(const ProcessProperties rh)
{
    if (this->StringVariables != rh.StringVariables)
        return false;
    return true;
}

    //  Items
    // --------------------

void MonitoredPath::SetDefault()
{
    this->Path       = "";

    this->LastUpdate = std::chrono::time_point<std::chrono::system_clock>{};
    this->Timeout    = std::chrono::system_clock::now();
}

void MonitoredWildcard::SetDefault()
{
    this->Check.RemoveFound();
    this->Check.SetCheckPath("");
    this->Check.SetWildcard("*");
    this->Check.SetDelimiters("~", "~");
}

void HubProperties::SetDefault()
{
    this->PathDependencies.resize(0);

    this->Path          = "";
    this->HubDependency = Monitor::InternalIDNone;

    this->Timeout    = std::chrono::system_clock::now();

    this->InputProcessProp.SetDefault();
}

bool HubProperties::EqualsAbstractly(const HubProperties rh)
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

void PipeProperties::SetDefault()
{
    this->PathDependencies.resize(0);
    
    this->Tool          = "";
    this->BasePathIn    = "";
    this->BasePathOut   = "";

    this->HubDependency = Monitor::InternalIDNone;
    this->Timeout       = std::chrono::system_clock::now();

    this->Settings      = {};
}

bool PipeProperties::EqualsAbstractly(const PipeProperties rh)
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

void PipeWildcards::SetDefault()
{
    this->Tool          = "";
    this->BasePathIn    = "";
    this->BasePathOut   = "";
    
    this->InputProcessProp.SetDefault();

    this->HubDependency       = Monitor::InternalIDNone;
    this->WildcardDependency  = Monitor::InternalIDNone;

    this->Settings      = {};
}

bool PipeWildcards::EqualsAbstractly(const PipeWildcards rh)
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
    if (this->WildcardDependency != Monitor::InternalIDNone &&
        this->WildcardDependency != rh.WildcardDependency)
        return false;
    if (!(this->Settings == rh.Settings))
        return false;
    return true;
}