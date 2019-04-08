/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 *   Quality of this code: sketch
 */

#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <map>

#include "BlackRoot/Pubc/Number Types.h"
#include "BlackRoot/Pubc/Files Types.h"

namespace Hephaestus {
namespace Pipeline {

    namespace Monitor {

        using InternalID = uint32;
        static const InternalID InternalIDNone = std::numeric_limits<InternalID>::max();

        using TimePoint  = std::chrono::system_clock::time_point;

        struct HubFile {
        };

        struct IMonitoredItem {
            InternalID                  ID;

            std::vector<InternalID>     ExistentialDependanciesHub;

            BlackRoot::IO::FilePath     BasePath;
            BlackRoot::IO::FileTime     LastChange;

            TimePoint                    Timeout;

            void    SetDefault();
        };

        template<typename type>
        struct MonitoredItem : public IMonitoredItem {
            using ElementType = type;

            ElementType     *Element;
        };
    }

    class FileChangeMonitor {
    protected:
        using InternalID  = Monitor::InternalID;
        using MonHubFile  = Monitor::MonitoredItem<Monitor::HubFile>;

        struct State {
            using Type = uint8_t;
            enum {
                Stopped,
                Starting,
                Running
            };
        };

        std::unique_ptr<BlackRoot::IO::IFileSource> FileSource;
        
        std::atomic<InternalID>             NextID;

        std::atomic<State::Type>            CurrentState, TargetState;
        std::thread                         UpdateThread;

        std::map<InternalID, MonHubFile>    HubFiles;
        std::vector<InternalID>             DirtyHubFiles;

        std::mutex                          MutexAccessFiles;

    public:
        FileChangeMonitor();
        ~FileChangeMonitor();

        void    InternalUpdateCycle();
        void    InternalHandleThreadException(BlackRoot::Debug::Exception *);

        void    InternalUpdateDirtyFiles();

        void    InternalRemoveAllHubFiles();

        bool    InternalShouldInterrupt();

        InternalID    InternalAddHubFile(const BlackRoot::IO::FilePath);
        void          InternalUpdateHubFile(InternalID);

        std::string   SimpleFormatDuration(long long);

        void    UpdateBaseHubFile(const BlackRoot::IO::FilePath);

        void    Begin();
        void    EndAndWait();
    };

}
}