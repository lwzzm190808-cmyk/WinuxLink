// core/ListCommand.cpp
#include "ListCommand.h"
#include "Logger.h"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <fileapi.h>   // FindFirstFileA, etc.
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

namespace winuxlink {

// ========== 跨平台 64 位字节序转换 ==========
#if defined(_WIN32)
    #define htobe64(x) _byteswap_uint64(x)
    #define be64toh(x) _byteswap_uint64(x)
#elif defined(__APPLE__)
    #include <libkern/OSByteOrder.h>
    #define htobe64(x) OSSwapHostToBigInt64(x)
    #define be64toh(x) OSSwapBigToHostInt64(x)
#else
    #include <endian.h>
#endif

// ========== 跨平台目录遍历实现 ==========
std::vector<FileInfo> ListCommand::listDirectory(const std::string& path) {
    std::vector<FileInfo> entries;

#ifdef _WIN32
    std::string searchPath = path + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_WARNING("ListCommand: Failed to open directory: " + path);
        return entries;
    }

    do {
        std::string name = findData.cFileName;
        if (name == "." || name == "..") continue;

        FileInfo info;
        info.name = name;
        info.isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        info.size = info.isDir ? 0 : 
            ((static_cast<uint64_t>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow);

        // FILETIME 转 Unix 时间戳
        ULARGE_INTEGER ull;
        ull.LowPart = findData.ftLastWriteTime.dwLowDateTime;
        ull.HighPart = findData.ftLastWriteTime.dwHighDateTime;
        // 1601-01-01 到 1970-01-01 的 100 纳秒间隔数
        info.mtime = (ull.QuadPart - 116444736000000000ULL) / 10000000ULL;

        entries.push_back(info);
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);

#else
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        LOG_WARNING("ListCommand: Failed to open directory: " + path);
        return entries;
    }

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = path + "/" + name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        FileInfo info;
        info.name = name;
        info.isDir = S_ISDIR(st.st_mode);
        info.size = info.isDir ? 0 : st.st_size;
        info.mtime = st.st_mtime;
        entries.push_back(info);
    }
    closedir(dir);
#endif

    // 按名称排序（可选）
    std::sort(entries.begin(), entries.end(), [](const FileInfo& a, const FileInfo& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir; // 目录在前
        return a.name < b.name;
    });

    return entries;
}

// ========== ICommand 接口实现 ==========
ListCommand::ListCommand(const std::string& path) : m_path(path) {}

Agreement::Command ListCommand::getCommandCode() const {
    return Agreement::Command::LIST;
}

std::vector<uint8_t> ListCommand::buildRequestPayload() const {
    std::vector<uint8_t> payload(m_path.begin(), m_path.end());
    payload.push_back('\0');   // 以空字符结尾
    return payload;
}

bool ListCommand::parseResponse(const std::vector<uint8_t>& payload) {
    m_files.clear();
    if (payload.size() < 4) {
        LOG_ERROR("ListCommand: Response too short");
        return false;
    }

    const uint8_t* ptr = payload.data();
    const uint8_t* end = ptr + payload.size();

    // 读取条目数量
    uint32_t count;
    std::memcpy(&count, ptr, sizeof(count));
    count = ntohl(count);
    ptr += sizeof(count);

    for (uint32_t i = 0; i < count; ++i) {
        if (ptr >= end) {
            LOG_ERROR("ListCommand: Response truncated at entry " + std::to_string(i));
            return false;
        }

        FileInfo info;

        // 文件名（以 '\0' 结尾）
        const char* namePtr = reinterpret_cast<const char*>(ptr);
        size_t nameLen = std::strlen(namePtr);
        info.name.assign(namePtr, nameLen);
        ptr += nameLen + 1;

        // 文件大小（8 字节，大端）
        if (ptr + sizeof(uint64_t) > end) {
            LOG_ERROR("ListCommand: Response missing size for " + info.name);
            return false;
        }
        uint64_t sizeBE;
        std::memcpy(&sizeBE, ptr, sizeof(sizeBE));
        info.size = be64toh(sizeBE);
        ptr += sizeof(sizeBE);

        // 是否目录
        if (ptr >= end) {
            LOG_ERROR("ListCommand: Response missing directory flag for " + info.name);
            return false;
        }
        info.isDir = (*ptr++) != 0;

        // 修改时间（8 字节，大端）
        if (ptr + sizeof(uint64_t) > end) {
            LOG_ERROR("ListCommand: Response missing mtime for " + info.name);
            return false;
        }
        uint64_t mtimeBE;
        std::memcpy(&mtimeBE, ptr, sizeof(mtimeBE));
        info.mtime = be64toh(mtimeBE);
        ptr += sizeof(mtimeBE);

        m_files.push_back(std::move(info));
    }

    return true;
}

std::vector<uint8_t> ListCommand::executeServer(const std::vector<uint8_t>& requestPayload) {
    // 解析请求路径
    std::string path(requestPayload.begin(), requestPayload.end());
    // 移除末尾可能的 '\0'
    if (!path.empty() && path.back() == '\0') {
        path.pop_back();
    }
    if (path.empty()) {
        path = ".";
    }

    LOG_INFO("ListCommand: Listing directory: " + path);
    auto entries = listDirectory(path);

    // 构建响应 payload
    std::vector<uint8_t> response;
    uint32_t count = static_cast<uint32_t>(entries.size());
    uint32_t countBE = htonl(count);
    response.insert(response.end(), reinterpret_cast<uint8_t*>(&countBE),
                    reinterpret_cast<uint8_t*>(&countBE) + sizeof(countBE));

    for (const auto& e : entries) {
        // 文件名 + '\0'
        response.insert(response.end(), e.name.begin(), e.name.end());
        response.push_back('\0');

        // 大小（8 字节，大端）
        uint64_t sizeBE = htobe64(e.size);
        auto* sizePtr = reinterpret_cast<uint8_t*>(&sizeBE);
        response.insert(response.end(), sizePtr, sizePtr + sizeof(sizeBE));

        // 是否目录
        response.push_back(e.isDir ? 1 : 0);

        // 修改时间（8 字节，大端）
        uint64_t mtimeBE = htobe64(e.mtime);
        auto* mtimePtr = reinterpret_cast<uint8_t*>(&mtimeBE);
        response.insert(response.end(), mtimePtr, mtimePtr + sizeof(mtimeBE));
    }

    LOG_INFO("ListCommand: Returning " + std::to_string(count) + " entries");
    return response;
}

} // namespace winuxlink