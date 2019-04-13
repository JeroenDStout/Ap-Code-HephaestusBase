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

    /* Dummy just does a simple copy and notes the change times of files
     */

    class Dummy : public IPipeTool {
    public:
        Dummy() : IPipeTool("dummy") { ; }

        void Run(PipeToolInstr &) const override;
    };
    
    void Dummy::Run(PipeToolInstr & instr) const
    {
        using cout       = BlackRoot::Util::Cout;
        using FileSource = BlackRoot::Util::FileSourceSnooper<BlackRoot::IO::BaseFileSource>;
        using Time       = BlackRoot::IO::FileTime;

            // Get an interface to open files with; we use FileSourceSnooper
            // which logs the opening of files. This will help us remember all
            // the files we use.
        FileSource fs;

            // Gather some data about the in- and out-file
        bool outExists = fs.Exists(instr.FileOut);

        Time lastWriteIn  = fs.LastWriteTime(instr.FileIn);
        Time lastWriteOut = outExists ? fs.LastWriteTime(instr.FileOut) : std::chrono::system_clock::time_point{};

            // Read the in file, to keep up appearances of being a real pipe
        auto file = fs.ReadFile(instr.FileIn, BlackRoot::IO::IFileSource::OpenInstr{}
                                                .Access(BlackRoot::IO::FileMode::Access::Read)
                                                .Share(BlackRoot::IO::FileMode::Share::Read));

            // Convert this to some strings so we can output it
            // (if you can make this elegant you are a better person than me)
        auto ttIn  = std::chrono::system_clock::to_time_t(lastWriteIn);
        auto ttOut = std::chrono::system_clock::to_time_t(lastWriteOut);

        char timeBufferIn[256];
        char timeBufferOut[256];
        
        auto tm = std::tm{0};
        localtime_s(&tm, &ttIn);
        asctime_s(timeBufferIn, sizeof(timeBufferIn), &tm);
        localtime_s(&tm, &ttOut);
        asctime_s(timeBufferOut, sizeof(timeBufferOut), &tm);

            // Write our nice output
        std::stringstream ss;
        ss << std::endl;
        ss << "~*~*~ dummy pipe tool ~*~*~" << std::endl;
        ss << " " << instr.FileIn << std::endl;
        ss << "  " << file.size() << " bytes, " << timeBufferIn;
        ss << " " << instr.FileOut << std::endl;
        if (outExists) {
            ss << "  " << timeBufferOut;
        }
        else {
            ss << "  (did not exist)" << std::endl;
        }
        ss << " Settings: " << instr.Settings.dump() << std::endl;
        ss << "~*~*~ this has been a dummy ~*~*~" << std::endl;
        
        cout{} << ss.str();
        
            // Check if the special string has been set to get this dummy to copy
        auto & val = instr.Settings["special"];
        if (val.is_string() && 0 == val.get<std::string>().compare("do it, you coward")) {
                // Ensure directory and copy file
            fs.CreateDirectories(instr.FileOut.parent_path());
            if (outExists) {
                fs.Remove(instr.FileOut);
            }
            fs.CopyFile(instr.FileIn, instr.FileOut);
        }

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

    HE_PIPE_DEFINE(Dummy);

}
}
}