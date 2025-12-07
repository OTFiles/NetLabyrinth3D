#include "NetworkManager.h"
#include "Logger.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <openssl/sha.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

// WebSocket帧类型
enum WebSocketOpcode {
    CONTINUATION_FRAME = 0x0,
    TEXT_FRAME = 0x1,
    BINARY_FRAME = 0x2,
    CLOSE_FRAME = 0x8,
    PING_FRAME = 0x9,
    PONG_FRAME = 0xA
};

// 客户端连接信息
struct ClientConnection {
    int clientId;
    SOCKET socket;
    std::string ipAddress;
    bool handshakeCompleted;
};

class NetworkManager::Impl {
public:
    std::atomic<bool> running{false};
    int serverPort;
    SOCKET serverSocket;
    std::thread serverThread;
    std::mutex clientsMutex;
    std::unordered_map<int, ClientConnection> clients;
    std::function<void(int, const std::string&)> messageCallback;
    std::atomic<int> nextClientId{1};
    
    // WebSocket常量
    static const std::string WEB_SOCKET_GUID;
    
    // 服务器线程函数
    void serverThreadFunc();
    
    // 处理新连接
    void handleNewConnection(SOCKET clientSocket, const std::string& clientIp);
    
    // 处理客户端数据
    void handleClientData(int clientId);
    
    // WebSocket握手
    bool performWebSocketHandshake(SOCKET clientSocket, const std::string& request);
    
    // WebSocket帧编码/解码
    std::vector<uint8_t> encodeWebSocketFrame(const std::string& message);
    std::string decodeWebSocketFrame(const std::vector<uint8_t>& data);
    
    // 发送原始数据
    bool sendRawData(SOCKET socket, const std::vector<uint8_t>& data);
    bool sendRawData(SOCKET socket, const std::string& data);
    
    // Base64编码
    std::string base64Encode(const std::string& input);
};

const std::string NetworkManager::Impl::WEB_SOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

NetworkManager::NetworkManager() : m_impl(std::make_unique<Impl>()) {}

NetworkManager::~NetworkManager() {
    stopServer();
}

NetworkManager& NetworkManager::getInstance() {
    static NetworkManager instance;
    return instance;
}

bool NetworkManager::initialize(int port) {
    m_impl->serverPort = port;
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Logger::getInstance().error(LogCategory::NETWORK, "WSAStartup failed");
        return false;
    }
#endif

    Logger::getInstance().info(LogCategory::NETWORK, "Network manager initialized for port " + std::to_string(port));
    return true;
}

