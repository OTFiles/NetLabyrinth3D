#include "Logger.h"
#include "GlobalState.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <filesystem>

namespace fs = std::filesystem;

Logger::Logger()
    : currentLevel_(LogLevel::INFO)
    , consoleOutput_(true)
    , fileOutput_(true)
    , logDirectory_("Data") {
    
    // 初始化日志级别颜色
    levelColors_[LogLevel::DEBUG] = "\033[36m";
    levelColors_[LogLevel::INFO] = "\033[32m";
    levelColors_[LogLevel::WARNING] = "\033[33m";
    levelColors_[LogLevel::ERROR] = "\033[31m";
    
    // 初始化日志类别前缀
    categoryPrefixes_[LogCategory::SYSTEM] = "SYS";
    categoryPrefixes_[LogCategory::NETWORK] = "NET";
    categoryPrefixes_[LogCategory::GAME] = "GAME";
    categoryPrefixes_[LogCategory::PLAYER] = "PLAYER";
    categoryPrefixes_[LogCategory::COMMAND] = "CMD";
    categoryPrefixes_[LogCategory::DATABASE] = "DB";
    categoryPrefixes_[LogCategory::WEB] = "WEB";
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

bool Logger::initialize(const std::string& logDirectory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    logDirectory_ = logDirectory;
    
    if (!ensureLogDirectory()) {
        std::cerr << "Failed to create log directory: " << logDirectory_ << std::endl;
        return false;
    }
    
    rotateLogFileIfNeeded();
    
    // 直接记录初始化消息，避免递归调用logSystemEvent
    std::string message = "Logger initialized - Log directory: " + logDirectory_;
    std::string timestamp = getCurrentTimeString();
    std::string levelStr = "INFO";
    std::string categoryStr = "SYS";
    
    std::stringstream logMessage;
    logMessage << "[" << timestamp << "] "
               << "[" << levelStr << "] "
               << "[" << categoryStr << "] "
               << message;
    
    std::string fullMessage = logMessage.str();
    
    // 输出到控制台
    if (consoleOutput_) {
        outputToConsole(LogLevel::INFO, fullMessage);
    }
    
    // 输出到文件
    if (fileOutput_) {
        writeToFile(fullMessage);
    }
    
    return true;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
    
    // 避免递归调用log，直接输出到std::cout
    std::cout << "Log level set to: " << getLevelString(level) << std::endl;
}

void Logger::setConsoleOutput(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    consoleOutput_ = enabled;
    
    // 避免递归调用log，直接输出到std::cout
    std::cout << "Console output " << (enabled ? "enabled" : "disabled") << std::endl;
}

void Logger::setFileOutput(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    fileOutput_ = enabled;
    
    // 避免递归调用log，直接输出到std::cout
    std::cout << "File output " << (enabled ? "enabled" : "disabled") << std::endl;
}

void Logger::log(LogLevel level, LogCategory category, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(currentLevel_)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    rotateLogFileIfNeeded();
    
    std::string timestamp = getCurrentTimeString();
    std::string levelStr = getLevelString(level);
    std::string categoryStr = getCategoryString(category);
    
    std::stringstream logMessage;
    logMessage << "[" << timestamp << "] "
               << "[" << levelStr << "] "
               << "[" << categoryStr << "] "
               << message;
    
    std::string fullMessage = logMessage.str();
    
    // 输出到控制台
    if (consoleOutput_) {
        // 如果有命令正在输入，先换行输出日志，然后重新显示命令提示符
        if (g_commandInputInProgress && !g_currentInputLine.empty()) {
            // 先换行到新的一行
            std::cout << std::endl;
            // 输出日志
            outputToConsole(level, fullMessage);
            // 重新显示命令提示符和当前输入的内容
            std::cout << "\033[1;32m命令>\033[0m " << g_currentInputLine;
            std::cout.flush();
        } else if (g_commandInputInProgress) {
            // 只是命令提示符，没有输入内容
            std::cout << std::endl;
            outputToConsole(level, fullMessage);
            std::cout << "\033[1;32m命令>\033[0m ";
            std::cout.flush();
        } else {
            // 直接输出日志
            outputToConsole(level, fullMessage);
            std::cout.flush();
        }
    }
    
    // 输出到文件
    if (fileOutput_) {
        writeToFile(fullMessage);
    }
}

void Logger::debug(LogCategory category, const std::string& message) {
    log(LogLevel::DEBUG, category, message);
}

void Logger::info(LogCategory category, const std::string& message) {
    log(LogLevel::INFO, category, message);
}

void Logger::warning(LogCategory category, const std::string& message) {
    log(LogLevel::WARNING, category, message);
}

void Logger::error(LogCategory category, const std::string& message) {
    log(LogLevel::ERROR, category, message);
}

void Logger::logPlayerAction(const std::string& playerId, const std::string& action, const std::string& details) {
    std::string message = "Player " + playerId + " " + action;
    if (!details.empty()) {
        message += " (" + details + ")";
    }
    log(LogLevel::INFO, LogCategory::PLAYER, message);
}

void Logger::logCommand(const std::string& executor, const std::string& command, const std::string& target, bool success) {
    std::string message = executor + " executed command: " + command;
    if (!target.empty()) {
        message += " on " + target;
    }
    message += " [" + std::string(success ? "SUCCESS" : "FAILED") + "]";
    log(LogLevel::INFO, LogCategory::COMMAND, message);
}

void Logger::logSystemEvent(const std::string& event, const std::string& details) {
    std::string message = event;
    if (!details.empty()) {
        message += " - " + details;
    }
    log(LogLevel::INFO, LogCategory::SYSTEM, message);
}

std::string Logger::getLogFilePath() const {
    return currentLogFile_;
}

void Logger::cleanupOldLogs(int daysToKeep) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!fs::exists(logDirectory_)) {
            return;
        }
        
        auto now = std::chrono::system_clock::now();
        auto cutoffTime = now - std::chrono::hours(24 * daysToKeep);
        
        for (const auto& entry : fs::directory_iterator(logDirectory_)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                
                // 检查是否是日志文件
                if (filename.find("server_") == 0 && filename.find(".log") != std::string::npos) {
                    auto fileTime = fs::last_write_time(entry);
                    auto sysTime = std::chrono::system_clock::to_time_t(
                        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()));
                    
                    if (sysTime < std::chrono::system_clock::to_time_t(cutoffTime)) {
                        fs::remove(entry.path());
                        log(LogLevel::INFO, LogCategory::SYSTEM, 
                            "Removed old log file: " + filename);
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        log(LogLevel::ERROR, LogCategory::SYSTEM, 
            "Failed to cleanup old logs: " + std::string(e.what()));
    }
}

