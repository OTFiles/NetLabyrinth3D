#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>

// 前向声明
struct PlayerData;
class GameLogic;
class MazeGenerator;

using json = nlohmann::json;

class DataManager {
public:
    DataManager();
    ~DataManager();

    // 初始化数据管理器
    bool Initialize(const std::string& dataPath);
    
    // 玩家数据管理
    bool SavePlayerData(const std::string& playerId, const PlayerData& data);
    bool LoadPlayerData(const std::string& playerId, PlayerData& data);
    bool SaveAllPlayersData(const std::map<std::string, PlayerData>& players);
    bool LoadAllPlayersData(std::map<std::string, PlayerData>& players);
    
    // 迷宫数据管理
    bool SaveMazeData(const std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                     const std::vector<std::tuple<int, int, int>>& coinPositions,
                     const std::tuple<int, int, int>& startPos,
                     const std::tuple<int, int, int>& endPos);
    bool LoadMazeData(std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                     std::vector<std::tuple<int, int, int>>& coinPositions,
                     std::tuple<int, int, int>& startPos,
                     std::tuple<int, int, int>& endPos);
    
    // 配置管理
    bool SaveConfig(const json& config);
    bool LoadConfig(json& config);
    
    // 聊天日志管理
    bool AppendChatLog(const std::string& playerName, const std::string& message);
    std::vector<std::string> GetChatLog(int maxLines = 100);
    bool ClearChatLog();
    
    // 备份和恢复
    bool CreateBackup();
    bool RestoreFromBackup(const std::string& backupFile);
    
    // 工具函数
    std::string GetDataPath() const { return dataPath_; }
    bool IsDataPathValid() const;
    bool CreateDataDirectory();

private:
    // JSON序列化辅助函数
    json PlayerDataToJson(const PlayerData& data);
    bool JsonToPlayerData(const json& j, PlayerData& data);
    
    json MazeDataToJson(const std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                       const std::vector<std::tuple<int, int, int>>& coinPositions,
                       const std::tuple<int, int, int>& startPos,
                       const std::tuple<int, int, int>& endPos);
    bool JsonToMazeData(const json& j, 
                       std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                       std::vector<std::tuple<int, int, int>>& coinPositions,
                       std::tuple<int, int, int>& startPos,
                       std::tuple<int, int, int>& endPos);
    
    // 文件操作
    bool WriteJsonToFile(const json& j, const std::string& filename);
    bool ReadJsonFromFile(json& j, const std::string& filename);
    
    // 路径构建
    std::string BuildFilePath(const std::string& filename) const;

private:
    std::string dataPath_;
    std::ofstream chatLogStream_;
    
    // 默认配置
    json defaultConfig_ = {
        {"server", {
            {"port", 8080},
            {"max_players", 10},
            {"game_name", "3D Maze Game"}
        }},
        {"game", {
            {"maze_width", 50},
            {"maze_height", 50},
            {"maze_layers", 7},
            {"total_coins", 110},
            {"enable_chat", true},
            {"max_chat_history", 1000}
        }},
        {"security", {
            {"allow_admin_commands", true},
            {"require_authentication", false},
            {"backup_interval_minutes", 30}
        }}
    };
};

#endif // DATAMANAGER_H