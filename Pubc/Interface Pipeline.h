#pragma once

#include "BlackRoot\JSON.h"
#include "Toolbox\Base Messages.h"

namespace Hephaestus {
namespace Core {

	class IPipeline : public virtual Toolbox::Messaging::BaseMessageReceiver {
	public:
        virtual ~IPipeline() { ; }

        virtual void Initialise(const BlackRoot::Format::JSON) = 0;
        virtual void Deinitialise(const BlackRoot::Format::JSON) = 0;
	};

}
}   