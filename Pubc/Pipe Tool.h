/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* {quality} This is very much a sketch
 */

#pragma once

#include "BlackRoot/Pubc/JSON.h"
#include "BlackRoot/Pubc/Number Types.h"
#include "BlackRoot/Pubc/Files Types.h"

namespace Hephaestus {
namespace Pipeline {

    struct PipeToolInstr {
        using Path = BlackRoot::IO::FilePath;
        using JSON = BlackRoot::Format::JSON;
        using Time = BlackRoot::IO::FileTime;
            
        Path     FileIn, FileOut;
        JSON     Settings;

        struct UsedFile {
            Path  Path;
            Time  LastChange;
        };

        std::vector<UsedFile>   UsedFiles;

        void SetDefault();
    };

    namespace DynLib {
        struct PipeToolInstr {
            const char *FileIn, *FileOut;
            const char *Settings;
            const char *Exception;

            struct UsedFile {
                const char *Path;
                uint64     LastChange;
            };
            int        UsedFileCount;
            UsedFile * UsedFiles;
        };

        class IPipeTool {
        protected:
            virtual void InternalRun(PipeToolInstr &) const noexcept = 0;
            virtual void InternalCleanup(PipeToolInstr &) const noexcept = 0;

        public:
            virtual const char * GetToolName() const noexcept = 0;
            
            void Run(Pipeline::PipeToolInstr &) const;
        };
    }

    class IPipeTool : public DynLib::IPipeTool {
    protected:
        std::string     Name;

    public:
        IPipeTool(std::string name);

        const char * GetToolName() const noexcept override;

        void InternalRun(DynLib::PipeToolInstr &) const noexcept override;
        void InternalCleanup(DynLib::PipeToolInstr &) const noexcept override;
            
        virtual void Run(PipeToolInstr &) const = 0;
    };

}
}