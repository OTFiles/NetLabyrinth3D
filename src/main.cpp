#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdlib>

#include "Logger.h"
#include "DataManager.h"
#include "MazeGenerator.h"
#include "GameLogic.h"
#include "PlayerManager.h"
#include "NetworkManager.h"
#include "CommandSystem.h"
#include "WebServer.h"

// 全局变量用于优雅关闭
std::atomic<bool> g_shutdownRequested(false);

// 信号处理函数
void signalHandler(int signal) {
    std::cout << "\n接收到信号 " << signal << ", 正在优雅关闭服务器..." << std::endl;
    g_shutdownRequested = true;
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
    std::string input;
    while (!g_shutdownRequested) {
        // 使用更明显的提示符
        std::cout << "\033[1;32m命令>\033[0m ";
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        if (input == "quit" || input == "exit") {
            g_shutdownRequested = true;
            break;
        }
        
        // 执行命令
        CommandResult result = commandSystem.ExecuteCommand(input, "console");
        std::cout << (result.success ? "\033[1;32m[成功]\033[0m " : "\033[1;31m[失败]\033[0m ") << result.message << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    CommandLineArgs args = parseCommandLine(argc, argv);
    
    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
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
        std::cout << "设置日志级别..." << std::endl;
        logger.setLogLevel(args.logLevel);
        std::cout << "设置控制台输出..." << std::endl;
        logger.setConsoleOutput(args.enableConsoleLog);
        std::cout << "设置文件输出..." << std::endl;
        logger.setFileOutput(args.enableFileLog);
        
        std::cout << "记录初始化完成消息..." << std::endl;
        logger.info(LogCategory::SYSTEM, "日志系统初始化完成");
        std::cout << "日志系统初始化完成" << std::endl;
        
        // 2. 初始化数据管理器
        std::cout << "初始化数据管理器..." << std::endl;
        std::unique_ptr<DataManager> dataManager = std::make_unique<DataManager>();
        if (!dataManager->Initialize(args.dataPath)) {
            std::cerr << "数据管理器初始化失败" << std::endl;
            return 1;
        }
        std::cout << "数据管理器初始化完成" << std::endl;
        logger.info(LogCategory::DATABASE, "数据管理器初始化完成");
        
        // 3. 生成或加载迷宫
        std::cout << "初始化迷宫生成器..." << std::endl;
        std::unique_ptr<MazeGenerator> mazeGenerator = std::make_unique<MazeGenerator>(50, 50, 7);
        
        std::vector<std::vector<std::vector<bool>>> mazeLayout;
        std::vector<std::tuple<int, int, int>> coinPositions;
        std::tuple<int, int, int> startPos, endPos;
        
        // 尝试加载现有迷宫数据
        std::cout << "加载迷宫数据..." << std::endl;
        if (!dataManager->LoadMazeData(mazeLayout, coinPositions, startPos, endPos)) {
            logger.info(LogCategory::GAME, "未找到迷宫数据，生成新迷宫...");
            std::cout << "生成新迷宫..." << std::endl;
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
        std::cout << "初始化游戏逻辑..." << std::endl;
        std::unique_ptr<GameLogic> gameLogic = std::make_unique<GameLogic>();
        if (!gameLogic->Initialize(mazeLayout, coinPositions, startPos, endPos)) {
            logger.error(LogCategory::GAME, "游戏逻辑初始化失败");
            return 1;
        }
        std::cout << "游戏逻辑初始化完成" << std::endl;
        logger.info(LogCategory::GAME, "游戏逻辑初始化完成");
        
        // 5. 初始化玩家管理器
        std::cout << "初始化玩家管理器..." << std::endl;
        std::unique_ptr<PlayerManager> playerManager = std::make_unique<PlayerManager>();
        if (!playerManager->Initialize(args.dataPath)) {
            logger.error(LogCategory::PLAYER, "玩家管理器初始化失败");
            return 1;
        }
        std::cout << "玩家管理器初始化完成" << std::endl;
        logger.info(LogCategory::PLAYER, "玩家管理器初始化完成");
        
        // 6. 初始化命令系统
        std::cout << "初始化命令系统..." << std::endl;
        std::unique_ptr<CommandSystem> commandSystem = std::make_unique<CommandSystem>(*gameLogic, *playerManager);
        std::cout << "命令系统初始化完成" << std::endl;
        logger.info(LogCategory::COMMAND, "命令系统初始化完成");
        
        // 7. 初始化网络管理器（使用端口+1，避免与Web服务器冲突）
        std::cout << "初始化网络管理器..." << std::endl;
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
        });
        
        std::cout << "网络管理器初始化完成" << std::endl;
        logger.info(LogCategory::NETWORK, "网络管理器初始化完成，端口: " + std::to_string(networkPort));
        
        // 8. 初始化Web服务器
        std::cout << "初始化Web服务器..." << std::endl;
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
        
        std::cout << "Web服务器初始化完成" << std::endl;
        logger.info(LogCategory::WEB, "Web服务器初始化完成");
        
        // 启动服务器
        std::cout << "启动网络服务器..." << std::endl;
        if (!networkManager.startServer()) {
            logger.error(LogCategory::NETWORK, "无法启动网络服务器");
            return 1;
        }
        
        std::cout << "启动Web服务器..." << std::endl;
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
        std::cout << "\n正在关闭服务器..." << std::endl;
        logger.logSystemEvent("服务器关闭", "开始优雅关闭");
        
        // 先停止服务器
        networkManager.stopServer();
        webServer.stopServer();
        
        // 停止控制台线程
        if (consoleThread.joinable()) {
            // 等待控制台线程结束，但设置超时
            for (int i = 0; i < 10; ++i) {
                if (consoleThread.joinable()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    break;
                }
            }
            
            // 如果还在运行，强制分离
            if (consoleThread.joinable()) {
                consoleThread.detach();
                std::cout << "控制台线程已分离" << std::endl;
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