/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot/Pubc/JSON.h"
#include "ToolboxBase/Pubc/Base Messages.h"

namespace Hephaestus {
namespace Core {

	class IPipeline : public virtual Toolbox::Messaging::BaseMessageReceiver {
	public:
        virtual ~IPipeline() { ; }

        virtual void Initialise(const BlackRoot::Format::JSON) = 0;
        virtual void Deinitialise(const BlackRoot::Format::JSON) = 0;

        virtual void SetBaseHubPath(const std::string) = 0;
	};

}
}   