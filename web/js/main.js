// 主游戏入口文件
class MazeGame {
    constructor() {
        this.networkClient = new NetworkClient();
        this.renderer3D = new Renderer3D();
        this.gameController = new GameController();
        this.uiManager = new UIManager();
        
        this.gameState = {
            isConnected: false,
            playerId: null,
            playerName: '',
            coins: 0,
            position: { x: 0, y: 0, z: 0 },
            inventory: {
                speed_potion: 0,
                compass: 0,
                hammer: 0,
                sword: 0,
                slow_trap: 0,
                swap_item: 0
            },
            activeItem: null,
            players: {},
            mazeData: null,
            gameStarted: false
        };

        // 调试系统
        this.debugEnabled = true;
        this.debugLogs = [];
        this.maxDebugLogs = 100;
        
        this.init();
    }
    
    async init() {
        // 初始化调试系统
        this.initDebugSystem();
        
        // 初始化UI管理器
        this.uiManager.init(this);
        
        // 初始化网络客户端
        this.networkClient.init(this);
        
        // 初始化游戏控制器
        this.gameController.init(this);
        
        // 显示加载界面
        this.showLoadingScreen();
        
        // 预加载资源
        await this.preloadResources();
        
        // 切换到登录界面
        this.showLoginScreen();

        this.log('游戏初始化完成', 'info');
    }
    
    async preloadResources() {
        const resources = [
            'img/textures/wall.png',
            'img/textures/floor.png',
            'img/textures/ceiling.png',
            'img/textures/player_skin.png',
            'img/ui/speed_potion.png',
            'img/ui/compass.png',
            'img/ui/hammer.png',
            'img/ui/sword.png',
            'img/ui/slow_trap.png',
            'img/ui/swap_item.png',
            'img/ui/coin.png',
            'img/effects/speed_effect.png',
            'img/effects/death_effect.png'
        ];
        
        let loaded = 0;
        const total = resources.length;
        
        for (const resource of resources) {
            try {
                await this.loadImage(resource);
                loaded++;
                const progress = (loaded / total) * 100;
                this.updateLoadingProgress(progress);
            } catch (error) {
                console.warn(`Failed to load resource: ${resource}`, error);
                loaded++;
                const progress = (loaded / total) * 100;
                this.updateLoadingProgress(progress);
            }
        }
    }
    
    loadImage(src) {
        return new Promise((resolve, reject) => {
            const img = new Image();
            img.onload = () => resolve(img);
            img.onerror = reject;
            img.src = src;
        });
    }

    // 调试系统方法
    initDebugSystem() {
        this.log('初始化调试系统', 'info');
        
        // 添加全局调试方法
        window.debugLog = (message, type = 'info') => {
            this.log(message, type);
        };

        // 添加键盘快捷键显示/隐藏调试面板
        document.addEventListener('keydown', (event) => {
            if (event.code === 'F12' || (event.ctrlKey && event.shiftKey && event.code === 'KeyD')) {
                event.preventDefault();
                this.toggleDebugPanel();
            }
        });
    }

    log(message, type = 'info') {
        if (!this.debugEnabled) return;
        
        const timestamp = new Date().toLocaleTimeString();
        const logEntry = {
            timestamp: timestamp,
            message: message,
            type: type
        };
        
        this.debugLogs.push(logEntry);
        
        // 限制日志数量
        if (this.debugLogs.length > this.maxDebugLogs) {
            this.debugLogs.shift();
        }
        
        // 更新调试面板
        this.updateDebugPanel();
        
        // 同时在控制台输出
        const consoleMethod = type === 'error' ? 'error' : 
                             type === 'warning' ? 'warn' : 'log';
        console[consoleMethod](`[${timestamp}] ${message}`);
    }

    updateDebugPanel() {
        const debugContent = document.getElementById('debugContent');
        if (!debugContent) return;
        
        debugContent.innerHTML = '';
        
        this.debugLogs.forEach(log => {
            const logElement = document.createElement('div');
            logElement.className = `debug-log ${log.type}`;
            logElement.innerHTML = `
                <span class="timestamp">[${log.timestamp}]</span> ${log.message}
            `;
            debugContent.appendChild(logElement);
        });
        
        // 自动滚动到底部
        debugContent.scrollTop = debugContent.scrollHeight;
    }

    toggleDebugPanel() {
        const debugPanel = document.getElementById('debugPanel');
        if (debugPanel) {
            const isHidden = debugPanel.style.display === 'none';
            debugPanel.style.display = isHidden ? 'flex' : 'none';
            
            const toggleButton = document.getElementById('toggleDebug');
            if (toggleButton) {
                toggleButton.textContent = isHidden ? '隐藏' : '显示';
            }
        }
    }

    clearDebugLogs() {
        this.debugLogs = [];
        this.updateDebugPanel();
        this.log('调试日志已清空', 'info');
    }
    
    updateLoadingProgress(progress) {
        const progressBar = document.querySelector('.loading-progress');
        if (progressBar) {
            progressBar.style.width = `${progress}%`;
        }
    }
    
