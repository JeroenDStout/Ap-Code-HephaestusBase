/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 *   Quality of this code: sketch
 */

#pragma once

#include "HephaestusBase/Pubc/Interface Pipeline.h"

namespace Hephaestus {
namespace Base {

	class Pipeline : public Hephaestus::Core::IPipeline {
        TB_MESSAGES_DECLARE_RECEIVER(Pipeline, Hephaestus::Core::IPipeline);

	public:
        ~Pipeline() override { ; }

        void Initialise(const BlackRoot::Format::JSON param) override;
        void Deinitialise(const BlackRoot::Format::JSON param) override;

        void SetBaseHubPath(std::string);

        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(setBaseHubPath);
	};

}
}