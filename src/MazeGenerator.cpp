#include "MazeGenerator.h"
#include <fstream>
#include <random>
#include <algorithm>
#include <iostream>

MazeGenerator::MazeGenerator(int width, int height, int layers) 
    : width(width), height(height), layers(layers), coinCount(0) {
    maze.resize(layers);
    for (int z = 0; z < layers; z++) {
        maze[z].resize(height);
        for (int y = 0; y < height; y++) {
            maze[z][y].resize(width, CellType::WALL);
        }
    }
}

MazeGenerator::~MazeGenerator() {}

void MazeGenerator::generateMaze() {
    initializeMaze();
    
    // 为每一层生成迷宫
    for (int z = 0; z < layers; z++) {
        recursiveDivision(1, width - 2, 1, height - 2, z, true);
    }
    
    addStairs();
    placeStartAndEnd();
    distributeCoins();
}

void MazeGenerator::initializeMaze() {
    // 初始化所有单元格为墙
    for (int z = 0; z < layers; z++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                maze[z][y][x] = CellType::WALL;
            }
        }
    }
    
    // 设置边界
    for (int z = 0; z < layers; z++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                    maze[z][y][x] = CellType::WALL;
                }
            }
        }
    }
}

void MazeGenerator::recursiveDivision(int minX, int maxX, int minY, int maxY, int layer, bool horizontal) {
    // 添加边界检查，确保索引在有效范围内
    if (minX < 1 || maxX >= width - 1 || minY < 1 || maxY >= height - 1) {
        return;
    }
    
    if (maxX - minX < 2 || maxY - minY < 2) {
        return;
    }
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    if (horizontal) {
        // 水平分割 - 确保wallY在有效范围内
        int rangeY = (maxY - minY) / 2 - 1;
        if (rangeY <= 0) return;
        
        int wallY = minY + 2 * (gen() % rangeY) + 1;
        if (wallY >= height - 1) wallY = height - 2;
        
        // 创建墙
        for (int x = minX; x <= maxX && x < width; x++) {
            if (wallY < height) {
                maze[layer][wallY][x] = CellType::WALL;
            }
        }
        
        // 开一个门 - 确保doorX在有效范围内
        int rangeX = (maxX - minX) / 2;
        if (rangeX <= 0) return;
        
        int doorX = minX + 2 * (gen() % rangeX) + 1;
        if (doorX >= width - 1) doorX = width - 2;
        
        if (wallY < height && doorX < width) {
            maze[layer][wallY][doorX] = CellType::PATH;
        }
        
        // 递归处理上下两个区域
        recursiveDivision(minX, maxX, minY, wallY - 1, layer, !horizontal);
        recursiveDivision(minX, maxX, wallY + 1, maxY, layer, !horizontal);
    } else {
        // 垂直分割 - 确保wallX在有效范围内
        int rangeX = (maxX - minX) / 2 - 1;
        if (rangeX <= 0) return;
        
        int wallX = minX + 2 * (gen() % rangeX) + 1;
        if (wallX >= width - 1) wallX = width - 2;
        
        // 创建墙
        for (int y = minY; y <= maxY && y < height; y++) {
            if (wallX < width) {
                maze[layer][y][wallX] = CellType::WALL;
            }
        }
        
        // 开一个门 - 确保doorY在有效范围内
        int rangeY = (maxY - minY) / 2;
        if (rangeY <= 0) return;
        
        int doorY = minY + 2 * (gen() % rangeY) + 1;
        if (doorY >= height - 1) doorY = height - 2;
        
        if (doorY < height && wallX < width) {
            maze[layer][doorY][wallX] = CellType::PATH;
        }
        
        // 递归处理左右两个区域
        recursiveDivision(minX, wallX - 1, minY, maxY, layer, !horizontal);
        recursiveDivision(wallX + 1, maxX, minY, maxY, layer, !horizontal);
    }
}

