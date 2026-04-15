#pragma once

#include <cstdint>
<<<<<<< HEAD
#include <cstddef>
=======
>>>>>>> b0bac17c4af0bcf0f9944b5db0056c9bd1427185

namespace winuxlink
{
class Agreement
{
public:
    // ========== 1. 协议常量 ==========
    // 魔数：0x4C494E4B 是 "LINK" 的十六进制表示（大端序）
    // 用于快速识别一个数据流是否遵循 WinuxLink 协议
    static constexpr uint32_t MAGIC = 0x4C494E4B;  // "LINK"
    
    // 协议版本号，用于未来升级时做兼容性判断
    static constexpr uint16_t VERSION = 1;

    // ========== 2. 命令枚举 ==========
    // 每一个枚举值代表一种请求/响应类型
    enum class Command : uint16_t {
        // 认证类 (0x7271, 0x7273)
        LOGIN_REQUEST  = 0x7271,   // 客户端 -> 服务端：发送用户名/密码
        LOGIN_RESPONSE = 0x7273,   // 服务端 -> 客户端：返回认证结果

        // 文件操作类 (0x0001 - 0x000F)
        LIST = 0x0001,          // 请求列出远程目录内容
        GET  = 0x0002,          // 请求下载文件
        PUT  = 0x0003,          // 请求上传文件
        
        // 预留: DELETE, RENAME, MKDIR 等
        
        // 远程控制类 (0x0010 - 0x001F)
        EXEC = 0x0010,          // 请求执行远程命令
        
        // 系统控制类 (0xFFF0 - 0xFFFF)
        HEARTBEAT  = 0xFFFF,         // 心跳包，用于保持连接和检测存活
        QUIT_REQUEST = 0xFFF0,       // 请求退出连接
        QUIT_RESPONSE = 0xFFF1,      // 服务端 -> 客户端：确认退出连接

        // 文件分块传输类 (0x0020 - 0x002F)
        DOWNLOAD_REQ   = 0x0020,   // 请求下载文件块（客户端 -> 服务端）
        DOWNLOAD_RSP   = 0x0021,   // 响应文件块（服务端 -> 客户端）
        UPLOAD_REQ     = 0x0022,   // 上传文件块（客户端 -> 服务端）
        UPLOAD_RSP     = 0x0023,   // 上传响应（服务端 -> 客户端）
    };
    // ========== 3. 响应状态码 ==========
    // 用于服务端告诉客户端“你刚才的请求处理结果是什么”
    enum class Status : uint16_t {
        OK_REQUEST        = 0x0000,  // 请求成功处理
        BAD_REQUEST       = 0x4000,  // 请求格式错误
        UNAUTHORIZED      = 0x4001,  // 未授权
        FORBIDDEN         = 0x4003,  // 禁止访问（如命令不在白名单）
        NOT_FOUND         = 0x4004,  // 请求的资源不存在
        INTERNAL_ERROR    = 0x5000,  // 服务端内部错误
        NOT_IMPLEMENTED   = 0x5001   // 命令未实现
    };

    // ========== 4. 协议头部结构体 ==========
    // 每一个 WinuxLink 数据包都以这个结构体开头
    // 注意：这个结构体在网络上传输时必须是“大端序”（网络字节序）
    struct Header {
        uint32_t magic;           // 魔数，必须等于 MAGIC
        uint16_t version;         // 协议版本号，必须等于 VERSION
        uint16_t command;         // 命令/状态码（Command 或 Status 的值）
        uint32_t payload_length;  // 载荷（payload）的字节数
        uint32_t sequence_id;     // 包序列号，用于请求-响应对应

        // 计算头部结构体的固定字节大小
        static constexpr size_t SIZE = 
            sizeof(uint32_t) +  // magic
            sizeof(uint16_t) +  // version
            sizeof(uint16_t) +  // command
            sizeof(uint32_t) +  // payload_length
            sizeof(uint32_t);   // sequence_id
        // 即 4 + 2 + 2 + 4 + 4 = 16 字节
    };
    // 编译期断言，确保结构体大小是我们预期的（避免对齐问题）
    static_assert(sizeof(Header) == 16, "Header size must be 16 bytes");

    // ========== 5. 字节序转换函数 ==========
    // 因为不同 CPU 架构对多字节数据的存储顺序不同
    // 网络传输统一使用“大端序”，所以需要转换
    
    // 将主机字节序的头部转换为网络字节序（发送前调用）
    static Header hostToNetwork(const Header& h);
    
    // 将网络字节序的头部转换为主机字节序（接收后调用）
    static Header networkToHost(const Header& h);

    // ========== 6. 辅助函数 ==========
    // 验证一个头部是否是合法的 WinuxLink 协议头
    static bool isValidHeader(const Header& h) {
        return h.magic == MAGIC && h.version == VERSION;
    }

    // 禁止实例化（这个类只包含静态成员和类型定义）
    Agreement() = delete;
    ~Agreement() = delete;

};

}// namespace winuxlink
