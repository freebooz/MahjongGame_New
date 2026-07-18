# 阶段 3：GameServer Allocator 实施状态

日期：2026-07-18

GameServer Allocator MVP 已实现于 `Services/GuiyangMahjong.Allocator`，并以
`Services/GuiyangMahjong.FakeGameServer` 作为阶段 3 的受控进程替身。真实 UE Dedicated Server 接入仍属于阶段 4。

| 项目 | 状态 | 验证 |
|---|---|---|
| 独立 Allocator HTTP 服务 | 已实现 | Release 构建 0 警告、0 错误 |
| 确定性端口池与实例状态机 | 已实现 | 并发分配端口唯一，停止后端口可复用 |
| 安全进程启动 | 已实现 | 使用 `ArgumentList`，注册与心跳凭据不写日志 |
| 一次性注册门禁 | 已实现 | 错误凭据和重复注册均拒绝 |
| 注册后路由发布 | 已实现 | 注册前返回 `ServerUnavailable`，注册后签发 30 秒 HMAC 入场票据 |
| 心跳监测与故障回调 | 已实现 | 进程退出或心跳超时后先停止进程，再回收端口并撤销 Lobby 路由 |
| 排空与端口归还 | 已实现 | `Allocated -> Draining -> Stopped` 自动停止进程并归还端口 |
| 独立 OpenAPI | 已实现 | `Contracts/OpenAPI/allocator-v1.openapi.yaml` |

自动化验证：Allocator `4/4`、Lobby `15/15` 测试通过。真实进程联调脚本
`Scripts/Test-Phase3Integration.ps1` 已同时创建两张牌桌，确认独立进程与独立端口；强制杀死其中一台后，实例在监测窗口内进入
`Failed`，Lobby 撤销路由，随后创建第三张牌桌并成功复用已归还端口。

阶段边界：Fake GameServer 只验证调度、注册、心跳、崩溃与回收，不实现麻将规则或 UE 网络复制。下一阶段应把相同启动参数、注册与心跳契约接入
`GuiyangMahjongServer`，并在 `PreLogin` 前完成入场票据校验。
