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
#include "BlackRoot/Pubc/Threaded Caller.h"

#include "HephaestusBase/Pubc/Pipeline Meta.h"
#include "HephaestusBase/Pubc/Pipe Tool.h"

#include <shared_mutex>

namespace Hephaestus {
namespace Pipeline {
namespace Wrangler {

    class PipeWrangler : public Pipeline::IWrangler {
    public:
        using ToolMap = std::map<std::string, const DynLib::IPipeTool*>;
        
    protected:
        struct Task {
            Pipeline::WranglerTask  *OriginTask;
        };

        BlackRoot::Util::ThreadedCaller     Caller;

        int      MaxThreadCount;

        std::shared_mutex   MxTools;
        ToolMap             Tools;
        
        std::mutex          MxTasks;
        std::vector<Task>   Tasks;

    public:
        PipeWrangler();
        ~PipeWrangler();

        void    ThreadedCall();
        
        void    AsynchReceiveTasks(const WranglerTaskList&) override;

        void    Begin();
        void    EndAndWait();

        void    RegisterTool(const DynLib::IPipeTool*);
        const DynLib::IPipeTool * FindTool(std::string);

        std::string GetAvailableTools();
    };

}
}
}