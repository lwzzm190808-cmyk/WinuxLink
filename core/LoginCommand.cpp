// LoginCommand.cpp
#include "LoginCommand.h"
#include "Logger.h"
#include <cstring>
#include <sstream>

// 跨平台系统认证头文件
#ifdef _WIN32
    #include <windows.h>
    #include <lmcons.h>   // for UNLEN
    #pragma comment(lib, "advapi32.lib")
#else
    #include <security/pam_appl.h>
    #include <unistd.h>
    #include <stdlib.h>
    #include <string.h>
#endif

namespace winuxlink {

// ========== 跨平台用户认证函数 ==========
static bool authenticateSystemUser(const std::string& username, const std::string& password) {
#ifdef _WIN32
    // Windows: 使用 LogonUser 验证网络登录
    HANDLE hToken;
    BOOL ok = LogonUserA(username.c_str(),
                         NULL,                     // 本地计算机
                         password.c_str(),
                         LOGON32_LOGON_NETWORK,    // 网络登录，不需要交互式权限
                         LOGON32_PROVIDER_DEFAULT,
                         &hToken);
    if (ok) {
        CloseHandle(hToken);
        return true;
    }
    // 可选：记录错误码
    DWORD err = GetLastError();
    if (err != 0) {
        LOG_DEBUG("LogonUser failed with error: " + std::to_string(err));
    }
    return false;
#else
    // Linux: 使用 PAM (Pluggable Authentication Modules)
    // PAM 对话回调函数，用于提供密码
    struct PamConvData {
        const char* password;
    };
    
    auto pamConvCallback = [](int num_msg, const struct pam_message** msg,
                              struct pam_response** resp, void* appdata_ptr) -> int {
        const char* password = static_cast<const char*>(appdata_ptr);
        *resp = static_cast<struct pam_response*>(calloc(num_msg, sizeof(struct pam_response)));
        if (!*resp) return PAM_BUF_ERR;
        
        for (int i = 0; i < num_msg; ++i) {
            if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
                (*resp)[i].resp = strdup(password);
                (*resp)[i].resp_retcode = 0;
            } else {
                // 其他类型消息（如信息提示）忽略
                (*resp)[i].resp = nullptr;
            }
        }
        return PAM_SUCCESS;
    };
    
    struct pam_conv conv = { pamConvCallback, const_cast<char*>(password.c_str()) };
    pam_handle_t* pamh = nullptr;
    int ret = pam_start("login", username.c_str(), &conv, &pamh);
    if (ret != PAM_SUCCESS) {
        LOG_DEBUG("PAM: pam_start failed: " + std::to_string(ret));
        return false;
    }
    
    ret = pam_authenticate(pamh, 0);
    pam_end(pamh, ret);
    return (ret == PAM_SUCCESS);
#endif
}

// ========== LoginCommand 成员函数实现 ==========
LoginCommand::LoginCommand(const std::string& user, const std::string& pass)
    : m_user(user), m_pass(pass) {}

Agreement::Command LoginCommand::getCommandCode() const {
    return Agreement::Command::LOGIN_REQUEST;
}

std::vector<uint8_t> LoginCommand::buildRequestPayload() const {
    std::vector<uint8_t> payload;
    payload.insert(payload.end(), m_user.begin(), m_user.end());
    payload.push_back('\0');
    payload.insert(payload.end(), m_pass.begin(), m_pass.end());
    payload.push_back('\0');
    return payload;
}

bool LoginCommand::parseResponse(const std::vector<uint8_t>& payload) {
    if (payload.size() < 2) {
        LOG_ERROR("LoginCommand: Response too short");
        return false;
    }
    uint16_t statusRaw;
    std::memcpy(&statusRaw, payload.data(), 2);
    uint16_t status = ntohs(statusRaw);
    m_success = (status == static_cast<uint16_t>(Agreement::Status::OK_REQUEST));
    
    if (payload.size() > 2) {
        m_message.assign(reinterpret_cast<const char*>(payload.data() + 2));
    }
    return true;
}

std::vector<uint8_t> LoginCommand::executeServer(const std::vector<uint8_t>& requestPayload) {
    // 解析用户名和密码
    const char* ptr = reinterpret_cast<const char*>(requestPayload.data());
    const char* end = ptr + requestPayload.size();
    std::string user, pass;
    while (ptr < end && *ptr) user.push_back(*ptr++);
    if (ptr < end) ptr++; // skip '\0'
    while (ptr < end && *ptr) pass.push_back(*ptr++);

    LOG_INFO("Login attempt: user=" + user);

    // 调用系统认证
    bool ok = authenticateSystemUser(user, pass);
    
    std::vector<uint8_t> response;
    uint16_t status = htons(ok ? static_cast<uint16_t>(Agreement::Status::OK_REQUEST)
                               : static_cast<uint16_t>(Agreement::Status::UNAUTHORIZED));
    response.insert(response.end(), reinterpret_cast<uint8_t*>(&status),
                    reinterpret_cast<uint8_t*>(&status) + 2);
    std::string msg = ok ? "Login success" : "Invalid credentials";
    response.insert(response.end(), msg.begin(), msg.end());
    response.push_back('\0');
    return response;
}

} // namespace winuxlink