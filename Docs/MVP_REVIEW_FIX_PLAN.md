# MVP 修复执行计划

## 阶段 1：工程与证据基线

- 完成现有工程和 MCP 能力报告。
- 建立全部交付文档骨架和持续操作日志。
- 构建 Editor、Game、Server，记录真实结果。

## 阶段 2：登录和会话

- 创建 `UGuiyangLoginSubsystem`、登录 Provider 接口和本地安全 Session 模型。
- 完成真实可用游客登录、PC 明确标记的模拟微信 Provider、自动登录、退出登录和过期处理。
- 创建 WBP_Login、WBP_RootHUD、WBP_ConfirmDialog。
- 自动化覆盖 Session 不进入日志/公共状态。

## 阶段 3：大厅和密码房

- 新增 Lobby/Room 数据协议、请求响应枚举和错误码。
- 实现服务端 RoomManager、6 位房间号、4–12 位密码校验、随机盐摘要、常量时间比较、限流和销毁清理。
- 创建 WBP_CreateRoomDialog、WBP_JoinRoomDialog，扩展 Lobby/Room。
- 实现自定义 PlayerState、房主、座位、准备/取消准备、离开和解散。

## 阶段 4：权威牌桌

- 实现 TableActor、TurnManager、ActionResolver、RoundId/TurnId/ActionSequence。
- 服务端洗牌、发牌、摸牌、出牌、碰、杠、胡、过和优先级。
- 实现翻鸡、结算、下一局和房间生命周期。
- 公共状态只进 GameState，私有手牌只进所属 PlayerController Client RPC。

## 阶段 5：重连

- 用账号 Session 绑定房间和座位。
- 生成公共/私有断线快照并设置过期时间。
- 恢复原座位、公共牌桌、本人手牌和当前操作窗口。

## 阶段 6：UMG 和视觉验收

- 15 个 P0 Widget 全部真实生成。
- 逐个记录树、坐标、Anchor、Size、Padding、ZOrder、字号、四态和九宫格。
- 绑定校验、Blueprint 编译、二次回读。
- 正式视觉切图到位后导入为 UserInterface2D 并完成分辨率截图审查。

## 阶段 7：测试和构建

- 扩展规则、房间、安全、RPC、Widget 自动化。
- 构建 Win64 Editor、Game、Server。
- 启动 Dedicated Server + 4 PC Client，跑完密码错误、完整牌局和重连脚本。
- 保存服务端/客户端中文日志和证据。

## 阶段 8：Android

- 修复 UE 5.8 Android SDK/NDK。
- APK 构建、横屏、SafeZone、DPI、触控和中文字体真机验证。
- 正式微信授权仅在开放平台凭据和服务端回调环境到位后接入。
