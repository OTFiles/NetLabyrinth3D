#include "PlayerManager.h"
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

PlayerManager::PlayerManager() {
    // 构造函数
}

PlayerManager::~PlayerManager() {
    // 析构时保存数据
    SaveAllPlayerData();
}

bool PlayerManager::Initialize(const std::string& dataPath) {
    dataPath_ = dataPath;
    
    // 确保数据目录存在
    std::filesystem::create_directories(dataPath_);
    
    return LoadAllPlayerData();
}

std::string PlayerManager::RegisterPlayer(const std::string& macAddress, const std::string& cookie) {
    // 验证MAC地址
    if (!ValidateMacAddress(macAddress)) {
        return "";
    }
    
    // 检查是否已存在
    std::string existingPlayerId = FindPlayerByIdentifier(macAddress, cookie);
    if (!existingPlayerId.empty()) {
        return existingPlayerId;
    }
    
    // 生成新玩家ID
    std::string playerId = GeneratePlayerId();
    
    // 创建玩家数据
    PlayerData newPlayer = CreateDefaultPlayerData(playerId, macAddress, cookie);
    
    // 添加到管理器中
    players_[playerId] = newPlayer;
    macToPlayerId_[macAddress] = playerId;
    
    if (!cookie.empty()) {
        cookieToPlayerId_[cookie] = playerId;
    }
    
    // 保存数据
    SaveAllPlayerData();
    
    return playerId;
}

bool PlayerManager::LoginPlayer(const std::string& playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    // 更新登录状态和时间
    it->second.isOnline = true;
    it->second.lastLogin = std::chrono::system_clock::now();
    
    // 添加到在线玩家列表
    if (std::find(onlinePlayers_.begin(), onlinePlayers_.end(), playerId) == onlinePlayers_.end()) {
        onlinePlayers_.push_back(playerId);
    }
    
    return true;
}

void PlayerManager::LogoutPlayer(const std::string& playerId) {
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        it->second.isOnline = false;
    }
    
    // 从在线玩家列表中移除
    onlinePlayers_.erase(
        std::remove(onlinePlayers_.begin(), onlinePlayers_.end(), playerId),
        onlinePlayers_.end()
    );
    
    // 保存数据
    SaveAllPlayerData();
}

PlayerData PlayerManager::GetPlayerData(const std::string& playerId) const {
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        return it->second;
    }
    return PlayerData(); // 返回空数据
}

bool PlayerManager::UpdatePlayerData(const std::string& playerId, const PlayerData& newData) {
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    it->second = newData;
    SaveAllPlayerData();
    return true;
}

void PlayerManager::HandlePlayerDeath(const std::string& playerId) {
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        // 没有死亡惩罚，仅标记为离线等待重生
        it->second.isOnline = false; // 标记为离线等待重生
        
        // 从在线玩家列表中移除
        onlinePlayers_.erase(
            std::remove(onlinePlayers_.begin(), onlinePlayers_.end(), playerId),
            onlinePlayers_.end()
        );
    }
}

void PlayerManager::RespawnPlayer(const std::string& playerId) {
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        it->second.isOnline = true;
        
        // 重新添加到在线玩家列表
        if (std::find(onlinePlayers_.begin(), onlinePlayers_.end(), playerId) == onlinePlayers_.end()) {
            onlinePlayers_.push_back(playerId);
        }
    }
}

bool PlayerManager::IsSessionValid(const std::string& playerId) const {
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    // 检查玩家是否在线且会话未过期
    // 这里可以添加更复杂的会话验证逻辑
    return it->second.isOnline;
}

std::string PlayerManager::FindPlayerByIdentifier(const std::string& macAddress, const std::string& cookie) const {
    // 优先使用MAC地址查找
    auto macIt = macToPlayerId_.find(macAddress);
    if (macIt != macToPlayerId_.end()) {
        return macIt->second;
    }
    
    // 如果提供了cookie，使用cookie查找
    if (!cookie.empty()) {
        auto cookieIt = cookieToPlayerId_.find(cookie);
        if (cookieIt != cookieToPlayerId_.end()) {
            return cookieIt->second;
        }
    }
    
    return "";
}

std::vector<std::string> PlayerManager::GetOnlinePlayers() const {
    return onlinePlayers_;
}

