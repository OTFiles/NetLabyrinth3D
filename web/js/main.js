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
        
        this.init();
    }
    
    async init() {
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
        this.gameState.playerName = playerName;
        
        this.networkClient.connect(serverAddress, playerName)
            .then(() => {
                this.gameState.isConnected = true;
                this.showGameScreen();
            })
            .catch(error => {
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
        const data = JSON.parse(message);
        
        switch (data.type) {
            case 'player_joined':
                this.handlePlayerJoined(data);
                break;
            case 'player_left':
                this.handlePlayerLeft(data);
                break;
            case 'player_moved':
                this.handlePlayerMoved(data);
                break;
            case 'player_data':
                this.handlePlayerData(data);
                break;
            case 'maze_data':
                this.handleMazeData(data);
                break;
            case 'chat_message':
                this.handleChatMessage(data);
                break;
            case 'game_state':
                this.handleGameState(data);
                break;
            case 'item_used':
                this.handleItemUsed(data);
                break;
            case 'game_over':
                this.handleGameOver(data);
                break;
            case 'system_message':
                this.handleSystemMessage(data);
                break;
        }
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
    
    handlePlayerData(data) {
        this.gameState.playerId = data.playerId;
        this.gameState.coins = data.coins;
        this.gameState.inventory = data.inventory;
        this.gameState.position = data.position;
        
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