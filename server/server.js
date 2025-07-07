#!/usr/bin/env node
// -*- coding: utf-8 -*-

/**
 * STM32 HBox 固件管理服务器
 * 
 * 功能:
 * 1. 固件版本列表管理
 * 2. 固件包上传和存储
 * 3. 固件包下载
 * 4. 固件包删除
 */

const express = require('express');
const multer = require('multer');
const cors = require('cors');
const path = require('path');
const fs = require('fs-extra');
const crypto = require('crypto');

// 引入固件模块
const { initFirmwareRoutes } = require('./firmware');

const app = express();
const PORT = process.env.PORT || 3000;

// 配置目录
const config = {
    uploadDir: path.join(__dirname, 'uploads'),
    dataFile: path.join(__dirname, 'firmware_list.json'),
    maxFileSize: 50 * 1024 * 1024, // 50MB
    allowedExtensions: ['.zip'],
    serverUrl: process.env.SERVER_URL || `http://localhost:${PORT}`
};

const volidateDefaultConfig = {
    expiresIn: 2 * 60,   // 签名 有效期2分钟
}

// 中间件配置
app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// 静态文件服务 - 用于下载固件包
app.use('/downloads', express.static(config.uploadDir));

// 创建上传目录
fs.ensureDirSync(config.uploadDir);

// 数据存储管理
class FirmwareStorage {
    constructor() {
        this.dataFile = config.dataFile;
        this.deviceDataFile = path.join(__dirname, 'device_ids.json');
        this.data = this.loadData();
        this.deviceData = this.loadDeviceData();
    }

    // 加载数据
    loadData() {
        try {
            if (fs.existsSync(this.dataFile)) {
                const content = fs.readFileSync(this.dataFile, 'utf8');
                return JSON.parse(content);
            }
        } catch (error) {
            console.error('加载数据失败:', error.message);
        }
        
        // 返回默认数据结构
        return {
            firmwares: [],
            lastUpdate: new Date().toISOString()
        };
    }

    // 加载设备数据
    loadDeviceData() {
        try {
            if (fs.existsSync(this.deviceDataFile)) {
                const content = fs.readFileSync(this.deviceDataFile, 'utf8');
                return JSON.parse(content);
            }
        } catch (error) {
            console.error('加载设备数据失败:', error.message);
        }
        
        // 返回默认设备数据结构
        return {
            devices: [],
            lastUpdate: new Date().toISOString()
        };
    }

    // 保存数据
    saveData() {
        try {
            this.data.lastUpdate = new Date().toISOString();
            fs.writeFileSync(this.dataFile, JSON.stringify(this.data, null, 2), 'utf8');
            return true;
        } catch (error) {
            console.error('保存数据失败:', error.message);
            return false;
        }
    }

    // 保存设备数据
    saveDeviceData() {
        try {
            this.deviceData.lastUpdate = new Date().toISOString();
            fs.writeFileSync(this.deviceDataFile, JSON.stringify(this.deviceData, null, 2), 'utf8');
            return true;
        } catch (error) {
            console.error('保存设备数据失败:', error.message);
            return false;
        }
    }

