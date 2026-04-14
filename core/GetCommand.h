// GetCommand.h
#pragma once
#include "ICommand.h"
#include <string>
#include <vector>

namespace winuxlink {

class GetCommand : public ICommand {
public:
    explicit GetCommand(const std::string& remotePath = "");
    ~GetCommand() override = default;

    Agreement::Command getCommandCode() const override;
    std::vector<uint8_t> buildRequestPayload() const override;
    bool parseResponse(const std::vector<uint8_t>& payload) override;
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) override;
    std::string getDescription() const override { return "Download file"; }

    // 客户端获取文件内容
    const std::vector<uint8_t>& getFileData() const { return m_data; }

private:
    std::string m_remotePath;
    std::vector<uint8_t> m_data;
};

} // namespace winuxlink