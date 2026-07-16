# MVP 完成度缺口

## 严重级

1. 没有登录会话系统，游客/微信/自动登录/退出登录均不可用。
2. 没有服务端房间管理和密码摘要，创建/加入密码房不可用。
3. 没有完整服务端牌桌、回合和操作裁决流程，四人无法完成一局。
4. 没有地图和启动 UI 流程，Standalone/Dedicated Server 无产品级入口。
5. 没有断线会话恢复，重连遮罩仅能重试地址，不能恢复账号/座位/手牌。
6. Game/Server Target 尚未构建验证，Dedicated Server 尚无运行证据。

## 高优先级

1. 15 个 P0 Widget 仅完成 10 个，缺 RootHUD、Login、CreateRoom、JoinRoom、ConfirmDialog。
2. Lobby、Room 控件低于总需求，缺头像、Provider、Ping、公告、密码图标、取消准备、解散等。
3. PlayerController 多数 Server RPC 只有日志，没有权威模型调用和完整上下文校验。
4. 自动化缺少密码安全、重复请求、公共状态隔离、Widget Parent/BindWidget、完整回合测试。
5. 没有四客户端启动脚本、测试文档和日志归档。
6. 无正式 UI 视觉资产、中文打包字体和截图对比证据。

## 中优先级

1. UI 手牌变化时全部重建，尚未做条目复用。
2. 公共弃牌区尚未按增量事件填充 WBP_DiscardTile。
3. 未定义网络版本兼容字段、RoundId、TurnId、ActionSequence 完整校验协议。
4. Android SafeZone/DPI 已有基础配置，但 SDK 无效，未真机验证。
5. 当前 `.git` 不是有效仓库，缺少可靠版本基线。

## 当前结论

- PC 可运行 MVP：否。
- 真实可玩：否。
- 允许进入 Android 深度部署：否。
- 最终可部署：否。
