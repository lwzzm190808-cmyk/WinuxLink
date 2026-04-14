#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <algorithm>
#include "Network.h"
#include "Logger.h"
#include "CmdManager.h"
#include "CommandVector.h"
#include "PacketBuilder.h"
#include "GetCommand.h"
#include "PutCommand.h"
#include "LoginCommand.h"
#include "ExecCommand.h"
#include "QuitCommand.h"
#include "Transfer.h"
#include "FileInfoCommand.h"

using namespace winuxlink;

// 全局连接器（用于信号处理）
std::shared_ptr<Connector> g_client = nullptr;

// 简单字符串分割
static std::vector<std::string> split(const std::string& s, char delim = ' ') {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) result.push_back(item);
    }
    return result;
}

// 显示帮助信息
void printHelp(const char* progName) {
    std::cout << "Usage: " << progName << " [options] <ip> <port>\n"
              << "Options:\n"
              << "  -h, --help          Show this help message\n"
              << "  -u, --user <name>   Username for login (optional)\n"
              << "  -p, --pass <pass>   Password for login (optional)\n"
              << "\nInteractive commands:\n"
              << "  login <user> <pass>   Authenticate to server\n"
              << "  list [path]           List remote directory contents\n"
              << "  get <remote> [local]  Download file (auto switch)\n"
              << "  put <local> [remote]  Upload file (auto switch)\n"
              << "  exec <command>        Execute remote command\n"
              << "  quit / exit           Disconnect and quit\n"
              << "  help / ?              Show this help\n"
              << std::endl;
}

// 命令行参数结构
struct CmdArgs {
    std::string ip;
    int port = 8899;
    std::string user;
    std::string pass;
    bool showHelp = false;
};

// 解析命令行参数
CmdArgs parseArgs(int argc, char* argv[]) {
    CmdArgs args;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            args.showHelp = true;
            return args;
        } else if ((strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--user") == 0) && i + 1 < argc) {
            args.user = argv[++i];
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pass") == 0) && i + 1 < argc) {
            args.pass = argv[++i];
        } else if (args.ip.empty()) {
            args.ip = argv[i];
        } else if (args.port == 8899) {
            args.port = std::stoi(argv[i]);
        }
    }
    if (args.ip.empty()) args.showHelp = true;
    return args;
}

