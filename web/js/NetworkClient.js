// NetworkClient.js - 完整的网络客户端实现
class NetworkClient {
    constructor() {
        this.socket = null;
        this.game = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 2000;
        this.wsPort = 8081; // 默认WebSocket端口
        this.serverConfig = null;
        this.connectionState = 'disconnected'; // disconnected, connecting, connected, error
        this.connectionCheckInterval = null;
    }

    init(game) {
        this.game = game;
    }

    async connect(serverAddress, playerName) {
        if (this.connectionState === 'connecting') {
            const errorMsg = '正在连接中，请稍候...';
            this.game.log(`网络连接错误: ${errorMsg}`, 'error');
            throw new Error(errorMsg);
        }
        
        this.connectionState = 'connecting';
        this.game.uiManager.showConnectionModal('正在连接服务器...');
        this.game.log(`开始连接服务器: ${serverAddress}, 玩家: ${playerName}`, 'network');
        
        try {
            // 1. 获取服务器配置
            await this.fetchServerConfig(serverAddress);
            
            // 2. 建立WebSocket连接
            await this.establishWebSocketConnection(serverAddress, playerName);
            
            this.connectionState = 'connected';
            this.reconnectAttempts = 0;
            this.game.uiManager.hideConnectionModal();
            this.game.uiManager.showMessage('连接服务器成功！', 'success');
            this.game.log('WebSocket连接建立成功', 'network');
            
            // 开始连接状态检查
            this.startConnectionMonitoring();
            
        } catch (error) {
            this.connectionState = 'error';
            this.game.uiManager.hideConnectionModal();
            this.game.log(`连接服务器失败: ${error.message}`, 'error');
            throw error;
        }
    }

    async fetchServerConfig(serverAddress) {
        this.game.uiManager.updateConnectionModal('正在获取服务器配置...', 30);
        
        try {
            const configUrl = this.buildHttpUrl(serverAddress, '/api/config');
            console.log('Fetching server config from:', configUrl);
            
            const response = await this.fetchWithTimeout(configUrl, {
                method: 'GET',
                timeout: 10000 // 10秒超时
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            this.serverConfig = await response.json();
            this.wsPort = this.serverConfig.websocketPort || 8081;
            
            console.log('Server config loaded:', this.serverConfig);
            this.game.uiManager.updateConnectionModal('服务器配置获取成功', 60);
            
        } catch (error) {
            console.warn('Failed to fetch server config, using defaults:', error.message);
            // 使用默认配置
            this.serverConfig = {
                websocketPort: 8081,
                gameVersion: '1.0.0',
                serverName: '3D迷宫游戏服务器'
            };
            this.wsPort = 8081;
            this.game.uiManager.updateConnectionModal('使用默认配置', 60);
        }
    }

    async establishWebSocketConnection(serverAddress, playerName) {
        this.game.uiManager.updateConnectionModal('正在建立WebSocket连接...', 80);
        
        return new Promise((resolve, reject) => {
            try {
                const wsUrl = this.buildWebSocketUrl(serverAddress);
                console.log('Connecting to WebSocket:', wsUrl);
                
                this.socket = new WebSocket(wsUrl);
                
                const connectionTimeout = setTimeout(() => {
                    if (this.socket.readyState !== WebSocket.OPEN) {
                        this.socket.close();
                        reject(new Error('WebSocket连接超时'));
                    }
                }, 10000);
                
                // 认证成功标志
                let authSuccess = false;
                const authTimeout = setTimeout(() => {
                    if (!authSuccess) {
                        this.socket.close();
                        reject(new Error('认证超时'));
                    }
                }, 5000);
                
                this.socket.onopen = () => {
                    clearTimeout(connectionTimeout);
                    console.log('WebSocket connection established');
                    
                    // 发送认证消息
                    this.sendAuth(playerName);
                };
                
                this.socket.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        // 检查是否是认证成功消息
                        if (data.type === 'auth_success') {
                            clearTimeout(authTimeout);
                            authSuccess = true;
                            resolve();
                        } else if (data.type === 'auth_failed') {
                            clearTimeout(authTimeout);
                            reject(new Error(data.data?.message || '认证失败'));
                        }
                        
                        if (this.game && typeof this.game.handleServerMessage === 'function') {
                            this.game.handleServerMessage(event.data);
                        }
                    } catch (error) {
                        console.error('处理消息错误:', error);
                    }
                };
                
                this.socket.onclose = (event) => {
                    clearTimeout(connectionTimeout);
                    clearTimeout(authTimeout);
                    console.log('WebSocket connection closed:', event.code, event.reason);
                    this.handleDisconnection(event);
                };
                
                this.socket.onerror = (error) => {
                    clearTimeout(connectionTimeout);
                    clearTimeout(authTimeout);
                    console.error('WebSocket error:', error);
                    reject(new Error('WebSocket连接错误'));
                };
                
            } catch (error) {
                reject(new Error('创建WebSocket连接失败: ' + error.message));
            }
        });
    }

