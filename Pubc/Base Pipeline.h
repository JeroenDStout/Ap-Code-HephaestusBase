#pragma once

#include "Hephaestus/Interface Pipeline.h"

namespace Hephaestus {
namespace Base {

	class Pipeline : public Hephaestus::Core::IPipeline {
        TB_MESSAGES_DECLARE_RECEIVER(Pipeline, Hephaestus::Core::IPipeline);

	public:
        ~Pipeline() override { ; }

        void Initialise(const BlackRoot::Format::JSON param) override;
        void Deinitialise(const BlackRoot::Format::JSON param) override;
	};

}
}