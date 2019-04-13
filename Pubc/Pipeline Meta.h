/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* {quality} This is very much a sketch
 */

#pragma once

#include <functional>
#include <vector>

#include "BlackRoot/Pubc/Exception.h"
#include "BlackRoot/Pubc/JSON.h"
#include "BlackRoot/Pubc/Files Types.h"

namespace Hephaestus {
namespace Pipeline {
    
    using ID    = std::size_t;

    struct WranglerTaskResult {
        using Path      = BlackRoot::IO::FilePath;
        using JSON      = BlackRoot::Format::JSON;
        using Time      = BlackRoot::IO::FileTime;
        using Duration  = std::chrono::milliseconds;

        std::size_t  UniqueID;
        Duration     ProcessDuration;

        BlackRoot::Debug::Exception * Exception;
        
        struct ReadFile {
            Path  Path;
            Time  LastChange;
        };
        struct WrittenFile {
            Path  Path;
        };
        std::vector<ReadFile>    ReadFiles;
        std::vector<WrittenFile> WrittenFiles;
    };

    struct WranglerTask {
        using Path  = BlackRoot::IO::FilePath;
        using JSON  = BlackRoot::Format::JSON;

        std::size_t  UniqueID;
        
        std::string  ToolName;
        Path         FileIn, FileOut;

        JSON         Settings;

        std::function<void(const WranglerTaskResult)> Callback;
    };

    using WranglerTaskList = std::vector<WranglerTask>;

    class IWrangler {
    public:
        virtual ~IWrangler() { ; }

        virtual void AsynchReceiveTasks(const WranglerTaskList&) = 0;
    };

}
}