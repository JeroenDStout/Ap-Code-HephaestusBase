/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ToolboxBase/Pubc/Base Environment.h"
#include "HephaestusBase/Pubc/Base Pipeline.h"

using namespace Hephaestus::Base;

TB_MESSAGES_BEGIN_DEFINE(Pipeline);

TB_MESSAGES_ENUM_BEGIN_MEMBER_FUNCTIONS(Pipeline);
TB_MESSAGES_ENUM_END_MEMBER_FUNCTIONS(Pipeline);

TB_MESSAGES_END_DEFINE(Pipeline);

void Pipeline::Initialise(const BlackRoot::Format::JSON param)
{
}

void Pipeline::Deinitialise(const BlackRoot::Format::JSON param)
{
}