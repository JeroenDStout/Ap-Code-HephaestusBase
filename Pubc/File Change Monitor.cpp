/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Sys Thread.h"

#include "HephaestusBase/Pubc/File Change Monitor.h"

using namespace Hephaestus::Pipeline;

FileChangeMonitor::FileChangeMonitor()
{
    this->CurrentState = State::Stopped;
}

FileChangeMonitor::~FileChangeMonitor()
{
    using cout = BlackRoot::Util::Cout;

    if (this->CurrentState != State::Stopped) {
        cout{} << "!!Change monitor was not stopped before being destructed!";
    }
}

void FileChangeMonitor::InternalUpdateCycle()
{
    while (this->TargetState == State::Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        std::unique_lock<std::mutex> lock(this->MutexAccessFiles);
        this->InternalUpdateDirtyFiles();
    }
}

void FileChangeMonitor::InternalUpdateDirtyFiles()
{

}

void FileChangeMonitor::InternalHandleThreadException(BlackRoot::Debug::Exception * e)
{
    // TODO
    throw e;
}

void FileChangeMonitor::Begin()
{
    using cout = BlackRoot::Util::Cout;

    auto * This = this;

    DbAssert(this->CurrentState == State::Stopped);
    
    this->TargetState  = State::Running;
    this->CurrentState = State::Starting;

    this->UpdateThread = std::thread([&] {
        BlackRoot::System::SetCurrentThreadPriority(BlackRoot::System::ThreadPriority::Lowest);

        cout{} << "Starting ChangeMontor thread." << std::endl;

        this->CurrentState = State::Running;
        try {
            this->InternalUpdateCycle();
        }
        catch (BlackRoot::Debug::Exception * e) {
            this->InternalHandleThreadException(e);
        }

        cout{} << "ChangeMontor thread ended." << std::endl;

        this->CurrentState = State::Stopped;
    });
}

void FileChangeMonitor::EndAndWait()
{
    this->TargetState = State::Stopped;
    this->UpdateThread.join();
}