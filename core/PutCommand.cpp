// PutCommand.cpp
#include "PutCommand.h"
#include "Logger.h"
#include <fstream>
#include <NetworkPremise.h>

namespace winuxlink {

PutCommand::PutCommand(const std::string& localPath, const std::vector<uint8_t>& data)
    : m_localPath(localPath), m_data(data), m_success(false) {}

Agreement::Command PutCommand::getCommandCode() const {
    return Agreement::Command::PUT;
}

std::vector<uint8_t> PutCommand::buildRequestPayload() const {
    std::vector<uint8_t> payload;
    payload.insert(payload.end(), m_localPath.begin(), m_localPath.end());
    payload.push_back('\0');
    payload.insert(payload.end(), m_data.begin(), m_data.end());
    return payload;
}

bool PutCommand::parseResponse(const std::vector<uint8_t>& payload) {
    // 服务端响应可为空或成功标志，简单起见，非空即成功
    m_success = !payload.empty();
    return m_success;
}

std::vector<uint8_t> PutCommand::executeServer(const std::vector<uint8_t>& requestPayload) {
    const uint8_t* ptr = requestPayload.data();
    const uint8_t* end = ptr + requestPayload.size();
    std::string path;
    while (ptr < end && *ptr) path.push_back(*ptr++);
    if (ptr < end) ptr++; // skip '\0'
    size_t dataSize = end - ptr;
    LOG_INFO("PutCommand: save to " + path + ", size=" + std::to_string(dataSize));

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("PutCommand: cannot create file " + path);
        return {};
    }
    file.write(reinterpret_cast<const char*>(ptr), dataSize);
    file.close();
    LOG_INFO("PutCommand: saved successfully");
    // 返回非空表示成功
    return {0x01}; // 简单成功标志
}

} // namespace winuxlink