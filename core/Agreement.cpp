// core/Agreement.cpp
#include "Agreement.h"

#ifdef _WIN32
    // Windows 下的网络字节序转换函数在 winsock2.h 中
    #include <winsock2.h>
#ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
#endif
#else
    // Linux/Unix 下在 arpa/inet.h 中
    #include <arpa/inet.h>
#endif

namespace winuxlink {

/*
 * @brief 将主机字节序转换为网络字节序
 * @param h 主机字节序的头信息
 * @return 网络字节序的头信息
*/
Agreement::Header Agreement::hostToNetwork(const Header& h) {
    Header net = h;
    net.magic = htonl(h.magic);
    net.version = htons(h.version);
    net.command = htons(h.command);
    net.payload_length = htonl(h.payload_length);
    net.sequence_id = htonl(h.sequence_id);
    return net;
}

/*
 * @brief 将网络字节序转换为主机字节序
 * @param h 网络字节序的头信息
 * @return 主机字节序的头信息
*/
Agreement::Header Agreement::networkToHost(const Header& h) {
    Header host = h;
    host.magic = ntohl(h.magic);
    host.version = ntohs(h.version);
    host.command = ntohs(h.command);
    host.payload_length = ntohl(h.payload_length);
    host.sequence_id = ntohl(h.sequence_id);
    return host;
}

} // namespace winuxlink
