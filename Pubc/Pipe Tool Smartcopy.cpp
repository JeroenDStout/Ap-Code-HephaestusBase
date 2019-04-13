/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Files.h"
#include "BlackRoot/Pubc/FileSource Snooper.h"

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
    
    void SmartCopy::Run(PipeToolInstr & instr) const
    {
        using cout       = BlackRoot::Util::Cout;
        using FileSource = BlackRoot::Util::FileSourceSnooper<BlackRoot::IO::BaseFileSource>;
        using Time       = BlackRoot::IO::FileTime;

            // Use FileSourceSnooper to log reads
        FileSource fs;

            // Ensure directory and copy file
        fs.CreateDirectories(instr.FileOut.parent_path());
        if (fs.Exists(instr.FileOut)) {
            fs.Remove(instr.FileOut);
        }
        fs.CopyFile(instr.FileIn, instr.FileOut);

            // Book-keep paths we used
        for (const auto & it : fs.GetList()) {
            if (it.WriteAccess) {
                instr.WrittenFiles.push_back({ it.Path });
            }
            else {
                instr.ReadFiles.push_back({ it.Path, it.PreviousLastWriteTime });
            }
        }
    }

    HE_PIPE_DEFINE(SmartCopy);

}
}
}