    // 计算设备ID哈希 (与固件中的算法保持一致)
    calculateDeviceIdHash(uid_word0, uid_word1, uid_word2) {
        // 与 utils.c 中相同的哈希算法
        // 盐值常量
        const salt1 = 0x48426F78;  // "HBox"
        const salt2 = 0x32303234;  // "2024"
        
        // 质数常量
        const prime1 = 0x9E3779B9;  // 黄金比例的32位表示
        const prime2 = 0x85EBCA6B;  // 另一个质数
        const prime3 = 0xC2B2AE35;  // 第三个质数
        
        // 第一轮哈希
        let hash1 = uid_word0 ^ salt1;
        hash1 = ((hash1 << 13) | (hash1 >>> 19)) >>> 0;  // 左循环移位13位
        hash1 = Math.imul(hash1, prime1) >>> 0;  // 使用Math.imul进行32位乘法
        hash1 ^= uid_word1;
        
        // 第二轮哈希
        let hash2 = uid_word1 ^ salt2;
        hash2 = ((hash2 << 17) | (hash2 >>> 15)) >>> 0;  // 左循环移位17位
        hash2 = Math.imul(hash2, prime2) >>> 0;  // 使用Math.imul进行32位乘法
        hash2 ^= uid_word2;
        
        // 第三轮哈希
        let hash3 = uid_word2 ^ ((salt1 + salt2) >>> 0);
        hash3 = ((hash3 << 21) | (hash3 >>> 11)) >>> 0;  // 左循环移位21位
        hash3 = Math.imul(hash3, prime3) >>> 0;  // 使用Math.imul进行32位乘法
        hash3 ^= hash1;
        
        // 最终组合
        const final_hash1 = (hash1 ^ hash2) >>> 0;
        const final_hash2 = (hash2 ^ hash3) >>> 0;
        
        // 转换为16位十六进制字符串 (64位哈希)
        const device_id = final_hash1.toString(16).toUpperCase().padStart(8, '0') + 
                         final_hash2.toString(16).toUpperCase().padStart(8, '0');
        
        return device_id;
    }

    // 验证设备ID哈希
    verifyDeviceIdHash(rawUniqueId, deviceIdHash) {
        try {
            // 解析原始唯一ID格式: XXXXXXXX-XXXXXXXX-XXXXXXXX
            const parts = rawUniqueId.split('-');
            if (parts.length !== 3) {
                return false;
            }
            
            const uid_word0 = parseInt(parts[0], 16);
            const uid_word1 = parseInt(parts[1], 16);
            const uid_word2 = parseInt(parts[2], 16);
            
            // 计算期望的哈希值
            const expectedHash = this.calculateDeviceIdHash(uid_word0, uid_word1, uid_word2);
            
            // 比较哈希值
            return expectedHash === deviceIdHash.toUpperCase();
        } catch (error) {
            console.error('验证设备ID哈希失败:', error.message);
            return false;
        }
    }

    // 查找设备
    findDevice(deviceId) {
        return this.deviceData.devices.find(d => d.deviceId === deviceId);
    }

    // 添加设备
    addDevice(deviceInfo) {
        // 检查设备是否已存在
        const existingDevice = this.findDevice(deviceInfo.deviceId);
        if (existingDevice) {
            return {
                success: true,
                existed: true,
                message: '设备已存在',
                device: existingDevice
            };
        }
        
        // 🔍 调试打印：哈希验证过程
        console.log('🔧 设备ID哈希验证:');
        console.log('  输入原始唯一ID:', deviceInfo.rawUniqueId);
        console.log('  输入设备ID:', deviceInfo.deviceId);
        
        // 计算服务器端的设备ID哈希
        const parts = deviceInfo.rawUniqueId.split('-');
        const uid_word0 = parseInt(parts[0], 16);
        const uid_word1 = parseInt(parts[1], 16);
        const uid_word2 = parseInt(parts[2], 16);
        const serverCalculatedDeviceId = this.calculateDeviceIdHash(uid_word0, uid_word1, uid_word2);
        
        console.log('  服务器计算的设备ID:', serverCalculatedDeviceId);
        console.log('  验证结果:', serverCalculatedDeviceId === deviceInfo.deviceId.toUpperCase() ? '✅ 匹配' : '❌ 不匹配');
        
        // 验证设备ID哈希
        if (!this.verifyDeviceIdHash(deviceInfo.rawUniqueId, deviceInfo.deviceId)) {
            return {
                success: false,
                message: '设备ID哈希验证失败'
            };
        }
        
        // 添加新设备
        const newDevice = {
            ...deviceInfo,
            registerTime: new Date().toISOString(),
            lastSeen: new Date().toISOString(),
            status: 'active'
        };
        
        this.deviceData.devices.push(newDevice);
        
        if (this.saveDeviceData()) {
            return {
                success: true,
                existed: false,
                message: '设备注册成功',
                device: newDevice
            };
        } else {
            return {
                success: false,
                message: '保存设备数据失败'
            };
        }
    }

