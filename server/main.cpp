#include <iostream>
#include "Network.h"
#include "Logger.h"
#include "CmdManager.h"
#include "CommandVector.h"
#include "PacketBuilder.h"

using namespace winuxlink;

int main(int argc, const char* argv[]) {
    Logger::setLevel(LogLevel::Info);

    if (argc != 3) {
        LOG_ERROR("Usage: server <ip> <port>");
        return 1;
    }

    // 注册所有命令
    registerAllCommands();

    try {
        auto server = Network::server(argv[1], atoi(argv[2]));

        // 设置新连接回调：每个客户端连接都会触发
        Network::setNewConnectorCallback([](const std::shared_ptr<Connector> conn) {
            LOG_INFO("New client: " + conn->getAddress() + ":" + std::to_string(conn->getPort()));

            // 为这个连接设置接收回调（处理粘包）
            conn->setRecvCallback([conn](std::vector<uint8_t>& data) -> bool {
                static std::vector<uint8_t> buffer;
                thread_local std::vector<uint8_t> tls_buffer;
                tls_buffer.insert(tls_buffer.end(), data.begin(), data.end());

                Agreement::Header header;
                std::vector<uint8_t> payload;
                while (PacketBuilder::parse(tls_buffer, header, payload)) {
                    // 系统命令（心跳、退出）可在此处理
                    if (header.command == static_cast<uint16_t>(Agreement::Command::HEARTBEAT)) {
                        // 心跳包，直接忽略或回复空包
                        continue;
                    }
                    if (header.command == static_cast<uint16_t>(Agreement::Command::QUIT_REQUEST)) {
                        LOG_INFO("Client requested quit");
                        // 发送 QUIT_RESPONSE
                        auto packet = PacketBuilder::build(Agreement::Command::QUIT_RESPONSE, header.sequence_id, std::vector<uint8_t>());
                        conn->nw_send(packet);
                        conn->reset();
                        return false;
                    }

                    // 业务命令交给 CmdManager
                    std::vector<uint8_t> response = CmdManager::instance().handleServerRequest(
                        static_cast<Agreement::Command>(header.command), payload);

                    // 发送响应（状态码 OK_REQUEST）
                    auto packet = PacketBuilder::build(
                        static_cast<Agreement::Command>(Agreement::Status::OK_REQUEST),
                        header.sequence_id, response);
                    conn->nw_send(packet);
                }
                return true;
            });
        });

    } catch (const std::exception& e) {
        LOG_ERROR("Exception: " + std::string(e.what()));
        return 1;
    }

    // 进入服务端主循环（阻塞）
    return Network::exec();
}