// CommandVector.h
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include "CmdManager.h"
#include "ListCommand.h"
#include "LoginCommand.h"
#include "GetCommand.h"
#include "PutCommand.h"
#include "ExecCommand.h"
#include "HeartbeatCommand.h"
#include "QuitCommand.h"
#include "DownloadCommand.h"
#include "UploadCommand.h"
#include "FileInfoCommand.h"

namespace winuxlink {

inline void registerAllCommands() {
    static bool registered = false;
    if (!registered) {
        CmdManager::instance().registerCommand(std::make_unique<ListCommand>());
        CmdManager::instance().registerCommand(std::make_unique<LoginCommand>());
        CmdManager::instance().registerCommand(std::make_unique<GetCommand>());
        CmdManager::instance().registerCommand(std::make_unique<PutCommand>());
        CmdManager::instance().registerCommand(std::make_unique<ExecCommand>());
        CmdManager::instance().registerCommand(std::make_unique<HeartbeatCommand>());
        CmdManager::instance().registerCommand(std::make_unique<QuitCommand>());
        CmdManager::instance().registerCommand(std::make_unique<DownloadCommand>());
        CmdManager::instance().registerCommand(std::make_unique<UploadCommand>());
        CmdManager::instance().registerCommand(std::make_unique<FileInfoCommand>());
        registered = true;
    }
}

} // namespace winuxlink