// 游戏控制器 - 处理用户输入和游戏逻辑
class GameController {
    constructor() {
        this.game = null;
        this.keys = {};
        this.mouse = { x: 0, y: 0 };
        this.movement = { x: 0, z: 0 };
        this.rotation = { x: 0, y: 0 };
        
        this.moveSpeed = 0.1;
        this.rotationSpeed = 0.002;
        
        this.lastUpdateTime = 0;
        this.updateInterval = 1000 / 60; // 60 FPS
        
        this.isChatFocused = false;
        this.isShopOpen = false;
    }
    
    init(game) {
        this.game = game;
        this.game.log('初始化游戏控制器', 'info');
        this.setupEventListeners();
        this.game.log('游戏事件监听器已设置', 'info');
    }
    
    setupEventListeners() {
        // 键盘事件
        document.addEventListener('keydown', (event) => this.onKeyDown(event));
        document.addEventListener('keyup', (event) => this.onKeyUp(event));
        
        // 鼠标事件
        document.addEventListener('mousemove', (event) => this.onMouseMove(event));
        document.addEventListener('mousedown', (event) => this.onMouseDown(event));
        
        // 防止右键菜单
        document.addEventListener('contextmenu', (event) => event.preventDefault());
        
        // 窗口失去焦点时停止移动
        window.addEventListener('blur', () => {
            this.keys = {};
        });
    }
    
    onKeyDown(event) {
        this.keys[event.code] = true;
        
        // 处理特殊按键
        switch (event.code) {
            case 'Enter':
                if (!this.isChatFocused && !this.isShopOpen) {
                    this.focusChat();
                    event.preventDefault();
                }
                break;
                
            case 'KeyE':
                if (!this.isChatFocused && !this.isShopOpen) {
                    this.toggleShop();
                    event.preventDefault();
                }
                break;
                
            case 'Escape':
                if (this.isChatFocused) {
                    this.unfocusChat();
                    event.preventDefault();
                } else if (this.isShopOpen) {
                    this.closeShop();
                    event.preventDefault();
                }
                break;
                
            case 'Digit1':
            case 'Digit2':
            case 'Digit3':
            case 'Digit4':
            case 'Digit5':
            case 'Digit6':
                if (!this.isChatFocused && !this.isShopOpen) {
                    const slot = parseInt(event.code[5]) - 1;
                    this.selectInventorySlot(slot);
                    event.preventDefault();
                }
                break;
        }
    }
    
    onKeyUp(event) {
        this.keys[event.code] = false;
    }
    
    onMouseMove(event) {
        if (this.isChatFocused || this.isShopOpen) return;
        
        const deltaX = event.movementX || 0;
        const deltaY = event.movementY || 0;
        
        this.rotation.y -= deltaX * this.rotationSpeed;
        this.rotation.x -= deltaY * this.rotationSpeed;
        
        // 限制垂直视角范围
        this.rotation.x = Math.max(-Math.PI / 2, Math.min(Math.PI / 2, this.rotation.x));
        
        // 更新相机旋转
        if (this.game.renderer3D.camera) {
            this.game.renderer3D.camera.rotation.x = this.rotation.x;
            this.game.renderer3D.camera.rotation.y = this.rotation.y;
        }
    }
    
    onMouseDown(event) {
        if (this.isChatFocused || this.isShopOpen) return;
        
        if (event.button === 0) { // 左键
            this.useActiveItem();
        }
    }
    
    update() {
        const currentTime = Date.now();
        if (currentTime - this.lastUpdateTime < this.updateInterval) return;
        
        this.lastUpdateTime = currentTime;
        
        if (this.isChatFocused || this.isShopOpen) return;
        
        // 处理移动输入
        this.movement.x = 0;
        this.movement.z = 0;
        
        if (this.keys['KeyW']) this.movement.z -= this.moveSpeed;
        if (this.keys['KeyS']) this.movement.z += this.moveSpeed;
        if (this.keys['KeyA']) this.movement.x -= this.moveSpeed;
        if (this.keys['KeyD']) this.movement.x += this.moveSpeed;
        
        // 应用旋转到移动方向
        if (this.movement.x !== 0 || this.movement.z !== 0) {
            const sinY = Math.sin(this.rotation.y);
            const cosY = Math.cos(this.rotation.y);
            
            const movedX = this.movement.x * cosY - this.movement.z * sinY;
            const movedZ = this.movement.x * sinY + this.movement.z * cosY;
            
            // 更新玩家位置
            if (this.game.gameState.position) {
                this.game.gameState.position.x += movedX;
                this.game.gameState.position.z += movedZ;
                
                // 发送位置更新到服务器
                this.game.networkClient.sendPlayerMove(
                    this.game.gameState.position,
                    this.rotation
                );
                
                // 更新本地玩家渲染
                this.game.renderer3D.updatePlayerPosition(
                    this.game.gameState.playerId,
                    this.game.gameState.position,
                    this.rotation
                );
            }
        }
    }
    
    focusChat() {
        const chatInput = document.getElementById('chatInput');
        if (chatInput) {
            chatInput.focus();
            this.isChatFocused = true;
        }
    }
    
    unfocusChat() {
        const chatInput = document.getElementById('chatInput');
        if (chatInput) {
            chatInput.blur();
            this.isChatFocused = false;
        }
    }
    
    toggleShop() {
        if (this.isShopOpen) {
            this.closeShop();
        } else {
            this.openShop();
        }
    }
    
    openShop() {
        this.game.uiManager.openShop();
        this.isShopOpen = true;
    }
    
    closeShop() {
        this.game.uiManager.closeShop();
        this.isShopOpen = false;
    }
    
    selectInventorySlot(slot) {
        this.game.uiManager.selectInventorySlot(slot);
    }
    
    useActiveItem() {
        const activeSlot = this.game.uiManager.getActiveSlot();
        if (activeSlot !== null) {
            const itemTypes = [
                'speed_potion', 'compass', 'hammer', 
                'sword', 'slow_trap', 'swap_item'
            ];
            const itemType = itemTypes[activeSlot];
            
            if (this.game.gameState.inventory[itemType] > 0) {
                this.game.useItem(itemType);
            }
        }
    }
    
    // 锁定/解锁鼠标指针
    lockPointer() {
        const canvas = this.game.renderer3D.renderer.domElement;
        canvas.requestPointerLock = canvas.requestPointerLock || canvas.mozRequestPointerLock;
        canvas.requestPointerLock();
    }
    
    unlockPointer() {
        document.exitPointerLock = document.exitPointerLock || document.mozExitPointerLock;
        document.exitPointerLock();
    }
}