#ifndef GAMELOGIC_H
#define GAMELOGIC_H

#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <memory>

// 道具类型枚举
enum class ItemType {
    SPEED_POTION = 0,  // 加速药水
    COMPASS,           // 指南针
    HAMMER,            // 锤子
    KILL_SWORD,        // 秒人剑
    SLOW_TRAP,         // 减速带
    SWAP_ITEM,         // 大局逆转
    COIN               // 金币（特殊道具）
};

// 玩家状态结构
struct PlayerState {
    int playerId;
    float x, y, z;      // 位置坐标
    float rotation;      // 旋转角度
    bool isAlive;
    bool hasCompass;
    bool hasSpeedBoost;
    std::chrono::steady_clock::time_point speedBoostEndTime;
    int coins;
    std::map<ItemType, int> inventory;  // 道具库存
    bool reachedGoal;
    int finishRank;      // 到达终点名次
};

// 游戏配置
struct GameConfig {
    int mazeWidth = 50;
    int mazeHeight = 50;
    int mazeLayers = 7;
    int totalCoins = 110;  // 100-120之间的随机值
    int maxPlayers = 10;
};

// 移动方向枚举
enum class MoveDirection {
    FORWARD = 0,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class GameLogic {
public:
    GameLogic();
    ~GameLogic();

    // 初始化游戏
    bool Initialize(const std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                   const std::vector<std::tuple<int, int, int>>& coinPositions,
                   const std::tuple<int, int, int>& startPos,
                   const std::tuple<int, int, int>& endPos);

    // 玩家移动
    bool MovePlayer(int playerId, MoveDirection direction);
    
    // 购买道具
    bool PurchaseItem(int playerId, ItemType itemType);
    
    // 使用道具
    bool UseItem(int playerId, ItemType itemType, int targetPlayerId = -1, 
                const std::tuple<int, int, int>& targetPos = {-1, -1, -1});
    
    // 拾取金币
    bool CollectCoin(int playerId, int coinId);
    
    // 检查玩家是否到达终点
    bool CheckPlayerReachedGoal(int playerId);
    
    // 获取玩家状态
    PlayerState GetPlayerState(int playerId) const;
    
    // 添加/移除玩家
    bool AddPlayer(int playerId, const std::tuple<int, int, int>& startPos);
    bool RemovePlayer(int playerId);
    
    // 重置玩家状态（死亡重生）
    void RespawnPlayer(int playerId);
    
    // 获取游戏状态
    bool IsGameRunning() const { return gameRunning_; }
    int GetRemainingCoins() const { return remainingCoins_; }
    int GetFinishedPlayersCount() const { return finishedPlayersCount_; }
    
    // 更新游戏逻辑（每帧调用）
    void Update();
    
    // 获取迷宫布局
    const std::vector<std::vector<std::vector<bool>>>& GetMazeLayout() const { return mazeLayout_; }
    
    // 获取金币位置
    const std::vector<std::tuple<int, int, int>>& GetCoinPositions() const { return coinPositions_; }
    
    // 获取起点和终点
    std::tuple<int, int, int> GetStartPosition() const { return startPosition_; }
    std::tuple<int, int, int> GetEndPosition() const { return endPosition_; }

    // ========== 为CommandSystem扩展的接口 ==========
    
    // 直接给予玩家道具
    bool GiveItem(int playerId, ItemType itemType, int count = 1);
    
    // 传送玩家到指定位置
    bool TeleportPlayer(int playerId, float x, float y, float z);
    
    // 强制杀死玩家
    bool KillPlayer(int playerId);
    
    // 直接修改玩家金币数量
    bool SetPlayerCoins(int playerId, int coins);
    
    // 获取所有玩家ID列表
    std::vector<int> GetAllPlayerIds() const;
    
    // 重置游戏状态（保留玩家数据）
    void ResetGameState();
    
    // 检查位置是否有效（非墙壁且在迷宫范围内）
    bool IsValidPosition(float x, float y, float z) const;

private:
    // 碰撞检测
    bool CheckCollision(float x, float y, float z) const;
    
    // 道具效果实现
    void ApplySpeedPotion(int playerId);
    void ApplyCompass(int playerId);
    void ApplyHammer(int playerId, const std::tuple<int, int, int>& targetPos);
    void ApplyKillSword(int playerId, int targetPlayerId);
    void ApplySlowTrap(int playerId, const std::tuple<int, int, int>& targetPos);
    void ApplySwapItem(int playerId, int targetPlayerId);
    
    // 计算金币奖励
    int CalculateCoinReward(int rank) const;
    
    // 检查速度提升是否过期
    void CheckSpeedBoostExpiry(int playerId);
    
    // 寻找随机重生点
    std::tuple<int, int, int> FindRandomSpawnPoint() const;

private:
    GameConfig config_;
    std::map<int, PlayerState> players_;
    std::vector<std::vector<std::vector<bool>>> mazeLayout_;
    std::vector<std::tuple<int, int, int>> coinPositions_;
    std::vector<bool> coinCollected_;
    std::tuple<int, int, int> startPosition_;
    std::tuple<int, int, int> endPosition_;
    
    bool gameRunning_ = false;
    int remainingCoins_ = 0;
    int finishedPlayersCount_ = 0;
    int nextFinishRank_ = 1;
    
    // 减速带位置和效果
    std::vector<std::tuple<int, int, int, std::chrono::steady_clock::time_point>> slowTraps_;
    
    // 被破坏的墙壁（临时）
    std::vector<std::tuple<int, int, int>> brokenWalls_;
    std::map<std::tuple<int, int, int>, std::chrono::steady_clock::time_point> wallRepairTimes_;
};

#endif // GAMELOGIC_H