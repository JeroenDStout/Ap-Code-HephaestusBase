/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 *   Quality of this code: sketch
 */

#pragma once

#include "HephaestusBase/Pubc/Interface Pipeline.h"
#include "HephaestusBase/Pubc/File Change Monitor.h"
#include "HephaestusBase/Pubc/Pipe Wrangler.h"

namespace Hephaestus {
namespace Base {

	class Pipeline : public Hephaestus::Core::IPipeline {
        CON_RMR_DECLARE_CLASS(Pipeline, Hephaestus::Core::IPipeline);

        using FileMonitor  = Hephaestus::Pipeline::Monitor::FileChangeMonitor;
        using PipeWrangler = Hephaestus::Pipeline::Wrangler::PipeWrangler;
    protected:

        struct __PipeProps {
            bool            Processing_Active;

            FileMonitor     Monitor;
            PipeWrangler    Wrangler;

        } Pipe_Props;

	public:
        ~Pipeline() override { ; }
        
        void initialise(const JSON) override;
        void deinitialise(const JSON) override;

        void add_base_hub_file(const Path) override;

        void start_processing() override;
        void stop_processing() override;
        
            // Http

        void savvy_handle_http(const JSON httpRequest, JSON & httpReply, std::string & outBody);

        CON_RMR_DECLARE_FUNC(set_reference_directory);
        CON_RMR_DECLARE_FUNC(set_persistent_directory);
	};

}
}