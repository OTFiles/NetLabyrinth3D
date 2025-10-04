#ifndef GLOBALSTATE_H
#define GLOBALSTATE_H

#include <atomic>
#include <string>

// 全局变量用于优雅关闭
extern std::atomic<bool> g_shutdownRequested;

// 全局变量用于命令输入状态
extern std::atomic<bool> g_commandInputInProgress;
extern std::string g_currentInputLine;

#endif // GLOBALSTATE_H