    // 获取所有设备
    getDevices() {
        return this.deviceData.devices;
    }

    // 获取所有固件
    getFirmwares() {
        return this.data.firmwares;
    }

    // 添加固件
    addFirmware(firmware) {
        // 生成唯一ID
        firmware.id = this.generateId();
        firmware.createTime = new Date().toISOString();
        firmware.updateTime = new Date().toISOString();
        
        this.data.firmwares.push(firmware);
        return this.saveData();
    }

    // 更新固件
    updateFirmware(id, updates) {
        const index = this.data.firmwares.findIndex(f => f.id === id);
        if (index !== -1) {
            this.data.firmwares[index] = {
                ...this.data.firmwares[index],
                ...updates,
                updateTime: new Date().toISOString()
            };
            return this.saveData();
        }
        return false;
    }

    // 删除固件
    deleteFirmware(id) {
        const index = this.data.firmwares.findIndex(f => f.id === id);
        if (index !== -1) {
            const firmware = this.data.firmwares[index];
            this.data.firmwares.splice(index, 1);
            
            // 删除相关文件
            this.deleteFiles(firmware);
            
            return this.saveData();
        }
        return false;
    }

    // 根据ID查找固件
    findFirmware(id) {
        return this.data.firmwares.find(f => f.id === id);
    }

    // 清空指定版本及之前的所有版本固件
    clearFirmwaresUpToVersion(targetVersion) {
        const toDelete = [];
        const toKeep = [];
        
        this.data.firmwares.forEach(firmware => {
            if (isValidVersion(firmware.version) && isValidVersion(targetVersion)) {
                if (compareVersions(firmware.version, targetVersion) <= 0) {
                    toDelete.push(firmware);
                } else {
                    toKeep.push(firmware);
                }
            } else {
                // 如果版本号格式不正确，保留固件
                toKeep.push(firmware);
            }
        });
        
        // 删除文件
        toDelete.forEach(firmware => {
            this.deleteFiles(firmware);
        });
        
        // 更新固件列表
        this.data.firmwares = toKeep;
        
        return {
            success: this.saveData(),
            deletedCount: toDelete.length,
            deletedFirmwares: toDelete.map(f => ({
                id: f.id,
                name: f.name,
                version: f.version
            }))
        };
    }

    // 删除固件相关文件
    deleteFiles(firmware) {
        try {
            if (firmware.slotA && firmware.slotA.filePath) {
                const fullPath = path.join(config.uploadDir, path.basename(firmware.slotA.filePath));
                if (fs.existsSync(fullPath)) {
                    fs.unlinkSync(fullPath);
                    console.log(`已删除槽A文件: ${fullPath}`);
                }
            }
            if (firmware.slotB && firmware.slotB.filePath) {
                const fullPath = path.join(config.uploadDir, path.basename(firmware.slotB.filePath));
                if (fs.existsSync(fullPath)) {
                    fs.unlinkSync(fullPath);
                    console.log(`已删除槽B文件: ${fullPath}`);
                }
            }
        } catch (error) {
            console.error('删除文件失败:', error.message);
        }
    }

    // 生成唯一ID
    generateId() {
        return crypto.randomBytes(16).toString('hex');
    }
}

// 创建存储实例
const storage_manager = new FirmwareStorage();

// 版本号比较工具函数 (保留在server.js中用于clearFirmwaresUpToVersion方法)
function compareVersions(version1, version2) {
    const v1Parts = version1.split('.').map(Number);
    const v2Parts = version2.split('.').map(Number);
    
    while (v1Parts.length < 3) v1Parts.push(0);
    while (v2Parts.length < 3) v2Parts.push(0);
    
    for (let i = 0; i < 3; i++) {
        if (v1Parts[i] < v2Parts[i]) return -1;
        if (v1Parts[i] > v2Parts[i]) return 1;
    }
    
    return 0;
}

function isValidVersion(version) {
    const versionPattern = /^\d+\.\d+\.\d+$/;
    return versionPattern.test(version);
}

