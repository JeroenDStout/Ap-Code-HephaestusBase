#include "Toolbox/Base Environment.h"
#include "Hephaestus/Base Pipeline.h"

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