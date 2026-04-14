// PutCommand.h
#pragma once
#include "ICommand.h"
#include <string>
#include <vector>

namespace winuxlink {

class PutCommand : public ICommand {
public:
    explicit PutCommand(const std::string& localPath = "", const std::vector<uint8_t>& data = {});
    ~PutCommand() override = default;

    Agreement::Command getCommandCode() const override;
    std::vector<uint8_t> buildRequestPayload() const override;
    bool parseResponse(const std::vector<uint8_t>& payload) override;
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) override;
    std::string getDescription() const override { return "Upload file"; }

    bool isSuccess() const { return m_success; }

private:
    std::string m_localPath;
    std::vector<uint8_t> m_data;
    bool m_success;
};

} // namespace winuxlink