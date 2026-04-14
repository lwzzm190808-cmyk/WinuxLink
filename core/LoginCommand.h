// LoginCommand.h
#pragma once
#include "ICommand.h"
#include <string>

namespace winuxlink {

class LoginCommand : public ICommand {
public:
    LoginCommand() = default;
    LoginCommand(const std::string& user, const std::string& pass);
    ~LoginCommand() override = default;

    Agreement::Command getCommandCode() const override;
    std::vector<uint8_t> buildRequestPayload() const override;
    bool parseResponse(const std::vector<uint8_t>& payload) override;
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) override;
    std::string getDescription() const override { return "User login"; }

    // 客户端获取认证结果
    bool isSuccess() const { return m_success; }
    std::string getMessage() const { return m_message; }

private:
    std::string m_user;
    std::string m_pass;
    bool m_success = false;
    std::string m_message;
};

} // namespace winuxlink