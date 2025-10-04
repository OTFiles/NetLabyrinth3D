cmake_minimum_required(VERSION 3.10)
project(3DMazeGame)

set(CMAKE_CXX_STANDARD 17)

# 包含目录
include_directories(include)

# 根据平台设置网络库
if(WIN32)
    set(NETWORK_LIBS ws2_32 mswsock crypt32)
    add_definitions(-D_WIN32_WINNT=0x0601)
else()
    set(NETWORK_LIBS pthread ssl crypto)
endif()

# 查找 OpenSSL（用于 WebSocket 握手）
find_package(OpenSSL REQUIRED)

# 多方式nlohmann/json库支持（优先级从高到低）
if(NOT TARGET nlohmann_json::nlohmann_json)
    # 方法1: 使用系统包管理器安装的nlohmann-json（如apt install nlohmann-json3-dev）
    find_package(nlohmann_json 3.11.2 QUIET)
    if(nlohmann_json_FOUND)
        message(STATUS "Using system nlohmann/json library")
    else()
        # 方法2: 如果third_party/nlohmann存在，使用本地版本
        if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/nlohmann")
            include_directories(third_party/nlohmann/include)
            message(STATUS "Using local nlohmann/json library")
            add_definitions(-DHAS_LOCAL_NLOHMANN)
        else()
            # 方法3: 使用FetchContent下载
            include(FetchContent)
            FetchContent_Declare(
                nlohmann_json
                GIT_REPOSITORY https://github.com/nlohmann/json.git
                GIT_TAG v3.11.2
            )
            FetchContent_MakeAvailable(nlohmann_json)
            message(STATUS "Using FetchContent for nlohmann/json library")
        endif()
    endif()
else()
    message(STATUS "nlohmann/json target already defined")
endif()

# 添加可执行文件
add_executable(server
    src/main.cpp
    src/NetworkManager.cpp
    src/MazeGenerator.cpp
    src/GameLogic.cpp
    src/PlayerManager.cpp
    src/CommandSystem.cpp
    src/DataManager.cpp
    src/WebServer.cpp
    src/Logger.cpp
)

# 链接库
target_link_libraries(server ${NETWORK_LIBS} OpenSSL::SSL OpenSSL::Crypto)

# 链接nlohmann/json（根据检测到的方式）
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(server nlohmann_json::nlohmann_json)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/nlohmann")
    # 本地版本不需要额外链接，只需要包含头文件
    message(STATUS "Linking: Using header-only local nlohmann/json")
else()
    # FetchContent版本需要链接
    target_link_libraries(server nlohmann_json::nlohmann_json)
endif()

# 设置输出目录
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装目标（可选）
install(TARGETS server
    RUNTIME DESTINATION bin
)

# 复制Data目录到构建目录（开发时使用）
# 只在 Data 目录存在时复制
if(EXISTS "${CMAKE_SOURCE_DIR}/Data")
    add_custom_command(TARGET server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Data
        ${CMAKE_BINARY_DIR}/bin/Data
        COMMENT "Copying Data directory to build output"
    )
else()
    message(STATUS "Data directory not found, skipping copy")
endif()

# 复制web目录到构建目录（开发时使用）
add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/web
    ${CMAKE_BINARY_DIR}/bin/web
    COMMENT "Copying web directory to build output"
)

# 禁用 OpenSSL 弃用警告
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(server PRIVATE -Wno-deprecated-declarations)
endif()