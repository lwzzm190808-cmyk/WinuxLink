// ExecCommand.cpp
#include "ExecCommand.h"
#include "Logger.h"
#include <cstdlib>
#include <cstdio>
#include <array>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace winuxlink {

ExecCommand::ExecCommand(const std::string& cmd) : m_command(cmd) {}

Agreement::Command ExecCommand::getCommandCode() const {
    return Agreement::Command::EXEC;
}

std::vector<uint8_t> ExecCommand::buildRequestPayload() const {
    std::vector<uint8_t> payload(m_command.begin(), m_command.end());
    payload.push_back('\0');
    return payload;
}

bool ExecCommand::parseResponse(const std::vector<uint8_t>& payload) {
    m_output.assign(reinterpret_cast<const char*>(payload.data()), payload.size());
    return true;
}

std::vector<uint8_t> ExecCommand::executeServer(const std::vector<uint8_t>& requestPayload) {
    std::string cmd(reinterpret_cast<const char*>(requestPayload.data()), requestPayload.size());
    if (!cmd.empty() && cmd.back() == '\0') cmd.pop_back();
    LOG_INFO("ExecCommand: execute " + cmd);

    std::array<char, 1024> buffer;
    std::string result;
    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        LOG_ERROR("ExecCommand: popen failed");
        return {};
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    PCLOSE(pipe);
    return std::vector<uint8_t>(result.begin(), result.end());
}

} // namespace winuxlink