bool PlayerManager::SaveAllPlayerData() {
    json playersJson = json::array();
    
    for (const auto& pair : players_) {
        const PlayerData& player = pair.second;
        
        json playerJson;
        playerJson["playerId"] = player.playerId;
        playerJson["macAddress"] = player.macAddress;
        playerJson["cookie"] = player.cookie;
        playerJson["totalCoins"] = player.totalCoins;
        playerJson["gamesPlayed"] = player.gamesPlayed;
        playerJson["gamesWon"] = player.gamesWon;
        
        // 转换时间戳为可读字符串
        auto time_t = std::chrono::system_clock::to_time_t(player.lastLogin);
        std::stringstream timeStream;
        timeStream << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        playerJson["lastLogin"] = timeStream.str();
        playerJson["isOnline"] = player.isOnline;
        
        playersJson.push_back(playerJson);
    }
    
    std::string filePath = GetPlayerDataFilePath();
    std::ofstream file(filePath);
    if (!file.is_open()) {
        // 如果文件打开失败，尝试创建目录
        std::filesystem::create_directories(dataPath_);
        file.open(filePath);
        if (!file.is_open()) {
            return false;
        }
    }
    
    file << playersJson.dump(4); // 使用4空格缩进
    file.close();
    
    return true;
}

bool PlayerManager::LoadAllPlayerData() {
    std::string filePath = GetPlayerDataFilePath();
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // 文件不存在是正常的（第一次运行），创建空的数据文件
        SaveAllPlayerData(); // 创建空的玩家数据文件
        return true;
    }
    
    try {
        json playersJson;
        file >> playersJson;
        file.close();
        
        // 清空现有数据
        players_.clear();
        macToPlayerId_.clear();
        cookieToPlayerId_.clear();
        onlinePlayers_.clear();
        
        for (const auto& playerJson : playersJson) {
            PlayerData player;
            player.playerId = playerJson["playerId"];
            player.macAddress = playerJson["macAddress"];
            player.cookie = playerJson.value("cookie", "");
            player.totalCoins = playerJson["totalCoins"];
            player.gamesPlayed = playerJson["gamesPlayed"];
            player.gamesWon = playerJson["gamesWon"];
            player.isOnline = playerJson["isOnline"];
            
            // 解析时间戳
            std::string timeStr = playerJson["lastLogin"];
            std::tm tm = {};
            std::istringstream ss(timeStr);
            
            // 尝试解析时间格式
            if (ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S")) {
                player.lastLogin = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            } else {
                // 如果解析失败，使用当前时间
                player.lastLogin = std::chrono::system_clock::now();
            }
            
            // 添加到管理器
            players_[player.playerId] = player;
            macToPlayerId_[player.macAddress] = player.playerId;
            
            if (!player.cookie.empty()) {
                cookieToPlayerId_[player.cookie] = player.playerId;
            }
            
            if (player.isOnline) {
                onlinePlayers_.push_back(player.playerId);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::string PlayerManager::GeneratePlayerId() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    return "PLAYER_" + std::to_string(dis(gen));
}

PlayerData PlayerManager::CreateDefaultPlayerData(const std::string& playerId, 
                                                 const std::string& macAddress, 
                                                 const std::string& cookie) const {
    PlayerData player;
    player.playerId = playerId;
    player.macAddress = macAddress;
    player.cookie = cookie;
    player.totalCoins = 0;
    player.gamesPlayed = 0;
    player.gamesWon = 0;
    player.lastLogin = std::chrono::system_clock::now();
    player.isOnline = false;
    
    return player;
}

bool PlayerManager::ValidateMacAddress(const std::string& macAddress) const {
    // 严格的MAC地址格式验证
    // MAC地址格式: XX:XX:XX:XX:XX:XX 或 XX-XX-XX-XX-XX-XX
    if (macAddress.length() != 17) {
        return false;
    }
    
    // 检查分隔符
    char separator = macAddress[2];
    if (separator != ':' && separator != '-') {
        return false;
    }
    
    // 检查格式一致性
    for (size_t i = 0; i < macAddress.length(); i++) {
        if ((i + 1) % 3 == 0) {
            // 每个第三位应该是分隔符
            if (macAddress[i] != separator) {
                return false;
            }
        } else {
            // 其他位置应该是十六进制字符
            char c = macAddress[i];
            if (!((c >= '0' && c <= '9') || 
                  (c >= 'A' && c <= 'F') || 
                  (c >= 'a' && c <= 'f'))) {
                return false;
            }
        }
    }
    
    return true;
}

std::string PlayerManager::GetPlayerDataFilePath() const {
    return dataPath_ + "/players.json";
}