// ==================== 设备认证中间件 ====================

/**
 * 生成设备签名 (与STM32端算法保持一致)
 */
function generateDeviceSignature(deviceId, challenge, timestamp) {
    console.log('\n🔐 ===== 设备签名生成过程 =====');
    console.log('📥 输入参数:');
    console.log(`  deviceId: "${deviceId}"`);
    console.log(`  challenge: "${challenge}"`);
    console.log(`  timestamp: ${timestamp}`);
    
    const signData = deviceId + challenge + timestamp.toString();
    console.log(`📝 签名数据拼接: "${signData}"`);
    console.log(`📏 签名数据长度: ${signData.length} 字符`);
    
    let hash = 0x9E3779B9;
    console.log(`🔢 初始哈希值: 0x${hash.toString(16).toUpperCase()}`);
    
    for (let i = 0; i < signData.length; i++) {
        const char = signData[i];
        const charCode = signData.charCodeAt(i);
        const oldHash = hash;
        
        hash = ((hash << 5) + hash) + charCode;
        hash = hash >>> 0; // 确保32位无符号整数
    }
    
    const finalSignature = `SIG_${hash.toString(16).toUpperCase().padStart(8, '0')}`;
    
    return finalSignature;
}

/**
 * 设备认证中间件
 * @param {Object} options - 配置选项
 * @param {string} options.source - token来源: 'headers', 'body', 'query'
 * @param {number} options.expiresIn - 过期时间(秒), 默认30分钟
 */
