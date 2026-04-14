#include "Transfer.h"
#include "CmdManager.h"
#include "DownloadCommand.h"
#include "UploadCommand.h"
#include "FileInfoCommand.h"
#include "Logger.h"
#include "PacketBuilder.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <algorithm>

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

namespace winuxlink {

// ==================== Transfer 实现 ====================
Transfer::Transfer(std::shared_ptr<Connector> conn)
    : m_conn(conn), m_cancelled(false) {}

Transfer::~Transfer() {
    cancel();
}

void Transfer::cancel() {
    m_cancelled.store(true);
}

uint64_t Transfer::getRemoteFileSize(const std::string& remotePath) {
    FileInfoCommand cmd(remotePath, "");
    if (CmdManager::instance().sendCommand(m_conn, cmd)) {
        return cmd.getSize();
    }
    return 0;
}

std::string Transfer::getRemoteFileHash(const std::string& remotePath, const std::string& type) {
    FileInfoCommand cmd(remotePath, type);
    if (CmdManager::instance().sendCommand(m_conn, cmd)) {
        return cmd.getHash();
    }
    return "";
}

bool Transfer::sendChunkRequest(Agreement::Command cmd, const std::vector<uint8_t>& payload,
                                std::vector<uint8_t>& response, int timeoutMs) {
    if (cmd == Agreement::Command::DOWNLOAD_REQ) {
        // 解析 payload 构造 DownloadCommand
        const uint8_t* ptr = payload.data();
        const uint8_t* end = ptr + payload.size();
        std::string path;
        while (ptr < end && *ptr) path.push_back(*ptr++);
        if (ptr < end) ptr++; // skip '\0'
        if (ptr + 8 + 4 > end) return false;
        uint64_t offset;
        uint32_t length;
        std::memcpy(&offset, ptr, 8); offset = be64toh(offset); ptr += 8;
        std::memcpy(&length, ptr, 4); length = ntohl(length);
        DownloadCommand cmdObj(path, offset, length);
        if (CmdManager::instance().sendCommand(m_conn, cmdObj, timeoutMs)) {
            response = cmdObj.getChunkData();
            return true;
        }
    } else if (cmd == Agreement::Command::UPLOAD_REQ) {
        const uint8_t* ptr = payload.data();
        const uint8_t* end = ptr + payload.size();
        std::string path;
        while (ptr < end && *ptr) path.push_back(*ptr++);
        if (ptr < end) ptr++;
        if (ptr + 8 > end) return false;
        uint64_t offset;
        std::memcpy(&offset, ptr, 8); offset = be64toh(offset); ptr += 8;
        std::vector<uint8_t> data(ptr, end);
        UploadCommand cmdObj(path, offset, data);
        if (CmdManager::instance().sendCommand(m_conn, cmdObj, timeoutMs)) {
            response = {0x01};
            return true;
        }
    }
    return false;
}

bool Transfer::download(const std::string& remotePath, const std::string& localPath,
                        const TransferOptions& opts, ProgressCallback progress) {
    uint64_t totalSize = getRemoteFileSize(remotePath);
    if (totalSize == 0) {
        LOG_ERROR("Transfer: cannot get remote file size: " + remotePath);
        return false;
    }

    uint64_t startOffset = 0;
    std::string stateFile = opts.resumeStateFile.empty() ? localPath + ".resume" : opts.resumeStateFile;
    if (opts.enableResume && loadResumeState(stateFile, startOffset)) {
        std::ifstream local(localPath, std::ios::binary | std::ios::ate);
        if (local.is_open() && local.tellg() == static_cast<std::streamoff>(startOffset)) {
            LOG_INFO("Transfer: resume download from offset " + std::to_string(startOffset));
        } else {
            startOffset = 0;
        }
    }

    std::ofstream localFile(localPath, std::ios::binary | std::ios::app);
    if (!localFile) {
        LOG_ERROR("Transfer: cannot open local file for write: " + localPath);
        return false;
    }

    uint64_t downloaded = startOffset;
    uint64_t remaining = totalSize - startOffset;
    RateLimiter limiter(opts.speedLimitBps);
    m_cancelled = false;

    while (remaining > 0 && !m_cancelled) {
        size_t chunkSize = static_cast<size_t>(std::min(remaining, static_cast<uint64_t>(opts.chunkSize)));
        std::vector<uint8_t> request;
        request.insert(request.end(), remotePath.begin(), remotePath.end());
        request.push_back('\0');
        uint64_t offsetBE = htobe64(downloaded);
        uint32_t lengthBE = htonl(chunkSize);
        request.insert(request.end(), reinterpret_cast<uint8_t*>(&offsetBE),
                       reinterpret_cast<uint8_t*>(&offsetBE) + 8);
        request.insert(request.end(), reinterpret_cast<uint8_t*>(&lengthBE),
                       reinterpret_cast<uint8_t*>(&lengthBE) + 4);

        std::vector<uint8_t> chunkData;
        if (!sendChunkRequest(Agreement::Command::DOWNLOAD_REQ, request, chunkData, 30000)) {
            LOG_ERROR("Transfer: download chunk failed at offset " + std::to_string(downloaded));
            saveResumeState(stateFile, downloaded);
            return false;
        }
        if (chunkData.empty()) break;

        localFile.write(reinterpret_cast<const char*>(chunkData.data()), chunkData.size());
        downloaded += chunkData.size();
        remaining -= chunkData.size();

        limiter.throttle(chunkData.size());

        if (progress) progress(downloaded, totalSize, false);

        if (downloaded % (totalSize / 20 + 1) == 0 || remaining == 0) {
            saveResumeState(stateFile, downloaded);
        }
    }

    localFile.close();
    if (m_cancelled) return false;

    if (opts.verifyChecksum) {
        std::string localHash = (opts.checksumType == "md5") ? computeMD5(localPath) : computeSHA256(localPath);
        std::string remoteHash = getRemoteFileHash(remotePath, opts.checksumType);
        if (localHash != remoteHash) {
            LOG_ERROR("Transfer: checksum mismatch");
            return false;
        }
        LOG_INFO("Transfer: checksum verified");
    }

    if (progress) progress(totalSize, totalSize, true);
    LOG_INFO("Transfer: download completed");
    return true;
}

bool Transfer::upload(const std::string& localPath, const std::string& remotePath,
                      const TransferOptions& opts, ProgressCallback progress) {
    std::ifstream localFile(localPath, std::ios::binary | std::ios::ate);
    if (!localFile) {
        LOG_ERROR("Transfer: cannot open local file: " + localPath);
        return false;
    }
    uint64_t totalSize = localFile.tellg();
    localFile.close();

    uint64_t startOffset = 0;
    std::string stateFile = opts.resumeStateFile.empty() ? localPath + ".upload_resume" : opts.resumeStateFile;
    if (opts.enableResume && loadResumeState(stateFile, startOffset)) {
        LOG_INFO("Transfer: resume upload from offset " + std::to_string(startOffset));
    }

    localFile.open(localPath, std::ios::binary);
    if (!localFile) return false;
    localFile.seekg(startOffset);

    uint64_t uploaded = startOffset;
    uint64_t remaining = totalSize - startOffset;
    RateLimiter limiter(opts.speedLimitBps);
    m_cancelled = false;

    while (remaining > 0 && !m_cancelled) {
        size_t chunkSize = static_cast<size_t>(std::min(remaining, static_cast<uint64_t>(opts.chunkSize)));
        std::vector<uint8_t> chunkData(chunkSize);
        localFile.read(reinterpret_cast<char*>(chunkData.data()), chunkSize);
        size_t actual = localFile.gcount();
        if (actual == 0) break;
        chunkData.resize(actual);

        std::vector<uint8_t> request;
        request.insert(request.end(), remotePath.begin(), remotePath.end());
        request.push_back('\0');
        uint64_t offsetBE = htobe64(uploaded);
        request.insert(request.end(), reinterpret_cast<uint8_t*>(&offsetBE),
                       reinterpret_cast<uint8_t*>(&offsetBE) + 8);
        request.insert(request.end(), chunkData.begin(), chunkData.end());

        std::vector<uint8_t> response;
        if (!sendChunkRequest(Agreement::Command::UPLOAD_REQ, request, response, 30000)) {
            LOG_ERROR("Transfer: upload chunk failed at offset " + std::to_string(uploaded));
            saveResumeState(stateFile, uploaded);
            return false;
        }
        if (response.empty() || response[0] != 0x01) {
            LOG_ERROR("Transfer: server rejected upload chunk");
            return false;
        }

        uploaded += actual;
        remaining -= actual;
        limiter.throttle(actual);

        if (progress) progress(uploaded, totalSize, false);

        if (uploaded % (totalSize / 20 + 1) == 0 || remaining == 0) {
            saveResumeState(stateFile, uploaded);
        }
    }

    localFile.close();
    if (m_cancelled) return false;

    if (opts.verifyChecksum) {
        std::string localHash = (opts.checksumType == "md5") ? computeMD5(localPath) : computeSHA256(localPath);
        std::string remoteHash = getRemoteFileHash(remotePath, opts.checksumType);
        if (localHash != remoteHash) {
            LOG_ERROR("Transfer: upload checksum mismatch");
            return false;
        }
    }

    if (progress) progress(totalSize, totalSize, true);
    LOG_INFO("Transfer: upload completed");
    return true;
}

// ==================== 断点续传状态文件 ====================
bool Transfer::loadResumeState(const std::string& stateFile, uint64_t& completedOffset) {
    std::ifstream file(stateFile);
    if (!file.is_open()) return false;
    file >> completedOffset;
    return file.good();
}

void Transfer::saveResumeState(const std::string& stateFile, uint64_t completedOffset) {
    std::ofstream file(stateFile, std::ios::trunc);
    if (file.is_open()) file << completedOffset;
}

// ==================== RateLimiter ====================
void Transfer::RateLimiter::throttle(size_t bytes) {
    if (m_bps == 0) return;
    auto now = std::chrono::steady_clock::now();
    if (m_lastTime.time_since_epoch().count() == 0) {
        m_lastTime = now;
        m_bytesSent = bytes;
        return;
    }
    m_bytesSent += bytes;
    double elapsed = std::chrono::duration<double>(now - m_lastTime).count();
    double expectedTime = static_cast<double>(m_bytesSent) / m_bps;
    if (expectedTime > elapsed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<int>((expectedTime - elapsed) * 1000)));
    }
    if (elapsed >= 1.0) {
        m_lastTime = now;
        m_bytesSent = 0;
    }
}

// ==================== 哈希计算（跨平台） ====================
#ifdef _WIN32
std::string Transfer::computeMD5(const std::string& filePath) {
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
    std::ifstream file(filePath, std::ios::binary);
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

std::string Transfer::computeSHA256(const std::string& filePath) {
    // Windows 下简单返回 MD5 作为 fallback，实际可调用 CryptCreateHash with CALG_SHA_256
    LOG_WARNING("SHA256 not implemented on Windows, fallback to MD5");
    return computeMD5(filePath);
}
#else
#include <openssl/md5.h>
#include <openssl/sha.h>

std::string Transfer::computeMD5(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
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

std::string Transfer::computeSHA256(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return "";
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&ctx, buffer, file.gcount());
    }
    SHA256_Update(&ctx, buffer, file.gcount());
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &ctx);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return oss.str();
}
#endif

} // namespace winuxlink