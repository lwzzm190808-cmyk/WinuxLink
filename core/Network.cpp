#include "Network.h"
#include <thread>
#include <cstring>
#include <string>
#include <atomic>
#include <csignal>

namespace winuxlink
{


    // 全局运行标志，信号触发时设为 false
    std::atomic<bool> g_running{true};

    // 信号处理函数（异步安全）
    void signalHandler(int signum) {
        (void)signum;
        g_running.store(false, std::memory_order_relaxed);
    }

#ifdef _WIN32
    BOOL WINAPI consoleHandler(DWORD ctrlType) {
        if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
            g_running.store(false, std::memory_order_relaxed);
            return TRUE;
        }
        return FALSE;
    }
#endif

    


class Network::NetworkImpl
{
public:
    NetworkImpl();
    ~NetworkImpl();

    bool initServer(std::string ip, int port);
    bool initClient(std::string ip, int port);
    std::shared_ptr<Connector> getConnector(ConnectorRole role);
    bool stop(ConnectorRole role=ALL);

    bool isRunning(ConnectorRole role=ALL) const;
    int exec();
    void setNewClientConnector(std::function<void(std::shared_ptr<Connector> connector)> callback);

   private:
    // 服务端连接器
    std::shared_ptr<Connector> m_server_connector;
    std::vector<std::shared_ptr<Connector>> m_client_connectors; // 客户端连接器列表

    // 客户端连接器
    std::shared_ptr<Connector> m_client_connector;

    // 当前连接器角色
    ConnectorRole m_current_role;

    // 新连接回调函数
    std::function<void(std::shared_ptr<Connector>)> m_new_client_connector_callback;

    // accept线程
    std::unique_ptr<std::thread> m_accept_thread;
};
/*  ======================= 网络类 ======================= */

std::unique_ptr<Network::NetworkImpl> Network::m_network_impl = std::make_unique<Network::NetworkImpl>();
Network::Network()
{

}
Network::~Network()
{
    m_network_impl->stop();
}

std::shared_ptr<Connector> Network::server(std::string ip, int port)
{
    static Network s_network;
    if(!s_network.m_network_impl->isRunning(Server))
    {
        s_network.m_network_impl->initServer(ip,port);
        LOG_INFO("Server started on " + std::string(ip) + ":" + std::to_string(port));
    }
    return s_network.m_network_impl->getConnector(Server);
}
std::shared_ptr<Connector> Network::client(std::string ip, int port)
{
    static Network s_network;
    if(!s_network.m_network_impl->isRunning(Client))
    {
        s_network.m_network_impl->initClient(ip,port);
    }
    return s_network.m_network_impl->getConnector(Client);
}
bool Network::stop(ConnectorRole role)
{
    LOG_INFO("Network stop...");
    if(!isRunning(role))
    {
        return false;
    }
    switch(role)
    {
        case Server:
            LOG_INFO("Server stopped...");
            m_network_impl->stop(Server);
            break;
        case Client:
            LOG_INFO("Client stopped...");
            m_network_impl->stop(Client);
            break;
        case ALL:
            LOG_INFO("All connectors stopped...");
            m_network_impl->stop(ALL);
            break;
        default:
            LOG_ERROR("Invalid role: " + std::to_string(role));
            return false;
    }
    LOG_INFO("Network stop done final");
    return true;
}
bool Network::isRunning(ConnectorRole role)
{
    return m_network_impl->isRunning(role);
}
int Network::exec()
{
    return m_network_impl->exec();
}
void Network::setNewConnectorCallback(std::function<void(std::shared_ptr<Connector> connector)> callback)
{
    static Network s_network;
    s_network.m_network_impl->setNewClientConnector(callback);
}
bool Network::isSystemRunning() {
    return g_running.load(std::memory_order_relaxed);
}

/*  ======================= 网络实现类 ======================= */
Network::NetworkImpl::NetworkImpl()
{
    // 初始化信号处理函数
    static std::once_flag initFlag;
    // 注册信号处理函数
    std::call_once(initFlag, []() {
#ifdef _WIN32
        SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
        struct sigaction sa;
        sa.sa_handler = signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
#endif
    });


    // 初始化当前连接器角色为ALL
    m_current_role = ALL;
    // 初始化新连接回调函数为空
    m_new_client_connector_callback = nullptr;

    // 初始化accept线程为空
    m_accept_thread = nullptr;
    // 初始化客户端连接器列表为空
    m_client_connectors.clear();

    // 初始化服务端连接器为空
    m_server_connector = nullptr;

    // 初始化客户端连接器为空
    m_client_connector = nullptr;
}

Network::NetworkImpl::~NetworkImpl()
{
    stop();
}

