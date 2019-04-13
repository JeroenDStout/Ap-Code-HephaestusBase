/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Stringstream.h"
#include "BlackRoot/Pubc/Sys Thread.h"

#include "HephaestusBase/Pubc/Pipe Wrangler.h"

namespace sys = BlackRoot::System;
using namespace Hephaestus::Pipeline;
using namespace Hephaestus::Pipeline::Wrangler;

    //  Setup
    // --------------------

PipeWrangler::PipeWrangler()
: Caller([&](){this->ThreadedCall();})
{
    this->MaxThreadCount    = std::thread::hardware_concurrency();
}

PipeWrangler::~PipeWrangler()
{
}

    //  Cycle
    // --------------------

void PipeWrangler::ThreadedCall()
{
    using cout = BlackRoot::Util::Cout;

    std::unique_lock<std::mutex> lk(this->MxTasks);

    if (this->Tasks.size() == 0)
        return;
    
    auto task = std::move(this->Tasks.front());
    this->Tasks.erase(this->Tasks.begin());
    lk.unlock();

    WranglerTaskResult result;

    try {
        cout{} << "Pipe: " << task.OriginTask->ToolName << std::endl
            << " " << task.OriginTask->FileIn << std::endl
            << " " << task.OriginTask->FileOut << std::endl
            << " " << task.OriginTask->Settings.dump() << std::endl;

        auto tool = this->FindTool(task.OriginTask->ToolName);

        Pipeline::PipeToolInstr instr;
        instr.FileIn    = task.OriginTask->FileIn;
        instr.FileOut   = task.OriginTask->FileOut;
        instr.Settings  = std::move(task.OriginTask->Settings);

        tool->Run(instr);
    }
    catch (BlackRoot::Debug::Exception * e) {
        result.ErrorString = e->GetPrettyDescription();
        delete e;
    }
    catch (std::exception * e) {
        result.ErrorString = e->what();
        delete e;
    }
    catch (...) {
        result.ErrorString = "Unknown error trying to process task";
    }

    if (result.ErrorString.size() > 0) {
        cout{} << std::endl << "Pipe error: " << task.OriginTask->ToolName << std::endl << " " << task.OriginTask->FileIn << std::endl << " " << result.ErrorString << std::endl;
    }

    task.OriginTask->Callback(result);
    delete task.OriginTask;
}

    //  Control
    // --------------------

void PipeWrangler::Begin()
{
    this->Caller.SetMaxThreadCount(this->MaxThreadCount);
}

void PipeWrangler::EndAndWait()
{
    this->Caller.EndAndWait();
}

    //  Tools
    // --------------------

void PipeWrangler::RegisterTool(const DynLib::IPipeTool * tool)
{
    std::unique_lock<std::shared_mutex> lk(this->MxTools);
    this->Tools[tool->GetToolName()] = tool;
}

const DynLib::IPipeTool * PipeWrangler::FindTool(std::string str)
{
    std::shared_lock<std::shared_mutex> lk(this->MxTools);

    auto & it = this->Tools.find(str);
    if (it == this->Tools.end()) {
        throw new BlackRoot::Debug::Exception((std::stringstream() << "The tool '" << str << "' is not known.").str(), BRGenDbgInfo);
    }
    return it->second;
}

    //  Asynch
    // --------------------
        
void PipeWrangler::AsynchReceiveTasks(const WranglerTaskList &list)
{
    std::vector<Task> newTasks;

    for (auto & inTask : list) {
        Task task;
        task.OriginTask = new WranglerTask(inTask);
        newTasks.push_back(std::move(task));
    }

    std::unique_lock<std::mutex> lk(this->MxTasks);
    this->Tasks.insert(this->Tasks.end(), std::make_move_iterator(newTasks.begin()), 
                                          std::make_move_iterator(newTasks.end()));

    // TODO: sort

    lk.unlock();

    this->Caller.RequestCalls(newTasks.size());
}
    //  Util
    // --------------------

std::string PipeWrangler::GetAvailableTools()
{
    std::stringstream ss;

    std::shared_lock<std::shared_mutex> lk(this->MxTools);
    bool first = true;
    for (auto & it : this->Tools) {
        if (!first) ss << L", ";
        ss << it.first;
        first = false;
    }
    lk.unlock();

    return ss.str();
}