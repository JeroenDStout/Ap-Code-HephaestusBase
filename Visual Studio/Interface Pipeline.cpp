/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "HephaestusBase/Pubc/Interface Pipeline.h"

using namespace Hephaestus::Core;

TB_MESSAGES_BEGIN_DEFINE(IPipeline);

TB_MESSAGES_ENUM_BEGIN_MEMBER_FUNCTIONS(IPipeline);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(IPipeline, startProcessing);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(IPipeline, stopProcessing);
TB_MESSAGES_ENUM_END_MEMBER_FUNCTIONS(IPipeline);

TB_MESSAGES_END_DEFINE(IPipeline);

void IPipeline::_startProcessing(Toolbox::Messaging::IAsynchMessage * message)
{
    this->StartProcessing();

    message->SetOK();
}

void IPipeline::_stopProcessing(Toolbox::Messaging::IAsynchMessage * message)
{
    this->StopProcessing();

    message->SetOK();
}