#include "HephaestusBase/Pubc/Environment.h"
#include "HephaestusBase/Pubc/Base Pipeline.h"

using namespace Hephaestus::Core;

TB_MESSAGES_BEGIN_DEFINE(Environment);

TB_MESSAGES_ENUM_BEGIN_MEMBER_FUNCTIONS(Environment);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(Environment, createPipeline);
TB_MESSAGES_ENUM_END_MEMBER_FUNCTIONS(Environment);

TB_MESSAGES_END_DEFINE(Environment);

Environment::Environment()
{
    this->Pipeline = nullptr;
}

Environment::~Environment()
{
}

void Environment::UnloadAll()
{
    this->MessengerBaseClass::UnloadAll();

    if (this->Pipeline) {
        this->Pipeline->Deinitialise(nullptr);
    }
}

void Environment::InternalSetupRelayMap()
{
    this->BaseEnvironment::InternalSetupRelayMap();
    
    this->MessageRelay.Emplace("pip", this, &BaseEnvironment::InternalMessageRelayToNone, this, &BaseEnvironment::InternalMessageSendToNone);
}

void Environment::InternalCompileStats(BlackRoot::Format::JSON & json)
{
    this->MessengerBaseClass::InternalCompileStats(json);

    BlackRoot::Format::JSON & Managers = json["Managers"];
    Managers["Pipeline"]   = this->Pipeline    ? "Loaded" : "Not Loaded";
}

IPipeline * Environment::AllocatePipeline()
{
    return new Hephaestus::Base::Pipeline();
}

void Environment::CreatePipeline()
{
    Toolbox::Messaging::JSON param;

    this->Pipeline = this->AllocatePipeline();
    this->Pipeline->Initialise( param );
    
    this->MessageRelay.Emplace("pip", this, &BaseEnvironment::InternalMessageRelayToNone, this, &Environment::InternalMessageSendToPip);
}

void Environment::InternalMessageSendToPip(std::string, Toolbox::Messaging::IAsynchMessage *message)
{
    this->Pipeline->MessageReceiveImmediate(message);
}

void Environment::_createPipeline(Toolbox::Messaging::IAsynchMessage * msg)
{
    if (this->Pipeline) {
        msg->Response = { "Pipeline already exists" };
        msg->SetFailed();
        return;
    }

    msg->Response = { "Creating Pipeline" };
    msg->SetOK();

    this->CreatePipeline();
}