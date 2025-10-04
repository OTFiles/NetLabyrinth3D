#ifndef PLAYERMANAGER_H
#define PLAYERMANAGER_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include "GameLogic.h"

// 玩家数据持久化结构
struct PlayerData {
    std::string playerId;
    std::string macAddress;
    std::string cookie;
    int totalCoins;
    int gamesPlayed;
    int gamesWon;
    std::chrono::system_clock::time_point lastLogin;
    bool isOnline;
};

class PlayerManager {
public:
    PlayerManager();
    ~PlayerManager();

    // 初始化玩家管理器
    bool Initialize(const std::string& dataPath);
    
    // 玩家身份识别和注册
    std::string RegisterPlayer(const std::string& macAddress, const std::string& cookie = "");
    
    // 玩家登录
    bool LoginPlayer(const std::string& playerId);
    
    // 玩家登出
    void LogoutPlayer(const std::string& playerId);
    
    // 获取玩家数据
    PlayerData GetPlayerData(const std::string& playerId) const;
    
    // 更新玩家数据
    bool UpdatePlayerData(const std::string& playerId, const PlayerData& newData);
    
    // 玩家死亡处理
    void HandlePlayerDeath(const std::string& playerId);
    
    // 玩家重生
    void RespawnPlayer(const std::string& playerId);
    
    // 检查会话有效性
    bool IsSessionValid(const std::string& playerId) const;
    
    // 通过MAC地址或Cookie查找玩家
    std::string FindPlayerByIdentifier(const std::string& macAddress, const std::string& cookie = "") const;
    
    // 获取所有在线玩家
    std::vector<std::string> GetOnlinePlayers() const;
    
    // 获取玩家总数
    int GetPlayerCount() const { return players_.size(); }
    
    // 获取在线玩家数量
    int GetOnlinePlayerCount() const { return onlinePlayers_.size(); }
    
    // 保存所有玩家数据到文件
    bool SaveAllPlayerData();
    
    // 加载玩家数据从文件
    bool LoadAllPlayerData();

private:
    // 生成唯一玩家ID
    std::string GeneratePlayerId() const;
    
    // 生成默认玩家数据
    PlayerData CreateDefaultPlayerData(const std::string& playerId, 
                                      const std::string& macAddress, 
                                      const std::string& cookie = "") const;
    
    // 验证MAC地址格式
    bool ValidateMacAddress(const std::string& macAddress) const;
    
    // 数据文件路径
    std::string GetPlayerDataFilePath() const;

private:
    std::map<std::string, PlayerData> players_;
    std::string dataPath_;
    
    // MAC地址到玩家ID的映射
    std::map<std::string, std::string> macToPlayerId_;
    
    // Cookie到玩家ID的映射
    std::map<std::string, std::string> cookieToPlayerId_;
    
    // 在线玩家列表
    std::vector<std::string> onlinePlayers_;
};

#endif // PLAYERMANAGER_H