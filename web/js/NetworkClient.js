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
            throw new Error('正在连接中，请稍候...');
        }
        
        this.connectionState = 'connecting';
        this.game.uiManager.showConnectionModal('正在连接服务器...');
        
        try {
            // 1. 获取服务器配置
            await this.fetchServerConfig(serverAddress);
            
            // 2. 建立WebSocket连接
            await this.establishWebSocketConnection(serverAddress, playerName);
            
            this.connectionState = 'connected';
            this.reconnectAttempts = 0;
            this.game.uiManager.hideConnectionModal();
            this.game.uiManager.showMessage('连接服务器成功！', 'success');
            
            // 开始连接状态检查
            this.startConnectionMonitoring();
            
        } catch (error) {
            this.connectionState = 'error';
            this.game.uiManager.hideConnectionModal();
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
                
                // 设置连接超时
                const connectionTimeout = setTimeout(() => {
                    if (this.socket.readyState !== WebSocket.OPEN) {
                        this.socket.close();
                        reject(new Error('WebSocket连接超时'));
                    }
                }, 10000);
                
                this.socket.onopen = () => {
                    clearTimeout(connectionTimeout);
                    console.log('WebSocket connection established');
                    
                    // 发送玩家加入消息
                    this.send({
                        type: 'player_join',
                        playerName: playerName,
                        clientVersion: '1.0.0',
                        timestamp: Date.now()
                    });
                    
                    resolve();
                };
                
                this.socket.onmessage = (event) => {
                    if (this.game && typeof this.game.handleServerMessage === 'function') {
                        this.game.handleServerMessage(event.data);
                    }
                };
                
                this.socket.onclose = (event) => {
                    clearTimeout(connectionTimeout);
                    console.log('WebSocket connection closed:', event.code, event.reason);
                    this.handleDisconnection(event);
                };
                
                this.socket.onerror = (error) => {
                    clearTimeout(connectionTimeout);
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
        return `ws://${host}:${this.wsPort}/ws`;
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
                return true;
            } catch (error) {
                console.error('Failed to send message:', error);
                return false;
            }
        } else {
            console.warn('WebSocket is not open. Message not sent:', data);
            return false;
        }
    }
    
    // 发送玩家移动消息
    sendPlayerMove(position, rotation) {
        this.send({
            type: 'player_move',
            position: position,
            rotation: rotation,
            timestamp: Date.now()
        });
    }
    
    // 发送聊天消息
    sendChatMessage(message) {
        this.send({
            type: 'chat_message',
            message: message,
            timestamp: Date.now()
        });
    }
    
    // 发送购买道具消息
    buyItem(itemType) {
        this.send({
            type: 'buy_item',
            itemType: itemType,
            timestamp: Date.now()
        });
    }
    
    // 发送使用道具消息
    useItem(itemType, target = null) {
        this.send({
            type: 'use_item',
            itemType: itemType,
            target: target,
            timestamp: Date.now()
        });
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
}

// 导出供其他模块使用
if (typeof module !== 'undefined' && module.exports) {
    module.exports = NetworkClient;
}