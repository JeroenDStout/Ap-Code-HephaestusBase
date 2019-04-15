/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"

#include "HephaestusBase/Pubc/Pipe Tool.h"

using namespace Hephaestus;
using namespace Hephaestus::Pipeline;

    //  Exe functions
    // --------------------

 void DynLib::IPipeTool::Run(Pipeline::PipeToolInstr & _instr) const
 {
        // Get string version of settings
     std::string settings = _instr.Settings.dump();
     std::string fileIn   = _instr.FileIn.string();
     std::string fileOut  = _instr.FileOut.string();

        // Create the instr in C-style for passing the lib border
     DynLib::PipeToolInstr instr;
     instr.FileIn           = fileIn.c_str();
     instr.FileOut          = fileOut.c_str();
     instr.Settings         = settings.c_str();
     instr.ReadFiles        = nullptr;
     instr.WrittenFiles     = nullptr;
     instr.ReadFileCount    = 0;
     instr.WrittenFileCount = 0;
     instr.Exception        = nullptr;

        // Call a function which converts back and runs; the
        // virtual pointer calls across to the dynlib side
     this->InternalRun(instr);  

        // Check for exception
     if (instr.Exception) {
         auto * e = new BlackRoot::Debug::Exception(instr.Exception, BRGenDbgInfo);
         this->InternalCleanup(instr);
         throw e;
     }

     auto clock = std::chrono::system_clock::time_point{};

        // Translate the C-style version back into C++
     _instr.ReadFiles.resize(instr.ReadFileCount);
     for (uint32 i = 0; i < instr.ReadFileCount; i++) {
         _instr.ReadFiles[i].Path       = instr.ReadFiles[i].Path;
         _instr.ReadFiles[i].LastChange = clock + std::chrono::milliseconds(instr.ReadFiles[i].LastChange);
     }

     _instr.WrittenFiles.resize(instr.WrittenFileCount);
     for (uint32 i = 0; i < instr.WrittenFileCount; i++) {
         _instr.WrittenFiles[i].Path    = instr.WrittenFiles[i].Path;
     }

        // Clean up elements allocated by the lib
     this->InternalCleanup(instr);
 }

    //  Dynlib functions
    // --------------------
 
IPipeTool::IPipeTool(std::string name)
: Name(name)
{
}
 
const char * IPipeTool::GetToolName() const noexcept
{
    return this->Name.c_str();
}

void IPipeTool::InternalRun(DynLib::PipeToolInstr & _instr) const noexcept
{
       // We are across the border; create the instr in C++ style
    PipeToolInstr instr;
    instr.FileIn   = _instr.FileIn;
    instr.FileOut  = _instr.FileOut;
    instr.Settings = BlackRoot::Format::JSON::parse(_instr.Settings);

    try {
            // Run the conversion
        this->Run(instr);
    }
    catch (BlackRoot::Debug::Exception * e) {
        _instr.Exception = _strdup(e->what());
        return;
    }
    catch (std::exception e) {
        _instr.Exception = _strdup(e.what());
        return;
    }
    catch (...) {
        _instr.Exception = _strdup("Unknown exception!");
        return;
    }

    auto clock = std::chrono::system_clock::time_point{};

       // Translate the C++ into C style
    _instr.ReadFileCount = (uint32)instr.ReadFiles.size();
    _instr.ReadFiles = (DynLib::PipeToolInstr::ReadFile*)malloc(sizeof(DynLib::PipeToolInstr::ReadFile) * _instr.ReadFileCount);
    for (uint32 i = 0; i < _instr.ReadFileCount; i++) {
        auto & orFile = instr.ReadFiles[i];
        auto & cvFile = _instr.ReadFiles[i];
        cvFile.Path = _strdup(orFile.Path.u8string().c_str());
        cvFile.LastChange = std::chrono::duration_cast<std::chrono::milliseconds>(orFile.LastChange - clock).count();
    }
    _instr.WrittenFileCount = (uint32)instr.WrittenFiles.size();
    _instr.WrittenFiles = (DynLib::PipeToolInstr::WrittenFile*)malloc(sizeof(DynLib::PipeToolInstr::WrittenFile) * _instr.WrittenFileCount);
    for (uint32 i = 0; i < _instr.WrittenFileCount; i++) {
        auto & orFile = instr.WrittenFiles[i];
        auto & cvFile = _instr.WrittenFiles[i];
        cvFile.Path = _strdup(orFile.Path.u8string().c_str());
    }
}

void IPipeTool::InternalCleanup(DynLib::PipeToolInstr & instr) const noexcept
{
        // All of these are allocated on our side (the dynlib),
        // and all with c-style allocation
    free((void*)(instr.Exception));
    for (uint32 i = 0; i < instr.ReadFileCount; i++) {
        free((void*)(instr.ReadFiles[i].Path));
    }
    for (uint32 i = 0; i < instr.WrittenFileCount; i++) {
        free((void*)(instr.WrittenFiles[i].Path));
    }
    free((void*)(instr.ReadFiles));
    free((void*)(instr.WrittenFiles));
}

    //  Util
    // --------------------

void PipeToolInstr::SetDefault()
{
    this->Settings = {};
    this->ReadFiles.resize(0);
    this->WrittenFiles.resize(0);
}