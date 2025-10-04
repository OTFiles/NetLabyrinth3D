#include "GlobalState.h"

// 定义全局变量
std::atomic<bool> g_shutdownRequested(false);
std::atomic<bool> g_commandInputInProgress(false);
std::string g_currentInputLine;