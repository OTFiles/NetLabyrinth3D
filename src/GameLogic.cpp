#include "GameLogic.h"
#include <cmath>
#include <random>
#include <algorithm>

GameLogic::GameLogic() {
    // 初始化随机数生成器
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

GameLogic::~GameLogic() {
    // TODO:清理资源
}

bool GameLogic::Initialize(const std::vector<std::vector<std::vector<bool>>>& mazeLayout,
                          const std::vector<std::tuple<int, int, int>>& coinPositions,
                          const std::tuple<int, int, int>& startPos,
                          const std::tuple<int, int, int>& endPos) {
    mazeLayout_ = mazeLayout;
    coinPositions_ = coinPositions;
    startPosition_ = startPos;
    endPosition_ = endPos;
    
    // 初始化金币收集状态
    coinCollected_.resize(coinPositions.size(), false);
    remainingCoins_ = static_cast<int>(coinPositions.size());
    
    gameRunning_ = true;
    return true;
}

bool GameLogic::MovePlayer(int playerId, MoveDirection direction) {
    auto it = players_.find(playerId);
    if (it == players_.end() || !it->second.isAlive) {
        return false;
    }
    
    PlayerState& player = it->second;
    
    // 计算移动向量
    float moveSpeed = player.hasSpeedBoost ? 0.2f : 0.1f;  // 加速时速度翻倍
    float dx = 0, dy = 0, dz = 0;
    
    switch (direction) {
        case MoveDirection::FORWARD:
            dx = -sin(player.rotation) * moveSpeed;
            dz = -cos(player.rotation) * moveSpeed;
            break;
        case MoveDirection::BACKWARD:
            dx = sin(player.rotation) * moveSpeed;
            dz = cos(player.rotation) * moveSpeed;
            break;
        case MoveDirection::LEFT:
            dx = -cos(player.rotation) * moveSpeed;
            dz = sin(player.rotation) * moveSpeed;
            break;
        case MoveDirection::RIGHT:
            dx = cos(player.rotation) * moveSpeed;
            dz = -sin(player.rotation) * moveSpeed;
            break;
        case MoveDirection::UP:
            if (player.y < config_.mazeLayers - 1) dy = moveSpeed;
            break;
        case MoveDirection::DOWN:
            if (player.y > 0) dy = -moveSpeed;
            break;
    }
    
    // 计算新位置
    float newX = player.x + dx;
    float newY = player.y + dy;
    float newZ = player.z + dz;
    
    // 检查碰撞
    if (!CheckCollision(newX, newY, newZ)) {
        player.x = newX;
        player.y = newY;
        player.z = newZ;
        
        // 检查是否到达终点
        int endX = std::get<0>(endPosition_);
        int endY = std::get<1>(endPosition_);
        int endZ = std::get<2>(endPosition_);
        
        if (static_cast<int>(player.x) == endX && 
            static_cast<int>(player.y) == endY && 
            static_cast<int>(player.z) == endZ) {
            CheckPlayerReachedGoal(playerId);
        }
        
        return true;
    }
    
    return false;
}

bool GameLogic::PurchaseItem(int playerId, ItemType itemType) {
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    PlayerState& player = it->second;
    int price = 0;
    
    // 根据道具类型设置价格
    switch (itemType) {
        case ItemType::SPEED_POTION: price = 20; break;
        case ItemType::COMPASS: price = 25; break;
        case ItemType::HAMMER: price = 50; break;
        case ItemType::KILL_SWORD: price = 50; break;
        case ItemType::SLOW_TRAP: price = 30; break;
        case ItemType::SWAP_ITEM: price = 60; break;
        default: return false;
    }
    
    // 检查金币是否足够
    if (player.coins >= price) {
        player.coins -= price;
        player.inventory[itemType]++;
        return true;
    }
    
    return false;
}

bool GameLogic::UseItem(int playerId, ItemType itemType, int targetPlayerId, 
                       const std::tuple<int, int, int>& targetPos) {
    auto it = players_.find(playerId);
    if (it == players_.end() || it->second.inventory[itemType] <= 0) {
        return false;
    }
    
    PlayerState& player = it->second;
    
    // 使用道具
    switch (itemType) {
        case ItemType::SPEED_POTION:
            ApplySpeedPotion(playerId);
            break;
        case ItemType::COMPASS:
            ApplyCompass(playerId);
            break;
        case ItemType::HAMMER:
            ApplyHammer(playerId, targetPos);
            break;
        case ItemType::KILL_SWORD:
            if (targetPlayerId != -1) {
                ApplyKillSword(playerId, targetPlayerId);
            }
            break;
        case ItemType::SLOW_TRAP:
            ApplySlowTrap(playerId, targetPos);
            break;
        case ItemType::SWAP_ITEM:
            if (targetPlayerId != -1) {
                ApplySwapItem(playerId, targetPlayerId);
            }
            break;
        default:
            return false;
    }
    
    // 减少道具数量
    player.inventory[itemType]--;
    return true;
}

bool GameLogic::CollectCoin(int playerId, int coinId) {
    if (coinId < 0 || coinId >= coinCollected_.size() || coinCollected_[coinId]) {
        return false;
    }
    
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    // 收集金币
    coinCollected_[coinId] = true;
    it->second.coins++;
    remainingCoins_--;
    
    return true;
}

bool GameLogic::CheckPlayerReachedGoal(int playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end() || it->second.reachedGoal) {
        return false;
    }
    
    PlayerState& player = it->second;
    player.reachedGoal = true;
    player.finishRank = nextFinishRank_++;
    
    // 给予金币奖励
    int reward = CalculateCoinReward(player.finishRank);
    player.coins += reward;
    
    finishedPlayersCount_++;
    return true;
}