void NetworkManager::Impl::serverThreadFunc() {
    fd_set readfds;
    
    while (running) {
        FD_ZERO(&readfds);
        
        int maxSocket = -1;
        
        // 添加服务器socket
        if (serverSocket != INVALID_SOCKET) {
            FD_SET(serverSocket, &readfds);
            maxSocket = serverSocket;
        }
        
        // 添加所有客户端socket
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (const auto& pair : clients) {
                if (pair.second.socket != INVALID_SOCKET) {
                    FD_SET(pair.second.socket, &readfds);
                    if (pair.second.socket > maxSocket) {
                        maxSocket = pair.second.socket;
                    }
                }
            }
        }
        
        // 如果没有有效的socket，等待一段时间后继续
        if (maxSocket == -1) {
            // 减少睡眠时间以提高响应性
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        
        // 设置超时 - 减少超时时间以提高响应性
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000; // 50ms
        
        int activity = select(maxSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        // 检查是否收到停止信号
        if (!running) {
            break;
        }
        
        if (activity < 0 && running) {
            // 在非Windows系统上，EINTR是正常的中断，不需要记录错误
#ifdef _WIN32
            Logger::getInstance().error(LogCategory::NETWORK, "select error");
#else
            if (errno != EINTR) {
                Logger::getInstance().error(LogCategory::NETWORK, "select error: " + std::string(strerror(errno)));
            }
#endif
            continue;
        } else if (activity == 0) {
            // 超时，继续循环检查running状态
            continue;
        }
        
        // 检查新连接
        if (FD_ISSET(serverSocket, &readfds)) {
            sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
            
            if (clientSocket != INVALID_SOCKET) {
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
                
                handleNewConnection(clientSocket, clientIP);
            }
        }
        
        // 检查客户端数据
        std::vector<int> clientsToRemove;
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (const auto& pair : clients) {
                if (pair.second.socket != INVALID_SOCKET && FD_ISSET(pair.second.socket, &readfds)) {
                    handleClientData(pair.first);
                }
            }
        }
        
        // 移除断开的客户端
        for (int clientId : clientsToRemove) {
            // 通过外部类实例调用disconnectClient
            NetworkManager::getInstance().disconnectClient(clientId);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void NetworkManager::Impl::handleNewConnection(SOCKET clientSocket, const std::string& clientIp) {
    // 设置非阻塞模式
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
#else
    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    // 读取握手请求 - 使用更宽松的超时和缓冲区
    std::string request;
    std::vector<uint8_t> buffer(4096); // 更大的缓冲区
    int totalBytesReceived = 0;
    const int maxAttempts = 20; // 更多尝试次数
    int attempts = 0;
    bool gotCompleteRequest = false;
    
    while (attempts < maxAttempts && !gotCompleteRequest) {
        int bytesReceived = recv(clientSocket, (char*)buffer.data(), buffer.size(), 0);
        
        if (bytesReceived > 0) {
            request.append(buffer.begin(), buffer.begin() + bytesReceived);
            totalBytesReceived += bytesReceived;
            
            // 检查是否收到完整的HTTP请求（以\r\n\r\n结束）
            if (request.find("\r\n\r\n") != std::string::npos) {
                gotCompleteRequest = true;
                break; // 收到完整请求
            }
            
            // 如果请求过大，可能有问题
            if (totalBytesReceived > 8192) {
                Logger::getInstance().warning(LogCategory::NETWORK, 
                    "请求过大 - IP: " + clientIp + " - 长度: " + std::to_string(totalBytesReceived));
                break;
            }
        } else if (bytesReceived == 0) {
            // 连接关闭
            closesocket(clientSocket);
            return;
        } else {
            // 错误或没有数据可读
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
#else
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
#endif
                closesocket(clientSocket);
                return;
            }
        }
        
        attempts++;
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 更长的等待时间
    }

    if (totalBytesReceived > 0 && !request.empty()) {
        Logger::getInstance().debug(LogCategory::NETWORK, 
            "收到握手请求 - IP: " + clientIp + " - 长度: " + std::to_string(request.length()));
        
        if (performWebSocketHandshake(clientSocket, request)) {
            int clientId = nextClientId++;
            
            ClientConnection client;
            client.clientId = clientId;
            client.socket = clientSocket;
            client.ipAddress = clientIp;
            client.handshakeCompleted = true;
            
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients[clientId] = client;
            }
            
            // 详细记录连接信息
            Logger::getInstance().info(LogCategory::NETWORK, 
                "WebSocket客户端连接 - IP: " + clientIp + 
                " | 客户端ID: " + std::to_string(clientId) + 
                " | 当前连接数: " + std::to_string(clients.size()));
            
            // 不再发送CONNECT消息，等待客户端发送认证消息
        } else {
            Logger::getInstance().warning(LogCategory::NETWORK, 
                "WebSocket握手失败 - IP: " + clientIp);
            
            // 发送HTTP 400响应
            std::string response = 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 23\r\n"
                "Connection: close\r\n\r\n"
                "Invalid WebSocket request";
            sendRawData(clientSocket, response);
            
            closesocket(clientSocket);
        }
    } else {
        Logger::getInstance().warning(LogCategory::NETWORK, 
            "未收到握手请求 - IP: " + clientIp);
        closesocket(clientSocket);
    }
}

bool NetworkManager::Impl::performWebSocketHandshake(SOCKET clientSocket, const std::string& request) {
    // 检查是否是WebSocket升级请求
    if (request.find("GET") != 0) {
        Logger::getInstance().debug(LogCategory::NETWORK, "握手失败: 不是GET请求");
        return false;
    }
    
    // 更宽松的头部检查 - 使用不区分大小写的查找
    std::string requestLower = request;
    std::transform(requestLower.begin(), requestLower.end(), requestLower.begin(), ::tolower);
    
    // 检查Upgrade头
    if (requestLower.find("upgrade: websocket") == std::string::npos) {
        Logger::getInstance().debug(LogCategory::NETWORK, "握手失败: 缺少Upgrade头");
        return false;
    }
    
    // 检查Connection头 - 更宽松的匹配
    if (requestLower.find("connection: upgrade") == std::string::npos && 
        requestLower.find("connection:") != std::string::npos && 
        requestLower.find("upgrade") == std::string::npos) {
        Logger::getInstance().debug(LogCategory::NETWORK, "握手失败: 缺少Connection: Upgrade头");
        return false;
    }
    
    // 查找WebSocket Key - 不区分大小写
    std::string keyHeader = "sec-websocket-key: ";
    size_t keyStart = requestLower.find(keyHeader);
    if (keyStart == std::string::npos) {
        // 尝试大写版本
        keyHeader = "Sec-WebSocket-Key: ";
        keyStart = request.find(keyHeader);
        if (keyStart == std::string::npos) {
            Logger::getInstance().debug(LogCategory::NETWORK, "握手失败: 缺少Sec-WebSocket-Key头");
            return false;
        }
    }
    
    keyStart += keyHeader.length();
    size_t keyEnd = requestLower.find("\r\n", keyStart);
    if (keyEnd == std::string::npos) {
        keyEnd = requestLower.find("\n", keyStart);
        if (keyEnd == std::string::npos) {
            Logger::getInstance().debug(LogCategory::NETWORK, "握手失败: Sec-WebSocket-Key格式错误");
            return false;
        }
    }
    
    std::string webSocketKey = request.substr(keyStart, keyEnd - keyStart);
    // 去除可能的空格
    webSocketKey.erase(std::remove_if(webSocketKey.begin(), webSocketKey.end(), ::isspace), webSocketKey.end());
    
    // 检查WebSocket版本
    std::string versionHeader = "Sec-WebSocket-Version: ";
    size_t versionStart = request.find(versionHeader);
    if (versionStart != std::string::npos) {
        versionStart += versionHeader.length();
        size_t versionEnd = request.find("\r\n", versionStart);
        if (versionEnd != std::string::npos) {
            std::string version = request.substr(versionStart, versionEnd - versionStart);
            if (version != "13") {
                Logger::getInstance().warning(LogCategory::NETWORK, 
                    "不支持的WebSocket版本: " + version);
                return false;
            }
        }
    }
    
    // 计算Accept Key
    std::string combined = webSocketKey + WEB_SOCKET_GUID;
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.length(), hash);
    
    // Base64编码
    std::string acceptKey = base64Encode(std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH));
    
    // 发送握手响应 - 使用更完整的响应格式
    std::string response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n";
    
    // 检查是否需要包含Sec-WebSocket-Version
    if (requestLower.find("sec-websocket-version:") != std::string::npos) {
        response += "Sec-WebSocket-Version: 13\r\n";
    }
    
    // 添加额外的兼容性头部
    response += "Server: MazeGameServer/1.0\r\n";
    response += "\r\n";
    
    Logger::getInstance().debug(LogCategory::NETWORK, 
        "WebSocket握手成功 - Key: " + webSocketKey + " - Accept: " + acceptKey);
    
    bool success = sendRawData(clientSocket, response);
    if (!success) {
        Logger::getInstance().warning(LogCategory::NETWORK, "发送握手响应失败");
    }
    return success;
}