    showLoadingScreen() {
        document.getElementById('loadingScreen').classList.remove('hidden');
        document.getElementById('loginScreen').classList.add('hidden');
        document.getElementById('gameScreen').classList.add('hidden');
    }
    
    showLoginScreen() {
        document.getElementById('loadingScreen').classList.add('hidden');
        document.getElementById('loginScreen').classList.remove('hidden');
        document.getElementById('gameScreen').classList.add('hidden');
        
        // 聚焦到玩家名称输入框
        setTimeout(() => {
            document.getElementById('playerName').focus();
        }, 100);
    }
    
    showGameScreen() {
        document.getElementById('loadingScreen').classList.add('hidden');
        document.getElementById('loginScreen').classList.add('hidden');
        document.getElementById('gameScreen').classList.remove('hidden');
        
        // 初始化3D渲染器
        this.renderer3D.init(this);
        
        // 开始游戏循环
        this.startGameLoop();
    }
    
    startGameLoop() {
        const gameLoop = () => {
            if (this.gameState.gameStarted) {
                this.update();
                this.render();
            }
            requestAnimationFrame(gameLoop);
        };
        gameLoop();
    }
    
    update() {
        // 更新游戏状态
        this.gameController.update();
        
        // 更新3D场景
        this.renderer3D.update();
        
        // 更新UI
        this.uiManager.update();
    }
    
    render() {
        // 渲染3D场景
        this.renderer3D.render();
    }
    
    // 连接服务器
    connectToServer(playerName, serverAddress) {
        this.log(`尝试连接到服务器: ${serverAddress}, 玩家: ${playerName}`, 'network');
        this.gameState.playerName = playerName;
        
        this.networkClient.connect(serverAddress, playerName)
            .then(() => {
                this.gameState.isConnected = true;
                this.log('服务器连接成功，切换到游戏界面', 'network');
                this.showGameScreen();
            })
            .catch(error => {
                this.log(`服务器连接失败: ${error.message}`, 'error');
                this.uiManager.showMessage(`连接失败: ${error.message}`, 'error');
            });
    }
    
    // 发送聊天消息
    sendChatMessage(message) {
        if (this.gameState.isConnected) {
            this.networkClient.sendChatMessage(message);
        }
    }
    
    // 购买道具
    buyItem(itemType) {
        if (this.gameState.isConnected) {
            this.networkClient.buyItem(itemType);
        }
    }
    
    // 使用道具
    useItem(itemType, target = null) {
        if (this.gameState.isConnected) {
            this.networkClient.useItem(itemType, target);
        }
    }
    
    // 处理服务器消息
    handleServerMessage(message) {
        try {
            let data;
            
            // 首先尝试直接解析消息
            try {
                data = JSON.parse(message);
            } catch (e) {
                // 如果不是JSON，可能是简单文本消息
                this.log(`收到非JSON消息: ${message}`, 'network');
                return;
            }
            
            // 处理不同的消息格式
            const messageType = data.type || data.eventType;
            const messageData = data.data || data; // 兼容两种格式

            this.log(`收到服务器消息: ${messageType}`, 'network');
            
            switch (messageType) {
                case 'player_join':
                    this.handlePlayerJoined(messageData);
                    break;
                case 'player_leave':
                case 'player_left': // 兼容两种消息类型
                    this.handlePlayerLeft(messageData);
                    break;
                case 'player_move':
                case 'player_moved': // 兼容两种消息类型
                    this.handlePlayerMoved(messageData);
                    break;
                case 'player_data':
                    this.handlePlayerData(messageData);
                    break;
                case 'maze_data':
                    this.handleMazeData(messageData);
                    break;
                case 'chat_message':
                    this.handleChatMessage(messageData);
                    break;
                case 'game_state':
                    this.handleGameState(messageData);
                    break;
                case 'item_effect':
                case 'item_used': // 兼容两种消息类型
                    this.handleItemUsed(messageData);
                    break;
                case 'game_event':
                    this.handleGameEvent(messageData);
                    break;
                case 'error':
                    this.handleErrorMessage(messageData);
                    break;
                case 'pong': // 处理ping/pong心跳
                    this.lastPongTime = Date.now();
                    this.log('收到心跳响应', 'network');
                    break;
                case 'auth_success': // 处理认证成功
                    this.handleAuthSuccess(messageData);
                    break;
                case 'auth_failed': // 处理认证失败
                    this.handleAuthFailed(messageData);
                    break;
                default:
                    this.log(`未知消息类型: ${messageType}`, 'warning');
                    console.warn('未知消息类型:', messageType, messageData);
            }
        } catch (error) {
            this.log(`消息处理错误: ${error.message}`, 'error');
            console.error('消息处理错误:', error, message);
        }
    }
    
    // 游戏事件处理
    handleGameEvent(data) {
        switch (data.eventType) {
            case 'player_reached_goal':
                this.handlePlayerReachedGoal(data);
                break;
            case 'coin_collected':
                this.handleCoinCollected(data);
                break;
            case 'game_over':
                this.handleGameOver(data);
                break;
            default:
                console.log('游戏事件:', data);
        }
    }
    
