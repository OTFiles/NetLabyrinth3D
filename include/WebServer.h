#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <sstream>

class WebServer {
public:
    static WebServer& getInstance();
    
    // 初始化Web服务器
    bool initialize(const std::string& webRootPath, int httpPort = 8080);
    
    // 启动HTTP服务器
    bool startServer();
    
    // 停止HTTP服务器
    void stopServer();
    
    // 获取HTTP端口
    int getHttpPort() const { return httpPort_; }
    
    // 设置Web根目录
    void setWebRootPath(const std::string& path) { webRootPath_ = path; }
    
    // 添加自定义路由
    void addRoute(const std::string& path, 
                  std::function<std::string(const std::string&)> handler);
    
    // 检查服务器是否运行中
    bool isRunning() const { return serverRunning_; }

private:
    WebServer();
    ~WebServer();
    
    // 禁止拷贝
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;
    
    // 服务器主循环
    void serverLoop();
    
    // 处理HTTP请求
    std::string handleRequest(const std::string& request);
    
    // 解析HTTP请求
    bool parseHttpRequest(const std::string& request, 
                         std::string& method, 
                         std::string& path, 
                         std::unordered_map<std::string, std::string>& headers);
    
    // 构建HTTP响应
    std::string buildHttpResponse(int statusCode, 
                                 const std::string& statusText,
                                 const std::string& content, 
                                 const std::string& contentType);
    
    // 获取MIME类型
    std::string getMimeType(const std::string& filePath) const;
    
    // 读取文件内容
    bool readFile(const std::string& filePath, std::string& content) const;
    
    // URL解码
    std::string urlDecode(const std::string& str) const;
    
    // 检查路径安全性
    bool isSafePath(const std::string& path) const;

private:
    std::string webRootPath_;
    int httpPort_;
    std::atomic<bool> serverRunning_;
    std::unique_ptr<std::thread> serverThread_;
    
    // 自定义路由处理
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> customRoutes_;
    
    // MIME类型映射
    std::unordered_map<std::string, std::string> mimeTypes_;
    
    mutable std::mutex mutex_;
};

#endif // WEBSERVER_H