std::string NetworkManager::Impl::base64Encode(const std::string& input) {
    // 符合RFC 4648的base64编码实现
    const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string encoded;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = input.size();
    const char* bytes_to_encode = input.c_str();
    
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; i < 4; i++) {
                encoded += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for(j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; j < i + 1; j++) {
            encoded += base64_chars[char_array_4[j]];
        }
        
        while(i++ < 3) {
            encoded += '=';
        }
    }
    
    return encoded;
}

void NetworkManager::Impl::handleClientData(int clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(clientId);
    if (it == clients.end()) return;
    
    SOCKET clientSocket = it->second.socket;
    std::vector<uint8_t> buffer(4096);
    
    int bytesReceived = recv(clientSocket, (char*)buffer.data(), buffer.size(), 0);
    
    if (bytesReceived > 0) {
        buffer.resize(bytesReceived);
        
        if (it->second.handshakeCompleted) {
            // WebSocket消息
            std::string message = decodeWebSocketFrame(buffer);
            if (!message.empty()) {
                // 记录接收到的消息（限制长度以避免日志过大）
                std::string logMessage = message;
                if (logMessage.length() > 200) {
                    logMessage = logMessage.substr(0, 200) + "...[截断]";
                }
                Logger::getInstance().debug(LogCategory::NETWORK, 
                    "收到客户端消息 - ID: " + std::to_string(clientId) + 
                    " | IP: " + it->second.ipAddress + 
                    " | 长度: " + std::to_string(bytesReceived) + 
                    " | 内容: " + logMessage);
                
                if (messageCallback) {
                    messageCallback(clientId, message);
                }
            } else {
                Logger::getInstance().warning(LogCategory::NETWORK,
                    "WebSocket消息解码失败 - ID: " + std::to_string(clientId) +
                    " | IP: " + it->second.ipAddress);
            }
        }
    } else if (bytesReceived == 0) {
        // 正常断开连接
        closesocket(clientSocket);
        std::string ip = it->second.ipAddress;
        clients.erase(it);
        
        Logger::getInstance().info(LogCategory::NETWORK, 
            "客户端断开连接 - IP: " + ip + 
            " | ID: " + std::to_string(clientId) + 
            " | 剩余连接数: " + std::to_string(clients.size()));
        
        if (messageCallback) {
            messageCallback(clientId, "DISCONNECT");
        }
    } else {
        // 连接错误
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
#else
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
#endif
            closesocket(clientSocket);
            std::string ip = it->second.ipAddress;
            clients.erase(it);
            
            Logger::getInstance().warning(LogCategory::NETWORK, 
                "客户端连接异常断开 - IP: " + ip + 
                " | ID: " + std::to_string(clientId) + 
                " | 错误码: " + std::to_string(
#ifdef _WIN32
                error
#else
                errno
#endif
                ));
            
            if (messageCallback) {
                messageCallback(clientId, "DISCONNECT");
            }
        }
    }
}

