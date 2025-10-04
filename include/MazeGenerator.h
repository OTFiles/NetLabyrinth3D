#ifndef MAZEGENERATOR_H
#define MAZEGENERATOR_H

#include <vector>
#include <string>
#include <cstdint>

enum class CellType {
    WALL = 0,
    PATH = 1,
    STAIR_UP = 2,
    STAIR_DOWN = 3,
    COIN = 4,
    START = 5,
    END = 6
};

enum class Direction {
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3,
    UP = 4,
    DOWN = 5
};

struct Position {
    int x, y, z;
    Position(int x = 0, int y = 0, int z = 0) : x(x), y(y), z(z) {}
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

class MazeGenerator {
public:
    MazeGenerator(int width = 50, int height = 50, int layers = 7);
    ~MazeGenerator();

    // 生成迷宫
    void generateMaze();
    
    // 序列化/反序列化
    bool saveToFile(const std::string& filename);
    bool loadFromFile(const std::string& filename);
    
    // 查询接口
    CellType getCellType(int x, int y, int z) const;
    bool canMove(int x, int y, int z, Direction dir) const;
    std::vector<Direction> getPossibleMoves(int x, int y, int z) const;
    Position getStartPosition() const { return startPosition; }
    Position getEndPosition() const { return endPosition; }
    int getCoinCount() const { return coinCount; }
    
    // 获取迷宫尺寸
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getLayers() const { return layers; }

private:
    int width, height, layers;
    std::vector<std::vector<std::vector<CellType>>> maze;
    Position startPosition;
    Position endPosition;
    int coinCount;
    
    // 迷宫生成算法
    void initializeMaze();
    void recursiveDivision(int minX, int maxX, int minY, int maxY, int layer, bool horizontal);
    void addStairs();
    void placeStartAndEnd();
    void distributeCoins();
    
    // 工具函数
    bool isValidPosition(int x, int y, int z) const;
    bool isBorder(int x, int y, int z) const;
    int calculateDistance(const Position& a, const Position& b) const;
    Position findFarthestPosition(const Position& from) const;
};

#endif // MAZEGENERATOR_H