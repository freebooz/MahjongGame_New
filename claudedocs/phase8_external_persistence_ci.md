# 阶段 8：外部持久化 CI 门禁

日期：2026-07-20

## 目标

把阶段 7 的 PostgreSQL/Redis 人工发布检查升级为可重复的 CI 集成测试，同时保留目标集群部署验收。

## 已实现

- Services CI 使用 PostgreSQL 17 与 Redis 7 service containers，并等待两项服务通过各自健康检查。
- PostgreSQL 测试以两个独立连接池并发创建房间，验证同一玩家只能获得一个活动房间租约，失败事务不会遗留第二个房间。
- Auth PostgreSQL 测试以两个独立 Auth 实例并发轮换同一个刷新令牌，验证只有一个实例成功、旧令牌不能重放、新令牌注销后不可刷新，数据库中不存在多余的轮换会话。
- Auth 身份测试验证同一安装标识跨实例保持相同 PlayerId 和初始昵称，并由两个实例分别执行真实数据库 readiness 查询。
- Redis 测试使用两个独立连接复现两个 Lobby 副本，验证同一幂等键只执行一次业务操作。
- Presence 测试验证跨副本在线人数聚合以及超时成员清理。
- 事件测试由一个 Lobby Hub 发布 Redis Pub/Sub 事件，并验证另一个 Hub 的 WebSocket 客户端能够收到，且全局事件序号递增。
- Allocator 进程级测试启动真实 `GuiyangMahjong.FakeGameServer` apphost，通过本地 Lobby Stub 完成注册与心跳；随后从 JSON 状态文件创建新的 Manager 和进程启动器，验证 PID/启动时间身份匹配、进程重新附着、端口保持租用、重复分配被拒绝以及 Drain 后进程退出和端口释放。
- Auth 外部测试需要 `AUTH_TEST_POSTGRES`；Lobby 外部测试需要 `LOBBY_TEST_POSTGRES` 和 `LOBBY_TEST_REDIS`。普通开发机未配置相应连接时明确显示为跳过，不会用内存实现冒充外部存储验收。

## 当前验证边界

- 本机 Release 构建为 0 警告、0 错误；Allocator 9 项、Auth 5 项、Lobby 28 项，共 42 项普通测试通过。
- Auth 2 项、Lobby 4 项，共 6 项外部持久化测试已完成发现、分类与条件跳过验证。
- Allocator 真实进程恢复测试作为独立 `ProcessIntegration` 类别执行，不依赖 Docker 或外部数据库，并强制清理子进程和临时状态目录；本机连续执行 3 次均通过，未残留 FakeGameServer 进程。
- 进程级测试首次执行发现 Drain 后读取已释放 `Process` 句柄会抛出异常；`ManagedGameServerProcess` 现已缓存 PID 与启动时间，停止后的快照和持久化仍可安全读取。修复后真实恢复测试通过。
- 当前机器没有 Docker/Podman、Redis Server 或 PostgreSQL Server，因此尚不能在本机宣称外部集成测试通过。
- GitHub Actions 实际运行成功后，才能关闭“真实 PostgreSQL/Redis CI”门禁；Kubernetes 多副本、Secret 注入和不可变镜像仍需在目标发布集群验收。
