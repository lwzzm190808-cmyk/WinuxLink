#include "FileInfoCommand.h"
#include "Logger.h"
#include <fstream>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ncrypt.h>
#include "NetworkPremise.h"

// 字节序转换宏
#if defined(_WIN32)
    #include <windows.h>
    #define htobe64(x) _byteswap_uint64(x)
    #define be64toh(x) _byteswap_uint64(x)
#elif defined(__APPLE__)
    #include <libkern/OSByteOrder.h>
    #define htobe64(x) OSSwapHostToBigInt64(x)
    #define be64toh(x) OSSwapBigToHostInt64(x)
#else
    #include <endian.h>
#endif

// 计算哈希的辅助函数（与 Transfer 中类似，简化版）
#ifdef _WIN32
static std::string computeFileHash(const std::string& path, const std::string& type) {
    if (type != "md5") return "";
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[16];
    DWORD cbHash = 16;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return "";
    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        CryptHashData(hHash, reinterpret_cast<BYTE*>(buffer), file.gcount(), 0);
    }
    CryptHashData(hHash, reinterpret_cast<BYTE*>(buffer), file.gcount(), 0);
    CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    std::ostringstream oss;
    for (DWORD i = 0; i < cbHash; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)rgbHash[i];
    return oss.str();
}
#else
#include <openssl/md5.h>
static std::string computeFileHash(const std::string& path, const std::string& type) {
    if (type != "md5") return "";
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    MD5_CTX ctx;
    MD5_Init(&ctx);
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        MD5_Update(&ctx, buffer, file.gcount());
    }
    MD5_Update(&ctx, buffer, file.gcount());
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);
    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return oss.str();
}
#endif

namespace winuxlink {

FileInfoCommand::FileInfoCommand(const std::string& path, const std::string& hashType)
    : m_path(path), m_hashType(hashType) {}

Agreement::Command FileInfoCommand::getCommandCode() const {
    return static_cast<Agreement::Command>(0x0011); // 未占用的命令码
}

std::vector<uint8_t> FileInfoCommand::buildRequestPayload() const {
    std::vector<uint8_t> payload;
    payload.insert(payload.end(), m_path.begin(), m_path.end());
    payload.push_back('\0');
    payload.insert(payload.end(), m_hashType.begin(), m_hashType.end());
    payload.push_back('\0');
    return payload;
}

bool FileInfoCommand::parseResponse(const std::vector<uint8_t>& payload) {
    if (payload.size() < 8) return false;
    uint64_t sizeBE;
    std::memcpy(&sizeBE, payload.data(), 8);
    m_size = be64toh(sizeBE);
    if (payload.size() > 8) {
        m_hash.assign(reinterpret_cast<const char*>(payload.data() + 8));
    }
    return true;
}

std::vector<uint8_t> FileInfoCommand::executeServer(const std::vector<uint8_t>& requestPayload) {
    const char* ptr = reinterpret_cast<const char*>(requestPayload.data());
    const char* end = ptr + requestPayload.size();
    std::string path;
    while (ptr < end && *ptr) path.push_back(*ptr++);
    if (ptr < end) ptr++; // skip '\0'
    std::string hashType;
    while (ptr < end && *ptr) hashType.push_back(*ptr++);

    LOG_INFO("FileInfoCommand: query " + path + " hash=" + hashType);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    uint64_t size = 0;
    if (file.is_open()) {
        size = file.tellg();
        file.close();
    } else {
        LOG_WARNING("FileInfoCommand: file not found: " + path);
    }

    std::string hash;
    if (!hashType.empty() && size > 0) {
        hash = computeFileHash(path, hashType);
    }

    std::vector<uint8_t> response;
    uint64_t sizeBE = htobe64(size);
    response.insert(response.end(), reinterpret_cast<uint8_t*>(&sizeBE),
                    reinterpret_cast<uint8_t*>(&sizeBE) + 8);
    if (!hash.empty()) {
        response.insert(response.end(), hash.begin(), hash.end());
    }
    response.push_back('\0');
    return response;
}

} // namespace winuxlink