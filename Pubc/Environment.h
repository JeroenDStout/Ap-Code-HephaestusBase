/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "ToolboxBase/Pubc/Base Environment.h"
#include "HephaestusBase/Pubc/Interface Pipeline.h"

namespace Hephaestus {
namespace Core {

	class Environment : public Toolbox::Base::BaseEnvironment {
        CON_RMR_DECLARE_CLASS(Environment, Toolbox::Base::BaseEnvironment);

    protected:
        Core::IPipeline * Pipeline;


            // Typed
        
        virtual Core::IPipeline * internal_allocate_pipeline();
    public:
        Environment();
        ~Environment() override;
        
            // Control
        
        void create_pipeline();
        void internal_unload_all() override;

            // Util

        void internal_compile_stats(JSON &) override;

            // Message
        
        CON_RMR_DECLARE_FUNC(create_pipeline);
	};

}
}