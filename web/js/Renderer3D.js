// 3D渲染器 - 使用Three.js渲染迷宫和玩家
class Renderer3D {
    constructor() {
        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.game = null;
        
        this.maze = null;
        this.players = {};
        this.playerMesh = null;
        
        // 纹理和材质
        this.textures = {};
        this.materials = {};
        
        // 迷宫尺寸
        this.mazeWidth = 50;
        this.mazeHeight = 50;
        this.mazeLevels = 7;
        
        // 方块尺寸
        this.blockSize = 2;
    }
    
    init(game) {
        this.game = game;
        
        // 创建场景
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x87CEEB); // 天空蓝
        
        // 创建相机
        this.camera = new THREE.PerspectiveCamera(
            75,
            window.innerWidth / window.innerHeight,
            0.1,
            1000
        );
        
        // 创建渲染器
        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.setSize(window.innerWidth, window.innerHeight);
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        
        // 添加到DOM
        const container = document.getElementById('gameContainer');
        container.appendChild(this.renderer.domElement);
        
        // 设置灯光
        this.setupLighting();
        
        // 加载纹理
        this.loadTextures();
        
        // 处理窗口大小变化
        window.addEventListener('resize', () => this.onWindowResize());
        
        // 添加雾效
        this.scene.fog = new THREE.Fog(0x87CEEB, 50, 200);
    }
    
    setupLighting() {
        // 环境光
        const ambientLight = new THREE.AmbientLight(0x404040, 0.6);
        this.scene.add(ambientLight);
        
        // 方向光（模拟太阳）
        const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
        directionalLight.position.set(50, 100, 50);
        directionalLight.castShadow = true;
        directionalLight.shadow.mapSize.width = 2048;
        directionalLight.shadow.mapSize.height = 2048;
        directionalLight.shadow.camera.near = 0.5;
        directionalLight.shadow.camera.far = 500;
        directionalLight.shadow.camera.left = -100;
        directionalLight.shadow.camera.right = 100;
        directionalLight.shadow.camera.top = 100;
        directionalLight.shadow.camera.bottom = -100;
        this.scene.add(directionalLight);
    }
    
    loadTextures() {
        const textureLoader = new THREE.TextureLoader();
        
        // 加载墙壁纹理
        this.textures.wall = textureLoader.load('img/textures/wall.png');
        this.textures.wall.wrapS = THREE.RepeatWrapping;
        this.textures.wall.wrapT = THREE.RepeatWrapping;
        this.textures.wall.repeat.set(4, 4);
        
        // 加载地板纹理
        this.textures.floor = textureLoader.load('img/textures/floor.png');
        this.textures.floor.wrapS = THREE.RepeatWrapping;
        this.textures.floor.wrapT = THREE.RepeatWrapping;
        this.textures.floor.repeat.set(8, 8);
        
        // 加载天花板纹理
        this.textures.ceiling = textureLoader.load('img/textures/ceiling.png');
        this.textures.ceiling.wrapS = THREE.RepeatWrapping;
        this.textures.ceiling.wrapT = THREE.RepeatWrapping;
        this.textures.ceiling.repeat.set(8, 8);
        
        // 创建材质
        this.materials.wall = new THREE.MeshLambertMaterial({ 
            map: this.textures.wall 
        });
        
        this.materials.floor = new THREE.MeshLambertMaterial({ 
            map: this.textures.floor 
        });
        
        this.materials.ceiling = new THREE.MeshLambertMaterial({ 
            map: this.textures.ceiling 
        });
        
        // 玩家材质
        const playerTexture = textureLoader.load('img/textures/player_skin.png');
        this.materials.player = new THREE.MeshLambertMaterial({ 
            map: playerTexture 
        });
    }
    
    loadMaze(mazeData) {
        // 清除现有迷宫
        if (this.maze) {
            this.scene.remove(this.maze);
        }
        
        this.maze = new THREE.Group();
        this.scene.add(this.maze);
        
        // 生成迷宫几何体
        for (let level = 0; level < this.mazeLevels; level++) {
            for (let x = 0; x < this.mazeWidth; x++) {
                for (let z = 0; z < this.mazeHeight; z++) {
                    const cell = mazeData[level][x][z];
                    
                    if (cell === 1) { // 墙壁
                        this.createWall(x, level, z);
                    } else if (cell === 2) { // 楼梯
                        this.createStairs(x, level, z);
                    } else if (cell === 3) { // 终点
                        this.createEndPoint(x, level, z);
                    }
                    
                    // 创建地板和天花板
                    this.createFloor(x, level, z);
                    if (level === this.mazeLevels - 1) {
                        this.createCeiling(x, level, z);
                    }
                }
            }
        }
        
        // 创建金币
        this.createCoins(mazeData.coins || []);
    }
    
    createWall(x, y, z) {
        const geometry = new THREE.BoxGeometry(
            this.blockSize, 
            this.blockSize * 2, 
            this.blockSize
        );
        const mesh = new THREE.Mesh(geometry, this.materials.wall);
        mesh.position.set(
            x * this.blockSize - (this.mazeWidth * this.blockSize) / 2,
            y * this.blockSize * 2 + this.blockSize,
            z * this.blockSize - (this.mazeHeight * this.blockSize) / 2
        );
        mesh.castShadow = true;
        mesh.receiveShadow = true;
        this.maze.add(mesh);
    }
    
    createFloor(x, y, z) {
        const geometry = new THREE.PlaneGeometry(this.blockSize, this.blockSize);
        const mesh = new THREE.Mesh(geometry, this.materials.floor);
        mesh.rotation.x = -Math.PI / 2;
        mesh.position.set(
            x * this.blockSize - (this.mazeWidth * this.blockSize) / 2,
            y * this.blockSize * 2,
            z * this.blockSize - (this.mazeHeight * this.blockSize) / 2
        );
        mesh.receiveShadow = true;
        this.maze.add(mesh);
    }
    
    createCeiling(x, y, z) {
        const geometry = new THREE.PlaneGeometry(this.blockSize, this.blockSize);
        const mesh = new THREE.Mesh(geometry, this.materials.ceiling);
        mesh.rotation.x = Math.PI / 2;
        mesh.position.set(
            x * this.blockSize - (this.mazeWidth * this.blockSize) / 2,
            (y + 1) * this.blockSize * 2,
            z * this.blockSize - (this.mazeHeight * this.blockSize) / 2
        );
        mesh.receiveShadow = true;
        this.maze.add(mesh);
    }
    
    createStairs(x, y, z) {
        const geometry = new THREE.BoxGeometry(
            this.blockSize, 
            this.blockSize * 0.5, 
            this.blockSize
        );
        const material = new THREE.MeshLambertMaterial({ color: 0x8B4513 }); // 棕色
        const mesh = new THREE.Mesh(geometry, material);
        mesh.position.set(
            x * this.blockSize - (this.mazeWidth * this.blockSize) / 2,
            y * this.blockSize * 2 + this.blockSize * 0.25,
            z * this.blockSize - (this.mazeHeight * this.blockSize) / 2
        );
        mesh.castShadow = true;
        mesh.receiveShadow = true;
        this.maze.add(mesh);
    }
    
    createEndPoint(x, y, z) {
        const geometry = new THREE.BoxGeometry(
            this.blockSize, 
            this.blockSize * 0.2, 
            this.blockSize
        );
        const material = new THREE.MeshLambertMaterial({ color: 0x00FF00 }); // 绿色
        const mesh = new THREE.Mesh(geometry, material);
        mesh.position.set(
            x * this.blockSize - (this.mazeWidth * this.blockSize) / 2,
            y * this.blockSize * 2 + this.blockSize * 0.1,
            z * this.blockSize - (this.mazeHeight * this.blockSize) / 2
        );
        mesh.receiveShadow = true;
        this.maze.add(mesh);
    }
    
    createCoins(coinPositions) {
        const coinGeometry = new THREE.CylinderGeometry(0.3, 0.3, 0.1, 16);
        const coinMaterial = new THREE.MeshLambertMaterial({ color: 0xFFD700 }); // 金色
        
        coinPositions.forEach(coin => {
            const mesh = new THREE.Mesh(coinGeometry, coinMaterial);
            mesh.position.set(
                coin.x * this.blockSize - (this.mazeWidth * this.blockSize) / 2,
                coin.y * this.blockSize * 2 + 1,
                coin.z * this.blockSize - (this.mazeHeight * this.blockSize) / 2
            );
            mesh.rotation.x = Math.PI / 2;
            mesh.castShadow = true;
            this.maze.add(mesh);
        });
    }
    
    createPlayerMesh(playerId, playerName, isLocalPlayer = false) {
        // 创建类似Minecraft的方块人
        const playerGroup = new THREE.Group();
        
        // 身体
        const bodyGeometry = new THREE.BoxGeometry(0.6, 1, 0.3);
        const body = new THREE.Mesh(bodyGeometry, this.materials.player);
        body.position.y = 0.5;
        playerGroup.add(body);
        
        // 头部
        const headGeometry = new THREE.BoxGeometry(0.5, 0.5, 0.5);
        const head = new THREE.Mesh(headGeometry, this.materials.player);
        head.position.y = 1.25;
        playerGroup.add(head);
        
        // 手臂
        const armGeometry = new THREE.BoxGeometry(0.2, 0.6, 0.2);
        
        const leftArm = new THREE.Mesh(armGeometry, this.materials.player);
        leftArm.position.set(-0.4, 0.7, 0);
        playerGroup.add(leftArm);
        
        const rightArm = new THREE.Mesh(armGeometry, this.materials.player);
        rightArm.position.set(0.4, 0.7, 0);
        playerGroup.add(rightArm);
        
        // 腿
        const legGeometry = new THREE.BoxGeometry(0.2, 0.6, 0.2);
        
        const leftLeg = new THREE.Mesh(legGeometry, this.materials.player);
        leftLeg.position.set(-0.2, -0.3, 0);
        playerGroup.add(leftLeg);
        
        const rightLeg = new THREE.Mesh(legGeometry, this.materials.player);
        rightLeg.position.set(0.2, -0.3, 0);
        playerGroup.add(rightLeg);
        
        // 玩家名称标签
        const canvas = document.createElement('canvas');
        const context = canvas.getContext('2d');
        canvas.width = 256;
        canvas.height = 64;
        
        context.fillStyle = 'rgba(0, 0, 0, 0.7)';
        context.fillRect(0, 0, canvas.width, canvas.height);
        
        context.font = '24px Arial';
        context.fillStyle = 'white';
        context.textAlign = 'center';
        context.fillText(playerName, canvas.width / 2, canvas.height / 2 + 8);
        
        const nameTexture = new THREE.CanvasTexture(canvas);
        const nameMaterial = new THREE.SpriteMaterial({ map: nameTexture });
        const nameSprite = new THREE.Sprite(nameMaterial);
        nameSprite.position.y = 2;
        nameSprite.scale.set(2, 0.5, 1);
        playerGroup.add(nameSprite);
        
        playerGroup.castShadow = true;
        
        // 保存玩家引用
        this.players[playerId] = playerGroup;
        this.scene.add(playerGroup);
        
        // 如果是本地玩家，保存引用并设置相机跟随
        if (isLocalPlayer) {
            this.playerMesh = playerGroup;
            this.camera.position.set(0, 1.6, 0);
            playerGroup.add(this.camera);
        }
        
        return playerGroup;
    }
    
    updatePlayerPosition(playerId, position, rotation) {
        const player = this.players[playerId];
        if (player) {
            player.position.set(
                position.x * this.blockSize - (this.mazeWidth * this.blockSize) / 2,
                position.y * this.blockSize * 2,
                position.z * this.blockSize - (this.mazeHeight * this.blockSize) / 2
            );
            
            if (rotation) {
                player.rotation.y = rotation.y;
            }
        }
    }
    
    removePlayer(playerId) {
        const player = this.players[playerId];
        if (player) {
            this.scene.remove(player);
            delete this.players[playerId];
        }
    }
    
    showEffect(effectType, position) {
        let particleGeometry, particleMaterial, particles;
        
        switch (effectType) {
            case 'speed':
                particleGeometry = new THREE.BufferGeometry();
                const speedPositions = new Float32Array(100 * 3);
                for (let i = 0; i < 100; i++) {
                    speedPositions[i * 3] = (Math.random() - 0.5) * 2;
                    speedPositions[i * 3 + 1] = Math.random() * 2;
                    speedPositions[i * 3 + 2] = (Math.random() - 0.5) * 2;
                }
                particleGeometry.setAttribute('position', new THREE.BufferAttribute(speedPositions, 3));
                particleMaterial = new THREE.PointsMaterial({ 
                    color: 0x00FFFF, 
                    size: 0.1,
                    transparent: true,
                    opacity: 0.8
                });
                particles = new THREE.Points(particleGeometry, particleMaterial);
                break;
                
            case 'death':
                particleGeometry = new THREE.BufferGeometry();
                const deathPositions = new Float32Array(50 * 3);
                for (let i = 0; i < 50; i++) {
                    deathPositions[i * 3] = (Math.random() - 0.5) * 3;
                    deathPositions[i * 3 + 1] = (Math.random() - 0.5) * 3;
                    deathPositions[i * 3 + 2] = (Math.random() - 0.5) * 3;
                }
                particleGeometry.setAttribute('position', new THREE.BufferAttribute(deathPositions, 3));
                particleMaterial = new THREE.PointsMaterial({ 
                    color: 0xFF0000, 
                    size: 0.15,
                    transparent: true,
                    opacity: 0.9
                });
                particles = new THREE.Points(particleGeometry, particleMaterial);
                break;
        }
        
        if (particles) {
            particles.position.set(
                position.x * this.blockSize - (this.mazeWidth * this.blockSize) / 2,
                position.y * this.blockSize * 2,
                position.z * this.blockSize - (this.mazeHeight * this.blockSize) / 2
            );
            this.scene.add(particles);
            
            // 5秒后移除粒子效果
            setTimeout(() => {
                this.scene.remove(particles);
            }, 5000);
        }
    }
    
    update() {
        // 更新玩家动画等
        Object.values(this.players).forEach(player => {
            // 简单的呼吸动画
            player.children[0].scale.y = 1 + Math.sin(Date.now() * 0.005) * 0.05;
        });
    }
    
    render() {
        this.renderer.render(this.scene, this.camera);
    }
    
    onWindowResize() {
        this.camera.aspect = window.innerWidth / window.innerHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(window.innerWidth, window.innerHeight);
    }
    
    // 设置相机控制
    setCameraControls(controls) {
        this.cameraControls = controls;
    }
}