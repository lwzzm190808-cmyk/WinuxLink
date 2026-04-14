// core/CmdManager.h
#pragma once

#include <unordered_map>
#include <memory>
#include <vector>
#include <cstdint>
#include "Agreement.h"
#include "ICommand.h"
#include "RequestTracker.h"

namespace winuxlink {

class Connector;

class CmdManager {
public:
    static CmdManager& instance();

    // 注册命令
    void registerCommand(std::unique_ptr<ICommand> cmd);

    // 服务端：根据命令码处理请求
    std::vector<uint8_t> handleServerRequest(Agreement::Command cmd, const std::vector<uint8_t>& payload);

    // 客户端：发送命令并同步等待响应（默认超时 5 秒）
    bool sendCommand(std::shared_ptr<Connector> conn, ICommand& cmd, int timeoutMs = 5000);

    // 客户端：当收到响应数据包时，通知追踪器
    void onResponseReceived(uint32_t seq, const std::vector<uint8_t>& payload);

    // 获取请求追踪器（供 Network 层调用）
    RequestTracker& getTracker() { return m_tracker; }

private:
    CmdManager() = default;
    std::unordered_map<Agreement::Command, std::unique_ptr<ICommand>> m_commands;
    RequestTracker m_tracker;
};

} // namespace winuxlink