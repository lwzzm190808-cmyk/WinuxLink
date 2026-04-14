#pragma once
#include "ICommand.h"
#include <string>
#include <vector>
#include <cstdint>
#include "NetworkPremise.h"

namespace winuxlink {

class UploadCommand : public ICommand {
public:
    UploadCommand() = default;
    explicit UploadCommand(const std::string& remotePath, uint64_t offset, const std::vector<uint8_t>& data);
    ~UploadCommand() override = default;

    Agreement::Command getCommandCode() const override;
    std::vector<uint8_t> buildRequestPayload() const override;
    bool parseResponse(const std::vector<uint8_t>& payload) override;
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) override;
    std::string getDescription() const override { return "Upload file chunk"; }

    bool isSuccess() const { return m_success; }

private:
    std::string m_remotePath;
    uint64_t m_offset;
    std::vector<uint8_t> m_data;
    bool m_success;
};

} // namespace winuxlink