/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Exception.h"
#include "BlackRoot/Pubc/Sys Path.h"

#include "ToolboxBase/Pubc/Base Environment.h"

#include "HephaestusBase/Pubc/Base Pipeline.h"

using namespace Hephaestus::Base;
namespace fs = std::experimental::filesystem;

TB_MESSAGES_BEGIN_DEFINE(Pipeline);

TB_MESSAGES_ENUM_BEGIN_MEMBER_FUNCTIONS(Pipeline);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(Pipeline, setReferenceDirectory);
TB_MESSAGES_ENUM_END_MEMBER_FUNCTIONS(Pipeline);

TB_MESSAGES_END_DEFINE(Pipeline);

void Pipeline::Initialise(const BlackRoot::Format::JSON param)
{
    this->PipeProps.Monitor.SetReferenceDirectory(Toolbox::Core::GetEnvironment()->GetRefDir() / "../../");
}

void Pipeline::Deinitialise(const BlackRoot::Format::JSON param)
{
}

void Pipeline::AddBaseHubFile(BlackRoot::IO::FilePath str)
{
    using cout      = BlackRoot::Util::Cout;
    using FilePath  = BlackRoot::IO::FilePath;

    FilePath base = Toolbox::Core::GetEnvironment()->GetRefDir();
    base = fs::canonical(base / str);

    cout{} << "Pipeline adding hub file " << std::endl << " " << base << std::endl;

    this->PipeProps.Monitor.AddBaseHubFile(base);
}

void Pipeline::StartProcessing()
{
    this->PipeProps.Monitor.Begin();
}

void Pipeline::StopProcessing()
{
    this->PipeProps.Monitor.EndAndWait();
}

void Pipeline::_setReferenceDirectory(Toolbox::Messaging::IAsynchMessage * message)
{
    // Todo: msg safety

    std::string s = message->Message.begin().value();
    this->PipeProps.Monitor.SetReferenceDirectory(Toolbox::Core::GetEnvironment()->GetRefDir() / s);

    message->SetOK();
}