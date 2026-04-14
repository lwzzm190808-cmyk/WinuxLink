// GetCommand.cpp
#include "GetCommand.h"
#include "Logger.h"
#include <fstream>
#include <cstring>

namespace winuxlink {

GetCommand::GetCommand(const std::string& remotePath) : m_remotePath(remotePath) {}

Agreement::Command GetCommand::getCommandCode() const {
    return Agreement::Command::GET;
}

std::vector<uint8_t> GetCommand::buildRequestPayload() const {
    std::vector<uint8_t> payload(m_remotePath.begin(), m_remotePath.end());
    payload.push_back('\0');
    return payload;
}

bool GetCommand::parseResponse(const std::vector<uint8_t>& payload) {
    m_data = payload;  // 客户端直接保存收到的文件内容
    return true;
}

std::vector<uint8_t> GetCommand::executeServer(const std::vector<uint8_t>& requestPayload) {
    std::string path(reinterpret_cast<const char*>(requestPayload.data()), requestPayload.size());
    if (!path.empty() && path.back() == '\0') path.pop_back();
    LOG_INFO("GetCommand: request file " + path);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_WARNING("GetCommand: file not found: " + path);
        return {};  // 返回空，上层应发送错误状态
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    if (file.read(reinterpret_cast<char*>(data.data()), size)) {
        LOG_INFO("GetCommand: read " + std::to_string(size) + " bytes");
        return data;
    } else {
        LOG_ERROR("GetCommand: read failed");
        return {};
    }
}

} // namespace winuxlink