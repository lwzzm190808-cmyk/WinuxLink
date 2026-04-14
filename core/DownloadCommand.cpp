#include "DownloadCommand.h"
#include "Logger.h"
#include <fstream>
#include <cstring>

namespace winuxlink {

DownloadCommand::DownloadCommand(const std::string& remotePath, uint64_t offset, uint32_t length)
    : m_remotePath(remotePath), m_offset(offset), m_length(length) {}

Agreement::Command DownloadCommand::getCommandCode() const {
    return Agreement::Command::DOWNLOAD_REQ;
}

std::vector<uint8_t> DownloadCommand::buildRequestPayload() const {
    std::vector<uint8_t> payload;
    payload.insert(payload.end(), m_remotePath.begin(), m_remotePath.end());
    payload.push_back('\0');
    uint64_t offsetBE = htobe64(m_offset);
    uint32_t lengthBE = htonl(m_length);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&offsetBE),
                   reinterpret_cast<uint8_t*>(&offsetBE) + 8);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&lengthBE),
                   reinterpret_cast<uint8_t*>(&lengthBE) + 4);
    return payload;
}

bool DownloadCommand::parseResponse(const std::vector<uint8_t>& payload) {
    m_data = payload;
    return true;
}

std::vector<uint8_t> DownloadCommand::executeServer(const std::vector<uint8_t>& requestPayload) {
    // 解析请求：路径\0 offset(8) length(4)
    const uint8_t* ptr = requestPayload.data();
    const uint8_t* end = ptr + requestPayload.size();
    std::string path;
    while (ptr < end && *ptr) path.push_back(*ptr++);
    if (ptr < end) ptr++; // skip '\0'
    if (ptr + 8 + 4 > end) {
        LOG_ERROR("DownloadCommand: invalid request payload");
        return {};
    }
    uint64_t offset;
    uint32_t length;
    std::memcpy(&offset, ptr, 8); offset = be64toh(offset); ptr += 8;
    std::memcpy(&length, ptr, 4); length = ntohl(length);   ptr += 4;

    LOG_INFO("DownloadCommand: read " + path + " offset=" + std::to_string(offset) +
             " length=" + std::to_string(length));

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOG_WARNING("DownloadCommand: file not found: " + path);
        return {};
    }
    file.seekg(offset);
    std::vector<uint8_t> data(length);
    file.read(reinterpret_cast<char*>(data.data()), length);
    data.resize(file.gcount());
    LOG_INFO("DownloadCommand: read " + std::to_string(data.size()) + " bytes");
    return data;
}

} // namespace winuxlink