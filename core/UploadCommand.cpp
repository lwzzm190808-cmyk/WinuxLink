#include "UploadCommand.h"
#include "Logger.h"
#include <fstream>
#include <cstring>
#include "NetworkPremise.h"

namespace winuxlink {

UploadCommand::UploadCommand(const std::string& remotePath, uint64_t offset, const std::vector<uint8_t>& data)
    : m_remotePath(remotePath), m_offset(offset), m_data(data), m_success(false) {}

Agreement::Command UploadCommand::getCommandCode() const {
    return Agreement::Command::UPLOAD_REQ;
}

std::vector<uint8_t> UploadCommand::buildRequestPayload() const {
    std::vector<uint8_t> payload;
    payload.insert(payload.end(), m_remotePath.begin(), m_remotePath.end());
    payload.push_back('\0');
    uint64_t offsetBE = htobe64(m_offset);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&offsetBE),
                   reinterpret_cast<uint8_t*>(&offsetBE) + 8);
    payload.insert(payload.end(), m_data.begin(), m_data.end());
    return payload;
}

bool UploadCommand::parseResponse(const std::vector<uint8_t>& payload) {
    m_success = (payload.size() == 1 && payload[0] == 0x01);
    return m_success;
}

std::vector<uint8_t> UploadCommand::executeServer(const std::vector<uint8_t>& requestPayload) {
    // 解析：路径\0 offset(8) + 数据
    const uint8_t* ptr = requestPayload.data();
    const uint8_t* end = ptr + requestPayload.size();
    std::string path;
    while (ptr < end && *ptr) path.push_back(*ptr++);
    if (ptr < end) ptr++; // skip '\0'
    if (ptr + 8 > end) {
        LOG_ERROR("UploadCommand: invalid request payload");
        return {};
    }
    uint64_t offset;
    std::memcpy(&offset, ptr, 8); offset = be64toh(offset); ptr += 8;
    size_t dataSize = end - ptr;
    LOG_INFO("UploadCommand: write " + path + " offset=" + std::to_string(offset) +
             " size=" + std::to_string(dataSize));

    // 打开文件（以读写方式，如果不存在则创建）
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        // 文件不存在，创建并打开
        file.open(path, std::ios::binary | std::ios::out);
        if (!file.is_open()) {
            LOG_ERROR("UploadCommand: cannot create file " + path);
            return {};
        }
        file.close();
        file.open(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return {};
    }
    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(ptr), dataSize);
    if (!file.good()) {
        LOG_ERROR("UploadCommand: write failed");
        return {};
    }
    LOG_INFO("UploadCommand: write success");
    return {0x01}; // 成功标志
}

} // namespace winuxlink