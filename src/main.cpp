#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <nlohmann/json.hpp>

#include "Logger.h"
#include "DataManager.h"
#include "MazeGenerator.h"
#include "GameLogic.h"
#include "PlayerManager.h"
#include "NetworkManager.h"
#include "CommandSystem.h"
#include "WebServer.h"
#include "GlobalState.h"

using json = nlohmann::json;

// 全局变量声明
extern std::atomic<bool> g_shutdownRequested;
extern std::atomic<bool> g_commandInputInProgress;
extern std::string g_currentInputLine;

// 平台相关的终端设置
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

// 跨平台终端设置类
class TerminalSettings {
private:
#ifdef _WIN32
    DWORD originalMode;
    HANDLE stdinHandle;
#else
    struct termios originalTermios;
#endif
    bool isSet;

public:
    TerminalSettings() : isSet(false) {
#ifdef _WIN32
        stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(stdinHandle, &originalMode);
#else
        tcgetattr(STDIN_FILENO, &originalTermios);
#endif
    }

    ~TerminalSettings() {
        restore();
    }

    void setRawMode() {
        if (isSet) return;
        
#ifdef _WIN32
        DWORD newMode = originalMode;
        newMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
        SetConsoleMode(stdinHandle, newMode);
#else
        struct termios newTermios = originalTermios;
        newTermios.c_lflag &= ~(ICANON | ECHO);
        newTermios.c_cc[VMIN] = 1;
        newTermios.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
#endif
        isSet = true;
    }

    void restore() {
        if (!isSet) return;
        
#ifdef _WIN32
        SetConsoleMode(stdinHandle, originalMode);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
#endif
        isSet = false;
    }
};

// 跨平台字符读取函数
bool readCharImmediate(char& ch) {
#ifdef _WIN32
    if (_kbhit()) {
        ch = _getch();
        return true;
    }
    return false;
#else
    struct termios oldt, newt;
    int ch_read;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;  // 非阻塞模式
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    ch_read = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    
    if (ch_read != EOF) {
        ch = static_cast<char>(ch_read);
        return true;
    }
    return false;
#endif
}

// 信号处理函数
void signalHandler(int signal) {
    std::cout << "\n接收到信号 " << signal << ", 正在关闭服务器..." << std::endl;
    g_shutdownRequested = true;
    
    // 强制退出命令输入模式
    if (g_commandInputInProgress) {
        // 恢复终端设置
        std::cout << "\n强制退出命令输入模式..." << std::endl;
    }
}

// 命令行参数解析
struct CommandLineArgs {
    int port = 8080;
    std::string dataPath = "./Data";
    std::string webRoot = "./web";
    bool enableConsoleLog = true;
    bool enableFileLog = true;
    LogLevel logLevel = LogLevel::INFO;
};

CommandLineArgs parseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                args.port = std::stoi(argv[++i]);
            }
        } else if (arg == "-d" || arg == "--data") {
            if (i + 1 < argc) {
                args.dataPath = argv[++i];
            }
        } else if (arg == "-w" || arg == "--web") {
            if (i + 1 < argc) {
                args.webRoot = argv[++i];
            }
        } else if (arg == "--no-console-log") {
            args.enableConsoleLog = false;
        } else if (arg == "--no-file-log") {
            args.enableFileLog = false;
        } else if (arg == "--log-level") {
            if (i + 1 < argc) {
                std::string level = argv[++i];
                if (level == "debug") args.logLevel = LogLevel::DEBUG;
                else if (level == "info") args.logLevel = LogLevel::INFO;
                else if (level == "warning") args.logLevel = LogLevel::WARNING;
                else if (level == "error") args.logLevel = LogLevel::ERROR;
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "用法: " << argv[0] << " [选项]\n"
                      << "选项:\n"
                      << "  -p, --port PORT          设置服务器端口 (默认: 8080)\n"
                      << "  -d, --data PATH          设置数据目录 (默认: ./Data)\n"
                      << "  -w, --web PATH           设置web根目录 (默认: ./web)\n"
                      << "  --no-console-log         禁用控制台日志输出\n"
                      << "  --no-file-log            禁用文件日志输出\n"
                      << "  --log-level LEVEL        设置日志级别 (debug, info, warning, error)\n"
                      << "  -h, --help               显示此帮助信息\n";
            exit(0);
        }
    }
    
    return args;
}

