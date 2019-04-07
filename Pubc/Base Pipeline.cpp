/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Exception.h"

#include "ToolboxBase/Pubc/Base Environment.h"

#include "HephaestusBase/Pubc/Base Pipeline.h"

using namespace Hephaestus::Base;

TB_MESSAGES_BEGIN_DEFINE(Pipeline);

TB_MESSAGES_ENUM_BEGIN_MEMBER_FUNCTIONS(Pipeline);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(Pipeline, setBaseHubPath);
TB_MESSAGES_ENUM_END_MEMBER_FUNCTIONS(Pipeline);

TB_MESSAGES_END_DEFINE(Pipeline);

void Pipeline::Initialise(const BlackRoot::Format::JSON param)
{
}

void Pipeline::Deinitialise(const BlackRoot::Format::JSON param)
{
}

void Pipeline::SetBaseHubPath(const std::string str)
{
    using cout = BlackRoot::Util::Cout;

    cout{} << "Pip setting base hub path to " << std::endl << " " << str << std::endl;
}

void Pipeline::_setBaseHubPath(Toolbox::Messaging::IAsynchMessage * msg)
{
    std::string s = msg->Message.begin().value();

    this->SetBaseHubPath(s);

    msg->Response = { "OK" };
    msg->SetOK();
}