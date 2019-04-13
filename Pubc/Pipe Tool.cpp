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
     instr.FileIn    = fileIn.c_str();
     instr.FileOut   = fileOut.c_str();
     instr.Settings  = settings.c_str();
     instr.Exception = nullptr;

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
     _instr.UsedFiles.resize(instr.UsedFileCount);
     for (int i = 0; i < instr.UsedFileCount; i++) {
         _instr.UsedFiles[i].Path       = instr.UsedFiles[i].Path;
         _instr.UsedFiles[i].LastChange = clock + std::chrono::milliseconds(instr.UsedFiles[i].LastChange);
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
    catch (std::exception * e) {
        _instr.Exception = _strdup(e->what());
        delete e;
        return;
    }
    catch (...) {
        _instr.Exception = _strdup("Unknown exception!");
        return;
    }

    auto clock = std::chrono::system_clock::time_point{};

       // Translate the C++ into C style
    _instr.UsedFileCount = (int)instr.UsedFiles.size();
    _instr.UsedFiles = (DynLib::PipeToolInstr::UsedFile*)malloc(sizeof(DynLib::PipeToolInstr::UsedFile) * _instr.UsedFileCount);
    for (int i = 0; i < _instr.UsedFileCount; i++) {
        auto & orFile = instr.UsedFiles[i];
        auto & cvFile = _instr.UsedFiles[i];
        cvFile.Path = _strdup(orFile.Path.u8string().c_str());
        cvFile.LastChange = std::chrono::duration_cast<std::chrono::milliseconds>(orFile.LastChange - clock).count();
    }
}

void IPipeTool::InternalCleanup(DynLib::PipeToolInstr & instr) const noexcept
{
    if (instr.Exception) {
        free((void*)(instr.Exception));
        return;
    }

    if (instr.UsedFileCount > 0) {
        for (int i = 0; i < instr.UsedFileCount; i++) {
            free((void*)(instr.UsedFiles[i].Path));
        }
        free((void*)(instr.UsedFiles));
    }
}

    //  Util
    // --------------------

void PipeToolInstr::SetDefault()
{
    this->Settings = {};
    this->UsedFiles.resize(0);
}