// UIManager.js - 完整的UI管理器实现
class UIManager {
    constructor() {
        this.game = null;
        this.activeInventorySlot = null;
        this.gameStartTime = null;
        this.gameTimer = null;
        this.isInitialized = false;
        this.serverStatusCheckInterval = null;
        this.currentServerAddress = '';
    }

    init(game) {
        if (this.isInitialized) return;
        
        this.game = game;
        this.isInitialized = true;
        this.setupEventListeners();
        this.setupShop();
        this.initializeUIState();
        
        // 初始检查服务器状态
        this.checkServerStatus();
    }

    initializeUIState() {
        // 初始化UI状态
        this.activeInventorySlot = 0;
        this.updateInventory({});
        this.selectInventorySlot(0);
        this.updateConnectionStatus('disconnected');
    }

    setupEventListeners() {
        // 连接按钮
        const connectButton = document.getElementById('connectButton');
        if (connectButton) {
            connectButton.addEventListener('click', () => {
                this.handleConnect();
            });
        }

        // 刷新服务器状态按钮
        const refreshStatus = document.getElementById('refreshStatus');
        if (refreshStatus) {
            refreshStatus.addEventListener('click', () => {
                this.checkServerStatus();
            });
        }

        // 玩家名称输入框回车事件
        const playerNameInput = document.getElementById('playerName');
        if (playerNameInput) {
            playerNameInput.addEventListener('keypress', (event) => {
                if (event.key === 'Enter') {
                    this.handleConnect();
                }
            });
        }

        // 服务器地址输入框变化事件
        const serverAddressInput = document.getElementById('serverAddress');
        if (serverAddressInput) {
            serverAddressInput.addEventListener('change', () => {
                // 服务器地址变化时重新检查状态
                setTimeout(() => this.checkServerStatus(), 500);
            });
        }

        // 聊天输入
        const chatInput = document.getElementById('chatInput');
        const chatSend = document.getElementById('chatSend');
        
        if (chatInput) {
            chatInput.addEventListener('keypress', (event) => {
                if (event.key === 'Enter') {
                    this.sendChatMessage();
                }
            });
        }

        if (chatSend) {
            chatSend.addEventListener('click', () => {
                this.sendChatMessage();
            });
        }

        // 聊天输入框焦点事件
        if (chatInput) {
            chatInput.addEventListener('focus', () => {
                if (this.game && this.game.gameController) {
                    this.game.gameController.isChatFocused = true;
                }
            });

            chatInput.addEventListener('blur', () => {
                if (this.game && this.game.gameController) {
                    this.game.gameController.isChatFocused = false;
                }
            });
        }

        // 商店关闭按钮
        const closeShop = document.getElementById('closeShop');
        if (closeShop) {
            closeShop.addEventListener('click', () => {
                this.closeShop();
            });
        }

        // 道具栏点击事件
        const inventorySlots = document.querySelectorAll('.inventory-slot');
        inventorySlots.forEach(slot => {
            slot.addEventListener('click', () => {
                const slotIndex = parseInt(slot.dataset.slot);
                this.selectInventorySlot(slotIndex);
            });

            // 鼠标悬停显示工具提示
            slot.addEventListener('mouseenter', () => {
                const tooltip = slot.querySelector('.slot-tooltip');
                if (tooltip) {
                    tooltip.style.display = 'block';
                }
            });

            slot.addEventListener('mouseleave', () => {
                const tooltip = slot.querySelector('.slot-tooltip');
                if (tooltip) {
                    tooltip.style.display = 'none';
                }
            });
        });

        // 重新开始按钮
        const restartButton = document.getElementById('restartGame');
        if (restartButton) {
            restartButton.addEventListener('click', () => {
                this.restartGame();
            });
        }

        // 取消连接按钮
        const cancelConnection = document.getElementById('cancelConnection');
        if (cancelConnection) {
            cancelConnection.addEventListener('click', () => {
                this.cancelConnection();
            });
        }

        // ESC键关闭模态窗口
        document.addEventListener('keydown', (event) => {
            if (event.code === 'Escape') {
                this.closeAllModals();
            }
        });

        // 数字键选择道具栏
        document.addEventListener('keydown', (event) => {
            if (this.game && this.game.gameController && 
                !this.game.gameController.isChatFocused && 
                !this.game.gameController.isShopOpen) {
                
                if (event.code >= 'Digit1' && event.code <= 'Digit6') {
                    const slotIndex = parseInt(event.code[5]) - 1;
                    this.selectInventorySlot(slotIndex);
                    event.preventDefault();
                }
            }
        });
    }

