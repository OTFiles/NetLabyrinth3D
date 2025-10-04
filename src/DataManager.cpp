#include "DataManager.h"
#include "PlayerManager.h"
#include "GameLogic.h"
#include "MazeGenerator.h"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>

// nlohmann/json头文件包含
// 如果使用系统包管理器安装，通常直接包含
// 如果使用本地版本，通过HAS_LOCAL_NLOHMANN标识
#ifdef HAS_LOCAL_NLOHMANN
    #include "nlohmann/json.hpp"
#else
    #include <nlohmann/json.hpp>
#endif

// PlayerData的JSON序列化辅助函数
namespace nlohmann {
    template <>
    struct adl_serializer<PlayerData> {
        static void to_json(json& j, const PlayerData& p) {
            j = json{
                {"player_id", p.playerId},
                {"mac_address", p.macAddress},
                {"cookie", p.cookie},
                {"total_coins", p.totalCoins},
                {"games_played", p.gamesPlayed},
                {"games_won", p.gamesWon},
                {"last_login", std::chrono::duration_cast<std::chrono::seconds>(p.lastLogin.time_since_epoch()).count()},
                {"is_online", p.isOnline}
            };
        }

        static void from_json(const json& j, PlayerData& p) {
            j.at("player_id").get_to(p.playerId);
            j.at("mac_address").get_to(p.macAddress);
            j.at("cookie").get_to(p.cookie);
            j.at("total_coins").get_to(p.totalCoins);
            j.at("games_played").get_to(p.gamesPlayed);
            j.at("games_won").get_to(p.gamesWon);
            
            int64_t lastLoginSeconds = j.at("last_login");
            p.lastLogin = std::chrono::system_clock::time_point(std::chrono::seconds(lastLoginSeconds));
            
            j.at("is_online").get_to(p.isOnline);
        }
    };
}

DataManager::DataManager() : dataPath_("./Data/") {
}

DataManager::~DataManager() {
    if (chatLogStream_.is_open()) {
        chatLogStream_.close();
    }
}

bool DataManager::Initialize(const std::string& dataPath) {
    dataPath_ = dataPath;
    
    // 确保数据目录存在
    if (!CreateDataDirectory()) {
        std::cerr << "Failed to create data directory: " << dataPath_ << std::endl;
        return false;
    }
    
    // 打开聊天日志文件（追加模式）
    std::string chatLogPath = BuildFilePath("chat_log.txt");
    chatLogStream_.open(chatLogPath, std::ios::app);
    if (!chatLogStream_.is_open()) {
        std::cerr << "Failed to open chat log file: " << chatLogPath << std::endl;
        return false;
    }
    
    // 如果配置文件不存在，创建默认配置
    std::string configPath = BuildFilePath("config.json");
    if (!std::filesystem::exists(configPath)) {
        if (!SaveConfig(defaultConfig_)) {
            std::cerr << "Failed to create default config file" << std::endl;
            return false;
        }
    }
    
    std::cout << "DataManager initialized successfully. Data path: " << dataPath_ << std::endl;
    return true;
}

bool DataManager::SavePlayerData(const std::string& playerId, const PlayerData& data) {
    json j = data; // 使用ADL序列化
    
    std::string playersPath = BuildFilePath("players.json");
    json allPlayers;
    
    // 读取现有的玩家数据
    if (std::filesystem::exists(playersPath)) {
        std::ifstream file(playersPath);
        if (file.is_open()) {
            try {
                file >> allPlayers;
            } catch (const json::exception& e) {
                std::cerr << "Error reading players.json: " << e.what() << std::endl;
                allPlayers = json::object();
            }
        }
    }
    
    // 更新或添加玩家数据
    allPlayers[playerId] = j;
    
    // 写回文件
    return WriteJsonToFile(allPlayers, playersPath);
}

bool DataManager::LoadPlayerData(const std::string& playerId, PlayerData& data) {
    std::string playersPath = BuildFilePath("players.json");
    
    if (!std::filesystem::exists(playersPath)) {
        return false;
    }
    
    json allPlayers;
    if (!ReadJsonFromFile(allPlayers, playersPath)) {
        return false;
    }
    
    if (allPlayers.contains(playerId)) {
        try {
            data = allPlayers[playerId].get<PlayerData>();
            return true;
        } catch (const json::exception& e) {
            std::cerr << "Error parsing player data for " << playerId << ": " << e.what() << std::endl;
        }
    }
    
    return false;
}

