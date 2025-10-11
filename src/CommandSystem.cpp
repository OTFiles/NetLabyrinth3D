#include "CommandSystem.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cctype>

// 默认管理员列表（可在配置中修改）
const std::vector<std::string> CommandSystem::DEFAULT_ADMINS = {
    "admin", "root"
};

CommandSystem::CommandSystem(GameLogic& gameLogic, PlayerManager& playerManager)
    : gameLogic_(gameLogic), playerManager_(playerManager) {
    RegisterCommands();
    
    // 添加默认管理员
    for (const auto& adminId : DEFAULT_ADMINS) {
        admins_[adminId] = AdminLevel::ADMIN;
    }
}

CommandSystem::~CommandSystem() {
    // 清理资源
}

void CommandSystem::RegisterCommands() {
    // 注册所有可用命令
    commandHandlers_ = {
        {"give", [this](const auto& args, const auto& executor) { return HandleGive(args, executor); }},
        {"tp", [this](const auto& args, const auto& executor) { return HandleTeleport(args, executor); }},
        {"kick", [this](const auto& args, const auto& executor) { return HandleKick(args, executor); }},
        {"kill", [this](const auto& args, const auto& executor) { return HandleKill(args, executor); }},
        {"clear", [this](const auto& args, const auto& executor) { return HandleClear(args, executor); }},
        {"coin", [this](const auto& args, const auto& executor) { return HandleCoin(args, executor); }},
        {"system", [this](const auto& args, const auto& executor) { return HandleSystem(args, executor); }},
        {"help", [this](const auto& args, const auto& executor) { return HandleHelp(args, executor); }},
        {"admin", [this](const auto& args, const auto& executor) { return HandleAdmin(args, executor); }},
        {"players", [this](const auto& args, const auto& executor) { return HandleListPlayers(args, executor); }},
        {"restart", [this](const auto& args, const auto& executor) { return HandleRestart(args, executor); }}
    };
}

CommandResult CommandSystem::ExecuteCommand(const std::string& command, const std::string& executorId) {
    // 记录命令历史
    commandHistory_.push_back("[" + executorId + "] " + command);
    
    // 限制历史记录大小
    if (commandHistory_.size() > 1000) {
        commandHistory_.erase(commandHistory_.begin());
    }
    
    // 解析命令
    std::vector<std::string> args = ParseCommand(command);
    if (args.empty()) {
        return CommandResult(false, "Empty command");
    }
    
    std::string commandName = args[0];
    std::transform(commandName.begin(), commandName.end(), commandName.begin(), ::tolower);
    
    // 查找命令处理器
    auto it = commandHandlers_.find(commandName);
    if (it == commandHandlers_.end()) {
        return CommandResult(false, "Unknown command: " + commandName);
    }
    
    // 执行命令
    try {
        return it->second(args, executorId);
    } catch (const std::exception& e) {
        return CommandResult(false, "Command execution error: " + std::string(e.what()));
    }
}

bool CommandSystem::CheckPermission(const std::string& playerId, AdminLevel requiredLevel) const {
    auto it = admins_.find(playerId);
    if (it == admins_.end()) {
        return false;
    }
    
    return static_cast<int>(it->second) >= static_cast<int>(requiredLevel);
}

void CommandSystem::AddAdmin(const std::string& playerId, AdminLevel level) {
    admins_[playerId] = level;
}

void CommandSystem::RemoveAdmin(const std::string& playerId) {
    admins_.erase(playerId);
}

std::vector<std::string> CommandSystem::ParseCommand(const std::string& command) {
    std::vector<std::string> tokens;
    std::stringstream ss(command);
    std::string token;
    
    while (ss >> token) {
        // 移除可能的引号
        if (token.front() == '"' && token.back() == '"') {
            token = token.substr(1, token.length() - 2);
        }
        tokens.push_back(token);
    }
    
    return tokens;
}

// ==================== 命令实现 ====================

CommandResult CommandSystem::HandleGive(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::ADMIN)) {
        return CommandResult(false, "Insufficient permissions for give command");
    }
    
    if (args.size() < 3) {
        return CommandResult(false, "Usage: give <player> <item> [count]");
    }
    
    std::string playerId = args[1];
    if (!IsValidPlayer(playerId)) {
        return CommandResult(false, "Invalid player: " + playerId);
    }
    
    ItemType item = ParseItemType(args[2]);
    int count = (args.size() > 3) ? std::stoi(args[3]) : 1;
    
    if (item == ItemType::COIN) {
        // 特殊处理金币
        PlayerData data = playerManager_.GetPlayerData(playerId);
        data.totalCoins += count;
        playerManager_.UpdatePlayerData(playerId, data);
        return CommandResult(true, "Gave " + std::to_string(count) + " coins to player " + playerId);
    } else {
        // 使用扩展的GameLogic功能给予道具
        int playerIdInt = ConvertPlayerIdToInt(playerId);
        if (playerIdInt == -1) {
            return CommandResult(false, "Invalid player ID format: " + playerId);
        }
        if (gameLogic_.GiveItem(playerIdInt, item, count)) {
            return CommandResult(true, "Gave " + std::to_string(count) + " " + 
                                ItemTypeToString(item) + " to player " + playerId);
        } else {
            return CommandResult(false, "Failed to give item to player " + playerId);
        }
    }
}

