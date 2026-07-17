# 阶段 16：跨地图断线重连 UI

## 完成内容

- 新增 `UGuiyangReconnectSubsystem`，在 `GameInstance` 生命周期内保存最近服务器地址、端口、玩家名、重连状态和截止时间。
- 订阅 Unreal 的网络失败和 Travel 失败事件，自动进入重连窗口。
- 断线后即使当前 `PlayerController` 和 RootHUD 因地图切换重建，新 RootHUD 仍会恢复同一重连遮罩与剩余时间。
- 活跃房间优先使用服务端锁定规则中的 `ReconnectTimeoutSeconds`；本地展示同样限制在 15–600 秒。
- 重试失败不会重置原截止时间，避免客户端界面无限延长服务端座位保留窗口。
- 重连按钮只在端点完整、窗口未过期且没有正在重试时启用。
- `WBP_ReconnectOverlay` 每秒值变化时更新倒计时；到期后显示明确提示并禁用重试。
- 重连成功后先清理跨地图重连状态，再恢复房间、公共牌桌、私有手牌和可操作列表。
- “返回连接界面”会取消本次重连，并由 RootHUD 路由到 `WBP_ConnectServer`。

## 安全与状态边界

- Subsystem 只保存服务器端点和显示名，不保存登录令牌、房间密码、手牌或其他私密快照。
- 会话凭据仍仅由登录子系统在进程内存中持有，并由新 PlayerController 重新认证。
- 服务端仍是重连窗口、座位恢复和快照内容的权威来源；客户端倒计时不延长服务器状态。

## 验证结果

- `GuiyangMahjongEditor Win64 Development`：编译成功。
- UMG 幂等生成：17/17，0 error，0 warning。
- `GuiyangMahjong` 自动化：30/30 Success。
- 新增测试：`GuiyangMahjong.UI.ReconnectPresentation`。
- 自动化报告：`Saved/Automation/Phase16ReconnectUI/index.html`。

## 下一阶段

- 增加可控断网/恢复的多客户端集成测试，验证真实 `NetworkFailure`、Travel 和快照恢复链路。
- 修复当前黑图截图捕获环境，补齐 ReconnectOverlay 与 GameHUD 的多分辨率证据。
- 在 Android 真机验证前后台切换、网络切换、横屏恢复和触控热区。
