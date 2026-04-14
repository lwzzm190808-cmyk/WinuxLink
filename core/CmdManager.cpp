// core/CmdManager.cpp
#include "CmdManager.h"
#include "Connector.h"
#include "PacketBuilder.h"
#include "Logger.h"
#include <chrono>

namespace winuxlink {

CmdManager& CmdManager::instance() {
    static CmdManager inst;
    return inst;
}

void CmdManager::registerCommand(std::unique_ptr<ICommand> cmd) {
    auto code = cmd->getCommandCode();
    m_commands[code] = std::move(cmd);
}

std::vector<uint8_t> CmdManager::handleServerRequest(Agreement::Command cmd, const std::vector<uint8_t>& payload) {
    auto it = m_commands.find(cmd);
    if (it == m_commands.end()) {
        LOG_WARNING("CmdManager: Unknown command " + std::to_string(static_cast<uint16_t>(cmd)));
        return {}; // 返回空 payload，上层应发送 NOT_IMPLEMENTED 状态
    }
    return it->second->executeServer(payload);
}

bool CmdManager::sendCommand(std::shared_ptr<Connector> conn, ICommand& cmd, int timeoutMs) {
    if (!conn || !conn->isConnected()) {
        LOG_ERROR("CmdManager: Connector not connected");
        return false;
    }

    // 1. 注册请求，获取序列号
    uint32_t seq = m_tracker.registerRequest();

    // 2. 构建请求包
    std::vector<uint8_t> payload = cmd.buildRequestPayload();
    std::vector<uint8_t> packet = PacketBuilder::build(cmd.getCommandCode(), seq, payload);

    // 3. 发送
    if (!conn->nw_send(packet)) {
        m_tracker.cancel(seq);
        LOG_ERROR("CmdManager: Failed to send command");
        return false;
    }

    // 4. 等待响应
    auto future = m_tracker.getFuture(seq);
    auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));
    if (status == std::future_status::timeout) {
        m_tracker.cancel(seq);
        LOG_ERROR("CmdManager: Request timeout");
        return false;
    }

    try {
        std::vector<uint8_t> response = future.get();
        return cmd.parseResponse(response);
    } catch (const std::exception& e) {
        LOG_ERROR("CmdManager: Exception while waiting for response: " + std::string(e.what()));
        return false;
    }
}

void CmdManager::onResponseReceived(uint32_t seq, const std::vector<uint8_t>& payload) {
    m_tracker.fulfill(seq, payload);
}

} // namespace winuxlink