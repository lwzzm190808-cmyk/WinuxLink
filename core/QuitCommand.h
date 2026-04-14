// QuitCommand.h
#pragma once
#include "ICommand.h"

namespace winuxlink {

class QuitCommand : public ICommand {
public:
    Agreement::Command getCommandCode() const override { return Agreement::Command::QUIT_REQUEST; }
    std::vector<uint8_t> buildRequestPayload() const override { return {}; }
    bool parseResponse(const std::vector<uint8_t>&) override { return true; }
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>&) override {
        // 服务端可以关闭连接，但这里只返回空，由上层处理
        return {};
    }
    std::string getDescription() const override { return "Quit connection"; }
};

} // namespace winuxlink