CommandResult CommandSystem::HandleTeleport(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::ADMIN)) {
        return CommandResult(false, "Insufficient permissions for tp command");
    }
    
    if (args.size() < 5) {
        return CommandResult(false, "Usage: tp <player> <x> <y> <z>");
    }
    
    std::string playerId = args[1];
    if (!IsValidPlayer(playerId)) {
        return CommandResult(false, "Invalid player: " + playerId);
    }
    
    float x, y, z;
    if (!ParsePosition(args, 2, x, y, z)) {
        return CommandResult(false, "Invalid position coordinates");
    }
    
    // 使用扩展的GameLogic功能传送玩家
    int playerIdInt = ConvertPlayerIdToInt(playerId);
    if (playerIdInt == -1) {
        return CommandResult(false, "Invalid player ID format: " + playerId);
    }
    if (gameLogic_.TeleportPlayer(playerIdInt, x, y, z)) {
        return CommandResult(true, "Teleported player " + playerId + " to (" + 
                            std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
    } else {
        return CommandResult(false, "Failed to teleport player " + playerId + " - invalid position");
    }
}

CommandResult CommandSystem::HandleKick(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::MODERATOR)) {
        return CommandResult(false, "Insufficient permissions for kick command");
    }
    
    if (args.size() < 2) {
        return CommandResult(false, "Usage: kick <player> [reason]");
    }
    
    std::string playerId = args[1];
    if (!IsValidPlayer(playerId)) {
        return CommandResult(false, "Invalid player: " + playerId);
    }
    
    std::string reason = (args.size() > 2) ? args[2] : "No reason specified";
    
    // 踢出玩家逻辑
    playerManager_.LogoutPlayer(playerId);
    
    return CommandResult(true, "Kicked player " + playerId + ": " + reason);
}

CommandResult CommandSystem::HandleKill(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::MODERATOR)) {
        return CommandResult(false, "Insufficient permissions for kill command");
    }
    
    if (args.size() < 2) {
        return CommandResult(false, "Usage: kill <player>");
    }
    
    std::string playerId = args[1];
    if (!IsValidPlayer(playerId)) {
        return CommandResult(false, "Invalid player: " + playerId);
    }
    
    // 使用扩展的GameLogic功能杀死玩家
    int playerIdInt = ConvertPlayerIdToInt(playerId);
    if (playerIdInt == -1) {
        return CommandResult(false, "Invalid player ID format: " + playerId);
    }
    if (gameLogic_.KillPlayer(playerIdInt)) {
        return CommandResult(true, "Killed player " + playerId);
    } else {
        return CommandResult(false, "Failed to kill player " + playerId);
    }
}

CommandResult CommandSystem::HandleClear(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::SUPER_ADMIN)) {
        return CommandResult(false, "Insufficient permissions for clear command");
    }
    
    // 重置游戏状态
    gameLogic_.ResetGameState();
    
    return CommandResult(true, "Game state cleared and reset");
}

CommandResult CommandSystem::HandleCoin(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::ADMIN)) {
        return CommandResult(false, "Insufficient permissions for coin command");
    }
    
    if (args.size() < 3) {
        return CommandResult(false, "Usage: coin <player> <amount>");
    }
    
    std::string playerId = args[1];
    if (!IsValidPlayer(playerId)) {
        return CommandResult(false, "Invalid player: " + playerId);
    }
    
    int amount = std::stoi(args[2]);
    
    // 使用扩展的GameLogic功能设置金币
    int playerIdInt = ConvertPlayerIdToInt(playerId);
    if (playerIdInt == -1) {
        return CommandResult(false, "Invalid player ID format: " + playerId);
    }
    if (gameLogic_.SetPlayerCoins(playerIdInt, amount)) {
        // 同时更新持久化数据
        PlayerData data = playerManager_.GetPlayerData(playerId);
        data.totalCoins = amount;
        playerManager_.UpdatePlayerData(playerId, data);
        
        return CommandResult(true, "Set coins to " + std::to_string(amount) + " for player " + playerId);
    } else {
        return CommandResult(false, "Failed to set coins for player " + playerId);
    }
}

CommandResult CommandSystem::HandleSystem(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::MODERATOR)) {
        return CommandResult(false, "Insufficient permissions for system command");
    }
    
    if (args.size() < 2) {
        return CommandResult(false, "Usage: system <message>");
    }
    
    // 组合消息（支持空格）
    std::string message;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) message += " ";
        message += args[i];
    }
    
    return CommandResult(true, "System message sent: " + message);
}

