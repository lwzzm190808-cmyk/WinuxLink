#pragma once

#include <vector>     
#include <cstdint>     
#include <string>      
#include "Agreement.h" 
namespace winuxlink
{
class ICommand {
public:
    virtual ~ICommand() = default;

    // 返回此命令对应的协议命令码
    virtual Agreement::Command getCommandCode() const = 0;

    // 构建请求 payload（由客户端调用）
    virtual std::vector<uint8_t> buildRequestPayload() const = 0;

    // 解析响应 payload（由客户端调用）
    // 返回 true 表示解析成功
    virtual bool parseResponse(const std::vector<uint8_t>& payload) = 0;

    // 执行服务端逻辑（由服务端调用）
    // 传入请求 payload，返回响应 payload
    virtual std::vector<uint8_t> executeServer(const std::vector<uint8_t>& requestPayload) = 0;

    // 可选：获取命令描述（用于帮助信息）
    virtual std::string getDescription() const { return ""; }
};
} // namespace winuxlink