// 控制台命令处理线程
void consoleCommandThread(CommandSystem& commandSystem) {
    std::string CommandUser = "root";
    std::string input;
    TerminalSettings terminal;
    
    while (!g_shutdownRequested) {
        g_commandInputInProgress = true;
        g_currentInputLine.clear();
        
        // 使用更明显的提示符
        std::cout << "\033[1;32m命令>\033[0m ";
        std::cout.flush();
        
        // 设置终端为原始模式
        terminal.setRawMode();
        
        char ch;
        input.clear();
        bool inputComplete = false;
        
        // 改进的退出检测：使用超时机制
        auto lastActivity = std::chrono::steady_clock::now();
        const auto timeoutDuration = std::chrono::milliseconds(50); // 减少超时时间，提高响应性
        
        while (!g_shutdownRequested && !inputComplete) {
            // 短暂睡眠以避免高CPU占用
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // 检查超时，确保能响应关闭信号
            auto now = std::chrono::steady_clock::now();
            if (now - lastActivity > timeoutDuration) {
                // 定期检查关闭状态
                if (g_shutdownRequested) {
                    std::cout << "\n检测到关闭信号，正在退出..." << std::endl;
                    break;
                }
                lastActivity = now;
            }

            
            if (readCharImmediate(ch)) {
                lastActivity = now; // 更新活动时间
                
                if (ch == '\n' || ch == '\r') {
                    // 回车键，结束输入
                    std::cout << std::endl;
                    inputComplete = true;
                } else if (ch == 127 || ch == 8) { // Backspace (Linux/Windows)
                    if (!input.empty()) {
                        input.pop_back();
                        g_currentInputLine = input;
                        // 退格处理：光标左移，删除字符，再左移
                        std::cout << "\b \b";
                        std::cout.flush();
                    }
                } else if (ch == 3) { // Ctrl+C
                    std::cout << "^C" << std::endl;
                    g_shutdownRequested = true;
                    inputComplete = true;
                    break; // 立即退出
                } else if (ch >= 32 && ch <= 126) { // 可打印字符
                    std::cout << ch;
                    input += ch;
                    g_currentInputLine = input;
                    std::cout.flush();
                }
                // 忽略其他控制字符
            }
        }
        
        // 恢复终端设置
        terminal.restore();
        
        g_commandInputInProgress = false;
        g_currentInputLine.clear();
        
        if (g_shutdownRequested) {
            break; // 立即退出
        }
        
        if (input.empty()) continue;
        
        if (input == "quit" || input == "exit") {
            g_shutdownRequested = true;
            break;
        }
        
        // 执行命令
        CommandResult result = commandSystem.ExecuteCommand(input, CommandUser);
        std::cout << (result.success ? "\033[1;32m[成功]\033[0m " : "\033[1;31m[失败]\033[0m ") << result.message << std::endl;
    }
    
    // 确保终端设置被恢复
    terminal.restore();
}

// 跨平台清理函数
void cleanupResources() {
    // TODO:添加其他需要清理的资源
    std::cout << "资源清理完成" << std::endl;
}

// 跨平台设置控制台标题
void setConsoleTitle(const std::string& title) {
#ifdef _WIN32
    SetConsoleTitleA(title.c_str());
#else
    // 对于Linux/macOS，使用ANSI转义序列设置终端标题
    std::cout << "\033]0;" << title << "\007";
    std::cout.flush();
#endif
}

