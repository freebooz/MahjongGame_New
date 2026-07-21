# 阶段 7：控制面一致性、持久化与部署工程化

日期：2026-07-20

## 已完成

- Lobby 新增 PostgreSQL `active_player_rooms(player_id PRIMARY KEY)`。建房、加入、关房和最终结算在同一事务中同步房间快照与玩家活动租约；状态更新使用严格的上一序号 CAS，禁止同序号覆盖。
- Redis 房间缓存使用 Lua 比较 `stateSequence`，较旧的提交后缓存写入不能覆盖新快照。
- Allocator 把实例、端口、PID/启动时间、状态、凭据摘要和待发送失败通知原子写入状态文件；启动时重新附着存活进程、重新租用端口，缺失或过期实例进入 Failed，并持久重试通知 Lobby。心跳检查点限频，状态转换仍强制落盘。
- 新增独立 `GuiyangMahjong.Auth` 应用。游客身份由安装标识的 HMAC 派生并由服务端持有；访问令牌与 Lobby 格式兼容；刷新令牌只保存摘要、单次事务轮换并支持注销。
- UE `GuiyangMahjongOnline` 在 `RemoteAuth` 模式调用 Auth guest/refresh/logout API，访问令牌和刷新令牌只保存在进程内存；SaveGame 仅保存非敏感安装标识与自动登录偏好。
- Lobby 生产模式将幂等结果、在线 Presence、事件序号和 Pub/Sub 迁入 Redis；WebSocket 使用每连接有界发送队列。
- UE Runtime 已拆为 `GuiyangMahjongCore`、`GuiyangMahjongOnline` 和主游戏模块；UI 生成 Commandlet 移入 `GuiyangMahjongEditorTools`，Runtime Build.cs 不再依赖 UnrealEd。
- 新增 `Services/GuiyangMahjong.Services.slnx`、Linux .NET CI、Windows UE 5.8 自托管 CI、Linux Auth/Lobby 镜像、Windows Allocator 镜像、Compose 与 Kubernetes 清单。
- `/health/ready` 不再固定返回 200：Auth 检查身份存储，Lobby 检查 PostgreSQL/Redis 和启用的 Allocator，Allocator 检查启动对账完成与 GameServer 可执行文件。

## 当前验证

- `.NET` 解决方案：Allocator 9 项、Auth 5 项、Lobby 28 项，共 42 项测试。
- UE Win64 Game Target：模块拆分与 RemoteAuth 客户端编译、链接成功。
- UE Win64 Server Target：`GuiyangMahjongServer.exe` 编译、链接成功（UBT `Result: Succeeded`）。
- UE Win64 Editor Target：在保留当前运行中 Editor 的前提下，以 `-NoLink` 完成所有模块、Commandlet 与 Editor 自动化测试源文件编译；未执行会覆盖已加载 DLL 的最终链接。
- UE Game 无界面冒烟：使用 `-unattended -nullrhi -ExecCmds=Quit` 启动并正常退出（退出码 0），未发现模块重定向、类加载或链接错误。

## 发布前外部门禁

当前开发机没有可调用的 Docker/Podman、Redis Server 或 PostgreSQL Server，因此本阶段不能声称完成真实外部存储/容器集群验收。发布环境必须执行：

1. 在临时 PostgreSQL/Redis 上运行 schema 升级，并发验证同一玩家跨实例建房/加入只能成功一次。
2. 分别在 Starting、Allocated、Draining 状态强制重启 Allocator，核对 PID 身份、端口不重租、心跳恢复和失败通知。
3. 启动两个 Lobby 副本，验证同一 Idempotency-Key 只执行一次、Presence 汇总一致、任一副本发布事件可被另一副本 WebSocket 客户端收到。
4. 轮换 Auth/Lobby 共用的玩家 Token 签名材料，并验证旧访问令牌过期、刷新重放拒绝和注销。
5. 将 Kubernetes 示例 Secret 替换为密钥系统注入，并把所有 `latest` 镜像改为不可变 digest。
