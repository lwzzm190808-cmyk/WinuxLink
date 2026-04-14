// core/Packet.cpp
#include "PacketBuilder.h"
#include <cstring>   // memcpy
#include <algorithm> 
#include <iostream>  // std::cout

namespace winuxlink {

std::vector<uint8_t> PacketBuilder::build(Agreement::Command cmd,
                                          uint32_t seq,
                                          const std::vector<uint8_t>& payload) {
    // 1. 构造协议头（主机序）
    Agreement::Header header;
    header.magic = Agreement::MAGIC;
    header.version = Agreement::VERSION;
    header.command = static_cast<uint16_t>(cmd);
    header.payload_length = static_cast<uint32_t>(payload.size());
    header.sequence_id = seq;

    // 2. 转换为网络字节序
    Agreement::Header netHeader = Agreement::hostToNetwork(header);

    // 3. 准备输出缓冲区：头部 (16字节) + payload
    std::vector<uint8_t> packet(sizeof(Agreement::Header) + payload.size());
    
    // 4. 拷贝头部（注意：netHeader 是一个结构体，我们将其按字节拷贝）
    std::memcpy(packet.data(), &netHeader, sizeof(Agreement::Header));
    
    // 5. 拷贝 payload（如果有的话）
    if (!payload.empty()) {
        std::memcpy(packet.data() + sizeof(Agreement::Header), 
                    payload.data(), 
                    payload.size());
    }
    
    return packet;
}

// 重载：接受 std::string 作为 payload
std::vector<uint8_t> PacketBuilder::build(Agreement::Command cmd,
                                          uint32_t seq,
                                          const std::string& payload) {
    // 将 string 转为 vector<uint8_t> 然后调用主函数
    std::vector<uint8_t> payloadVec(payload.begin(), payload.end());
    return build(cmd, seq, payloadVec);
}

bool PacketBuilder::parse(std::vector<uint8_t>& buffer,
                          Agreement::Header& header,
                          std::vector<uint8_t>& payload) {
    const size_t headerSize = sizeof(Agreement::Header);
    
    // 1. 检查缓冲区是否至少包含一个完整的头部
    if (buffer.size() < headerSize) {
        return false;   // 数据不够，需要继续接收
    }
    
    // 2. 读取头部（网络序）
    Agreement::Header netHeader;
    std::memcpy(&netHeader, buffer.data(), headerSize);
    
    // 3. 转换为主机序
    header = Agreement::networkToHost(netHeader);
    
    // 4. 验证魔数和版本（防止解析到非法数据）
    if (!Agreement::isValidHeader(header)) {
        // 这里可以选择抛出异常或返回 false。
        // 对于网络数据，建议返回 false，并可以选择丢弃缓冲区前几个字节进行重同步（高级特性）。
        // 简单起见，我们直接返回 false，调用者应关闭连接。
        return false;
    }
    
    // 5. 检查整个包是否已完整接收（头部 + payload）
    size_t totalSize = headerSize + header.payload_length;
    if (buffer.size() < totalSize) {
        return false;   // 包体不完整，继续接收
    }
    
    // 6. 提取 payload
    payload.clear();
    if (header.payload_length > 0) {
        payload.resize(header.payload_length);
        std::memcpy(payload.data(), 
                    buffer.data() + headerSize, 
                    header.payload_length);
    }
    
    // 7. 从缓冲区中移除已解析的数据
    buffer.erase(buffer.begin(), buffer.begin() + totalSize);
    return true;
}


} // namespace winuxlink