    buildHttpUrl(serverAddress, path = '') {
        if (serverAddress.includes('://')) {
            const url = new URL(serverAddress);
            url.pathname = path;
            return url.toString();
        } else {
            // 简单地址格式，添加http://前缀
            return `http://${serverAddress}${path}`;
        }
    }

    buildWebSocketUrl(serverAddress) {
        const [host] = serverAddress.split(':');
        return `ws://${host}:${this.wsPort}/`;
    }

    async fetchWithTimeout(url, options = {}) {
        const { timeout = 10000, ...fetchOptions } = options;
        
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), timeout);
        
        try {
            const response = await fetch(url, {
                ...fetchOptions,
                signal: controller.signal
            });
            clearTimeout(timeoutId);
            return response;
        } catch (error) {
            clearTimeout(timeoutId);
            if (error.name === 'AbortError') {
                throw new Error('请求超时');
            }
            throw error;
        }
    }

    handleDisconnection(event) {
        this.connectionState = 'disconnected';
        this.stopConnectionMonitoring();
        
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            const delay = this.reconnectDelay * this.reconnectAttempts;
            
            console.log(`Attempting to reconnect... (${this.reconnectAttempts}/${this.maxReconnectAttempts}) in ${delay}ms`);
            
            this.game.uiManager.showMessage(
                `连接断开，${delay/1000}秒后尝试重新连接... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`, 
                'warning'
            );
            
            setTimeout(() => {
                if (this.game && this.game.gameState.playerName) {
                    const serverAddress = document.getElementById('serverAddress').value;
                    this.connect(serverAddress, this.game.gameState.playerName)
                        .catch(error => {
                            console.error('Reconnection failed:', error);
                        });
                }
            }, delay);
        } else {
            this.game.uiManager.showMessage('与服务器的连接已断开，请手动重新连接', 'error');
            this.game.showLoginScreen();
        }
    }

    startConnectionMonitoring() {
        this.stopConnectionMonitoring();
        
        this.connectionCheckInterval = setInterval(() => {
            if (this.socket && this.socket.readyState === WebSocket.OPEN) {
                // 发送ping消息检查连接状态
                this.send({
                    type: 'ping',
                    timestamp: Date.now()
                });
            }
        }, 30000); // 每30秒发送一次ping
    }

    stopConnectionMonitoring() {
        if (this.connectionCheckInterval) {
            clearInterval(this.connectionCheckInterval);
            this.connectionCheckInterval = null;
        }
    }

    send(data) {
        if (this.socket && this.socket.readyState === WebSocket.OPEN) {
            try {
                const message = JSON.stringify(data);
                this.socket.send(message);
                this.game.log(`发送消息: ${data.type}`, 'network');
                return true;
            } catch (error) {
                this.game.log(`发送消息失败: ${error.message}`, 'error');
                return false;
            }
        } else {
            this.game.log(`WebSocket未连接，消息未发送: ${data.type}`, 'warning');
            return false;
        }
    }

    // 新的消息发送方法，使用统一的消息格式
    sendMessage(type, data = {}) {
        const message = {
            type: type,
            timestamp: Date.now(),
            data: data
        };
        return this.send(message);
    }

    // 发送玩家移动消息
    sendPlayerMove(position, rotation) {
        return this.sendMessage('move', {
            position: position,
            rotation: rotation
        });
    }

    // 发送消息的方法，统一消息格式
    sendMessage(type, data = {}) {
        if (this.socket && this.socket.readyState === WebSocket.OPEN) {
            const message = {
                type: type,
                timestamp: Date.now(),
                data: data
            };
            
            try {
                this.socket.send(JSON.stringify(message));
                return true;
            } catch (error) {
                console.error('发送消息失败:', error);
                return false;
            }
        }
        return false;
    }

    // 发送聊天消息
    sendChatMessage(message) {
        return this.sendMessage('chat_message', {
            message: message
        });
    }

    // 发送购买道具消息
    buyItem(itemType) {
        return this.sendMessage('purchase_item', {
            itemType: this.mapItemType(itemType)
        });
    }

    // 发送使用道具消息
    useItem(itemType, target = null) {
        const data = {
            itemType: this.mapItemType(itemType)
        };
        
        if (target && target.playerId) {
            data.targetPlayerId = target.playerId;
        }
        if (target && target.position) {
            data.targetPosition = target.position;
        }
        
        return this.sendMessage('use_item', data);
    }

    // 道具类型映射
    mapItemType(frontendType) {
        const typeMap = {
            'speed_potion': 'speed_potion',
            'compass': 'compass',
            'hammer': 'hammer',
            'sword': 'kill_sword', // 注意：后端使用kill_sword
            'slow_trap': 'slow_trap',
            'swap_item': 'swap_item'
        };
        return typeMap[frontendType] || frontendType;
    }

    // 添加认证消息发送
    sendAuth(playerName) {
        const playerId = localStorage.getItem('playerId') || this.generatePlayerId();
        const token = localStorage.getItem('playerToken') || '';
        
        // 立即发送认证消息，不等待sendMessage的队列
        if (this.socket && this.socket.readyState === WebSocket.OPEN) {
            const authMessage = {
                type: 'auth',
                timestamp: Date.now(),
                data: {
                    playerId: playerId,
                    playerName: playerName,
                    token: token
                }
            };
            
            try {
                this.socket.send(JSON.stringify(authMessage));
                this.game.log(`发送认证消息: ${playerName}`, 'network');
                return true;
            } catch (error) {
                this.game.log(`发送认证消息失败: ${error.message}`, 'error');
                return false;
            }
        }
        return false;
    }

    // 生成玩家ID
    generatePlayerId() {
        const playerId = 'player_' + Math.random().toString(36).substr(2, 9);
        localStorage.setItem('playerId', playerId);
        return playerId;
    }

    // 获取服务器状态
    async getServerStatus(serverAddress) {
        try {
            const statusUrl = this.buildHttpUrl(serverAddress, '/api/status');
            const response = await this.fetchWithTimeout(statusUrl, {
                method: 'GET',
                timeout: 5000
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            
            return await response.json();
        } catch (error) {
            console.warn('Failed to fetch server status:', error);
            return {
                status: 'unknown',
                connectedPlayers: 0,
                totalPlayers: 0,
                onlinePlayers: 0,
                serverTime: new Date().toISOString()
            };
        }
    }

    // 断开连接
    disconnect() {
        this.connectionState = 'disconnected';
        this.stopConnectionMonitoring();
        
        if (this.socket) {
            this.socket.close();
            this.socket = null;
        }
        
        this.reconnectAttempts = 0;
    }

    // 获取连接状态
    getConnectionState() {
        return this.connectionState;
    }

    // 强制重连
    async forceReconnect(serverAddress, playerName) {
        this.reconnectAttempts = 0;
        this.disconnect();
        return await this.connect(serverAddress, playerName);
    }
    
    startHeartbeat() {
        this.heartbeatInterval = setInterval(() => {
            if (this.socket && this.socket.readyState === WebSocket.OPEN) {
                this.sendMessage('ping');
            }
        }, 30000); // 每30秒发送一次ping
    }
    
    stopHeartbeat() {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
            this.heartbeatInterval = null;
        }
    }
}

// 导出供其他模块使用
if (typeof module !== 'undefined' && module.exports) {
    module.exports = NetworkClient;
}