void MazeGenerator::addStairs() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // 在每层之间添加楼梯连接
    for (int z = 0; z < layers - 1; z++) {
        // 每层添加2-3个楼梯
        int stairCount = 2 + (gen() % 2);
        
        for (int i = 0; i < stairCount; i++) {
            int attempts = 0;
            while (attempts < 100) {
                int x = 1 + gen() % (width - 2);
                int y = 1 + gen() % (height - 2);
                
                // 确保位置是路径且不在边界上
                if (maze[z][y][x] == CellType::PATH && 
                    maze[z + 1][y][x] == CellType::PATH &&
                    !isBorder(x, y, z) && !isBorder(x, y, z + 1)) {
                    
                    maze[z][y][x] = CellType::STAIR_DOWN;
                    maze[z + 1][y][x] = CellType::STAIR_UP;
                    break;
                }
                attempts++;
            }
        }
    }
}

void MazeGenerator::placeStartAndEnd() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // 起点在第一层
    int attempts = 0;
    bool startFound = false;
    while (attempts < 100) {
        int x = 1 + gen() % (width - 2);
        int y = 1 + gen() % (height - 2);
        
        if (x > 0 && x < width - 1 && y > 0 && y < height - 1 &&
            maze[0][y][x] == CellType::PATH && !isBorder(x, y, 0)) {
            startPosition = Position(x, y, 0);
            maze[0][y][x] = CellType::START;
            startFound = true;
            break;
        }
        attempts++;
    }
    
    // 如果没有找到合适的起点，使用备用位置
    if (!startFound) {
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                if (maze[0][y][x] == CellType::PATH) {
                    startPosition = Position(x, y, 0);
                    maze[0][y][x] = CellType::START;
                    startFound = true;
                    break;
                }
            }
            if (startFound) break;
        }
    }
    
    // 如果还是没有找到，使用默认位置
    if (!startFound) {
        startPosition = Position(1, 1, 0);
        if (isValidPosition(1, 1, 0)) {
            maze[0][1][1] = CellType::START;
        }
    }
    
    // 终点在最高层，距离起点足够远
    endPosition = findFarthestPosition(startPosition);
    if (isValidPosition(endPosition.x, endPosition.y, endPosition.z)) {
        maze[endPosition.z][endPosition.y][endPosition.x] = CellType::END;
    }
}

void MazeGenerator::distributeCoins() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // 生成100-120个金币
    coinCount = 100 + (gen() % 21);
    int coinsPlaced = 0;
    int attempts = 0;
    
    while (coinsPlaced < coinCount && attempts < 10000) {
        int x = 1 + gen() % (width - 2);
        int y = 1 + gen() % (height - 2);
        int z = gen() % layers;
        
        // 确保位置是路径，且不是起点或终点
        if (maze[z][y][x] == CellType::PATH && 
            !(x == startPosition.x && y == startPosition.y && z == startPosition.z) &&
            !(x == endPosition.x && y == endPosition.y && z == endPosition.z)) {
            
            maze[z][y][x] = CellType::COIN;
            coinsPlaced++;
        }
        attempts++;
    }
    
    coinCount = coinsPlaced; // 实际放置的金币数量
}

Position MazeGenerator::findFarthestPosition(const Position& from) const {
    Position farthest;
    int maxDistance = 0;
    
    // 在最高层寻找距离最远的位置
    int z = layers - 1;
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            if (maze[z][y][x] == CellType::PATH) {
                int dist = calculateDistance(from, Position(x, y, z));
                if (dist > maxDistance) {
                    maxDistance = dist;
                    farthest = Position(x, y, z);
                }
            }
        }
    }
    
    // 确保距离至少是迷宫尺寸的1/3
    int minRequiredDistance = (width + height + layers) / 3;
    if (maxDistance < minRequiredDistance) {
        // 如果找不到足够远的点，选择最远的可用点
        return farthest;
    }
    
    return farthest;
}

bool MazeGenerator::saveToFile(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // 写入尺寸信息
    file.write(reinterpret_cast<const char*>(&width), sizeof(width));
    file.write(reinterpret_cast<const char*>(&height), sizeof(height));
    file.write(reinterpret_cast<const char*>(&layers), sizeof(layers));
    
    // 写入迷宫数据
    for (int z = 0; z < layers; z++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                CellType cell = maze[z][y][x];
                file.write(reinterpret_cast<const char*>(&cell), sizeof(cell));
            }
        }
    }
    
    file.close();
    return true;
}