bool DataManager::SaveAllPlayersData(const std::map<std::string, PlayerData>& players) {
    json allPlayers = json::object();
    
    for (const auto& [playerId, data] : players) {
        allPlayers[playerId] = data;
    }
    
    std::string playersPath = BuildFilePath("players.json");
    return WriteJsonToFile(allPlayers, playersPath);
}

bool DataManager::LoadAllPlayersData(std::map<std::string, PlayerData>& players) {
    std::string playersPath = BuildFilePath("players.json");
    
    if (!std::filesystem::exists(playersPath)) {
        return false;
    }
    
    json allPlayers;
    if (!ReadJsonFromFile(allPlayers, playersPath)) {
        return false;
    }
    
    players.clear();
    for (auto it = allPlayers.begin(); it != allPlayers.end(); ++it) {
        try {
            PlayerData data = it.value().get<PlayerData>();
            players[it.key()] = data;
        } catch (const json::exception& e) {
            std::cerr << "Error parsing player data for " << it.key() << ": " << e.what() << std::endl;
        }
    }
    
    return true;
}

bool DataManager::SaveMazeData(const std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                              const std::vector<std::tuple<int, int, int>>& coinPositions,
                              const std::tuple<int, int, int>& startPos,
                              const std::tuple<int, int, int>& endPos) {
    json j = MazeDataToJson(mazeLayout, coinPositions, startPos, endPos);
    std::string mazePath = BuildFilePath("maze_data.json");
    return WriteJsonToFile(j, mazePath);
}

bool DataManager::LoadMazeData(std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                              std::vector<std::tuple<int, int, int>>& coinPositions,
                              std::tuple<int, int, int>& startPos,
                              std::tuple<int, int, int>& endPos) {
    std::string mazePath = BuildFilePath("maze_data.json");
    
    if (!std::filesystem::exists(mazePath)) {
        return false;
    }
    
    json j;
    if (!ReadJsonFromFile(j, mazePath)) {
        return false;
    }
    
    return JsonToMazeData(j, mazeLayout, coinPositions, startPos, endPos);
}

bool DataManager::SaveConfig(const json& config) {
    std::string configPath = BuildFilePath("config.json");
    return WriteJsonToFile(config, configPath);
}

bool DataManager::LoadConfig(json& config) {
    std::string configPath = BuildFilePath("config.json");
    
    if (!std::filesystem::exists(configPath)) {
        config = defaultConfig_;
        return false;
    }
    
    return ReadJsonFromFile(config, configPath);
}

bool DataManager::AppendChatLog(const std::string& playerName, const std::string& message) {
    if (!chatLogStream_.is_open()) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "[%Y-%m-%d %H:%M:%S]");
    ss << " [" << playerName << "]: " << message << "\n";
    
    chatLogStream_ << ss.str();
    chatLogStream_.flush();
    
    return chatLogStream_.good();
}

std::vector<std::string> DataManager::GetChatLog(int maxLines) {
    std::vector<std::string> chatLines;
    std::string chatPath = BuildFilePath("chat_log.txt");
    
    if (!std::filesystem::exists(chatPath)) {
        return chatLines;
    }
    
    std::ifstream file(chatPath);
    if (!file.is_open()) {
        return chatLines;
    }
    
    std::string line;
    std::vector<std::string> allLines;
    
    while (std::getline(file, line)) {
        allLines.push_back(line);
    }
    
    // 返回最新的maxLines行
    int startIndex = std::max(0, static_cast<int>(allLines.size()) - maxLines);
    for (int i = startIndex; i < allLines.size(); ++i) {
        chatLines.push_back(allLines[i]);
    }
    
    return chatLines;
}

bool DataManager::ClearChatLog() {
    if (chatLogStream_.is_open()) {
        chatLogStream_.close();
    }
    
    std::string chatPath = BuildFilePath("chat_log.txt");
    chatLogStream_.open(chatPath, std::ios::trunc);
    
    return chatLogStream_.is_open();
}

bool DataManager::CreateBackup() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream backupName;
    backupName << "backup_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    std::string backupDir = dataPath_ + "/backups/";
    if (!std::filesystem::exists(backupDir)) {
        std::filesystem::create_directories(backupDir);
    }
    
    // 备份玩家数据
    std::string playersPath = BuildFilePath("players.json");
    std::string backupPlayersPath = backupDir + backupName.str() + "_players.json";
    if (std::filesystem::exists(playersPath)) {
        std::filesystem::copy_file(playersPath, backupPlayersPath);
    }
    
    // 备份配置
    std::string configPath = BuildFilePath("config.json");
    std::string backupConfigPath = backupDir + backupName.str() + "_config.json";
    if (std::filesystem::exists(configPath)) {
        std::filesystem::copy_file(configPath, backupConfigPath);
    }
    
    // 备份迷宫数据
    std::string mazePath = BuildFilePath("maze_data.json");
    std::string backupMazePath = backupDir + backupName.str() + "_maze.json";
    if (std::filesystem::exists(mazePath)) {
        std::filesystem::copy_file(mazePath, backupMazePath);
    }
    
    return true;
}

