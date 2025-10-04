#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>
#include <vector>
#include <filesystem>

// 日志级别枚举
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR
};

// 日志类别枚举
enum class LogCategory {
    SYSTEM = 0,
    NETWORK,
    GAME,
    PLAYER,
    COMMAND,
    DATABASE,
    WEB
};

class Logger {
public:
    static Logger& getInstance();
    
    // 初始化日志系统
    bool initialize(const std::string& logDirectory = "Data");
    
    // 设置日志级别（低于此级别的日志将被忽略）
    void setLogLevel(LogLevel level);
    
    // 设置是否输出到控制台
    void setConsoleOutput(bool enabled);
    
    // 设置是否输出到文件
    void setFileOutput(bool enabled);
    
    // 记录日志
    void log(LogLevel level, LogCategory category, const std::string& message);
    
    // 便捷日志方法
    void debug(LogCategory category, const std::string& message);
    void info(LogCategory category, const std::string& message);
    void warning(LogCategory category, const std::string& message);
    void error(LogCategory category, const std::string& message);
    
    // 玩家行为记录（特殊格式）
    void logPlayerAction(const std::string& playerId, const std::string& action, const std::string& details = "");
    
    // 命令执行记录
    void logCommand(const std::string& executor, const std::string& command, const std::string& target = "", bool success = true);
    
    // 系统事件记录
    void logSystemEvent(const std::string& event, const std::string& details = "");
    
    // 获取日志文件路径
    std::string getLogFilePath() const;
    
    // 清理过期的日志文件（保留最近N天）
    void cleanupOldLogs(int daysToKeep = 7);

    // 获取当前时间的ISO格式字符串（用于API等）
    std::string getCurrentISOTimeString() const;

private:
    Logger();
    ~Logger();
    
    // 禁止拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // 获取当前时间字符串
    std::string getCurrentTimeString() const;
    
    // 获取当前日期字符串（用于日志文件命名）
    std::string getCurrentDateString() const;
    
    // 获取日志级别字符串
    std::string getLevelString(LogLevel level) const;
    
    // 获取日志类别字符串
    std::string getCategoryString(LogCategory category) const;
    
    // 确保日志目录存在
    bool ensureLogDirectory() const;
    
    // 轮转日志文件（按日期）
    void rotateLogFileIfNeeded();
    
    // 写入日志到文件
    void writeToFile(const std::string& logMessage);
    
    // 输出到控制台（带颜色）
    void outputToConsole(LogLevel level, const std::string& message);

private:
    std::ofstream logFile_;
    std::mutex mutex_;
    
    LogLevel currentLevel_;
    bool consoleOutput_;
    bool fileOutput_;
    std::string logDirectory_;
    std::string currentLogFile_;
    
    // 日志级别到颜色的映射（控制台输出）
    std::map<LogLevel, std::string> levelColors_;
    
    // 日志类别到前缀的映射
    std::map<LogCategory, std::string> categoryPrefixes_;
};

#endif // LOGGER_H