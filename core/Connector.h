#pragma once 
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include "NetworkPremise.h"

namespace winuxlink
{
enum ConnectorRole{Requester,Responder,Server,Client,ALL,INVALID};
class Connector
{
public:
/* === 构造函数和析构函数 === */
    Connector(ConnectorRole role, NW_SOCKET socket, NW_SOCKET_INFO& info);
    ~Connector();
    Connector(Connector&& other) =delete;
    Connector(const Connector&) = delete;
    Connector& operator=(const Connector&) = delete;

    /*
     * 发brief 发送数据
     * @param data 要发送的数据
     * @return true 发送成功
     * @return false 发送失败
     */
    bool nw_send(const std::vector<uint8_t>& data);
    bool nw_send(const std::string& data);
    /*
     * 发brief 检查连接是否已建立
     * @return true 连接已建立
     * @return false 连接未建立
     */
    bool isConnected() const;
    /*
     * 发brief 获取连接器的角色
     * @return ConnectorRole 连接器的角色
     */
    ConnectorRole getRole() const;
    /*
     * 发brief 关闭连接器
     * @return true 关闭成功
     * @return false 关闭失败
     */
    bool reset();
    /*
     * 发brief 获取连接器的IP地址
     * @return std::string 连接器的IP地址
    */
   std::string getAddress() const;
    /*
     * 发brief 获取连接器的端口号
     * @return int 连接器的端口号
     */
    int getPort() const;
    /**
     * @brief 获取连接器的套接字
     * @return NW_SOCKET 连接器的套接字
     */
    NW_SOCKET getSocket() const;
    /*
     * 发brief 设置接收回调函数
     * @param callback 接收回调函数
     */
    void setRecvCallback(std::function<bool(std::vector<uint8_t>&)> callback);

private:
    class impl;
    std::unique_ptr<impl> pimpl;
};

}