    // 处理金币收集
    handleCoinCollected(data) {
        if (data.playerId === this.gameState.playerId) {
            this.gameState.coins = data.totalCoins || this.gameState.coins;
            this.uiManager.updatePlayerInfo(this.gameState.playerName, this.gameState.coins);
        }
    }
    
    // 处理认证成功
    handleAuthSuccess(data) {
        this.log('认证成功', 'network');
        this.gameState.playerId = data.playerId || data.id;
        this.gameState.isConnected = true;
        
        // 保存token到本地存储
        if (data.token) {
            localStorage.setItem('playerToken', data.token);
        }
        
        this.uiManager.showMessage('认证成功，加入游戏中...', 'success');
        
        // 认证成功后切换到游戏界面
        this.showGameScreen();
    }
    
    // 处理认证失败
    handleAuthFailed(data) {
        this.log(`认证失败: ${data.message}`, 'error');
        this.uiManager.showMessage(`认证失败: ${data.message}`, 'error');
        this.networkClient.disconnect();
        this.showLoginScreen();
    }
    
    // 处理玩家到达终点
    handlePlayerReachedGoal(data) {
        const playerName = data.playerName || '未知玩家';
        const isCurrentPlayer = data.playerId === this.gameState.playerId;
        
        if (isCurrentPlayer) {
            this.uiManager.showMessage('恭喜！你第一个到达终点！', 'success');
        } else {
            this.uiManager.addChatMessage('system', `${playerName} 第一个到达终点！`);
        }
    }
    
    // 处理错误消息
    handleErrorMessage(data) {
        const errorMessages = {
            'INVALID_MOVE': '无法移动到该位置',
            'INSUFFICIENT_COINS': '金币不足',
            'ITEM_NOT_OWNED': '未拥有该道具',
            'PLAYER_NOT_FOUND': '玩家不存在',
            'INVALID_TARGET': '无效的目标',
            'GAME_NOT_RUNNING': '游戏未运行'
        };
        
        const message = errorMessages[data.code] || data.message || '发生未知错误';
        this.uiManager.showMessage(message, 'error');
    }
    
    handlePlayerJoined(data) {
        this.gameState.players[data.playerId] = {
            id: data.playerId,
            name: data.playerName,
            position: data.position,
            coins: data.coins
        };
        
        this.uiManager.addChatMessage('system', `${data.playerName} 加入了游戏`);
        this.uiManager.updatePlayerCount(Object.keys(this.gameState.players).length);
    }
    
    handlePlayerLeft(data) {
        if (this.gameState.players[data.playerId]) {
            const playerName = this.gameState.players[data.playerId].name;
            delete this.gameState.players[data.playerId];
            
            this.uiManager.addChatMessage('system', `${playerName} 离开了游戏`);
            this.uiManager.updatePlayerCount(Object.keys(this.gameState.players).length);
        }
    }
    
    handlePlayerMoved(data) {
        if (this.gameState.players[data.playerId]) {
            this.gameState.players[data.playerId].position = data.position;
        }
    }
    
    // 修改玩家数据处理的兼容性
    handlePlayerData(data) {
        // 兼容不同的数据结构
        this.gameState.playerId = data.playerId || data.id;
        this.gameState.coins = data.coins || data.gold || 0;
        this.gameState.position = data.position || { x: 0, y: 0, z: 0 };
        this.gameState.inventory = data.inventory || {
            speed_potion: 0,
            compass: 0,
            hammer: 0,
            sword: 0,
            slow_trap: 0,
            swap_item: 0
        };
        
        this.uiManager.updatePlayerInfo(this.gameState.playerName, this.gameState.coins);
        this.uiManager.updateInventory(this.gameState.inventory);
    }
    
    handleMazeData(data) {
        this.gameState.mazeData = data.maze;
        this.renderer3D.loadMaze(data.maze);
        this.gameState.gameStarted = true;
    }
    
    handleChatMessage(data) {
        this.uiManager.addChatMessage(data.playerName, data.message);
    }
    
    handleGameState(data) {
        // 更新游戏状态
        this.gameState.coins = data.coins;
        this.gameState.inventory = data.inventory;
        
        this.uiManager.updatePlayerInfo(this.gameState.playerName, this.gameState.coins);
        this.uiManager.updateInventory(this.gameState.inventory);
    }
    
    handleItemUsed(data) {
        if (data.playerId === this.gameState.playerId) {
            this.gameState.inventory = data.inventory;
            this.uiManager.updateInventory(this.gameState.inventory);
        }
        
        if (data.effect) {
            this.renderer3D.showEffect(data.effect, data.targetPosition);
        }
    }
    
    handleGameOver(data) {
        this.gameState.gameStarted = false;
        this.uiManager.showGameOver(data.winner, data.leaderboard);
    }
    
    handleSystemMessage(data) {
        this.uiManager.addChatMessage('system', data.message);
    }
}

// 启动游戏
window.addEventListener('DOMContentLoaded', () => {
    window.game = new MazeGame();
});