std::string Logger::getCurrentTimeString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << milliseconds.count();
    
    return ss.str();
}

std::string Logger::getCurrentISOTimeString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

std::string Logger::getCurrentDateString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    
    ss << std::put_time(&tm, "%Y%m%d");
    return ss.str();
}

std::string Logger::getLevelString(LogLevel level) const {
    static const std::map<LogLevel, std::string> levelStrings = {
        {LogLevel::DEBUG, "DEBUG"},
        {LogLevel::INFO, "INFO"},
        {LogLevel::WARNING, "WARN"},
        {LogLevel::ERROR, "ERROR"}
    };
    
    auto it = levelStrings.find(level);
    return it != levelStrings.end() ? it->second : "UNKNOWN";
}

std::string Logger::getCategoryString(LogCategory category) const {
    auto it = categoryPrefixes_.find(category);
    return it != categoryPrefixes_.end() ? it->second : "UNKNOWN";
}

bool Logger::ensureLogDirectory() const {
    try {
        // 如果目录不存在，创建它；如果已存在，也返回成功
        if (!fs::exists(logDirectory_)) {
            return fs::create_directories(logDirectory_);
        }
        return true;  // 目录已存在
    }
    catch (const std::exception& e) {
        std::cerr << "Error creating log directory: " << e.what() << std::endl;
        return false;
    }
}

void Logger::rotateLogFileIfNeeded() {
    std::string today = getCurrentDateString();
    std::string newLogFile = (fs::path(logDirectory_) / ("server_" + today + ".log")).string();
    
    if (currentLogFile_ != newLogFile) {
        if (logFile_.is_open()) {
            logFile_.close();
        }
        
        currentLogFile_ = newLogFile;
        logFile_.open(currentLogFile_, std::ios::app);
        
        if (!logFile_.is_open()) {
            std::cerr << "Failed to open log file: " << currentLogFile_ << std::endl;
        }
    }
}

void Logger::writeToFile(const std::string& logMessage) {
    if (logFile_.is_open()) {
        logFile_ << logMessage << std::endl;
        logFile_.flush();
    }
}

void Logger::outputToConsole(LogLevel level, const std::string& message) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    
    WORD originalColor = consoleInfo.wAttributes;
    WORD textColor;
    
    switch (level) {
        case LogLevel::DEBUG: textColor = FOREGROUND_BLUE | FOREGROUND_GREEN; break;
        case LogLevel::INFO: textColor = FOREGROUND_GREEN; break;
        case LogLevel::WARNING: textColor = FOREGROUND_RED | FOREGROUND_GREEN; break;
        case LogLevel::ERROR: textColor = FOREGROUND_RED; break;
        default: textColor = originalColor;
    }
    
    SetConsoleTextAttribute(hConsole, textColor);
    std::cout << message;
    SetConsoleTextAttribute(hConsole, originalColor);
    std::cout << std::endl;
#else
    std::string colorCode = levelColors_.at(level);
    std::string resetCode = "\033[0m";
    std::cout << colorCode << message << resetCode << std::endl;
#endif
}