bool Network::NetworkImpl::initServer(std::string ip, int port)
{
    // 初始化网络
    nw_init(); // 初始化网络库,如果为Windows,则需要调用WSAStartup初始化
    
    NW_SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == NW_SOCKET_INVALID)
    {
        LOG_ERROR("socket() failed: ");
        nw_uninit();
        server_socket = NW_SOCKET_INVALID;
        return false;
    }

    LOG_INFO("Server initialized...");

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(server_socket, FIONBIO, &mode);
#else
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
#endif

    // 绑定地址
    NW_SOCKET_INFO server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    server_addr.sin_port = htons(port);
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        LOG_ERROR("bind() failed: ");
        nw_uninit();
        nw_shutdown(server_socket);
        server_socket = NW_SOCKET_INVALID;
        return false;
    }

    LOG_INFO("Server bound address successfully");

    // 监听连接
    if (listen(server_socket, 5) == -1)
    {
        LOG_ERROR("listen() failed: ");
        nw_uninit();
        nw_shutdown(server_socket);
        server_socket = NW_SOCKET_INVALID;
        return false;
    }

    LOG_INFO("Server listening...");

    // 创建服务端连接器
    m_server_connector = std::make_shared<Connector>(ConnectorRole::Server,server_socket,server_addr);
    return true;
}


bool Network::NetworkImpl::initClient(std::string ip, int port)
{
    // 初始化网络
    nw_init(); // 初始化网络库,如果为Windows,则需要调用WSAStartup初始化
    LOG_INFO("Client initialized...");

    NW_SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == NW_SOCKET_INVALID)
    {
        LOG_ERROR("socket() failed: ");
        nw_uninit();
        client_socket = NW_SOCKET_INVALID;
        return false;
    }
    
    // 连接服务器
    NW_SOCKET_INFO server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    server_addr.sin_port = htons(port);
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        LOG_ERROR("connect() failed: ");
        nw_uninit();
        nw_shutdown(client_socket);
        client_socket = NW_SOCKET_INVALID;
        return false;
    }
    LOG_INFO("Client connected to " + std::string(ip) + ":" + std::to_string(port));

    // 创建客户端连接器
    m_client_connector = std::make_shared<Connector>(ConnectorRole::Client,client_socket,server_addr);
    return true;
}

std::shared_ptr<Connector> Network::NetworkImpl::getConnector(ConnectorRole role)
{
    switch(role)
    {
        case Server:
            return m_server_connector;
        case Client:
            return m_client_connector;
        default:
            return nullptr;
    }
}

bool Network::NetworkImpl::stop(ConnectorRole role)
{
    LOG_INFO("Network impl stop...");

    if (m_server_connector != nullptr)
    {
        if (role == ALL)
        {
            LOG_INFO("Network stop all...");
            m_server_connector->reset();
            m_client_connector->reset();

            // 关闭所有客户端连接器
            for (auto& connector : m_client_connectors)
            {
                LOG_INFO("Network stop client connector...");
                connector->reset();
                LOG_INFO("Network stop client connector done...");
            }
            m_client_connectors.clear();
        }
        else if (role == Server)
            m_server_connector->reset();
        else if (role == Client)
            m_client_connector->reset();
        
        LOG_INFO("Network stop done...");
        return true;
    }
    return false;
}
bool Network::NetworkImpl::isRunning(ConnectorRole role) const
{

    // 检查是否连接器是否存在
    switch(role)
    {
        case ALL:
            // 检查连接器是否存在
            if (m_server_connector == nullptr || m_client_connector == nullptr)
                return false;

            return m_server_connector->isConnected() || m_client_connector->isConnected();
        case Server:
            // 检查连接器是否存在
            if (m_server_connector == nullptr)
                return false;
            return m_server_connector->isConnected();
        case Client:
            // 检查连接器是否存在
            if (m_client_connector == nullptr)
                return false;
            return m_client_connector->isConnected();
        default:
            return false;
    }
}
int Network::NetworkImpl::exec()
{
    // 判断网络角色
    if(m_current_role == ConnectorRole::Client)
        throw std::runtime_error("Client role is not supported.");

    // 开启accept线程
    if (m_accept_thread == nullptr)
    {
        m_accept_thread = std::make_unique<std::thread>([this]()
        {
            // accept流程
            while(isRunning(Server) && isSystemRunning())   
            {
                NW_SOCKET_INFO client_addr;
                nw_size_t size = nw_sizeof(client_addr);
                NW_SOCKET client_socket = accept(m_server_connector->getSocket(), (struct sockaddr*)&client_addr, &size); 
                if (client_socket == NW_SOCKET_INVALID)
                {
                    if (!Network::isSystemRunning()) {
                        LOG_INFO("accept loop exiting due to system signal.");
                        break;
                    }
                    // 非阻塞模式下的正常无连接状态，稍作休眠避免 CPU 空转
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                LOG_INFO("Client connected from " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" + std::to_string(ntohs(client_addr.sin_port)));

                // 创建客户端连接器
                std::shared_ptr<Connector> connector = std::make_shared<Connector>(ConnectorRole::Client,client_socket,client_addr);
                m_client_connectors.push_back(connector);

                // 如果有新连接回调,则调用
                if (m_new_client_connector_callback != nullptr)
                    m_new_client_connector_callback(connector);
            }
        });

        m_accept_thread->join();
    }
    return 0;
}
void Network::NetworkImpl::setNewClientConnector(std::function<void(std::shared_ptr<Connector> connector)> callback)
{
    m_new_client_connector_callback = callback;
}

}