// ExecCommand.h
#pragma once
#include "ICommand.h"
#include <string>

namespace winuxlink {

class ExecCommand : public ICommand {
public:
    explicit ExecCommand(const std::string& cmd = "");
    ~ExecCommand() override = default;

    Agreement::Command getCommandCode() const override;
    std::vector<uint8_t> buildRequestPayload() const override;
    bool parseResponse(const std::vector<uint8_t>& payload) override;
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) override;
    std::string getDescription() const override { return "Execute remote command"; }

    const std::string& getOutput() const { return m_output; }

private:
    std::string m_command;
    std::string m_output;
};

} // namespace winuxlink