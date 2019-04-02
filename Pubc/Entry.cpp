#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>

#include "BlackRoot/Pubc/Math Types.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"

#include "ToolboxBase/Pubc/Base Messages.h"
#include "ToolboxBase/Pubc/Environment Bootstrap.h"

#include "HephaestusBase/Pubc/Version.h"
#include "HephaestusBase/Pubc/Environment.h"

#include "HephaestusBase/Pubc/Register.h"

void envTask(Hephaestus::Core::Environment * env) {
	env->Run();
}

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <complex>

int main(int argc, char* argv[])
{
    using cout = BlackRoot::Util::Cout;

    cout{} << BlackRoot::Repo::VersionRegistry::GetVersionString() << std::endl << std::endl;

	Hephaestus::Core::Environment * environment = new Hephaestus::Core::Environment();
	Toolbox::Core::SetEnvironment(environment);
	std::thread t1(envTask, environment);
    
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Toolbox::Util::EnvironmentBootstrap bootstrap(environment);
    if (!bootstrap.HasStartupFile()) {
        cout{} << "Default start-up" << std::endl;

        if (!bootstrap.ExecuteFromString( R"(
            { "serious" : [{ "env" : [ "createSocketMan" ] },
                           { "env" : [ "createPipeline" ] }
                          ]
            } )" ))
        {
            cout{} << "Cannot recover from startup errors" << std::endl;
            goto End;
        }
    }

End:
    t1.join();
    
    environment->UnloadAll();

    delete environment;
}