PlayerState GameLogic::GetPlayerState(int playerId) const {
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        return it->second;
    }
    return PlayerState(); // 返回默认状态
}

bool GameLogic::AddPlayer(int playerId, const std::tuple<int, int, int>& startPos) {
    if (players_.find(playerId) != players_.end()) {
        return false; // 玩家已存在
    }
    
    PlayerState newPlayer;
    newPlayer.playerId = playerId;
    newPlayer.x = static_cast<float>(std::get<0>(startPos));
    newPlayer.y = static_cast<float>(std::get<1>(startPos));
    newPlayer.z = static_cast<float>(std::get<2>(startPos));
    newPlayer.isAlive = true;
    newPlayer.hasCompass = false;
    newPlayer.hasSpeedBoost = false;
    newPlayer.coins = 0;
    newPlayer.reachedGoal = false;
    newPlayer.finishRank = 0;
    
    // 初始化道具库存
    newPlayer.inventory[ItemType::SPEED_POTION] = 0;
    newPlayer.inventory[ItemType::COMPASS] = 0;
    newPlayer.inventory[ItemType::HAMMER] = 0;
    newPlayer.inventory[ItemType::KILL_SWORD] = 0;
    newPlayer.inventory[ItemType::SLOW_TRAP] = 0;
    newPlayer.inventory[ItemType::SWAP_ITEM] = 0;
    
    players_[playerId] = newPlayer;
    return true;
}

bool GameLogic::RemovePlayer(int playerId) {
    return players_.erase(playerId) > 0;
}

void GameLogic::RespawnPlayer(int playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return;
    }
    
    PlayerState& player = it->second;
    auto spawnPoint = FindRandomSpawnPoint();
    
    player.x = static_cast<float>(std::get<0>(spawnPoint));
    player.y = static_cast<float>(std::get<1>(spawnPoint));
    player.z = static_cast<float>(std::get<2>(spawnPoint));
    player.isAlive = true;
    player.hasSpeedBoost = false;
    // 保留金币和道具，但重置其他状态
}

