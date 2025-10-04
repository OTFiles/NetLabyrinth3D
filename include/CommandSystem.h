#ifndef COMMANDSYSTEM_H
#define COMMANDSYSTEM_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include "GameLogic.h"
#include "PlayerManager.h"

// 命令执行结果
struct CommandResult {
    bool success;
    std::string message;
    
    CommandResult(bool s = false, const std::string& msg = "") 
        : success(s), message(msg) {}
};

// 管理员权限级别
enum class AdminLevel {
    NONE = 0,
    MODERATOR = 1,
    ADMIN = 2,
    SUPER_ADMIN = 3
};

class CommandSystem {
public:
    CommandSystem(GameLogic& gameLogic, PlayerManager& playerManager);
    ~CommandSystem();

    // 执行命令
    CommandResult ExecuteCommand(const std::string& command, const std::string& executorId = "");
    
    // 检查权限
    bool CheckPermission(const std::string& playerId, AdminLevel requiredLevel) const;
    
    // 添加管理员
    void AddAdmin(const std::string& playerId, AdminLevel level);
    
    // 移除管理员
    void RemoveAdmin(const std::string& playerId);
    
    // 获取命令历史
    const std::vector<std::string>& GetCommandHistory() const { return commandHistory_; }
    
    // 清除命令历史
    void ClearCommandHistory() { commandHistory_.clear(); }

private:
    // 命令处理函数类型
    using CommandHandler = std::function<CommandResult(const std::vector<std::string>&, const std::string&)>;
    
    // 注册所有命令
    void RegisterCommands();
    
    // 解析命令字符串
    std::vector<std::string> ParseCommand(const std::string& command);
    
    // 命令实现
    CommandResult HandleGive(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleTeleport(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleKick(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleKill(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleClear(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleCoin(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleSystem(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleHelp(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleAdmin(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleListPlayers(const std::vector<std::string>& args, const std::string& executorId);
    CommandResult HandleRestart(const std::vector<std::string>& args, const std::string& executorId);
    
    // 工具函数
    ItemType ParseItemType(const std::string& itemStr);
    std::string ItemTypeToString(ItemType item);
    bool ParsePosition(const std::vector<std::string>& args, int startIndex, float& x, float& y, float& z);
    bool IsValidPlayer(const std::string& playerId);
    int ConvertPlayerIdToInt(const std::string& playerId);

private:
    GameLogic& gameLogic_;
    PlayerManager& playerManager_;
    
    // 命令映射
    std::map<std::string, CommandHandler> commandHandlers_;
    
    // 命令历史
    std::vector<std::string> commandHistory_;
    
    // 管理员列表
    std::map<std::string, AdminLevel> admins_;
    
    // 默认管理员（首次启动时自动添加）
    static const std::vector<std::string> DEFAULT_ADMINS;
};

#endif // COMMANDSYSTEM_H