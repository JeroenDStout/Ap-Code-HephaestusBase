/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Sys Path.h"

#include "HephaestusBase/Pubc/Interface Pipeline.h"

using namespace Hephaestus::Core;

    //  Relay message receiver
    // --------------------

CON_RMR_DEFINE_CLASS(IPipeline);

CON_RMR_REGISTER_FUNC(IPipeline, start_processing);
CON_RMR_REGISTER_FUNC(IPipeline, stop_processing);
CON_RMR_REGISTER_FUNC(IPipeline, add_base_hub_file);

void IPipeline::_start_processing(Conduits::Raw::IMessage * msg) noexcept
{
    this->savvy_try_wrap(msg, [&]{
        this->start_processing();
        msg->set_OK();
    });
}

void IPipeline::_stop_processing(Conduits::Raw::IMessage * msg) noexcept
{
    this->savvy_try_wrap(msg, [&]{
        this->stop_processing();
        msg->set_OK();
    });
}

void IPipeline::_add_base_hub_file(Conduits::Raw::IMessage * msg) noexcept
{
    this->savvy_try_wrap(msg, [&]{
        JSON json = JSON::parse((char*)msg->get_message_segment(0).Data);
        this->add_base_hub_file(json["path"].get<std::string>());
        msg->set_OK();
    });
}