void GameLogic::Update() {
    auto currentTime = std::chrono::steady_clock::now();
    
    // 检查所有玩家的速度提升是否过期
    for (auto& pair : players_) {
        CheckSpeedBoostExpiry(pair.first);
    }
    
    // 检查减速带是否过期（30秒后消失）
    slowTraps_.erase(
        std::remove_if(slowTraps_.begin(), slowTraps_.end(),
            [currentTime](const auto& trap) {
                return std::chrono::duration_cast<std::chrono::seconds>(
                    currentTime - std::get<3>(trap)).count() > 30;
            }),
        slowTraps_.end()
    );
    
    // 修复被破坏的墙壁（60秒后恢复）
    for (auto it = wallRepairTimes_.begin(); it != wallRepairTimes_.end();) {
        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - it->second).count() > 60) {
            // 恢复墙壁
            auto pos = it->first;
            mazeLayout_[std::get<1>(pos)][std::get<0>(pos)][std::get<2>(pos)] = true;
            it = wallRepairTimes_.erase(it);
        } else {
            ++it;
        }
    }
}

bool GameLogic::CheckCollision(float x, float y, float z) const {
    // 转换为网格坐标
    int gridX = static_cast<int>(std::round(x));
    int gridY = static_cast<int>(std::round(y));
    int gridZ = static_cast<int>(std::round(z));
    
    // 检查边界
    if (gridX < 0 || gridX >= config_.mazeWidth ||
        gridY < 0 || gridY >= config_.mazeLayers ||
        gridZ < 0 || gridZ >= config_.mazeHeight) {
        return true;
    }
    
    // 检查墙壁碰撞
    return mazeLayout_[gridY][gridX][gridZ];
}

void GameLogic::ApplySpeedPotion(int playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end()) return;
    
    PlayerState& player = it->second;
    player.hasSpeedBoost = true;
    player.speedBoostEndTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
}

void GameLogic::ApplyCompass(int playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end()) return;
    
    PlayerState& player = it->second;
    player.hasCompass = true;
}

void GameLogic::ApplyHammer(int playerId, const std::tuple<int, int, int>& targetPos) {
    int x = std::get<0>(targetPos);
    int y = std::get<1>(targetPos);
    int z = std::get<2>(targetPos);
    
    // 检查目标位置是否有效
    if (x >= 0 && x < config_.mazeWidth &&
        y >= 0 && y < config_.mazeLayers &&
        z >= 0 && z < config_.mazeHeight &&
        mazeLayout_[y][x][z]) {  // 确保是墙壁
        
        // 破坏墙壁
        mazeLayout_[y][x][z] = false;
        brokenWalls_.push_back(targetPos);
        wallRepairTimes_[targetPos] = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    }
}

void GameLogic::ApplyKillSword(int playerId, int targetPlayerId) {
    auto targetIt = players_.find(targetPlayerId);
    if (targetIt == players_.end() || !targetIt->second.isAlive) {
        return;
    }
    
    // 杀死目标玩家
    targetIt->second.isAlive = false;
    
    // 3秒后重生
    // 注意：在实际实现中，这里应该使用计时器或在下一次更新时重生
    RespawnPlayer(targetPlayerId);
}

void GameLogic::ApplySlowTrap(int playerId, const std::tuple<int, int, int>& targetPos) {
    // 放置减速带
    slowTraps_.emplace_back(
        std::get<0>(targetPos),
        std::get<1>(targetPos),
        std::get<2>(targetPos),
        std::chrono::steady_clock::now()
    );
}

void GameLogic::ApplySwapItem(int playerId, int targetPlayerId) {
    auto player1It = players_.find(playerId);
    auto player2It = players_.find(targetPlayerId);
    
    if (player1It == players_.end() || player2It == players_.end()) {
        return;
    }
    
    // 交换玩家位置
    PlayerState& player1 = player1It->second;
    PlayerState& player2 = player2It->second;
    
    std::swap(player1.x, player2.x);
    std::swap(player1.y, player2.y);
    std::swap(player1.z, player2.z);
}

int GameLogic::CalculateCoinReward(int rank) const {
    // 第一名60金币，第二名59，以此类推
    return 61 - rank;  // 61 - 1 = 60, 61 - 2 = 59, ...
}

void GameLogic::CheckSpeedBoostExpiry(int playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end() || !it->second.hasSpeedBoost) {
        return;
    }
    
    PlayerState& player = it->second;
    if (std::chrono::steady_clock::now() > player.speedBoostEndTime) {
        player.hasSpeedBoost = false;
    }
}

