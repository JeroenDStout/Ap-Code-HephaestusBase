/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "HephaestusBase/Pubc/Pipe Tool Register.h"

using namespace Hephaestus::Pipeline;

PipeRegistry * PipeRegistry::Registry = nullptr;

PipeRegistry * PipeRegistry::GetRegistry()
{
    return Registry ? Registry : (Registry = new PipeRegistry);
}
        
void PipeRegistry::AddPipe(DynLib::IPipeTool * tool)
{
    GetRegistry()->Pipes.push_back(tool);
}

const PipeToolList & PipeRegistry::GetPipeList()
{
    return GetRegistry()->Pipes;
}