function validateDeviceAuth(options = {}) {
    options = { ...volidateDefaultConfig, ...options };
    const { source = 'headers', expiresIn } = options;
    
    return (req, res, next) => {
        console.log('\n🛡️  ===== 设备认证验证开始 =====');
        console.log(`📍 请求路径: ${req.method} ${req.path}`);
        console.log(`📡 认证来源: ${source}`);
        console.log(`⏰ 过期时间: ${expiresIn}秒 (${Math.round(expiresIn/60)}分钟)`);
        
        try {
            let authData;
            
            // 从不同来源获取认证数据
            console.log('\n🔍 步骤1: 获取认证数据');
            if (source === 'headers') {
                const authHeader = req.headers['x-device-auth'];
                console.log(`📋 从Headers获取: ${authHeader ? '找到' : '未找到'}`);
                if (!authHeader) {
                    console.log('❌ 认证失败: Headers中缺少认证信息');
                    return res.status(401).json({
                        error: 'AUTH_MISSING',
                        message: 'Device authentication required'
                    });
                }
                try {
                    authData = JSON.parse(Buffer.from(authHeader, 'base64').toString());
                    console.log('✅ Headers认证数据解析成功');
                } catch (e) {
                    console.log('❌ 认证失败: Headers认证数据格式错误');
                    return res.status(401).json({
                        error: 'AUTH_INVALID_FORMAT',
                        message: 'Invalid authentication format'
                    });
                }
            } else if (source === 'body') {
                authData = req.body.deviceAuth;
                console.log(`📋 从Body获取: ${authData ? '找到' : '未找到'}`);
                if (authData) {
                    console.log('✅ Body认证数据获取成功');
                } else {
                    console.log('❌ 认证失败: Body中缺少deviceAuth字段');
                }
            } else if (source === 'query') {
                const authParam = req.query.deviceAuth;
                console.log(`📋 从Query获取: ${authParam ? '找到' : '未找到'}`);
                if (authParam) {
                    try {
                        authData = JSON.parse(Buffer.from(authParam, 'base64').toString());
                        console.log('✅ Query认证数据解析成功');
                    } catch (e) {
                        console.log('❌ 认证失败: Query认证数据格式错误');
                        return res.status(401).json({
                            error: 'AUTH_INVALID_FORMAT',
                            message: 'Invalid authentication format'
                        });
                    }
                }
            }
            
            if (!authData) {
                console.log('❌ 认证失败: 未找到认证数据');
                return res.status(401).json({
                    error: 'AUTH_MISSING',
                    message: 'Device authentication data missing'
                });
            }
            
            // 验证必需字段
            console.log('\n🔍 步骤2: 验证认证数据字段');
            const { deviceId, challenge, timestamp, signature } = authData;
            console.log('📋 认证数据字段检查:');
            console.log(`  deviceId: ${deviceId ? '✅' : '❌'} "${deviceId || 'undefined'}"`);
            console.log(`  challenge: ${challenge ? '✅' : '❌'} "${challenge || 'undefined'}"`);
            console.log(`  timestamp: ${timestamp ? '✅' : '❌'} ${timestamp || 'undefined'}`);
            console.log(`  signature: ${signature ? '✅' : '❌'} "${signature || 'undefined'}"`);
            
            if (!deviceId || !challenge || !timestamp || !signature) {
                console.log('❌ 认证失败: 认证数据字段不完整');
                return res.status(401).json({
                    error: 'AUTH_INCOMPLETE',
                    message: 'Authentication data incomplete'
                });
            }
            console.log('✅ 认证数据字段完整');
            
            // 验证设备是否已注册
            console.log('\n🔍 步骤3: 验证设备注册状态');
            const device = storage_manager.findDevice(deviceId);
            if (!device) {
                console.log(`❌ 认证失败: 设备 "${deviceId}" 未注册`);
                return res.status(401).json({
                    error: 'DEVICE_NOT_REGISTERED',
                    message: 'Device not registered'
                });
            }
            console.log(`✅ 设备已注册: ${device.deviceName} (${device.deviceId})`);
            
            // 验证签名
            console.log('\n🔍 步骤4: 验证设备签名');
            console.log('🔐 开始生成期望签名...');
            const expectedSignature = generateDeviceSignature(deviceId, challenge, timestamp);
            
            console.log('📋 签名比较:');
            console.log(`  收到的签名: "${signature}"`);
            console.log(`  期望的签名: "${expectedSignature}"`);
            console.log(`  签名匹配: ${signature === expectedSignature ? '✅' : '❌'}`);
            
            if (signature !== expectedSignature) {
                console.log('❌ 认证失败: 设备签名验证失败');
                return res.status(401).json({
                    error: 'INVALID_SIGNATURE',
                    message: 'Invalid device signature'
                });
            }
            console.log('✅ 设备签名验证成功');
            
            // 基于挑战的重放攻击防护
            console.log('\n🔍 步骤5: 挑战重放攻击防护');
            // 存储挑战首次使用时间，允许在有效期内重复使用
            if (!global.usedChallenges) {
                global.usedChallenges = new Map();
                console.log('🆕 初始化挑战存储Map');
            }
            
            console.log(`📋 当前已记录的挑战数量: ${global.usedChallenges.size}`);
            
            // 检查挑战是否已被使用过
            const challengeFirstUsed = global.usedChallenges.get(challenge);
            if (challengeFirstUsed) {
                // 如果挑战已被使用，检查是否超出有效期
                const timeSinceFirstUse = Date.now() - challengeFirstUsed;
                console.log(`🔄 挑战已存在: "${challenge}"`);
                console.log(`⏰ 首次使用时间: ${new Date(challengeFirstUsed).toISOString()}`);
                console.log(`⏰ 已使用时长: ${Math.round(timeSinceFirstUse / 1000)}秒`);
                console.log(`⏰ 有效期限制: ${expiresIn}秒`);
                
                if (timeSinceFirstUse > (expiresIn * 1000)) {
                    console.log('❌ 认证失败: 挑战已过期');
                    return res.status(401).json({
                        error: 'CHALLENGE_EXPIRED',
                        message: 'Challenge has expired'
                    });
                }
                // 在有效期内，允许重复使用
                console.log(`✅ 挑战复用成功: ${challenge}, 已使用 ${Math.round(timeSinceFirstUse / 1000)}秒`);
            } else {
                // 第一次使用此挑战，记录使用时间
                global.usedChallenges.set(challenge, Date.now());
                console.log(`🆕 挑战首次使用: "${challenge}"`);
                console.log(`⏰ 记录时间: ${new Date().toISOString()}`);
            }
            
            // 定期清理过期的挑战记录（保留最近30分钟的记录）
            const cleanupThreshold = Date.now() - (expiresIn * 1000);
            let cleanupCount = 0;
            for (const [usedChallenge, usedTime] of global.usedChallenges.entries()) {
                if (usedTime < cleanupThreshold) {
                    global.usedChallenges.delete(usedChallenge);
                    cleanupCount++;
                }
            }
            if (cleanupCount > 0) {
                console.log(`🧹 清理过期挑战: ${cleanupCount}个, 剩余: ${global.usedChallenges.size}个`);
            }
            
            // 验证通过，将设备信息添加到请求对象
            req.authenticatedDevice = device;
            console.log('\n🎉 ===== 设备认证验证成功 =====');
            console.log(`✅ 认证通过: ${device.deviceName} (${device.deviceId})`);
            console.log(`🔐 使用的挑战: "${challenge}"`);
            console.log('🛡️  ===== 认证验证完成 =====\n');
            
            next();
            
        } catch (error) {
            console.error('\n💥 ===== 设备认证验证异常 =====');
            console.error('❌ 认证过程中发生错误:', error);
            console.error('🛡️  ===== 认证验证异常结束 =====\n');
            return res.status(500).json({
                error: 'AUTH_SERVER_ERROR',
                message: 'Authentication server error'
            });
        }
    };
}

