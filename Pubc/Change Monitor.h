/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <thread>
#include <atomic>

namespace Hephaestus {
namespace Pipeline {

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

    public:
        ChangeMonitor();
        ~ChangeMonitor();

        void    InternalUpdateCycle();
        void    InternalHandleThreadException(BlackRoot::Debug::Exception *);

        void    Begin();
        void    EndAndWait();
    };

}
}