    setupShop() {
        // 商店购买按钮事件
        const buyButtons = document.querySelectorAll('.buy-btn');
        buyButtons.forEach(button => {
            button.addEventListener('click', (event) => {
                const shopItem = event.target.closest('.shop-item');
                if (shopItem) {
                    const itemType = shopItem.dataset.item;
                    this.buyItem(itemType);
                }
            });
        });
    }

    async checkServerStatus() {
        const serverAddressInput = document.getElementById('serverAddress');
        if (!serverAddressInput) return;

        const serverAddress = serverAddressInput.value.trim();
        if (!serverAddress) return;

        this.currentServerAddress = serverAddress;

        const statusContent = document.getElementById('serverStatusContent');
        if (!statusContent) return;

        try {
            statusContent.innerHTML = '<div class="status-loading">正在获取服务器状态...</div>';
            
            const status = await this.game.networkClient.getServerStatus(serverAddress);
            this.displayServerStatus(status);
            
        } catch (error) {
            console.error('Failed to check server status:', error);
            statusContent.innerHTML = `
                <div class="status-error">
                    <div class="status-icon">❌</div>
                    <div class="status-details">
                        <strong>服务器状态:</strong> 无法连接<br>
                        <strong>错误信息:</strong> ${error.message}<br>
                        <small>请检查服务器地址和网络连接</small>
                    </div>
                </div>
            `;
        }
    }

    displayServerStatus(status) {
        const statusContent = document.getElementById('serverStatusContent');
        if (!statusContent) return;

        const statusClass = status.status === 'running' ? 'status-online' : 
                           status.status === 'maintenance' ? 'status-maintenance' : 'status-offline';
        
        const statusText = status.status === 'running' ? '运行中' :
                          status.status === 'maintenance' ? '维护中' : '离线';

        statusContent.innerHTML = `
            <div class="status-${statusClass}">
                <div class="status-icon">${status.status === 'running' ? '✅' : '⚠️'}</div>
                <div class="status-details">
                    <strong>服务器状态:</strong> ${statusText}<br>
                    <strong>在线玩家:</strong> ${status.connectedPlayers || 0}<br>
                    <strong>总玩家数:</strong> ${status.totalPlayers || 0}<br>
                    <strong>运行时间:</strong> ${status.uptime || 'unknown'}<br>
                    <strong>版本:</strong> ${status.gameVersion || '1.0.0'}
                </div>
            </div>
        `;
    }

    handleConnect() {
        const playerNameInput = document.getElementById('playerName');
        const serverAddressInput = document.getElementById('serverAddress');
        
        if (!playerNameInput || !serverAddressInput) {
            this.showMessage('UI元素加载失败，请刷新页面', 'error');
            return;
        }

        const playerName = playerNameInput.value.trim();
        const serverAddress = serverAddressInput.value.trim();

        if (!playerName) {
            this.showMessage('请输入玩家名称', 'error');
            playerNameInput.focus();
            return;
        }

        if (playerName.length < 2 || playerName.length > 20) {
            this.showMessage('玩家名称长度必须在2-20个字符之间', 'error');
            playerNameInput.focus();
            return;
        }

        if (!serverAddress) {
            this.showMessage('请输入服务器地址', 'error');
            serverAddressInput.focus();
            return;
        }

        // 验证服务器地址格式
        const addressRegex = /^([\w.-]+)(:\d+)?$/;
        if (!addressRegex.test(serverAddress)) {
            this.showMessage('服务器地址格式不正确，应为: 地址:端口', 'error');
            serverAddressInput.focus();
            return;
        }

        this.showMessage('正在连接服务器...', 'info');
        
        if (this.game && typeof this.game.connectToServer === 'function') {
            this.game.connectToServer(playerName, serverAddress);
        } else {
            this.showMessage('游戏初始化失败，请刷新页面', 'error');
        }
    }

    sendChatMessage() {
        const chatInput = document.getElementById('chatInput');
        if (!chatInput) return;

        const message = chatInput.value.trim();
        
        if (!message) {
            chatInput.blur();
            return;
        }

        if (message.length > 200) {
            this.showMessage('消息长度不能超过200个字符', 'error');
            return;
        }

        if (this.game && typeof this.game.sendChatMessage === 'function') {
            this.game.sendChatMessage(message);
            chatInput.value = '';
        }

        chatInput.blur();
    }

