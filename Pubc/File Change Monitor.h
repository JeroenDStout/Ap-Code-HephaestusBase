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
#include "BlackRoot/Pubc/Files.h"

namespace Hephaestus {
namespace Pipeline {

    namespace Monitor {

        using InternalID = uint32;

        struct PathLink {
        };

        struct HubFile {
            InternalID                  ID;

            BlackRoot::IO::FilePath     Path;
            BlackRoot::IO::FileTime     LastChange;
        };

    }

    class ChangeMonitor {
    protected:
        struct State {
            using Type = uint8_t;
            enum {
                Stopped,
                Starting,
                Running
            };
        };

        std::atomic<State::Type>    CurrentState, TargetState;
        std::thread                 UpdateThread;

        std::map<Monitor::InternalID, Monitor::HubFile>    HubFiles;
        std::vector<Monitor::InternalID>                   DirtyHubFiles;

        std::mutex                  MutexAccessFiles;

    public:
        ChangeMonitor();
        ~ChangeMonitor();

        void    InternalUpdateCycle();
        void    InternalHandleThreadException(BlackRoot::Debug::Exception *);

        void    InternalUpdateDirtyFiles();

        void    Begin();
        void    EndAndWait();
    };

}
}