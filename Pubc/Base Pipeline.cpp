/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Exception.h"
#include "BlackRoot/Pubc/Sys Path.h"

#include "ToolboxBase/Pubc/Base Environment.h"

#include "HephaestusBase/Pubc/Base Pipeline.h"
#include "HephaestusBase/Pubc/Pipe Tool Register.h"

using namespace Hephaestus::Base;
namespace fs = std::experimental::filesystem;

    //  Relay message receiver
    // --------------------

CON_RMR_DEFINE_CLASS(Pipeline);

CON_RMR_REGISTER_FUNC(Pipeline, set_reference_directory);
CON_RMR_REGISTER_FUNC(Pipeline, set_persistent_directory);
//CON_RMR_REGISTER_FUNC(Pipeline, http);

    //  Setup
    // --------------------

void Pipeline::initialise(const JSON param)
{
    auto env_ref_dir = Toolbox::Core::Get_Environment()->get_ref_dir();

    this->Pipe_Props.Monitor.SetReferenceDirectory(env_ref_dir / "../..");
    this->Pipe_Props.Monitor.SetPersistentDirectory(env_ref_dir / "../../.hep");
    this->Pipe_Props.Monitor.SetWrangler(&this->Pipe_Props.Wrangler);

    for (const auto & it : Hephaestus::Pipeline::PipeRegistry::GetPipeList()) {
        this->Pipe_Props.Wrangler.RegisterTool(it);
    }
}

void Pipeline::deinitialise(const JSON param)
{
}

void Pipeline::add_base_hub_file(const Path str)
{
    using cout      = BlackRoot::Util::Cout;
    using FilePath  = BlackRoot::IO::FilePath;
    
    auto base_dir = Toolbox::Core::Get_Environment()->get_ref_dir();
    base_dir = fs::canonical(base_dir / str);

    cout{} << "Pipeline adding hub file " << std::endl << " " << base_dir << std::endl;

    this->Pipe_Props.Monitor.AddBaseHubFile(base_dir);
}

void Pipeline::start_processing()
{
    using cout = BlackRoot::Util::Cout;
    cout{} << "Available pipeline tools: " << std::endl << " " << this->Pipe_Props.Wrangler.GetAvailableTools() << std::endl;

    this->Pipe_Props.Monitor.Begin();
    this->Pipe_Props.Wrangler.Begin();
}

void Pipeline::stop_processing()
{
    this->Pipe_Props.Monitor.EndAndWait();
    this->Pipe_Props.Wrangler.EndAndWait();
}

void Pipeline::_set_persistent_directory(Conduits::Raw::IRelayMessage * msg) noexcept
{
    this->savvy_try_wrap_read_json(msg, 0, [&](JSON json) {
        auto dir = Toolbox::Core::Get_Environment()->get_ref_dir();

        if (json.is_object()) {
            json = json["path"];
        }

        DbAssertMsgFatal(json.is_string(), "Malformed JSON: cannot get path");

        this->Pipe_Props.Monitor.SetPersistentDirectory(dir / json.get<JSON::string_t>());
        msg->set_OK();
    });
}

void Pipeline::_set_reference_directory(Conduits::Raw::IRelayMessage * msg) noexcept
{
    this->savvy_try_wrap_read_json(msg, 0, [&](JSON json) {
        auto dir = Toolbox::Core::Get_Environment()->get_ref_dir();

        if (json.is_object()) {
            json = json["path"];
        }

        DbAssertMsgFatal(json.is_string(), "Malformed JSON: cannot get path");

        this->Pipe_Props.Monitor.SetReferenceDirectory(dir / json.get<JSON::string_t>());
        msg->set_OK();
    });
}
        
            // Http

void Pipeline::savvy_handle_http(const JSON httpRequest, JSON & httpReply, std::string & outBody)
{
    using JSON = BlackRoot::Format::JSON;

	std::stringstream ss;
        
	ss << "<!doctype html>" << std::endl
		<< "<html>" << std::endl
		<< " <head>" << std::endl
		<< "  <title>Pipeline</title>" << std::endl
		<< " </head>" << std::endl
		<< " <body>" << std::endl
		<< "  <h1>Pipeline</h1>" << std::endl
		<< "  <h1>" << this->internal_get_rmr_class_name() << " (base relay)</h1>" << std::endl
		<< "  <p>" << this->html_create_action_relay_string() << "</p>" << std::endl
		<< "  <p><b>Available pipeline tools:</b></p><p>" << this->Pipe_Props.Wrangler.GetAvailableTools() << "</p>" << std::endl
		<< "  <p><b>Hubs tracked:</b></p><p>";
        
    JSON info = this->Pipe_Props.Monitor.AsynchGetTrackedInformation();
    
    JSON hubs = info["hubs"];
    if (hubs.is_array()) {
        bool anyWritten = false;
        for (auto & it : hubs) {
            if (anyWritten) ss << "<br/>";
		    ss << it["path"].get<std::string>();
            anyWritten = true;
        }
    }

	ss	<< "  </p><p><b>Paths tracked:</b></p><p>";
    JSON paths = info["paths"];
    if (paths.is_array()) {
        bool anyWritten = false;
        for (auto & it : paths) {
            if (anyWritten) ss << "<br/>";
		    ss << it["path"].get<std::string>();
            anyWritten = true;
        }
    }

	ss	<< "  </p><p><b>Wildcards:</b></p><p>";
    JSON wild = info["wildcards"];
    if (wild.is_array()) {
        bool anyWritten = false;
        for (auto & it : wild) {
            if (anyWritten) ss << "<br/>";
		    ss << it["path"].get<std::string>();
            anyWritten = true;
        }
    }
        
    ss << "</p>" << std::endl
		<< " </body>" << std::endl
		<< "</html>";

    outBody = ss.str();
}