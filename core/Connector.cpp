#include "Connector.h"

#include <atomic>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <csignal>
#include <stdexcept>
#include "Logger.h"
#include "Network.h"

namespace winuxlink
{

/* ============== Connector impl 层实现 ============== */
class Connector::impl
{
public:
    impl(ConnectorRole role, NW_SOCKET socket, NW_SOCKET_INFO& info);
    ~impl();

    std::string getAddress() const;
    int getPort() const;
    ConnectorRole getRole() const;
    NW_SOCKET getSocket() const;
    bool reset();
    bool nw_send(const std::vector<uint8_t>& data);
    bool nw_send(const std::string& data);
    bool isConnected() const;
    void setRecvCallback(std::function<bool(std::vector<uint8_t>&)> callback);
    

private:
    ConnectorRole       socket_role;    // 连接器的角色
    NW_SOCKET           socket_id;      // 连接器的socket
    NW_SOCKET_INFO      socket_info;    // 连接器的socket信息   
    std::atomic<bool>   is_connected;   // 连接器是否已连接
    std::unique_ptr<std::thread> recv_thread;    // 接收线程
    std::mutex                   recv_mutex;     // 接收线程互斥锁
    std::function<bool(std::vector<uint8_t>&)> recv_callback; // 接收回调函数

};

/* ============== Connector 层实现 ============== */
Connector::Connector(ConnectorRole role, NW_SOCKET socket, NW_SOCKET_INFO& info)
{
    pimpl = std::make_unique<impl>(role, socket, info);
}
Connector::~Connector()
{
    pimpl->reset();
}
/*
    * 发brief 发送数据
    * @param data 要发送的数据
    * @return true 发送成功
    * @return false 发送失败
    */
bool Connector::nw_send(const std::vector<uint8_t>& data)
{
    return pimpl->nw_send(data);    
}

bool Connector::nw_send(const std::string& data)
{
    return pimpl->nw_send(data);
}



/*
    * 发brief 检查连接是否已建立
    * @return true 连接已建立
    * @return false 连接未建立
    */
bool Connector::isConnected() const
{
    return pimpl->isConnected();
}
/*
    * 发brief 获取连接器的角色
    * @return ConnectorRole 连接器的角色
    */
ConnectorRole Connector::getRole() const
{
    return pimpl->getRole();
}
/*
    * 发brief 关闭连接器
    * @return true 关闭成功
    * @return false 关闭失败
    */
bool Connector::reset()
{
    return pimpl->reset();
}
/*
    * 发brief 获取连接器的IP地址
    * @return std::string 连接器的IP地址
*/
std::string Connector::getAddress() const
{
    return pimpl->getAddress();
}
/*
    * 发brief 获取连接器的端口号
    * @return int 连接器的端口号
    */
int Connector::getPort() const
{
    return pimpl->getPort();
}
/*
    * 发brief 获取连接器的套接字
    * @return NW_SOCKET 连接器的套接字
    */
NW_SOCKET Connector::getSocket() const
{
    return pimpl->getSocket();
}

/*
    * 发brief 设置接收回调函数
    * @param callback 接收回调函数
    */
void Connector::setRecvCallback(std::function<bool(std::vector<uint8_t>&)> callback)
{
    pimpl->setRecvCallback(callback);
}


/* ============== Connector impl 层实现 ============== */
Connector::impl::impl(ConnectorRole role, NW_SOCKET socket, NW_SOCKET_INFO& info)
{
    this->socket_role = role;
    this->socket_id = socket;
    this->socket_info = info;
    this->is_connected.store(true);
    this->recv_callback = nullptr;
    this->recv_thread = nullptr;
}

Connector::impl::~impl()
{
    reset();
}

bool Connector::impl::reset()
{
    // 防止重复清理
    bool expected = true;
    if (!is_connected.compare_exchange_strong(expected, false)) {
        return false;   // 已经 reset 过
    }

    // 关闭socket
    if (socket_id != NW_SOCKET_INVALID)
    {
        nw_shutdown(socket_id);
        socket_id = NW_SOCKET_INVALID;
    }

    // 重置连接器状态
    is_connected.store(false);

    std::lock_guard<std::mutex> lock(recv_mutex);
    recv_callback = nullptr;

    // 等待接收线程结束
    if(recv_thread!=nullptr)
    {
        recv_thread->join();
    }
    LOG_INFO("Connector reset done...");
    return true;
}

std::string Connector::impl::getAddress() const
{
    return inet_ntoa(socket_info.sin_addr);
}
int Connector::impl::getPort() const
{
    return ntohs(socket_info.sin_port);
}
ConnectorRole Connector::impl::getRole() const
{
    return socket_role;
}
NW_SOCKET Connector::impl::getSocket() const
{
    return socket_id;
}
bool Connector::impl::nw_send(const std::vector<uint8_t>& data)
{
    if(is_connected.load())
    {
        return send(socket_id, (const char*)data.data(), data.size(), 0) != NW_SOCKET_ERROR;
    }
    return false;
}

bool Connector::impl::nw_send(const std::string& data)
{
    std::vector<uint8_t> data_vec(data.begin(), data.end());
    return nw_send(data_vec);
}

bool Connector::impl::isConnected() const
{
    return is_connected.load();
}
void Connector::impl::setRecvCallback(std::function<bool(std::vector<uint8_t>&)> callback)
{
    // callback 不能为空指针
    if(callback==nullptr)return;

    // 只有在连接已建立与回调函数未设置时，才设置回调函数
    if(is_connected.load()&&recv_callback==nullptr)
    {
        recv_callback = callback;

        // 只有在连接已建立与回调函数已设置时，才启动接收线程
        recv_thread = std::make_unique<std::thread>([this]()
        {
            // 设置 socket 为非阻塞
        #ifdef _WIN32
            u_long mode = 1;
            ioctlsocket(socket_id, FIONBIO, &mode);
        #else
            int flags = fcntl(socket_id, F_GETFL, 0);
            fcntl(socket_id, F_SETFL, flags | O_NONBLOCK);
        #endif

            // 接收循环
            std::vector<uint8_t> buffer(BUFFER_SIZE);
            while (is_connected.load() && Network::isSystemRunning()) {
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(socket_id, &readSet);

                struct timeval tv;
                tv.tv_sec = 1;   // 1秒超时，可调整
                tv.tv_usec = 0;

                int ret = select(static_cast<int>(socket_id) + 1, &readSet, nullptr, nullptr, &tv);
                if (ret < 0) {
                    // select 出错，socket 无效
                    is_connected.store(false);
                    break;
                }
                if (ret == 0) {
                    // 超时，可在此发送心跳（如果需要）
                    continue;
                }

                // 有数据可读
                int len = recv(socket_id, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
                if (len > 0) {
                    buffer.resize(len);
                    // 保护回调函数调用，避免竞态条件
                    std::lock_guard<std::mutex> lock(recv_mutex);
                    if (recv_callback) recv_callback(buffer);
                    
                    // 重置接收缓冲区
                    buffer.resize(BUFFER_SIZE);
                } else if (len == 0) {
                    // 对端正常关闭连接
                    is_connected.store(false);
                    break;
                } else {
                    // recv 返回 -1
                #ifdef _WIN32
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) {
                        continue;   // 非阻塞模式下无数据，正常
                    }
                #else
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                #endif
                    // 其他错误，视为连接断开
                    is_connected.store(false);
                    break;
                }
            }

            // 线程退出前关闭 socket
            if (socket_id != NW_SOCKET_INVALID) {
                nw_shutdown(socket_id);
                socket_id = NW_SOCKET_INVALID;
            }
        });
    }
    // 当连接已建立时，只能设置一次回调函数
    else if(recv_callback!=nullptr)
        throw std::runtime_error("setRecvCallback: callback already set");
    // 当连接未建立时，不能设置回调函数
    else
        throw std::runtime_error("setRecvCallback: connector not connected yet");
}

}