    buyItem(itemType) {
        if (!this.game || typeof this.game.buyItem !== 'function') {
            this.showMessage('游戏未初始化', 'error');
            return;
        }

        // 检查金币是否足够（这里只是UI检查，实际检查在服务器）
        const currentCoins = this.game.gameState ? this.game.gameState.coins : 0;
        const itemPrices = {
            speed_potion: 20,
            compass: 25,
            hammer: 50,
            sword: 50,
            slow_trap: 30,
            swap_item: 60
        };

        if (currentCoins < (itemPrices[itemType] || 0)) {
            this.showMessage('金币不足！', 'error');
            return;
        }

        this.game.buyItem(itemType);
    }

    getItemName(itemType) {
        const itemNames = {
            'speed_potion': '加速药水',
            'compass': '指南针', 
            'hammer': '锤子',
            'sword': '秒人剑',
            'kill_sword': '秒人剑', // 兼容后端名称
            'slow_trap': '减速带',
            'swap_item': '大局逆转'
        };
        return itemNames[itemType] || '未知道具';
    }
    
    // 道具价格
    getItemPrice(itemType) {
        const itemPrices = {
            'speed_potion': 20,
            'compass': 25,
            'hammer': 50,
            'sword': 50,
            'kill_sword': 50, // 兼容后端名称
            'slow_trap': 30,
            'swap_item': 60
        };
        return itemPrices[itemType] || 0;
    }

