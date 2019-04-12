/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 *   Quality of this code: sketch
 */

#pragma once

#include "ToolboxBase/Pubc/Base Messages.h"

#include "HephaestusBase/Pubc/Interface Pipeline.h"
#include "HephaestusBase/Pubc/File Change Monitor.h"
#include "HephaestusBase/Pubc/Pipe Wrangler.h"

namespace Hephaestus {
namespace Base {

	class Pipeline : public Hephaestus::Core::IPipeline {
        TB_MESSAGES_DECLARE_RECEIVER(Pipeline, Hephaestus::Core::IPipeline);

        using FileMonitor  = Hephaestus::Pipeline::Monitor::FileChangeMonitor;
        using PipeWrangler = Hephaestus::Pipeline::Wrangler::PipeWrangler;
    protected:

        struct __Pipeprops {
            bool        ProcessingActive;

            FileMonitor     Monitor;
            PipeWrangler    Wrangler;

        } PipeProps;

	public:
        ~Pipeline() override { ; }

        void Initialise(const BlackRoot::Format::JSON param) override;
        void Deinitialise(const BlackRoot::Format::JSON param) override;

        void AddBaseHubFile(const BlackRoot::IO::FilePath) override;
        
        void StartProcessing() override;
        void StopProcessing() override;

        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(setReferenceDirectory);
	};

}
}