// ==================== API 路由 ====================

// 健康检查
app.get('/health', (req, res) => {
    res.json({
        status: 'ok',
        message: 'STM32 HBox 固件服务器运行正常',
        timestamp: new Date().toISOString(),
        version: '1.0.0'
    });
});

// 设备注册接口
app.post('/api/device/register', (req, res) => {
    try {
        const { rawUniqueId, deviceId, deviceName } = req.body;
        
        // 🔍 调试打印：收到的注册请求数据
        console.log('📥 设备注册请求:');
        console.log('  原始唯一ID (rawUniqueId):', rawUniqueId);
        console.log('  设备ID (deviceId):', deviceId);
        console.log('  设备名称 (deviceName):', deviceName);
        
        // 验证必需参数
        if (!rawUniqueId || !deviceId) {
            return res.status(400).json({
                success: false,
                message: '原始唯一ID和设备ID是必需的',
                errNo: 1,
                errorMessage: 'rawUniqueId and deviceId are required'
            });
        }

        // 验证原始唯一ID格式 (XXXXXXXX-XXXXXXXX-XXXXXXXX)
        const uniqueIdPattern = /^[A-Fa-f0-9]{8}-[A-Fa-f0-9]{8}-[A-Fa-f0-9]{8}$/;
        if (!uniqueIdPattern.test(rawUniqueId.trim())) {
            return res.status(400).json({
                success: false,
                message: '原始唯一ID格式错误，必须是 XXXXXXXX-XXXXXXXX-XXXXXXXX 格式',
                errNo: 1,
                errorMessage: 'rawUniqueId format error, must be XXXXXXXX-XXXXXXXX-XXXXXXXX format'
            });
        }

        // 验证设备ID格式 (16位十六进制)
        const deviceIdPattern = /^[A-Fa-f0-9]{16}$/;
        if (!deviceIdPattern.test(deviceId.trim())) {
            return res.status(400).json({
                success: false,
                message: '设备ID格式错误，必须是16位十六进制字符串',
                errNo: 1,
                errorMessage: 'deviceId format error, must be 16-digit hexadecimal string'
            });
        }

        // 构建设备信息
        const deviceInfo = {
            rawUniqueId: rawUniqueId.trim(),
            deviceId: deviceId.trim().toUpperCase(),
            deviceName: deviceName ? deviceName.trim() : `HBox-${deviceId.trim().substring(0, 8)}`,
            registerIP: req.ip || req.connection.remoteAddress || 'unknown'
        };

        // 注册设备
        const result = storage_manager.addDevice(deviceInfo);
        
        if (result.success) {
            const statusCode = result.existed ? 200 : 201;
            res.status(statusCode).json({
                success: true,
                message: result.message,
                errNo: 0,
                data: {
                    deviceId: result.device.deviceId,
                    deviceName: result.device.deviceName,
                    registerTime: result.device.registerTime,
                    existed: result.existed
                }
            });
            
            if (!result.existed) {
                console.log(`Device registered successfully: ${result.device.deviceName} (${result.device.deviceId})`);
            } else {
                console.log(`Device already exists: ${result.device.deviceName} (${result.device.deviceId})`);
            }
        } else {
            res.status(400).json({
                success: false,
                message: result.message,
                errNo: 1,
                errorMessage: result.message
            });
        }

    } catch (error) {
        console.error('Device registration failed:', error);
        res.status(500).json({
            success: false,
            message: '设备注册失败',
            errNo: 1,
            errorMessage: 'Device registration failed: ' + error.message,
            error: error.message
        });
    }
});

