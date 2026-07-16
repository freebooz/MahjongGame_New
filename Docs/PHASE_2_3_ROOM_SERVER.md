# 阶段 2/3：Dedicated Server 房间核心

更新时间：2026-07-16

## 已实现范围

- 服务端 `UGuiyangRoomManager`，房间记录和密码凭据不在客户端创建。
- 随机 6 位纯数字房间号，创建时检测活动房间冲突。
- 创建房间时生成并锁定 `FGuiyangRuleSnapshot`，公开状态携带规则哈希。
- 固定 4 个座位、房主、加入、准备/取消准备、离开、房主转移和空房销毁。
- 仅当 4 人全部准备时进入 `Starting`，进入启动状态后拒绝继续修改准备状态。
- GameMode 建立临时游客服务端会话，并处理创建、按房间号加入、准备和离开 RPC。
- GameState 只复制公开房间状态；密码盐、密码摘要、失败次数均不进入复制结构。

## 密码安全

- 房间密码限制为 6-12 位且拒绝空白/控制字符。
- 每个密码房使用独立 256 位随机盐（两个 GUID 串联）。
- Win64 使用系统 CNG：PBKDF2-HMAC-SHA256，100,000 次迭代。
- 非 Windows 构建使用 Unreal 第三方 OpenSSL 的 PBKDF2-HMAC-SHA256 分支；Android SDK 修复后必须在目标 ABI 上重新验证。
- 摘要采用常量时间比较。
- 同一“房间+账号”连续 5 次失败后锁定 30 秒；锁定期内正确密码也不能绕过。
- 日志只记录房间号、账号标识、规则哈希和是否启用密码，不记录密码、盐或摘要。

## 当前部署边界

PC MVP 采用“一台 Dedicated Server 进程对应一张活动牌桌”。因此该进程的 GameState 可以复制其房间公开状态。多房间大厅、匹配与 GameServer 分配需由后续 Lobby/Allocator 服务完成，不能直接让同一 GameState 向不同房间广播不同状态。

`PostLogin` 当前生成短期游客服务端身份，用于跑通 PC 房间链路。正式微信/账号 Token 校验仍需接入后端 Auth API；不能把当前临时游客身份描述为正式生产认证。

## 验证

- `GuiyangMahjongEditor Win64 Development`：Succeeded。
- `GuiyangMahjong` 自动化：14/14 Success。
- 新增：
  - `GuiyangMahjong.Room.FourPlayersReady`
  - `GuiyangMahjong.Room.PasswordSecurityAndLifecycle`
- 密码测试覆盖错误密码、第五次失败锁定、锁定期绕过阻止、正确密码加入、房主转移和空房清理。

## 后续入口

1. 补齐 `WBP_CreateRoomDialog`、`WBP_JoinRoomDialog`、`WBP_RuleConfig` 并调用完整参数 RPC。
2. 将 `Starting` 交给 Table/Turn/ActionResolver，使用房间规则快照初始化 108/136 牌墙。
3. 实现断线保留、账号到座位绑定和公开/私有重连快照。
