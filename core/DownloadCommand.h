#pragma once
#include "ICommand.h"
#include <string>
#include <vector>
#include <cstdint>
#include "NetworkPremise.h"

namespace winuxlink {

class DownloadCommand : public ICommand {
public:
    DownloadCommand() = default;
    explicit DownloadCommand(const std::string& remotePath, uint64_t offset, uint32_t length);
    ~DownloadCommand() override = default;

    Agreement::Command getCommandCode() const override;
    std::vector<uint8_t> buildRequestPayload() const override;
    bool parseResponse(const std::vector<uint8_t>& payload) override;
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) override;
    std::string getDescription() const override { return "Download file chunk"; }

    // 客户端获取块数据
    const std::vector<uint8_t>& getChunkData() const { return m_data; }

private:
    std::string m_remotePath;
    uint64_t m_offset;
    uint32_t m_length;
    std::vector<uint8_t> m_data;
};

} // namespace winuxlink