// 获取设备列表接口
app.get('/api/devices', validateDeviceAuth(), (req, res) => {
    try {
        const devices = storage_manager.getDevices();
        res.json({
            success: true,
            data: devices,
            total: devices.length,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('获取设备列表失败:', error);
        res.status(500).json({
            success: false,
            message: '获取设备列表失败',
            error: error.message
        });
    }
});

// 注册固件路由
const firmwareRouter = initFirmwareRoutes(storage_manager, config, validateDeviceAuth);
app.use('/', firmwareRouter);

// 错误处理中间件
app.use((error, req, res, next) => {
    if (error instanceof multer.MulterError) {
        if (error.code === 'LIMIT_FILE_SIZE') {
            return res.status(400).json({
                success: false,
                message: `file size cannot exceed ${config.maxFileSize / (1024 * 1024)}MB`
            });
        }
    }
    
    console.error('服务器错误:', error);
    res.status(500).json({
        success: false,
        message: 'Server internal error',
        error: error.message
    });
});

// 404 处理
app.use((req, res) => {
    res.status(404).json({
        success: false,
        message: 'API not found',
        path: req.path
    });
});

// 优雅关闭处理
process.on('SIGINT', () => {
    console.log('\nReceived interrupt signal, shutting down server...');
    process.exit(0);
});

process.on('SIGTERM', () => {
    console.log('\nReceived termination signal, shutting down server...');
    process.exit(0);
});

// 启动服务器
app.listen(PORT, () => {
    console.log('='.repeat(60));
    console.log('STM32 HBox 固件管理服务器');
    console.log('='.repeat(60));
    console.log(`服务器地址: http://localhost:${PORT}`);
    console.log(`上传目录: ${config.uploadDir}`);
    console.log(`数据文件: ${config.dataFile}`);
    console.log(`最大文件大小: ${config.maxFileSize / (1024 * 1024)}MB`);
    console.log(`支持文件类型: ${config.allowedExtensions.join(', ')}`);
    console.log('='.repeat(60));
    console.log('可用接口:');
    console.log('  GET    /health                 - 健康检查');
    console.log('  POST   /api/device/register    - 注册设备ID');
    console.log('  GET    /api/devices            - 获取设备列表');
    console.log('  GET    /api/firmwares          - 获取固件列表');
    console.log('  POST   /api/firmware-check-update - 检查固件更新');
    console.log('  POST   /api/firmwares/upload   - 上传固件包');
    console.log('  GET    /api/firmwares/:id      - 获取固件详情');
    console.log('  PUT    /api/firmwares/:id      - 更新固件信息');
    console.log('  DELETE /api/firmwares/:id      - 删除固件包');
    console.log('  POST   /api/firmwares/clear-up-to-version - 清空指定版本及之前的固件');
    console.log('  GET    /downloads/:filename    - 下载固件包');
    console.log('='.repeat(60));
    console.log('服务器启动成功！按 Ctrl+C 停止服务');
});

module.exports = app; 