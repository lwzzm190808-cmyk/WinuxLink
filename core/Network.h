#pragma once

#include <vector>
#include <memory>
#include <functional>
#include "Connector.h"
#include "Logger.h"

namespace winuxlink
{
class Network
{
public:
    ~Network();
    static std::shared_ptr<Connector> server(std::string ip="0.0.0.0", int port=8899);
    static std::shared_ptr<Connector> client(std::string ip, int port=8899);
    static bool stop(ConnectorRole role=ALL);
    static bool isRunning(ConnectorRole role=ALL);
    static int exec();
    static void setNewConnectorCallback(std::function<void(std::shared_ptr<Connector> connector)> callback);
    static bool isSystemRunning();
private:
    Network();
    Network(const Network&) = delete;
    Network& operator=(const Network&) = delete;

    class NetworkImpl;
    static std::unique_ptr<NetworkImpl> m_network_impl;
};

}