int main(int argc, char* argv[]) {
    // 设置控制台标题
    setConsoleTitle("3D迷宫游戏服务器");
    
    // 解析命令行参数
    CommandLineArgs args = parseCommandLine(argc, argv);
    
    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // 注册清理函数
    std::atexit(cleanupResources);
    
    std::cout << "=== 3D迷宫游戏服务器启动中 ===" << std::endl;
    std::cout << "端口: " << args.port << std::endl;
    std::cout << "数据目录: " << args.dataPath << std::endl;
    std::cout << "Web根目录: " << args.webRoot << std::endl;
    
    try {
        // 1. 初始化日志系统
        Logger& logger = Logger::getInstance();
        std::cout << "初始化日志系统..." << std::endl;
        if (!logger.initialize(args.dataPath)) {
            std::cerr << "错误: 无法初始化日志系统" << std::endl;
            return 1;
        }
        logger.setLogLevel(args.logLevel);
        logger.setConsoleOutput(args.enableConsoleLog);
        logger.setFileOutput(args.enableFileLog);
        
        logger.info(LogCategory::SYSTEM, "日志系统初始化完成");
        
        // 2. 初始化数据管理器
        std::unique_ptr<DataManager> dataManager = std::make_unique<DataManager>();
        if (!dataManager->Initialize(args.dataPath)) {
            std::cerr << "数据管理器初始化失败" << std::endl;
            return 1;
        }
        logger.info(LogCategory::DATABASE, "数据管理器初始化完成");
        
        // 3. 生成或加载迷宫
        std::unique_ptr<MazeGenerator> mazeGenerator = std::make_unique<MazeGenerator>(50, 50, 7);
        
        std::vector<std::vector<std::vector<bool>>> mazeLayout;
        std::vector<std::tuple<int, int, int>> coinPositions;
        std::tuple<int, int, int> startPos, endPos;
        
        // 尝试加载现有迷宫数据
        if (!dataManager->LoadMazeData(mazeLayout, coinPositions, startPos, endPos)) {
            logger.info(LogCategory::GAME, "未找到迷宫数据，生成新迷宫...");
            mazeGenerator->generateMaze();
            
            // 转换迷宫数据格式
            int width = mazeGenerator->getWidth();
            int height = mazeGenerator->getHeight();
            int layers = mazeGenerator->getLayers();
            
            mazeLayout.resize(layers, std::vector<std::vector<bool>>(
                height, std::vector<bool>(width, false)));
            
            // 填充迷宫布局
            for (int z = 0; z < layers; ++z) {
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        CellType cellType = mazeGenerator->getCellType(x, y, z);
                        mazeLayout[z][y][x] = (cellType == CellType::WALL);
                    }
                }
            }
            
            // 获取起点和终点
            Position start = mazeGenerator->getStartPosition();
            Position end = mazeGenerator->getEndPosition();
            startPos = {start.x, start.y, start.z};
            endPos = {end.x, end.y, end.z};
            
            // 保存迷宫数据
            if (!dataManager->SaveMazeData(mazeLayout, coinPositions, startPos, endPos)) {
                logger.warning(LogCategory::DATABASE, "无法保存迷宫数据");
            }
        } else {
            logger.info(LogCategory::GAME, "成功加载现有迷宫数据");
        }
        
        // 4. 初始化游戏逻辑
        std::unique_ptr<GameLogic> gameLogic = std::make_unique<GameLogic>();
        if (!gameLogic->Initialize(mazeLayout, coinPositions, startPos, endPos)) {
            logger.error(LogCategory::GAME, "游戏逻辑初始化失败");
            return 1;
        }
        logger.info(LogCategory::GAME, "游戏逻辑初始化完成");
        
        // 5. 初始化玩家管理器
        std::unique_ptr<PlayerManager> playerManager = std::make_unique<PlayerManager>();
        if (!playerManager->Initialize(args.dataPath)) {
            logger.error(LogCategory::PLAYER, "玩家管理器初始化失败");
            return 1;
        }
        logger.info(LogCategory::PLAYER, "玩家管理器初始化完成");
        
        // 6. 初始化命令系统
        std::unique_ptr<CommandSystem> commandSystem = std::make_unique<CommandSystem>(*gameLogic, *playerManager);
        logger.info(LogCategory::COMMAND, "命令系统初始化完成");
        
        // 7. 初始化网络管理器（使用端口+1，避免与Web服务器冲突）
        NetworkManager& networkManager = NetworkManager::getInstance();
        int networkPort = args.port + 1; // WebSocket使用下一个端口
        if (!networkManager.initialize(networkPort)) {
            logger.error(LogCategory::NETWORK, "网络管理器初始化失败");
            return 1;
        }
        
        // 设置网络消息回调
        networkManager.setMessageCallback([&](int clientId, const std::string& message) {
            // 这里处理网络消息
            logger.debug(LogCategory::NETWORK, "收到来自客户端 " + std::to_string(clientId) + " 的消息: " + message);
            
            // 处理特殊消息类型
            if (message == "CONNECT") {
                logger.info(LogCategory::NETWORK, "客户端连接: " + std::to_string(clientId));
                return;
            }
            
            if (message == "DISCONNECT") {
                logger.info(LogCategory::NETWORK, "客户端断开: " + std::to_string(clientId));
                // 注销玩家
                // 注意：这里需要知道玩家ID，但目前没有存储映射关系
                return;
            }
            
            // 尝试解析JSON消息
            try {
                nlohmann::json jsonData = nlohmann::json::parse(message);
                std::string messageType = jsonData.value("type", "");
                
                if (messageType == "auth") {
                    // 处理认证消息
                    std::string playerId = jsonData.value("playerId", "");
                    std::string playerName = jsonData.value("playerName", "");
                    std::string token = jsonData.value("token", "");
                    
                    logger.info(LogCategory::NETWORK, "处理认证请求 - 玩家: " + playerName + ", ID: " + playerId);
                    
                    // 改进的认证逻辑：如果没有有效的playerId，生成一个新的
                    if (playerId.empty() || !playerManager->IsValidPlayerId(playerId)) {
                        // 使用客户端IP作为MAC地址的替代，使用playerName作为cookie的替代
                        std::string clientIdentifier = "client_" + std::to_string(clientId);
                        playerId = playerManager->RegisterPlayer(clientIdentifier, playerName);
                        
                        if (playerId.empty()) {
                            // 注册失败，发送错误响应
                            nlohmann::json authResponse;
                            authResponse["type"] = "auth_failed";
                            authResponse["message"] = "无法注册玩家，请重试";
                            authResponse["status"] = "failed";
                            
                            networkManager.sendToClient(clientId, authResponse.dump());
                            logger.error(LogCategory::PLAYER, "玩家注册失败: " + playerName);
                            return; // 使用return而不是continue，因为我们不在循环中
                        }
                    }
                    
                    // 登录玩家
                    if (playerManager->LoginPlayer(playerId)) {
                        // 发送认证成功消息
                        nlohmann::json authResponse;
                        authResponse["type"] = "auth_success";
                        authResponse["playerId"] = playerId;
                        authResponse["playerName"] = playerName;
                        authResponse["status"] = "success";
                        authResponse["token"] = "session_" + std::to_string(std::time(nullptr)); // 生成简单token
                        
                        networkManager.sendToClient(clientId, authResponse.dump());
                        
                        // 发送初始游戏数据
                        nlohmann::json playerDataResponse;
                        playerDataResponse["type"] = "player_data";
                        playerDataResponse["playerId"] = playerId;
                        playerDataResponse["playerName"] = playerName;
                        playerDataResponse["coins"] = 0; // 初始金币
                        playerDataResponse["position"] = {{"x", 0}, {"y", 0}, {"z", 0}};
                        
                        networkManager.sendToClient(clientId, playerDataResponse.dump());
                        
                        logger.info(LogCategory::PLAYER, "玩家认证成功: " + playerName + " (ID: " + playerId + ")");
                    } else {
                        // 发送认证失败消息
                        nlohmann::json authResponse;
                        authResponse["type"] = "auth_failed";
                        authResponse["message"] = "登录失败，请重试";
                        authResponse["status"] = "failed";
                        
                        networkManager.sendToClient(clientId, authResponse.dump());
                        
                        logger.warning(LogCategory::PLAYER, "玩家登录失败: " + playerName);
                    }
                } else if (messageType == "ping") {
                    // 处理心跳消息
                    nlohmann::json pongResponse;
                    pongResponse["type"] = "pong";
                    pongResponse["timestamp"] = jsonData.value("timestamp", 0);
                    
                    networkManager.sendToClient(clientId, pongResponse.dump());
                }
                // 可以在这里添加更多消息类型的处理
                
            } catch (const std::exception& e) {
                logger.error(LogCategory::NETWORK, "消息解析错误: " + std::string(e.what()));
            }
        });
        
        logger.info(LogCategory::NETWORK, "网络管理器初始化完成，端口: " + std::to_string(networkPort));
        
        // 8. 初始化Web服务器
        WebServer& webServer = WebServer::getInstance();
        if (!webServer.initialize(args.webRoot, args.port)) {
            logger.error(LogCategory::WEB, "Web服务器初始化失败");
            return 1;
        }
        
        // 添加API路由
        webServer.addRoute("/api/config", [networkPort](const std::string& request) -> std::string {
            std::string response = 
                "{\n"
                "  \"websocketPort\": " + std::to_string(networkPort) + ",\n"
                "  \"gameVersion\": \"1.0.0\",\n"
                "  \"serverName\": \"3D迷宫游戏服务器\",\n"
                "  \"mazeSize\": \"50x50x7\",\n"
                "  \"maxPlayers\": 50\n"
                "}";
            return response;
        });
        
        webServer.addRoute("/api/status", [&gameLogic, &playerManager, &networkManager](const std::string& request) -> std::string {
            int connectedPlayers = networkManager.getConnectedClientsCount();
            std::string response = 
                "{\n"
                "  \"status\": \"running\",\n"
                "  \"connectedPlayers\": " + std::to_string(connectedPlayers) + ",\n"
                "  \"totalPlayers\": " + std::to_string(playerManager->GetPlayerCount()) + ",\n"
                "  \"onlinePlayers\": " + std::to_string(playerManager->GetOnlinePlayerCount()) + ",\n"
                "  \"uptime\": \"unknown\",\n"
                "  \"serverTime\": \"" + Logger::getInstance().getCurrentISOTimeString() + "\"\n"
                "}";
            return response;
        });
        
        logger.info(LogCategory::WEB, "Web服务器初始化完成");
        
        // 启动服务器
        if (!networkManager.startServer()) {
            logger.error(LogCategory::NETWORK, "无法启动网络服务器");
            return 1;
        }
        
        if (!webServer.startServer()) {
            logger.error(LogCategory::WEB, "无法启动Web服务器");
            return 1;
        }
        
        logger.logSystemEvent("服务器启动成功", "Web服务器端口: " + std::to_string(args.port) + ", WebSocket端口: " + std::to_string(networkPort));
        std::cout << "\n=== 服务器启动成功 ===" << std::endl;
        std::cout << "Web服务器运行在: http://localhost:" << args.port << std::endl;
        std::cout << "WebSocket服务器运行在端口: " << networkPort << std::endl;
        std::cout << "API接口可用:" << std::endl;
        std::cout << "  - http://localhost:" << args.port << "/api/config" << std::endl;
        std::cout << "  - http://localhost:" << args.port << "/api/status" << std::endl;
        std::cout << "输入 'quit' 或 'exit' 退出服务器" << std::endl;
        std::cout << "输入命令进行管理操作" << std::endl;
        std::cout << std::endl; // 添加空行分隔
        
        logger.info(LogCategory::SYSTEM, "以 Admin 启动命令行");
        // 启动控制台命令线程
        std::thread consoleThread(consoleCommandThread, std::ref(*commandSystem));
        
        // 主循环
        auto lastUpdateTime = std::chrono::steady_clock::now();
        const std::chrono::milliseconds updateInterval(100); // 10 FPS
        
        while (!g_shutdownRequested) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedTime = currentTime - lastUpdateTime;
            
            if (elapsedTime >= updateInterval) {
                // 更新游戏逻辑
                gameLogic->Update();
                lastUpdateTime = currentTime;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // 优雅关闭
        logger.logSystemEvent("服务器关闭", "开始优雅关闭");
        
        // 快速停止网络服务器（优先，因为它可能阻塞）
        logger.info(LogCategory::SYSTEM, "正在关闭网络管理器...");
        networkManager.stopServer();
        
        // 停止Web服务器
        logger.info(LogCategory::SYSTEM, "正在关闭Web服务器...");
        webServer.stopServer();
        
        // 停止控制台线程 - 改进的关闭机制
        if (consoleThread.joinable()) {
            // 先通知线程应该退出
            g_shutdownRequested = true;
            
            // 等待控制台线程结束，但设置超时
            auto startTime = std::chrono::steady_clock::now();
            const auto maxWaitTime = std::chrono::seconds(2); // 最多等待2秒
            
            while (consoleThread.joinable()) {
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed > maxWaitTime) {
                    // 超时，强制分离
                    consoleThread.detach();
                    std::cout << "控制台线程已分离 (超时)" << std::endl;
                    break;
                }
                
                // 短暂等待
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // 如果线程已经结束，join它
                if (!g_commandInputInProgress) {
                    consoleThread.join();
                    break;
                }
            }
        }
        
        // 保存玩家数据
        playerManager->SaveAllPlayerData();
        
        logger.logSystemEvent("服务器关闭", "服务器已完全关闭");
        std::cout << "服务器已关闭" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "发生异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "发生未知异常" << std::endl;
        return 1;
    }
    
    return 0;
}