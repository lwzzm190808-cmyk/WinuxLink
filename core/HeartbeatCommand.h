// HeartbeatCommand.h
#pragma once
#include "ICommand.h"

namespace winuxlink {

class HeartbeatCommand : public ICommand {
public:
    Agreement::Command getCommandCode() const override { return Agreement::Command::HEARTBEAT; }
    std::vector<uint8_t> buildRequestPayload() const override { return {}; }
    bool parseResponse(const std::vector<uint8_t>&) override { return true; }
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>&) override { return {}; }
    std::string getDescription() const override { return "Heartbeat"; }
};

} // namespace winuxlink