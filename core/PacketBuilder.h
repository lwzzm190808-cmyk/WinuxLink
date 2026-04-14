// core/Packet.h
#pragma once

#include "Agreement.h"
#include <vector>
#include <string>
#include <cstdint>

namespace winuxlink {

class PacketBuilder {
public:
    // 打包：命令 + 序列号 + 二进制载荷
    static std::vector<uint8_t> build(Agreement::Command cmd,uint32_t seq,const std::vector<uint8_t>& payload);

    // 打包：命令 + 序列号 + 字符串载荷
    static std::vector<uint8_t> build(Agreement::Command cmd,uint32_t seq,const std::string& payload);

    // 解包：从累积缓冲区中提取一个完整包
    static bool parse(std::vector<uint8_t>& buffer,Agreement::Header& header,std::vector<uint8_t>& payload);

    // 禁止实例化
    PacketBuilder() = delete;
    ~PacketBuilder() = delete;
    PacketBuilder(const PacketBuilder&) = delete;
    PacketBuilder& operator=(const PacketBuilder&) = delete;
};

} // namespace winuxlink