    addChatMessage(sender, message) {
        const chatMessages = document.getElementById('chatMessages');
        if (!chatMessages) return;

        const messageElement = document.createElement('div');
        messageElement.className = 'chat-message';
        
        // 添加时间戳
        const timestamp = new Date().toLocaleTimeString('zh-CN', { 
            hour12: false,
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        });
        
        if (sender === 'system') {
            messageElement.className += ' system-message';
            messageElement.innerHTML = `<span class="timestamp">[${timestamp}]</span> [系统] ${this.escapeHtml(message)}`;
        } else {
            messageElement.innerHTML = `
                <span class="timestamp">[${timestamp}]</span>
                <span class="player-name">${sender}:</span> ${this.escapeHtml(message)}
            `;
        }

        chatMessages.appendChild(messageElement);
        
        // 限制消息数量，最多保留100条
        const messages = chatMessages.querySelectorAll('.chat-message');
        if (messages.length > 100) {
            messages[0].remove();
        }
        
        chatMessages.scrollTop = chatMessages.scrollHeight;
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    updatePlayerInfo(playerName, coins) {
        const nameDisplay = document.getElementById('playerNameDisplay');
        const coinsDisplay = document.getElementById('playerCoins');
        
        if (nameDisplay) nameDisplay.textContent = playerName || '未知玩家';
        if (coinsDisplay) coinsDisplay.textContent = `金币: ${coins || 0}`;
    }

    updatePlayerCount(count) {
        const playerCount = document.getElementById('playerCount');
        if (playerCount) {
            playerCount.textContent = `在线: ${count || 0}`;
        }
    }

    updateConnectionStatus(status) {
        const connectionStatus = document.getElementById('connectionStatus');
        if (!connectionStatus) return;

        const statusConfig = {
            connected: { text: '● 已连接', class: 'status-connected' },
            connecting: { text: '● 连接中...', class: 'status-connecting' },
            disconnected: { text: '● 未连接', class: 'status-disconnected' },
            error: { text: '● 连接错误', class: 'status-error' }
        };

        const config = statusConfig[status] || statusConfig.disconnected;
        connectionStatus.textContent = config.text;
        connectionStatus.className = config.class;
    }

    updateInventory(inventory) {
        const itemTypes = [
            'speed_potion', 'compass', 'hammer', 
            'sword', 'slow_trap', 'swap_item'
        ];
        
        itemTypes.forEach((itemType, index) => {
            const slot = document.querySelector(`.inventory-slot[data-slot="${index}"]`);
            if (!slot) return;
            
            const countElement = slot.querySelector('.slot-count');
            const count = inventory[itemType] || 0;
            
            if (countElement) {
                countElement.textContent = count;
                countElement.style.display = count > 0 ? 'block' : 'none';
            }
            
            // 更新槽位状态
            if (count > 0) {
                slot.classList.add('has-item');
            } else {
                slot.classList.remove('has-item');
            }
        });
    }

    selectInventorySlot(slotIndex) {
        // 取消之前选中的槽位
        if (this.activeInventorySlot !== null) {
            const previousSlot = document.querySelector(`.inventory-slot[data-slot="${this.activeInventorySlot}"]`);
            if (previousSlot) {
                previousSlot.classList.remove('active');
            }
        }

        // 选中新槽位
        const newSlot = document.querySelector(`.inventory-slot[data-slot="${slotIndex}"]`);
        if (newSlot) {
            newSlot.classList.add('active');
            this.activeInventorySlot = slotIndex;
            
            // 显示选中道具提示
            const itemTypes = [
                'speed_potion', 'compass', 'hammer', 
                'sword', 'slow_trap', 'swap_item'
            ];
            const itemType = itemTypes[slotIndex];
            const itemName = this.getItemName(itemType);
            
            this.showMessage(`已选择: ${itemName}`, 'info', 1000);
        }
    }

    getActiveSlot() {
        return this.activeInventorySlot;
    }

    openShop() {
        const shopModal = document.getElementById('shopModal');
        if (shopModal) {
            shopModal.classList.remove('hidden');
            if (this.game && this.game.gameController) {
                this.game.gameController.isShopOpen = true;
            }
            
            // 更新商店界面
            this.updateShopInterface();
        }
    }

    updateShopInterface() {
        if (!this.game || !this.game.gameState) return;
        
        const currentCoins = this.game.gameState.coins || 0;
        const shopBalance = document.getElementById('shopBalance');
        if (shopBalance) {
            shopBalance.textContent = currentCoins;
        }
        
        const buyButtons = document.querySelectorAll('.buy-btn');
        
        buyButtons.forEach(button => {
            const shopItem = button.closest('.shop-item');
            if (!shopItem) return;
            
            const itemType = shopItem.dataset.item;
            const itemPrices = {
                speed_potion: 20,
                compass: 25,
                hammer: 50,
                sword: 50,
                slow_trap: 30,
                swap_item: 60
            };
            
            const price = itemPrices[itemType] || 0;
            
            if (currentCoins >= price) {
                button.disabled = false;
                button.textContent = '购买';
                button.title = `花费 ${price} 金币购买`;
            } else {
                button.disabled = true;
                button.textContent = '金币不足';
                button.title = `需要 ${price} 金币，当前只有 ${currentCoins} 金币`;
            }
        });
    }

    closeShop() {
        const shopModal = document.getElementById('shopModal');
        if (shopModal) {
            shopModal.classList.add('hidden');
            if (this.game && this.game.gameController) {
                this.game.gameController.isShopOpen = false;
            }
        }
    }

    showConnectionModal(message, progress = 0) {
        const connectionModal = document.getElementById('connectionModal');
        const connectionMessage = document.getElementById('connectionMessage');
        const progressFill = document.querySelector('.progress-fill');
        
        if (connectionModal && connectionMessage) {
            connectionMessage.textContent = message;
            connectionModal.classList.remove('hidden');
            
            if (progressFill) {
                progressFill.style.width = `${progress}%`;
            }
        }
    }

    updateConnectionModal(message, progress) {
        const connectionMessage = document.getElementById('connectionMessage');
        const progressFill = document.querySelector('.progress-fill');
        
        if (connectionMessage) {
            connectionMessage.textContent = message;
        }
        
        if (progressFill) {
            progressFill.style.width = `${progress}%`;
        }
    }

    hideConnectionModal() {
        const connectionModal = document.getElementById('connectionModal');
        if (connectionModal) {
            connectionModal.classList.add('hidden');
        }
    }

    cancelConnection() {
        if (this.game && this.game.networkClient) {
            this.game.networkClient.disconnect();
        }
        this.hideConnectionModal();
        this.showMessage('连接已取消', 'info');
    }

    closeAllModals() {
        this.closeShop();
        this.hideConnectionModal();
        
        const gameOverModal = document.getElementById('gameOverModal');
        if (gameOverModal) {
            gameOverModal.classList.add('hidden');
        }
    }

    showGameOver(winner, leaderboard) {
        const gameOverModal = document.getElementById('gameOverModal');
        const gameOverTitle = document.getElementById('gameOverTitle');
        const gameOverMessage = document.getElementById('gameOverMessage');
        const leaderboardList = document.getElementById('leaderboardList');
        
        if (!gameOverModal || !gameOverTitle || !gameOverMessage || !leaderboardList) {
            return;
        }

        // 停止游戏计时器
        this.stopGameTimer();

        if (winner === this.game.gameState.playerId) {
            gameOverTitle.textContent = '🎉 恭喜获胜！ 🎉';
            gameOverMessage.textContent = '你是第一个到达终点的玩家！获得最高奖励！';
            gameOverMessage.style.color = '#FFD700';
        } else {
            gameOverTitle.textContent = '游戏结束';
            gameOverMessage.textContent = '其他玩家已到达终点，下次继续努力！';
            gameOverMessage.style.color = '#4cc9f0';
        }

        // 更新排行榜
        leaderboardList.innerHTML = '';
        if (leaderboard && Array.isArray(leaderboard)) {
            leaderboard.forEach((player, index) => {
                const item = document.createElement('div');
                item.className = 'leaderboard-item';
                
                const rankIcon = index === 0 ? '🥇' : index === 1 ? '🥈' : index === 2 ? '🥉' : `${index + 1}.`;
                const isCurrentPlayer = player.id === this.game.gameState.playerId;
                const playerClass = isCurrentPlayer ? 'current-player' : '';
                
                item.innerHTML = `
                    <span class="${playerClass}">${rankIcon} ${player.name}</span>
                    <span class="${playerClass}">${player.coins} 金币</span>
                `;
                leaderboardList.appendChild(item);
            });
        } else {
            leaderboardList.innerHTML = '<div class="no-data">暂无数据</div>';
        }

        gameOverModal.classList.remove('hidden');
    }

    restartGame() {
        this.closeAllModals();
        if (this.game && typeof this.game.showLoginScreen === 'function') {
            this.game.showLoginScreen();
        } else {
            location.reload();
        }
    }

    startGameTimer() {
        this.gameStartTime = Date.now();
        this.stopGameTimer(); // 清除之前的计时器
        
        this.gameTimer = setInterval(() => {
            const elapsed = Math.floor((Date.now() - this.gameStartTime) / 1000);
            const minutes = Math.floor(elapsed / 60);
            const seconds = elapsed % 60;
            
            const gameTimeElement = document.getElementById('gameTime');
            if (gameTimeElement) {
                gameTimeElement.textContent = 
                    `时间: ${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
            }
        }, 1000);
    }

    stopGameTimer() {
        if (this.gameTimer) {
            clearInterval(this.gameTimer);
            this.gameTimer = null;
        }
    }

    showMessage(message, type = 'info', duration = 3000) {
        // 移除现有的消息
        const existingMessages = document.querySelectorAll('.ui-message');
        existingMessages.forEach(msg => {
            if (msg.parentNode) {
                msg.parentNode.removeChild(msg);
            }
        });

        // 创建新的消息元素
        const messageElement = document.createElement('div');
        messageElement.className = `ui-message message-${type}`;
        messageElement.textContent = message;
        
        // 样式
        messageElement.style.cssText = `
            position: fixed;
            top: 80px;
            left: 50%;
            transform: translateX(-50%) translateY(-20px);
            background: ${this.getMessageColor(type)};
            color: white;
            padding: 12px 24px;
            border-radius: 8px;
            z-index: 10000;
            font-size: 14px;
            font-weight: bold;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
            transition: all 0.3s ease;
            max-width: 80%;
            text-align: center;
            word-wrap: break-word;
            opacity: 0;
        `;

        document.body.appendChild(messageElement);

        // 动画进入
        requestAnimationFrame(() => {
            messageElement.style.opacity = '1';
            messageElement.style.transform = 'translateX(-50%) translateY(0)';
        });

        // 自动移除
        setTimeout(() => {
            if (messageElement.parentNode) {
                messageElement.style.opacity = '0';
                messageElement.style.transform = 'translateX(-50%) translateY(-20px)';
                setTimeout(() => {
                    if (messageElement.parentNode) {
                        messageElement.parentNode.removeChild(messageElement);
                    }
                }, 300);
            }
        }, duration);
    }

    getMessageColor(type) {
        const colors = {
            info: 'linear-gradient(135deg, #4cc9f0, #4361ee)',
            success: 'linear-gradient(135deg, #4ade80, #22c55e)',
            error: 'linear-gradient(135deg, #f87171, #ef4444)',
            warning: 'linear-gradient(135deg, #fbbf24, #f59e0b)'
        };
        return colors[type] || colors.info;
    }

    updateGameUI() {
        // 更新游戏UI状态
        if (this.game && this.game.gameState) {
            this.updatePlayerInfo(this.game.gameState.playerName, this.game.gameState.coins);
            this.updateInventory(this.game.gameState.inventory);
            this.updateShopInterface();
            
            // 更新连接状态
            if (this.game.networkClient) {
                this.updateConnectionStatus(this.game.networkClient.getConnectionState());
            }
        }
    }

    update() {
        // 每帧更新UI
        this.updateGameUI();
    }

    destroy() {
        // 清理资源
        this.stopGameTimer();
        this.closeAllModals();
        this.isInitialized = false;
        
        if (this.serverStatusCheckInterval) {
            clearInterval(this.serverStatusCheckInterval);
            this.serverStatusCheckInterval = null;
        }
    }
}

// 导出供其他模块使用
if (typeof module !== 'undefined' && module.exports) {
    module.exports = UIManager;
}