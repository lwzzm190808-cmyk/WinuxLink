#pragma once
#include "ICommand.h"
#include <string>
#include <cstdint>
#include "NetworkPremise.h"

namespace winuxlink {

class FileInfoCommand : public ICommand {
public:
    FileInfoCommand() = default;
    FileInfoCommand(const std::string& path, const std::string& hashType);
    ~FileInfoCommand() override = default;

    Agreement::Command getCommandCode() const override;
    std::vector<uint8_t> buildRequestPayload() const override;
    bool parseResponse(const std::vector<uint8_t>& payload) override;
    std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) override;
    std::string getDescription() const override { return "Get file size and hash"; }

    uint64_t getSize() const { return m_size; }
    const std::string& getHash() const { return m_hash; }

private:
    std::string m_path;
    std::string m_hashType;
    uint64_t m_size = 0;
    std::string m_hash;
};

} // namespace winuxlink