std::vector<uint8_t> NetworkManager::Impl::encodeWebSocketFrame(const std::string& message) {
    std::vector<uint8_t> frame;
    
    // FIN位和文本帧操作码
    frame.push_back(0x81);
    
    // 载荷长度
    size_t length = message.length();
    if (length <= 125) {
        frame.push_back(static_cast<uint8_t>(length));
    } else if (length <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(length & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((length >> (8 * i)) & 0xFF));
        }
    }
    
    // 添加载荷数据
    for (char c : message) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    
    return frame;
}

std::string NetworkManager::Impl::decodeWebSocketFrame(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return "";
    
    // 检查FIN位和操作码
    bool fin = (data[0] & 0x80) != 0;
    uint8_t opcode = data[0] & 0x0F;
    
    if (!fin || opcode != TEXT_FRAME) {
        return ""; // 只处理完整的文本帧
    }
    
    // 解析载荷长度
    uint8_t secondByte = data[1];
    bool masked = (secondByte & 0x80) != 0;
    uint64_t payloadLength = secondByte & 0x7F;
    
    size_t index = 2;
    
    if (payloadLength == 126) {
        if (data.size() < 4) return "";
        payloadLength = (data[2] << 8) | data[3];
        index += 2;
    } else if (payloadLength == 127) {
        if (data.size() < 10) return "";
        payloadLength = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLength = (payloadLength << 8) | data[index++];
        }
    }
    
    // 解析掩码
    uint8_t maskingKey[4];
    if (masked) {
        if (data.size() < index + 4) return "";
        for (int i = 0; i < 4; ++i) {
            maskingKey[i] = data[index++];
        }
    }
    
    // 解码载荷
    if (data.size() < index + payloadLength) return "";
    
    std::string decoded;
    for (size_t i = 0; i < payloadLength; ++i) {
        uint8_t byte = data[index + i];
        if (masked) {
            byte ^= maskingKey[i % 4];
        }
        decoded += static_cast<char>(byte);
    }
    
    return decoded;
}

