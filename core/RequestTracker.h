// core/RequestTracker.h
#pragma once

#include <unordered_map>
#include <mutex>
#include <future>
#include <chrono>

namespace winuxlink {

class RequestTracker {
public:
    // 注册一个新请求，返回分配的序列号
    uint32_t registerRequest() {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32_t seq = m_nextSeq++;
        m_pending.emplace(seq, PendingRequest{});
        return seq;
    }

    // 获取与序列号关联的 future
    std::future<std::vector<uint8_t>> getFuture(uint32_t seq) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pending.find(seq);
        if (it != m_pending.end()) {
            return it->second.promise.get_future();
        }
        throw std::runtime_error("Invalid sequence ID");
    }

    // 响应到达时履行承诺
    void fulfill(uint32_t seq, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pending.find(seq);
        if (it != m_pending.end()) {
            it->second.promise.set_value(data);
            m_pending.erase(it);
        }
    }

    // 取消请求（如超时）
    void cancel(uint32_t seq) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pending.find(seq);
        if (it != m_pending.end()) {
            it->second.promise.set_exception(
                std::make_exception_ptr(std::runtime_error("Request cancelled")));
            m_pending.erase(it);
        }
    }

private:
    struct PendingRequest {
        std::promise<std::vector<uint8_t>> promise;
        std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
    };

    std::unordered_map<uint32_t, PendingRequest> m_pending;
    std::mutex m_mutex;
    uint32_t m_nextSeq = 1;
};

} // namespace winuxlink