bool DataManager::RestoreFromBackup(const std::string& backupFile) {
    if (!std::filesystem::exists(backupFile)) {
        return false;
    }
    
    // 这里简化实现，实际应该根据备份文件类型进行恢复
    std::filesystem::copy(backupFile, BuildFilePath("players.json"), std::filesystem::copy_options::overwrite_existing);
    
    return true;
}

bool DataManager::IsDataPathValid() const {
    return std::filesystem::exists(dataPath_) && std::filesystem::is_directory(dataPath_);
}

bool DataManager::CreateDataDirectory() {
    if (std::filesystem::exists(dataPath_)) {
        return true;
    }
    
    return std::filesystem::create_directories(dataPath_);
}

json DataManager::PlayerDataToJson(const PlayerData& data) {
    return json(data); // 使用ADL序列化
}

bool DataManager::JsonToPlayerData(const json& j, PlayerData& data) {
    try {
        data = j.get<PlayerData>();
        return true;
    } catch (const json::exception& e) {
        std::cerr << "Error converting JSON to PlayerData: " << e.what() << std::endl;
        return false;
    }
}

json DataManager::MazeDataToJson(const std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                                const std::vector<std::tuple<int, int, int>>& coinPositions,
                                const std::tuple<int, int, int>& startPos,
                                const std::tuple<int, int, int>& endPos) {
    json j;
    
    // 序列化迷宫布局
    j["maze_layout"] = json::array();
    for (const auto& layer : mazeLayout) {
        json layerJson = json::array();
        for (const auto& row : layer) {
            json rowJson = json::array();
            for (bool cell : row) {
                rowJson.push_back(cell);
            }
            layerJson.push_back(rowJson);
        }
        j["maze_layout"].push_back(layerJson);
    }
    
    // 序列化金币位置
    j["coin_positions"] = json::array();
    for (const auto& coin : coinPositions) {
        json coinJson = json::array({std::get<0>(coin), std::get<1>(coin), std::get<2>(coin)});
        j["coin_positions"].push_back(coinJson);
    }
    
    // 序列化起点和终点
    j["start_position"] = json::array({std::get<0>(startPos), std::get<1>(startPos), std::get<2>(startPos)});
    j["end_position"] = json::array({std::get<0>(endPos), std::get<1>(endPos), std::get<2>(endPos)});
    
    return j;
}

bool DataManager::JsonToMazeData(const json& j, 
                                std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                                std::vector<std::tuple<int, int, int>>& coinPositions,
                                std::tuple<int, int, int>& startPos,
                                std::tuple<int, int, int>& endPos) {
    try {
        // 反序列化迷宫布局
        mazeLayout.clear();
        for (const auto& layerJson : j["maze_layout"]) {
            std::vector<std::vector<bool>> layer;
            for (const auto& rowJson : layerJson) {
                std::vector<bool> row;
                for (bool cell : rowJson) {
                    row.push_back(cell);
                }
                layer.push_back(row);
            }
            mazeLayout.push_back(layer);
        }
        
        // 反序列化金币位置
        coinPositions.clear();
        for (const auto& coinJson : j["coin_positions"]) {
            coinPositions.emplace_back(coinJson[0], coinJson[1], coinJson[2]);
        }
        
        // 反序列化起点和终点
        startPos = {j["start_position"][0], j["start_position"][1], j["start_position"][2]};
        endPos = {j["end_position"][0], j["end_position"][1], j["end_position"][2]};
        
        return true;
    } catch (const json::exception& e) {
        std::cerr << "Error converting JSON to MazeData: " << e.what() << std::endl;
        return false;
    }
}

bool DataManager::WriteJsonToFile(const json& j, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    try {
        file << j.dump(4); // 使用4空格缩进
        return true;
    } catch (const json::exception& e) {
        std::cerr << "Error writing JSON to file: " << e.what() << std::endl;
        return false;
    }
}

bool DataManager::ReadJsonFromFile(json& j, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }
    
    try {
        file >> j;
        return true;
    } catch (const json::exception& e) {
        std::cerr << "Error reading JSON from file: " << e.what() << std::endl;
        return false;
    }
}

std::string DataManager::BuildFilePath(const std::string& filename) const {
    return dataPath_ + "/" + filename;
}