bool NetworkManager::Impl::sendRawData(SOCKET socket, const std::vector<uint8_t>& data) {
    return sendRawData(socket, std::string(data.begin(), data.end()));
}

bool NetworkManager::Impl::sendRawData(SOCKET socket, const std::string& data) {
    int totalSent = 0;
    while (totalSent < data.length()) {
        int sent = send(socket, data.c_str() + totalSent, data.length() - totalSent, 0);
        if (sent <= 0) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

bool NetworkManager::startServer() {
    if (m_impl->running) {
        return true;
    }

    // 创建服务器socket
    m_impl->serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_impl->serverSocket == INVALID_SOCKET) {
        Logger::getInstance().error(LogCategory::NETWORK, "Failed to create server socket");
        return false;
    }

    // 设置socket选项
    int opt = 1;
#ifdef _WIN32
    if (setsockopt(m_impl->serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
#else
    if (setsockopt(m_impl->serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
#endif
        Logger::getInstance().error(LogCategory::NETWORK, "setsockopt failed");
        closesocket(m_impl->serverSocket);
        return false;
    }

    // 绑定地址和端口
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(m_impl->serverPort);

    if (bind(m_impl->serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        Logger::getInstance().error(LogCategory::NETWORK, 
            "Bind failed on port " + std::to_string(m_impl->serverPort));
        closesocket(m_impl->serverSocket);
        return false;
    }

    // 开始监听
    if (listen(m_impl->serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        Logger::getInstance().error(LogCategory::NETWORK, "Listen failed");
        closesocket(m_impl->serverSocket);
        return false;
    }

    // 设置非阻塞模式
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(m_impl->serverSocket, FIONBIO, &mode) != 0) {
#else
    int flags = fcntl(m_impl->serverSocket, F_GETFL, 0);
    if (fcntl(m_impl->serverSocket, F_SETFL, flags | O_NONBLOCK) < 0) {
#endif
        Logger::getInstance().error(LogCategory::NETWORK, "Failed to set non-blocking mode");
        closesocket(m_impl->serverSocket);
        return false;
    }

    m_impl->running = true;
    m_impl->serverThread = std::thread(&NetworkManager::Impl::serverThreadFunc, m_impl.get());
    
    Logger::getInstance().info(LogCategory::NETWORK, 
        "WebSocket server started on port " + std::to_string(m_impl->serverPort));
    return true;
}

void NetworkManager::stopServer() {
    if (!m_impl->running) {
        return;
    }

    // 首先设置停止标志
    m_impl->running = false;
    
    // 先唤醒select循环
    if (m_impl->serverSocket != INVALID_SOCKET) {
        // 创建一个临时连接来唤醒select
        SOCKET wakeupSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (wakeupSocket != INVALID_SOCKET) {
            // 设置为非阻塞
#ifdef _WIN32
            u_long mode = 1;
            ioctlsocket(wakeupSocket, FIONBIO, &mode);
#else
            int flags = fcntl(wakeupSocket, F_GETFL, 0);
            fcntl(wakeupSocket, F_SETFL, flags | O_NONBLOCK);
#endif
            
            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            serverAddr.sin_port = htons(m_impl->serverPort);
            
            // 尝试连接，只是为了唤醒select
            connect(wakeupSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
            closesocket(wakeupSocket);
        }
    }
    
    // 给服务器线程一些时间来响应停止信号
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
        for (const auto& pair : m_impl->clients) {
            if (pair.second.socket != INVALID_SOCKET) {
                closesocket(pair.second.socket);
            }
        }
        m_impl->clients.clear();
    }
    
    // 关闭服务器socket
    if (m_impl->serverSocket != INVALID_SOCKET) {
        closesocket(m_impl->serverSocket);
        m_impl->serverSocket = INVALID_SOCKET;
    }
    
    // 等待服务器线程结束，但设置合理的超时
    if (m_impl->serverThread.joinable()) {
        auto startTime = std::chrono::steady_clock::now();
        const auto maxWaitTime = std::chrono::milliseconds(1500); // 1.5秒超时
        
        bool threadJoined = false;
        while (!threadJoined) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > maxWaitTime) {
                // 超时，记录警告并分离线程
                m_impl->serverThread.detach();
                Logger::getInstance().warning(LogCategory::NETWORK, "WebSocket server thread detached after timeout");
                threadJoined = true;
                break;
            }
            
            // 尝试等待线程结束
            try {
                // 使用短时间睡眠避免CPU占用过高
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                // 检查线程是否已经结束
                // 由于没有非阻塞join，我们使用超时机制
                if (elapsed > std::chrono::milliseconds(500)) {
                    // 超过500ms后，尝试join（可能会阻塞，但应该很快返回）
                    m_impl->serverThread.join();
                    threadJoined = true;
                }
            } catch (const std::exception& e) {
                Logger::getInstance().error(LogCategory::NETWORK, "Exception while waiting for server thread: " + std::string(e.what()));
                m_impl->serverThread.detach();
                threadJoined = true;
            }
        }
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    Logger::getInstance().info(LogCategory::NETWORK, "WebSocket server stopped");
}

void NetworkManager::sendToClient(int clientId, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    auto it = m_impl->clients.find(clientId);
    if (it != m_impl->clients.end()) {
        std::vector<uint8_t> frame = m_impl->encodeWebSocketFrame(message);
        m_impl->sendRawData(it->second.socket, frame);
        
        Logger::getInstance().debug(LogCategory::NETWORK, 
            "Sent message to client " + std::to_string(clientId) + ": " + message);
    }
}

void NetworkManager::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    std::vector<uint8_t> frame = m_impl->encodeWebSocketFrame(message);
    
    for (const auto& pair : m_impl->clients) {
        m_impl->sendRawData(pair.second.socket, frame);
    }
    
    Logger::getInstance().debug(LogCategory::NETWORK, 
        "Broadcast message to " + std::to_string(m_impl->clients.size()) + " clients: " + message);
}

void NetworkManager::broadcastExcept(int excludeClientId, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    std::vector<uint8_t> frame = m_impl->encodeWebSocketFrame(message);
    
    int count = 0;
    for (const auto& pair : m_impl->clients) {
        if (pair.first != excludeClientId) {
            m_impl->sendRawData(pair.second.socket, frame);
            count++;
        }
    }
    
    Logger::getInstance().debug(LogCategory::NETWORK, 
        "Broadcast message to " + std::to_string(count) + " clients (excluding " + 
        std::to_string(excludeClientId) + "): " + message);
}

int NetworkManager::getConnectedClientsCount() const {
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    return m_impl->clients.size();
}

void NetworkManager::disconnectClient(int clientId) {
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    auto it = m_impl->clients.find(clientId);
    if (it != m_impl->clients.end()) {
        closesocket(it->second.socket);
        m_impl->clients.erase(it);
        
        Logger::getInstance().info(LogCategory::NETWORK, 
            "Forcefully disconnected client " + std::to_string(clientId));
        
        if (m_impl->messageCallback) {
            m_impl->messageCallback(clientId, "DISCONNECT");
        }
    }
}

void NetworkManager::setMessageCallback(std::function<void(int, const std::string&)> callback) {
    m_impl->messageCallback = callback;
}