/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <vector>

#include "BlackRoot/Pubc/Stringstream.h"

namespace Hephaestus {
namespace Pipeline {
    namespace DynLib {
        class IPipeTool;
    }

    using PipeToolList = std::vector<DynLib::IPipeTool*>;

    class PipeRegistry {
    protected:
        static PipeRegistry * Registry;

        PipeToolList    Pipes;
    public:
        static PipeRegistry * GetRegistry();
        
        static void AddPipe(DynLib::IPipeTool*);
        static const PipeToolList & GetPipeList();
    };

    namespace Helper {

        struct RegisterPipe {
            RegisterPipe(DynLib::IPipeTool * pipe) {
                PipeRegistry::AddPipe(pipe);
            }
        };

    };

#define HE_PIPEREG_HELPER(x, y) x ## y
#define HE_PIPEREG_HELPER2(x, y) HE_PIPEREG_HELPER(x, y)

#define HE_PIPE_DEFINE(x) \
    static x __PipeDec##x{}; \
    Hephaestus::Pipeline::Helper::RegisterPipe __RegisterPipe_##x(&__PipeDec##x);

    

}
}