std::tuple<int, int, int> GameLogic::FindRandomSpawnPoint() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(0, config_.mazeWidth - 1);
    std::uniform_int_distribution<> disY(0, config_.mazeLayers - 1);
    std::uniform_int_distribution<> disZ(0, config_.mazeHeight - 1);
    
    // 寻找空位置（非墙壁）
    while (true) {
        int x = disX(gen);
        int y = disY(gen);
        int z = disZ(gen);
        
        if (!mazeLayout_[y][x][z]) {  // 如果是空位置
            return {x, y, z};
        }
    }
}

// ========== 为CommandSystem扩展的实现 ==========

bool GameLogic::GiveItem(int playerId, ItemType itemType, int count) {
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    PlayerState& player = it->second;
    player.inventory[itemType] += count;
    return true;
}

bool GameLogic::TeleportPlayer(int playerId, float x, float y, float z) {
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    // 检查目标位置是否有效
    if (!IsValidPosition(x, y, z)) {
        return false;
    }
    
    PlayerState& player = it->second;
    player.x = x;
    player.y = y;
    player.z = z;
    
    return true;
}

bool GameLogic::KillPlayer(int playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end() || !it->second.isAlive) {
        return false;
    }
    
    // 标记玩家为死亡
    it->second.isAlive = false;
    
    // 立即重生
    RespawnPlayer(playerId);
    
    return true;
}

bool GameLogic::SetPlayerCoins(int playerId, int coins) {
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    it->second.coins = coins;
    return true;
}

std::vector<int> GameLogic::GetAllPlayerIds() const {
    std::vector<int> playerIds;
    for (const auto& pair : players_) {
        playerIds.push_back(pair.first);
    }
    return playerIds;
}

void GameLogic::ResetGameState() {
    // 重置所有玩家状态
    for (auto& pair : players_) {
        PlayerState& player = pair.second;
        
        // 重置位置到起点
        player.x = static_cast<float>(std::get<0>(startPosition_));
        player.y = static_cast<float>(std::get<1>(startPosition_));
        player.z = static_cast<float>(std::get<2>(startPosition_));
        
        // 重置游戏状态
        player.isAlive = true;
        player.hasCompass = false;
        player.hasSpeedBoost = false;
        player.reachedGoal = false;
        player.finishRank = 0;
        
        // 保留金币和道具库存
        // player.coins 保持不变
        // player.inventory 保持不变
    }
    
    // 重置金币收集状态
    std::fill(coinCollected_.begin(), coinCollected_.end(), false);
    remainingCoins_ = static_cast<int>(coinPositions_.size());
    
    // 重置完成状态
    finishedPlayersCount_ = 0;
    nextFinishRank_ = 1;
    
    // 清除减速带
    slowTraps_.clear();
    
    // 恢复所有被破坏的墙壁
    for (const auto& wallPos : brokenWalls_) {
        int x = std::get<0>(wallPos);
        int y = std::get<1>(wallPos);
        int z = std::get<2>(wallPos);
        if (x >= 0 && x < config_.mazeWidth &&
            y >= 0 && y < config_.mazeLayers &&
            z >= 0 && z < config_.mazeHeight) {
            mazeLayout_[y][x][z] = true;
        }
    }
    brokenWalls_.clear();
    wallRepairTimes_.clear();
}

bool GameLogic::IsValidPosition(float x, float y, float z) const {
    // 转换为网格坐标
    int gridX = static_cast<int>(std::round(x));
    int gridY = static_cast<int>(std::round(y));
    int gridZ = static_cast<int>(std::round(z));
    
    // 检查边界
    if (gridX < 0 || gridX >= config_.mazeWidth ||
        gridY < 0 || gridY >= config_.mazeLayers ||
        gridZ < 0 || gridZ >= config_.mazeHeight) {
        return false;
    }
    
    // 检查是否为墙壁
    return !mazeLayout_[gridY][gridX][gridZ];
}