int main(int argc, char* argv[]) {
    Logger::setLevel(LogLevel::Info);

    CmdArgs args = parseArgs(argc, argv);
    if (args.showHelp) {
        printHelp(argv[0]);
        return 0;
    }

    // 注册所有命令（必须在连接前）
    registerAllCommands();

    // 连接服务器
    g_client = Network::client(args.ip, args.port);
    if (!g_client || !g_client->isConnected()) {
        LOG_ERROR("Failed to connect to " + args.ip + ":" + std::to_string(args.port));
        return 1;
    }

    // 设置接收回调（处理粘包和协议解析）
    g_client->setRecvCallback([](std::vector<uint8_t>& data) -> bool {
        static std::vector<uint8_t> buffer;
        buffer.insert(buffer.end(), data.begin(), data.end());

        Agreement::Header header;
        std::vector<uint8_t> payload;
        while (PacketBuilder::parse(buffer, header, payload)) {
            if (header.command == static_cast<uint16_t>(Agreement::Command::HEARTBEAT)) {
                LOG_DEBUG("Received heartbeat");
                continue;
            }
            if (header.command == static_cast<uint16_t>(Agreement::Command::QUIT_RESPONSE)) {
                LOG_INFO("Server quit response");
                CmdManager::instance().onResponseReceived(header.sequence_id, payload);
                continue;
            }
            // 其他包（响应或主动推送）交由 CmdManager 处理
            CmdManager::instance().onResponseReceived(header.sequence_id, payload);
        }
        return true;
    });

    // 自动登录（如果提供了用户名密码）
    bool loggedIn = false;
    if (!args.user.empty() && !args.pass.empty()) {
        LoginCommand loginCmd(args.user, args.pass);
        if (CmdManager::instance().sendCommand(g_client, loginCmd)) {
            if (loginCmd.isSuccess()) {
                LOG_INFO("Auto login successful: " + loginCmd.getMessage());
                loggedIn = true;
            } else {
                LOG_ERROR("Auto login failed: " + loginCmd.getMessage());
            }
        } else {
            LOG_ERROR("Auto login request timeout");
        }
    }

    LOG_INFO("Connected. Type 'help' for commands.");

    const uint64_t SMALL_FILE_THRESHOLD = 10 * 1024 * 1024; // 10MB

    // 交互循环
    std::string line;
    while (Network::isSystemRunning() && g_client->isConnected()) {
        std::cout << "> ";
        std::getline(std::cin, line);
        if (line.empty()) continue;

        auto parts = split(line);
        std::string cmd = parts[0];

        if (cmd == "quit" || cmd == "exit") {
            // 发送退出请求
            QuitCommand quitCmd;
            // 异步发送，不等待响应
            uint32_t seq = CmdManager::instance().getTracker().registerRequest();
            auto packet = PacketBuilder::build(quitCmd.getCommandCode(), seq, quitCmd.buildRequestPayload());
            g_client->nw_send(packet);
            // 主动关闭连接
            g_client->reset();
            break;
        }
        else if (cmd == "help" || cmd == "?") {
            printHelp(argv[0]);
        }
        else if (cmd == "clear" || cmd == "cls") {
        #ifdef _WIN32
            system("cls");   // Windows
        #else
            system("clear"); // Linux
        #endif
        }
        else if (cmd == "login") {
            if (parts.size() < 3) {
                std::cout << "Usage: login <username> <password>\n";
                continue;
            }
            LoginCommand loginCmd(parts[1], parts[2]);
            if (CmdManager::instance().sendCommand(g_client, loginCmd)) {
                if (loginCmd.isSuccess()) {
                    std::cout << "Login success: " << loginCmd.getMessage() << "\n";
                    loggedIn = true;
                } else {
                    std::cout << "Login failed: " << loginCmd.getMessage() << "\n";
                }
            } else {
                std::cout << "Login request timeout.\n";
            }
        }
        else if (cmd == "list") {
            std::string path = parts.size() > 1 ? parts[1] : ".";
            ListCommand listCmd(path);
            if (CmdManager::instance().sendCommand(g_client, listCmd)) {
                for (const auto& f : listCmd.getFiles()) {
                    std::cout << (f.isDir ? "[DIR] " : "[FILE] ") << f.name
                              << " (" << f.size << " bytes)\n";
                }
            } else {
                std::cout << "LIST request failed or timed out.\n";
            }
        }
        else if (cmd == "get") {
            if (parts.size() < 2) {
                std::cout << "Usage: get <remote_file> [local_file]\n";
                continue;
            }
            std::string remote = parts[1];
            std::string local = (parts.size() > 2) ? parts[2] : remote;
            // 提取本地文件名（去掉路径）
            size_t slash = local.find_last_of("/\\");
            if (slash != std::string::npos) local = local.substr(slash + 1);

            // 先获取远程文件信息（大小和哈希）
            FileInfoCommand infoCmd(remote, "");
            if (!CmdManager::instance().sendCommand(g_client, infoCmd)) {
                std::cout << "Failed to get remote file info.\n";
                continue;
            }
            uint64_t fileSize = infoCmd.getSize();
            if (fileSize == 0) {
                std::cout << "File not found or empty: " << remote << "\n";
                continue;
            }

            if (fileSize <= SMALL_FILE_THRESHOLD) {
                // 小文件：使用简单 GetCommand
                GetCommand getCmd(remote);
                if (CmdManager::instance().sendCommand(g_client, getCmd)) {
                    const auto& data = getCmd.getFileData();
                    if (data.empty()) {
                        std::cout << "Downloaded file is empty.\n";
                    } else {
                        std::ofstream out(local, std::ios::binary);
                        if (out.write(reinterpret_cast<const char*>(data.data()), data.size())) {
                            std::cout << "Downloaded " << data.size() << " bytes to " << local << "\n";
                        } else {
                            std::cout << "Failed to write local file.\n";
                        }
                    }
                } else {
                    std::cout << "GET request failed or timed out.\n";
                }
            } else {
                // 大文件：使用 Transfer 分块下载
                Transfer transfer(g_client);
                TransferOptions opts;
                opts.chunkSize = 2 * 1024 * 1024;   // 2MB per chunk
                opts.enableResume = true;
                opts.verifyChecksum = true;
                opts.checksumType = "md5";
                auto progress = [](uint64_t current, uint64_t total, bool done) {
                    if (!done) {
                        int percent = static_cast<int>(current * 100 / total);
                        std::cout << "\rDownload progress: " << percent << "% ("
                                  << current << "/" << total << ")" << std::flush;
                    } else {
                        std::cout << "\nDownload completed.\n";
                    }
                };
                if (transfer.download(remote, local, opts, progress)) {
                    std::cout << "File downloaded successfully.\n";
                } else {
                    std::cout << "Download failed.\n";
                }
            }
        }
        else if (cmd == "put") {
            if (parts.size() < 2) {
                std::cout << "Usage: put <local_file> [remote_file]\n";
                continue;
            }
            std::string local = parts[1];
            std::string remote = (parts.size() > 2) ? parts[2] : local;
            // 获取本地文件大小
            std::ifstream in(local, std::ios::binary | std::ios::ate);
            if (!in.is_open()) {
                std::cout << "Cannot open local file: " << local << "\n";
                continue;
            }
            uint64_t fileSize = in.tellg();
            in.close();

            if (fileSize <= SMALL_FILE_THRESHOLD) {
                // 小文件：使用 PutCommand
                std::ifstream inFile(local, std::ios::binary);
                if (!inFile) {
                    std::cout << "Cannot read local file.\n";
                    continue;
                }
                std::vector<uint8_t> data(fileSize);
                inFile.read(reinterpret_cast<char*>(data.data()), fileSize);
                inFile.close();

                PutCommand putCmd(remote, data);
                if (CmdManager::instance().sendCommand(g_client, putCmd)) {
                    std::cout << "Uploaded " << fileSize << " bytes to " << remote << "\n";
                } else {
                    std::cout << "PUT request failed or timed out.\n";
                }
            } else {
                // 大文件：使用 Transfer 分块上传
                Transfer transfer(g_client);
                TransferOptions opts;
                opts.chunkSize = 2 * 1024 * 1024;
                opts.enableResume = true;
                opts.verifyChecksum = true;
                opts.checksumType = "md5";
                auto progress = [](uint64_t current, uint64_t total, bool done) {
                    if (!done) {
                        int percent = static_cast<int>(current * 100 / total);
                        std::cout << "\rUpload progress: " << percent << "% ("
                                  << current << "/" << total << ")" << std::flush;
                    } else {
                        std::cout << "\nUpload completed.\n";
                    }
                };
                if (transfer.upload(local, remote, opts, progress)) {
                    std::cout << "File uploaded successfully.\n";
                } else {
                    std::cout << "Upload failed.\n";
                }
            }
        }
        else if (cmd == "exec") {
            if (parts.size() < 2) {
                std::cout << "Usage: exec <command>\n";
                continue;
            }
            std::string command = line.substr(5); // 去掉 "exec " 前缀
            ExecCommand execCmd(command);
            if (CmdManager::instance().sendCommand(g_client, execCmd)) {
                std::cout << "Command output:\n" << execCmd.getOutput() << "\n";
            } else {
                std::cout << "EXEC request failed or timed out.\n";
            }
        }
        else {
            std::cout << "Unknown command. Type 'help' for available commands.\n";
        }
    }

    Network::stop(Client);
    LOG_INFO("Client exiting.");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
}