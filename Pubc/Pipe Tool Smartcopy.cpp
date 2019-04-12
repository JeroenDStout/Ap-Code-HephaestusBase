/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Threaded IO Stream.h"

#include "HephaestusBase/Pubc/Pipe Tool.h"
#include "HephaestusBase/Pubc/Pipe Tool Register.h"

namespace Hephaestus {
namespace Pipeline {
namespace Tools {

    class SmartCopy : public IPipeTool {
    public:
        SmartCopy() : IPipeTool("smartcopy") { ; }

        void Run(PipeToolInstr &) const override;
    };
    
    void SmartCopy::Run(PipeToolInstr &) const
    {
        using cout = BlackRoot::Util::Cout;

        cout{} << "Dummy SmartCopy" << std::endl;
    }

    HE_PIPE_DEFINE(SmartCopy);

}
}
}