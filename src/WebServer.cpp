#include "WebServer.h"
#include "Logger.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstring>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

WebServer::WebServer() 
    : httpPort_(8080)
    , serverRunning_(false) {
    
    // 初始化MIME类型映射
    mimeTypes_[".html"] = "text/html; charset=utf-8";
    mimeTypes_[".htm"] = "text/html; charset=utf-8";
    mimeTypes_[".css"] = "text/css; charset=utf-8";
    mimeTypes_[".js"] = "application/javascript; charset=utf-8";
    mimeTypes_[".json"] = "application/json; charset=utf-8";
    mimeTypes_[".png"] = "image/png";
    mimeTypes_[".jpg"] = "image/jpeg";
    mimeTypes_[".jpeg"] = "image/jpeg";
    mimeTypes_[".gif"] = "image/gif";
    mimeTypes_[".ico"] = "image/x-icon";
    mimeTypes_[".txt"] = "text/plain; charset=utf-8";
    mimeTypes_[".xml"] = "application/xml; charset=utf-8";
    mimeTypes_[".pdf"] = "application/pdf";
    mimeTypes_[".zip"] = "application/zip";
    mimeTypes_[".mp3"] = "audio/mpeg";
    mimeTypes_[".mp4"] = "video/mp4";
}

WebServer::~WebServer() {
    stopServer();
}

WebServer& WebServer::getInstance() {
    static WebServer instance;
    return instance;
}

bool WebServer::initialize(const std::string& webRootPath, int httpPort) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    webRootPath_ = webRootPath;
    httpPort_ = httpPort;
    
    // 检查web根目录是否存在
    std::ifstream testFile(webRootPath_ + "/index.html");
    if (!testFile.is_open()) {
        Logger::getInstance().error(LogCategory::WEB, "WebServer: Web root directory not found: " + webRootPath_);
        return false;
    }
    testFile.close();
    
    Logger::getInstance().info(LogCategory::WEB, "WebServer: Initialized with web root: " + webRootPath_);
    return true;
}

bool WebServer::startServer() {
    if (serverRunning_) {
        Logger::getInstance().warning(LogCategory::WEB, "WebServer: Server is already running");
        return true;
    }
    
    serverRunning_ = true;
    serverThread_ = std::make_unique<std::thread>(&WebServer::serverLoop, this);
    
    Logger::getInstance().info(LogCategory::WEB, "WebServer: HTTP server started on port " + std::to_string(httpPort_));
    return true;
}

void WebServer::stopServer() {
    if (!serverRunning_) return;
    
    serverRunning_ = false;
    if (serverThread_ && serverThread_->joinable()) {
        serverThread_->join();
    }
    
    Logger::getInstance().info(LogCategory::WEB, "WebServer: HTTP server stopped");
}

void WebServer::serverLoop() {
#ifdef _WIN32
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Logger::getInstance().error(LogCategory::WEB, "WebServer: WSAStartup failed");
        return;
    }
#endif

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        Logger::getInstance().error(LogCategory::WEB, "WebServer: Failed to create socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    
    // 设置socket为非阻塞模式以便优雅关闭
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode);
#else
    int flags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    // 设置SO_REUSEADDR
    int opt = 1;
#ifdef _WIN32
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
#else
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
#endif
        Logger::getInstance().warning(LogCategory::WEB, "WebServer: setsockopt failed");
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(httpPort_);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        Logger::getInstance().error(LogCategory::WEB, "WebServer: Failed to bind to port " + std::to_string(httpPort_));
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return;
    }

    if (listen(serverSocket, 10) < 0) {
        Logger::getInstance().error(LogCategory::WEB, "WebServer: Failed to listen on socket");
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return;
    }

    Logger::getInstance().info(LogCategory::WEB, "WebServer: Listening on port " + std::to_string(httpPort_));

    while (serverRunning_) {
        // 设置超时以定期检查serverRunning_
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        
        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(serverSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (activity < 0) {
            Logger::getInstance().error(LogCategory::WEB, "WebServer: select error");
            break;
        }
        
        if (!serverRunning_) break;
        
        if (activity == 0) {
            // 超时，继续循环
            continue;
        }

        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (serverRunning_) {
                Logger::getInstance().error(LogCategory::WEB, "WebServer: Failed to accept client connection");
            }
            continue;
        }

        // 处理客户端请求
        char buffer[4096];
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string request(buffer);
            
            std::string response = handleRequest(request);
            
            send(clientSocket, response.c_str(), response.length(), 0);
        }

#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
    }

#ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
#else
    close(serverSocket);
#endif
}

