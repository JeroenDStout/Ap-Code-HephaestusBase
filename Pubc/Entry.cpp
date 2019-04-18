/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>

#include "BlackRoot/Pubc/Math Types.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"

#include "ToolboxBase/Pubc/Entry.h"
#include "ToolboxBase/Pubc/Environment Bootstrap.h"

#include "HephaestusBase/Pubc/Version.h"
#include "HephaestusBase/Pubc/Environment.h"

#include "HephaestusBase/Pubc/Register.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <complex>

int Hep_Main(Toolbox::Util::EnvironmentBootstrap &bootstrap)
{
    using cout = BlackRoot::Util::Cout;
    
        // Introduce ourselves
    cout{} << BlackRoot::Repo::VersionRegistry::GetBootString() << std::endl << std::endl;

        // Create an environment and start its thread
	Hephaestus::Core::Environment * environment = new Hephaestus::Core::Environment();
	Toolbox::Core::Set_Environment(environment);
	std::thread t1([=]{
        environment->run_with_current_thread();
    });
    
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bootstrap.setup_environment(environment);

    if (!bootstrap.execute_from_boot_file()) {
        cout{} << "Default start-up" << std::endl;

        if (!bootstrap.execute_from_json( R"(
                { "serious" : [ { "env / create_socketman" : {} },
                                { "env / create_pipeline"  : {} }
                              ]
                } )"_json ))
        {
            cout{} << "Cannot recover from startup errors" << std::endl;
            goto End;
        }
    }

End:
    t1.join();

    delete environment;

    return 0;
}

TB_STARTFUNC_DEF(Hep_Main);