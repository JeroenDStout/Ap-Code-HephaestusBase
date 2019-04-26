/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"

#include "HephaestusBase/Pubc/Environment.h"
#include "HephaestusBase/Pubc/Base Pipeline.h"

using namespace Hephaestus::Core;

    //  Relay message receiver
    // --------------------

CON_RMR_DEFINE_CLASS(Environment);
CON_RMR_REGISTER_FUNC(Environment, create_pipeline);

    //  Setup
    // --------------------

Environment::Environment()
{
    this->Pipeline = nullptr;
}

Environment::~Environment()
{
}

    //  Control
    // --------------------

void Environment::create_pipeline()
{
    using namespace std::placeholders;

    this->Pipeline = this->internal_allocate_pipeline();
    this->Pipeline->initialise({});
    
    this->Simple_Relay.Call_Map["pipe"] = std::bind(&Core::IPipeline::rmr_handle_message_immediate_and_release, this->Pipeline, _1);
}

void Environment::internal_unload_all()
{
    this->RelayReceiverBaseClass::internal_unload_all();

    if (this->Pipeline) {
        this->Pipeline->deinitialise({});
    }

    this->Simple_Relay.Call_Map.erase("pipe");
}

    //  Util
    // --------------------

void Environment::internal_compile_stats(JSON & json)
{
    this->RelayReceiverBaseClass::internal_compile_stats(json);

    BlackRoot::Format::JSON & Managers = json["Managers"];
    Managers["Pipeline"]   = this->Pipeline    ? "Loaded" : "Not Loaded";
}

    //  Typed
    // --------------------

IPipeline * Environment::internal_allocate_pipeline()
{
    return new Base::Pipeline;
}

    //  Messages
    // --------------------

void Environment::_create_pipeline(Conduits::Raw::IRelayMessage * msg) noexcept
{
    this->savvy_try_wrap(msg, [&] {
        DbAssertMsgFatal(!this->Pipeline, "Pipeline already exists");
        this->create_pipeline();
        msg->set_OK();
    });
}