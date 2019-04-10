/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot/Pubc/JSON.h"
#include "BlackRoot/Pubc/Files Types.h"

#include "ToolboxBase/Pubc/Base Messages.h"

namespace Hephaestus {
namespace Core {

	class IPipeline : public virtual Toolbox::Messaging::BaseMessageReceiver {
        TB_MESSAGES_DECLARE_RECEIVER(IPipeline,  Toolbox::Messaging::BaseMessageReceiver);

	public:
        virtual ~IPipeline() { ; }

        virtual void Initialise(const BlackRoot::Format::JSON) = 0;
        virtual void Deinitialise(const BlackRoot::Format::JSON) = 0;

        virtual void AddBaseHubFile(const BlackRoot::IO::FilePath) = 0;

        virtual void StartProcessing() = 0;
        virtual void StopProcessing() = 0;

        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(startProcessing);
        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(stopProcessing);
        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(addBaseHubFile);
	};

}
}   