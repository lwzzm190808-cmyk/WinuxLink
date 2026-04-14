#pragma once

#include <cstring>
#include <string>
#include <cstdint>
#include <stdlib.h> 
 
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
    #endif
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <endian.h>
#endif

namespace winuxlink
{
    
#ifdef _WIN32
    #define NW_SOCKET           SOCKET
    #define NW_SOCKET_INVALID   INVALID_SOCKET
    #define NW_SOCKET_ERROR     SOCKET_ERROR
    #define nw_shutdown         closesocket
    #define nw_size_t           int
    #define nw_sizeof(s)        (nw_size_t)sizeof(s)

    inline void nw_init()
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    inline void nw_uninit()
    {
        WSACleanup();
    }

    
    #define htobe16(x) _byteswap_ushort(x)
    #define htole16(x) (x)
    #define be16toh(x) _byteswap_ushort(x)
    #define le16toh(x) (x)
    #define htobe32(x) _byteswap_ulong(x)
    #define htole32(x) (x)
    #define be32toh(x) _byteswap_ulong(x)
    #define le32toh(x) (x)
    #define htobe64(x) _byteswap_uint64(x)
    #define htole64(x) (x)
    #define be64toh(x) _byteswap_uint64(x)
    #define le64toh(x) (x)
#else
    #define NW_SOCKET           int
    #define NW_SOCKET_INVALID   -1
    #define NW_SOCKET_ERROR     -1
    #define nw_shutdown         close
    #define nw_size_t           socklen_t
    #define nw_sizeof(s)        (nw_size_t)sizeof(s)
    
    inline void nw_init()
    {
        // 无需初始化
    }
    inline void nw_uninit()
    {
        // 无需关闭
    }
#endif   

    // 接收缓冲区
    #define BUFFER_SIZE 2048 // 最大接收缓冲区大小
    #define NW_SOCKET_INFO      sockaddr_in
    
}

