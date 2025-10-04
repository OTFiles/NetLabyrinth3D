#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

class NetworkManager {
public:
    static NetworkManager& getInstance();
    
    // 初始化网络管理器
    bool initialize(int port);
    
    // 启动服务器
    bool startServer();
    
    // 停止服务器
    void stopServer();
    
    // 发送消息给指定客户端
    void sendToClient(int clientId, const std::string& message);
    
    // 广播消息给所有客户端
    void broadcast(const std::string& message);
    
    // 广播消息给除指定客户端外的所有客户端
    void broadcastExcept(int excludeClientId, const std::string& message);
    
    // 获取连接客户端数量
    int getConnectedClientsCount() const;
    
    // 断开指定客户端
    void disconnectClient(int clientId);
    
    // 设置消息回调函数
    void setMessageCallback(std::function<void(int, const std::string&)> callback);

private:
    NetworkManager();
    ~NetworkManager();
    
    // 禁止拷贝
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    
    // 内部实现
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // NETWORKMANAGER_H