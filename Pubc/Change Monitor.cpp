/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Sys Thread.h"

#include "HephaestusBase/Pubc/Change Monitor.h"

using namespace Hephaestus::Pipeline;

ChangeMonitor::ChangeMonitor()
{
    this->CurrentState = State::Stopped;
}

ChangeMonitor::~ChangeMonitor()
{
    using cout = BlackRoot::Util::Cout;

    if (this->CurrentState != State::Stopped) {
        cout{} << "!!Change monitor was not stopped before being destructed!";
    }
}

void ChangeMonitor::InternalUpdateCycle()
{
    while (this->TargetState == State::Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

void ChangeMonitor::InternalHandleThreadException(BlackRoot::Debug::Exception * e)
{
    // TODO
    throw e;
}

void ChangeMonitor::Begin()
{
    using cout = BlackRoot::Util::Cout;

    auto * This = this;

    DbAssert(this->CurrentState == State::Stopped);
    
    this->TargetState  = State::Running;
    this->CurrentState = State::Starting;

    this->UpdateThread = std::thread([&] {
        BlackRoot::System::SetCurrentThreadPriority(BlackRoot::System::ThreadPriority::Lowest);

        this->CurrentState = State::Running;
        try {
            this->InternalUpdateCycle();
        }
        catch (BlackRoot::Debug::Exception * e) {
            this->InternalHandleThreadException(e);
        }
        this->CurrentState = State::Stopped;
    });
}

void ChangeMonitor::EndAndWait()
{
    this->TargetState = State::Stopped;
    this->UpdateThread.join();
}