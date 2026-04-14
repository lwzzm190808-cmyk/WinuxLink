#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include "Connector.h"
#include "Agreement.h"

namespace winuxlink {

// 进度回调：当前已传输字节数，总字节数，是否完成
using ProgressCallback = std::function<void(uint64_t current, uint64_t total, bool done)>;

// 传输选项
struct TransferOptions {
    size_t chunkSize = 1024 * 1024;      // 1MB 每块
    size_t maxConcurrentChunks = 1;      // 并发块数（当前版本仅支持串行）
    uint64_t speedLimitBps = 0;          // 限速（字节/秒），0 表示不限速
    bool enableResume = true;             // 启用断点续传
    bool verifyChecksum = true;           // 校验文件完整性
    std::string checksumType = "md5";     // "md5" 或 "sha256"
    std::string resumeStateFile;          // 断点续传状态文件路径（为空则自动生成）
};

class Transfer {
public:
    Transfer(std::shared_ptr<Connector> conn);
    ~Transfer();

    // 下载文件（远程 -> 本地）
    bool download(const std::string& remotePath,
                  const std::string& localPath,
                  const TransferOptions& opts = TransferOptions(),
                  ProgressCallback progress = nullptr);

    // 上传文件（本地 -> 远程）
    bool upload(const std::string& localPath,
                const std::string& remotePath,
                const TransferOptions& opts = TransferOptions(),
                ProgressCallback progress = nullptr);

    // 取消当前传输（异步）
    void cancel();

    // 静态工具：计算文件哈希
    static std::string computeMD5(const std::string& filePath);
    static std::string computeSHA256(const std::string& filePath);

private:
    std::shared_ptr<Connector> m_conn;
    std::atomic<bool> m_cancelled;
    std::mutex m_mutex;

    // 获取远程文件大小和哈希（通过 FileInfoCommand）
    uint64_t getRemoteFileSize(const std::string& remotePath);
    std::string getRemoteFileHash(const std::string& remotePath, const std::string& type);

    // 断点续传状态保存/加载
    bool loadResumeState(const std::string& stateFile, uint64_t& completedOffset);
    void saveResumeState(const std::string& stateFile, uint64_t completedOffset);

    // 限速控制
    class RateLimiter {
        uint64_t m_bps;
        std::chrono::steady_clock::time_point m_lastTime;
        uint64_t m_bytesSent;
    public:
        RateLimiter(uint64_t bps) : m_bps(bps), m_bytesSent(0) {}
        void throttle(size_t bytes);
    };

    // 发送分块请求并接收响应（内部调用 DownloadCommand 或 UploadCommand）
    bool sendChunkRequest(Agreement::Command cmd, const std::vector<uint8_t>& payload,
                          std::vector<uint8_t>& response, int timeoutMs = 30000);
};

} // namespace winuxlink