bool MazeGenerator::loadFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // 读取尺寸信息
    int newWidth, newHeight, newLayers;
    file.read(reinterpret_cast<char*>(&newWidth), sizeof(newWidth));
    file.read(reinterpret_cast<char*>(&newHeight), sizeof(newHeight));
    file.read(reinterpret_cast<char*>(&newLayers), sizeof(newLayers));
    
    if (newWidth != width || newHeight != height || newLayers != layers) {
        // 尺寸不匹配，需要重新调整
        width = newWidth;
        height = newHeight;
        layers = newLayers;
        
        maze.resize(layers);
        for (int z = 0; z < layers; z++) {
            maze[z].resize(height);
            for (int y = 0; y < height; y++) {
                maze[z][y].resize(width);
            }
        }
    }
    
    // 读取迷宫数据
    for (int z = 0; z < layers; z++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                CellType cell;
                file.read(reinterpret_cast<char*>(&cell), sizeof(cell));
                maze[z][y][x] = cell;
                
                // 更新起点和终点位置
                if (cell == CellType::START) {
                    startPosition = Position(x, y, z);
                } else if (cell == CellType::END) {
                    endPosition = Position(x, y, z);
                } else if (cell == CellType::COIN) {
                    coinCount++;
                }
            }
        }
    }
    
    file.close();
    return true;
}

CellType MazeGenerator::getCellType(int x, int y, int z) const {
    if (!isValidPosition(x, y, z)) {
        return CellType::WALL;
    }
    return maze[z][y][x];
}

bool MazeGenerator::canMove(int x, int y, int z, Direction dir) const {
    if (!isValidPosition(x, y, z)) {
        return false;
    }
    
    CellType currentCell = maze[z][y][x];
    
    switch (dir) {
        case Direction::NORTH:
            return isValidPosition(x, y - 1, z) && 
                   maze[z][y - 1][x] != CellType::WALL;
        case Direction::SOUTH:
            return isValidPosition(x, y + 1, z) && 
                   maze[z][y + 1][x] != CellType::WALL;
        case Direction::EAST:
            return isValidPosition(x + 1, y, z) && 
                   maze[z][y][x + 1] != CellType::WALL;
        case Direction::WEST:
            return isValidPosition(x - 1, y, z) && 
                   maze[z][y][x - 1] != CellType::WALL;
        case Direction::UP:
            return currentCell == CellType::STAIR_UP && 
                   isValidPosition(x, y, z + 1) && 
                   maze[z + 1][y][x] != CellType::WALL;
        case Direction::DOWN:
            return currentCell == CellType::STAIR_DOWN && 
                   isValidPosition(x, y, z - 1) && 
                   maze[z - 1][y][x] != CellType::WALL;
        default:
            return false;
    }
}

std::vector<Direction> MazeGenerator::getPossibleMoves(int x, int y, int z) const {
    std::vector<Direction> moves;
    
    if (canMove(x, y, z, Direction::NORTH)) moves.push_back(Direction::NORTH);
    if (canMove(x, y, z, Direction::SOUTH)) moves.push_back(Direction::SOUTH);
    if (canMove(x, y, z, Direction::EAST)) moves.push_back(Direction::EAST);
    if (canMove(x, y, z, Direction::WEST)) moves.push_back(Direction::WEST);
    if (canMove(x, y, z, Direction::UP)) moves.push_back(Direction::UP);
    if (canMove(x, y, z, Direction::DOWN)) moves.push_back(Direction::DOWN);
    
    return moves;
}

bool MazeGenerator::isValidPosition(int x, int y, int z) const {
    return x >= 0 && x < width && y >= 0 && y < height && z >= 0 && z < layers;
}

bool MazeGenerator::isBorder(int x, int y, int z) const {
    return x == 0 || x == width - 1 || y == 0 || y == height - 1;
}

int MazeGenerator::calculateDistance(const Position& a, const Position& b) const {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z);
}