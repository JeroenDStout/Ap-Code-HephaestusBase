/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot/Pubc/JSON.h"
#include "BlackRoot/Pubc/Files Types.h"

#include "Conduits/Pubc/Savvy Relay Receiver.h"

namespace Hephaestus {
namespace Core {

	class IPipeline : public Conduits::SavvyRelayMessageReceiver {
        CON_RMR_DECLARE_CLASS(IPipeline, SavvyRelayMessageReceiver);

        using JSON = BlackRoot::Format::JSON;
        using Path = BlackRoot::IO::FilePath;

	public:
        virtual ~IPipeline() { ; }

        virtual void initialise(const JSON) = 0;
        virtual void deinitialise(const JSON) = 0;

        virtual void add_base_hub_file(const Path) = 0;

        virtual void start_processing() = 0;
        virtual void stop_processing() = 0;
        
        CON_RMR_DECLARE_FUNC(start_processing);
        CON_RMR_DECLARE_FUNC(stop_processing);
        CON_RMR_DECLARE_FUNC(add_base_hub_file);
	};

}
}   