CommandResult CommandSystem::HandleHelp(const std::vector<std::string>& args, const std::string& executorId) {
    std::string helpText = 
        "Available commands:\n"
        "  give <player> <item> [count]    - Give item to player\n"
        "  tp <player> <x> <y> <z>         - Teleport player\n"
        "  kick <player> [reason]          - Kick player from game\n"
        "  kill <player>                   - Kill player\n"
        "  clear                           - Clear all game data\n"
        "  coin <player> <amount>          - Set player coins\n"
        "  system <message>                - Send system message\n"
        "  admin <player> <level>          - Set admin level\n"
        "  players                         - List online players\n"
        "  restart                         - Restart game\n"
        "  help                            - Show this help";
    
    return CommandResult(true, helpText);
}

CommandResult CommandSystem::HandleAdmin(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::SUPER_ADMIN)) {
        return CommandResult(false, "Insufficient permissions for admin command");
    }
    
    if (args.size() < 3) {
        return CommandResult(false, "Usage: admin <player> <level>");
    }
    
    std::string playerId = args[1];
    int level = std::stoi(args[2]);
    
    if (level < 0 || level > 3) {
        return CommandResult(false, "Admin level must be 0-3");
    }
    
    AdminLevel adminLevel = static_cast<AdminLevel>(level);
    
    if (adminLevel == AdminLevel::NONE) {
        RemoveAdmin(playerId);
        return CommandResult(true, "Removed admin privileges from " + playerId);
    } else {
        AddAdmin(playerId, adminLevel);
        return CommandResult(true, "Set admin level " + std::to_string(level) + " for " + playerId);
    }
}

CommandResult CommandSystem::HandleListPlayers(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::MODERATOR)) {
        return CommandResult(false, "Insufficient permissions for players command");
    }
    
    auto onlinePlayers = playerManager_.GetOnlinePlayers();
    if (onlinePlayers.empty()) {
        return CommandResult(true, "No players online");
    }
    
    std::string result = "Online players (" + std::to_string(onlinePlayers.size()) + "):\n";
    for (const auto& playerId : onlinePlayers) {
        PlayerData data = playerManager_.GetPlayerData(playerId);
        result += "  " + playerId + " - Coins: " + std::to_string(data.totalCoins) + 
                 ", Games: " + std::to_string(data.gamesPlayed) + "\n";
    }
    
    return CommandResult(true, result);
}

CommandResult CommandSystem::HandleRestart(const std::vector<std::string>& args, const std::string& executorId) {
    if (!CheckPermission(executorId, AdminLevel::SUPER_ADMIN)) {
        return CommandResult(false, "Insufficient permissions for restart command");
    }
    
    // 重置游戏状态
    gameLogic_.ResetGameState();
    
    // TODO : 添加更复杂的重启逻辑，比如重新生成迷宫等
    // 目前只是重置玩家状态和游戏进度
    
    return CommandResult(true, "Game restarted - all players reset to start position");
}

// ==================== 工具函数 ====================

ItemType CommandSystem::ParseItemType(const std::string& itemStr) {
    std::string lowerItem = itemStr;
    std::transform(lowerItem.begin(), lowerItem.end(), lowerItem.begin(), ::tolower);
    
    if (lowerItem == "speed_potion" || lowerItem == "speed") return ItemType::SPEED_POTION;
    if (lowerItem == "compass") return ItemType::COMPASS;
    if (lowerItem == "hammer") return ItemType::HAMMER;
    if (lowerItem == "kill_sword" || lowerItem == "sword") return ItemType::KILL_SWORD;
    if (lowerItem == "slow_trap" || lowerItem == "trap") return ItemType::SLOW_TRAP;
    if (lowerItem == "swap_item" || lowerItem == "swap") return ItemType::SWAP_ITEM;
    if (lowerItem == "coin" || lowerItem == "coins") return ItemType::COIN;
    
    return ItemType::COIN; // 默认返回金币
}

std::string CommandSystem::ItemTypeToString(ItemType item) {
    switch (item) {
        case ItemType::SPEED_POTION: return "Speed Potion";
        case ItemType::COMPASS: return "Compass";
        case ItemType::HAMMER: return "Hammer";
        case ItemType::KILL_SWORD: return "Kill Sword";
        case ItemType::SLOW_TRAP: return "Slow Trap";
        case ItemType::SWAP_ITEM: return "Swap Item";
        case ItemType::COIN: return "Coin";
        default: return "Unknown";
    }
}

bool CommandSystem::ParsePosition(const std::vector<std::string>& args, int startIndex, float& x, float& y, float& z) {
    try {
        if (args.size() < startIndex + 3) return false;
        
        x = std::stof(args[startIndex]);
        y = std::stof(args[startIndex + 1]);
        z = std::stof(args[startIndex + 2]);
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool CommandSystem::IsValidPlayer(const std::string& playerId) {
    // 检查玩家是否存在且在线
    return playerManager_.IsSessionValid(playerId);
}

// 工具函数：将字符串玩家ID转换为整数（用于GameLogic）
int CommandSystem::ConvertPlayerIdToInt(const std::string& playerId) {
    try {
        return std::stoi(playerId);
    } catch (const std::exception&) {
        return -1; // 无效的玩家ID
    }
}