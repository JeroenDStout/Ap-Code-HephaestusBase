/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* {quality} This is very much a sketch
 */

#pragma once

#include "BlackRoot/Pubc/JSON.h"
#include "BlackRoot/Pubc/Number Types.h"
#include "BlackRoot/Pubc/Files.h"

namespace Hephaestus {
namespace Pipeline {

        // Writing a pipe tool, these the instructions are given to you in the
        // 'run' function, which the tool can (and often must) modify in return
    struct PipeToolInstr {
        using Path = BlackRoot::IO::FilePath;
        using JSON = BlackRoot::Format::JSON;
        using Time = BlackRoot::IO::FileTime;
            
        Path     FileIn, FileOut;
        JSON     Settings;

        struct ReadFile {
            Path  Path;
            Time  LastChange;
        };
        struct WrittenFile {
            Path  Path;
        };
        std::vector<ReadFile>    ReadFiles;
        std::vector<WrittenFile> WrittenFiles;

        void SetDefault();
    };

        // Because pipe tools can be loaded across DLL boundaries, we have to
        // use a C-like interface which we translate to and from. The final
        // pipe tool can remain ignorant of this part.
    namespace DynLib {
        struct PipeToolInstr {
            const char *FileIn, *FileOut;
            const char *Settings;
            const char *Exception;

            struct ReadFile {
                uint64     LastChange;
                const char *Path;
            };
            uint32   ReadFileCount;
            ReadFile *ReadFiles;

            struct WrittenFile {
                const char *Path;
            };
            uint32   WrittenFileCount;
            WrittenFile *WrittenFiles;
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

        // Inherit from this class to make a pipe tool and overload 'Run'
    class IPipeTool : public DynLib::IPipeTool {
    protected:
        std::string     Name;

    public:
        IPipeTool(std::string name);

        const char * GetToolName() const noexcept override;

        void InternalRun(DynLib::PipeToolInstr &) const noexcept final override;
        void InternalCleanup(DynLib::PipeToolInstr &) const noexcept final override;
            
        virtual void Run(PipeToolInstr &) const = 0;
    };

}
}