/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* {quality} This is very much a sketch
 */

#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <map>

#include "BlackRoot/Pubc/Number Types.h"
#include "BlackRoot/Pubc/Files Types.h"
#include "BlackRoot/Pubc/JSON.h"

#include "HephaestusBase/Pubc/Pipeline Meta.h"

namespace Hephaestus {
namespace Pipeline {
namespace Monitor {

    using InternalID = uint32;
    static const InternalID InternalIDNone = std::numeric_limits<InternalID>::max();

    using TimePoint       = std::chrono::system_clock::time_point;
    using JSON            = BlackRoot::Format::JSON;
    using Path            = BlackRoot::IO::FilePath;
    using InternalIDList  = std::vector<InternalID>;

    struct ProcessProperties {
        std::unordered_map<std::string, std::string>    StringVariables;

        void    SetDefault();
        bool    Equals(const ProcessProperties);

        void         AdaptVariables(const JSON);
        std::string  ProcessString(std::string);
        void         ProcessJSONRecursively(JSON *);
    };

    struct MonitoredPath {
        Path                Path;
        TimePoint           LastUpdate, Timeout;

        void    SetDefault();
    };

    struct HubProperties {
        InternalIDList      PathDependencies;

        InternalID          HubDependency;
        Path                Path;
        TimePoint           Timeout;

        ProcessProperties   InputProcessProp;

        void    SetDefault();
        bool    EqualsAbstractly(const HubProperties);
    };

    struct PipeProperties {
        InternalIDList      PathDependencies;
            
        InternalID          HubDependency;
        std::string         Tool;
        Path                BasePathIn, BasePathOut;
            
        TimePoint           Timeout;

        JSON                Settings;

        void    SetDefault();
        bool    EqualsAbstractly(const PipeProperties);
    };

    class FileChangeMonitor {
    protected:
        using InternalID      = Monitor::InternalID;
        using MonPath         = Monitor::MonitoredPath;
        using HubProp         = Monitor::HubProperties;
        using PipeProp        = Monitor::PipeProperties;
        using Count           = InternalID;

        struct State {
            using Type = uint8_t;
            enum {
                Stopped,
                Starting,
                Running
            };
        };

        std::unique_ptr<BlackRoot::IO::IFileSource> FileSource;
        
        std::atomic<InternalID>               NextID;

        std::atomic<State::Type>              CurrentState, TargetState;
        std::thread                           UpdateThread;

        std::map<InternalID, MonPath>         MonitoredPaths;
        std::map<InternalID, HubProp>         HubProperties;
        std::map<InternalID, PipeProp>        PipeProperties;

        std::vector<InternalID>               SuspectPaths, FutureSuspectPaths;
        std::vector<InternalID>               DirtyHubs, FutureDirtyHubs, PotentiallyOrphanedHubs, OrphanedDirtyHubs;
        std::vector<InternalID>               DirtyPipes, FutureDirtyPipes, OrphanedDirtyPipes;
        std::vector<InternalID>               OutboxPipes, PendingPipes, InboxPipes;

        std::mutex                            MutexAccessFiles;

        InternalID                            OriginalHubDependancy;
        
        std::atomic<uint32>                   WranglerResultCount;
        std::mutex                            MxWranglerResults;
        std::vector<WranglerTaskResult>       WranglerResults;

        Pipeline::IWrangler                   *Wrangler;

        bool                                  PendingSaveChanges;
        
        Monitor::Path                         PersistentDirectory;
        Monitor::Path                         InfoReferenceDirectory;
        
        void    UpdateCycle();
        void    UpdateSuspectPaths();
        void    UpdateSuspectPath(InternalID);
        void    UpdateDirtyHubs();
        void    UpdateDirtyHub(InternalID);
        void    UpdateDirtyPipes();
        void    UpdateDirtyPipe(InternalID);
        void    UpdatePipeOutbox();
        void    UpdatePipeInbox();

        void    CleanupOrphanedHubs();
        void    CleanupOrphanedPipes();

        void    ProcessHubGroup(InternalID, const Monitor::ProcessProperties prop, Monitor::JSON group);

        void    HandleThreadException(BlackRoot::Debug::Exception *);

        bool    ShouldInterrupt();
        Count   GetActiveDirtyHubCount();
        Count   GetActiveDirtyPipeCount();

        InternalID    GetNewID();
        
        InternalID    FindOrAddMonitoredPath(Monitor::Path, Monitor::TimePoint * prevUpdate = nullptr);
        InternalID    FindOrAddHub(HubProp);
        InternalID    FindOrAddPipe(PipeProp);

        void     MakeUsersOfPathDirty(InternalID);
        void     MakeDependantsOnHubOrphan(InternalID);

        void     HandleMonitoredPathMissing(InternalID);
        void     HandleMonitoredPathError(InternalID, BlackRoot::Debug::Exception*);
        void     HandleHubFileError(InternalID, BlackRoot::Debug::Exception*);
        void     HandleWrangledPipeError(InternalID, BlackRoot::Debug::Exception*);

        std::string   SimpleFormatHub(HubProp);
        std::string   SimpleFormatPipe(PipeProp);
        Monitor::Path SimpleFormatPath(Monitor::Path);
        
        void    LoadFromPersistent();
        void    SaveToPersistent();

        bool    FileTimeEqualsWithEpsilon(TimePoint, TimePoint);
        
        void    AsynchReceiveTaskResult(const WranglerTaskResult&);

    public:
        FileChangeMonitor();
        ~FileChangeMonitor();

        std::string   SimpleFormatDuration(long long);
        bool    PathContainsWildcards(const std::string);
        
        void    SetPersistentDirectory(const BlackRoot::IO::FilePath);
        void    SetReferenceDirectory(const BlackRoot::IO::FilePath);

        void    AddBaseHubFile(const BlackRoot::IO::FilePath);

        void    SetWrangler(Pipeline::IWrangler*);

        void    Begin();
        void    EndAndWait();

        bool    IsStopped();
    };

}
}
}