std::string WebServer::handleRequest(const std::string& request) {
    std::string method, path;
    std::unordered_map<std::string, std::string> headers;
    
    if (!parseHttpRequest(request, method, path, headers)) {
        Logger::getInstance().warning(LogCategory::WEB, "HTTP请求解析失败: " + request.substr(0, 100));
        return buildHttpResponse(400, "Bad Request", "Invalid HTTP request", "text/plain");
    }
    
    // 记录HTTP访问日志
    std::string clientIP = "unknown";
    auto userAgentIt = headers.find("User-Agent");
    std::string userAgent = (userAgentIt != headers.end()) ? userAgentIt->second : "unknown";
    
    Logger::getInstance().info(LogCategory::WEB, 
        "HTTP请求 - 方法: " + method + 
        " | 路径: " + path + 
        " | User-Agent: " + userAgent);
    
    // 只处理GET请求
    if (method != "GET") {
        Logger::getInstance().warning(LogCategory::WEB, 
            "不支持的HTTP方法 - 方法: " + method + " | 路径: " + path);
        return buildHttpResponse(405, "Method Not Allowed", "Only GET method is supported", "text/plain");
    }
    
    // URL解码路径
    path = urlDecode(path);
    
    // 检查自定义路由
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = customRoutes_.find(path);
        if (it != customRoutes_.end()) {
            std::string customResponse = it->second(request);
            // 根据路径确定内容类型
            std::string contentType = "text/html";
            if (path.find("/api/") == 0) {
                contentType = "application/json; charset=utf-8";
            }
            
            Logger::getInstance().debug(LogCategory::WEB, 
                "处理API路由 - 路径: " + path + " | 响应长度: " + std::to_string(customResponse.length()));
            
            return buildHttpResponse(200, "OK", customResponse, contentType);
        }
    }
    
    // 默认路由处理
    if (path == "/") {
        path = "/index.html";
    }
    
    // 安全检查
    if (!isSafePath(path)) {
        Logger::getInstance().warning(LogCategory::WEB, 
            "路径安全检查失败 - 路径: " + path);
        return buildHttpResponse(403, "Forbidden", "Access denied", "text/plain");
    }
    
    // 构建完整文件路径
    std::string fullPath = webRootPath_ + path;
    
    // 读取文件内容
    std::string fileContent;
    if (!readFile(fullPath, fileContent)) {
        // 文件不存在，尝试添加.html扩展名
        if (fullPath.find('.') == std::string::npos) {
            fullPath += ".html";
            if (!readFile(fullPath, fileContent)) {
                Logger::getInstance().warning(LogCategory::WEB, 
                    "文件未找到 - 路径: " + path + " | 完整路径: " + fullPath);
                return buildHttpResponse(404, "Not Found", "File not found: " + path, "text/plain");
            }
        } else {
            Logger::getInstance().warning(LogCategory::WEB, 
                "文件未找到 - 路径: " + path + " | 完整路径: " + fullPath);
            return buildHttpResponse(404, "Not Found", "File not found: " + path, "text/plain");
        }
    }
    
    // 获取MIME类型
    std::string contentType = getMimeType(fullPath);
    
    Logger::getInstance().debug(LogCategory::WEB, 
        "提供静态文件 - 路径: " + path + 
        " | 类型: " + contentType + 
        " | 大小: " + std::to_string(fileContent.length()) + " bytes");
    
    return buildHttpResponse(200, "OK", fileContent, contentType);
}

bool WebServer::parseHttpRequest(const std::string& request, 
                                std::string& method, 
                                std::string& path, 
                                std::unordered_map<std::string, std::string>& headers) {
    std::istringstream stream(request);
    std::string line;
    
    // 解析请求行
    if (!std::getline(stream, line)) return false;
    
    std::istringstream requestLine(line);
    if (!(requestLine >> method >> path)) return false;
    
    // 解析headers
    while (std::getline(stream, line) && line != "\r") {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // 去除首尾空白字符
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            headers[key] = value;
        }
    }
    
    return true;
}

std::string WebServer::buildHttpResponse(int statusCode, 
                                        const std::string& statusText,
                                        const std::string& content, 
                                        const std::string& contentType) {
    std::stringstream response;
    
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "\r\n";
    response << content;
    
    return response.str();
}

std::string WebServer::getMimeType(const std::string& filePath) const {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string extension = filePath.substr(dotPos);
    auto it = mimeTypes_.find(extension);
    if (it != mimeTypes_.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

bool WebServer::readFile(const std::string& filePath, std::string& content) const {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    
    return true;
}

std::string WebServer::urlDecode(const std::string& str) const {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            std::string hex = str.substr(i + 1, 2);
            char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
            result += ch;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    
    return result;
}

bool WebServer::isSafePath(const std::string& path) const {
    // 防止目录遍历攻击
    if (path.find("..") != std::string::npos) {
        return false;
    }
    
    // 在Linux/Unix系统上，允许以/开头的路径，因为这是正常的URL路径
    // 我们只需要确保路径不会跳出web根目录
    std::string fullPath = webRootPath_ + path;
    
    // 检查完整路径是否以web根目录开头
    // 将路径转换为规范形式进行比较
    std::filesystem::path webRoot(webRootPath_);
    std::filesystem::path requestedPath(fullPath);
    
    try {
        std::filesystem::path canonicalWebRoot = std::filesystem::canonical(webRoot);
        std::filesystem::path canonicalRequested = std::filesystem::canonical(requestedPath);
        
        // 检查请求的路径是否在web根目录内
        auto [rootEnd, _] = std::mismatch(canonicalWebRoot.begin(), canonicalWebRoot.end(), canonicalRequested.begin());
        if (rootEnd != canonicalWebRoot.end()) {
            // 请求的路径不在web根目录内
            return false;
        }
    } catch (const std::exception& e) {
        // 如果无法规范化路径，使用简单的字符串比较
        std::string normalizedWebRoot = webRootPath_;
        std::string normalizedFullPath = fullPath;
        
        // 确保路径以/结尾用于比较
        if (!normalizedWebRoot.empty() && normalizedWebRoot.back() != '/') {
            normalizedWebRoot += '/';
        }
        
        if (normalizedFullPath.find(normalizedWebRoot) != 0) {
            return false;
        }
    }
    
    return true;
}

void WebServer::addRoute(const std::string& path, 
                         std::function<std::string(const std::string&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    customRoutes_[path] = handler;
    
    Logger::getInstance().info(LogCategory::WEB, "WebServer: Added custom route: " + path);
}