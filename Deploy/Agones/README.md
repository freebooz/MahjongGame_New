# Agones 专用游戏服务器部署

本目录提供可选的 Agones 生产部署骨架。现有 Lobby/Allocator 本地与 WSL 流程保持不变；只有显式传入
`-MahjongOrchestrator=Agones` 或环境变量 `MAHJONG_ORCHESTRATOR=agones` 时，UE 专用服务器才连接
Agones Sidecar。

## 前置条件

- Linux Server 包已输出到 `Artifacts/LinuxServer/`。
- Kubernetes 集群已安装 Agones，且已创建 `guiyang-mahjong` namespace。
- 镜像使用不可变版本或 digest；禁止直接把清单中的占位值用于生产。

## 构建镜像

在仓库根目录执行：

```bash
docker build -f Deploy/Docker/Dockerfile.gameserver \
  -t ghcr.io/freebooz/guiyang-mahjong-server:<version> .
```

## 部署与分配

先将 Fleet 中的镜像和构建版本占位值替换为本次发布值，然后执行：

```bash
kubectl create namespace guiyang-mahjong
kubectl apply -f Deploy/Agones/guiyang-mahjong-fleet.yaml
kubectl apply -f Deploy/Agones/guiyang-mahjong-autoscaler.yaml
kubectl get fleet,gameserver,fleetautoscaler -n guiyang-mahjong
```

Lobby 的分配器需要以 `guiyang-mahjong-allocation.yaml` 为模板，为每次分配写入唯一的 `room-id`
和 `match-id`，提交 `GameServerAllocation`，再把响应中的地址与端口返回给客户端。

## 当前集成边界

Agones 适配器已经负责 SDK 连接、Ready、玩家计数和关闭通知。现有 Lobby/Allocator 模式仍负责
房间事务、单活动房间约束、票据签发及服务注册。把 Lobby 分配器切换为 Agones API 时，还必须将
Allocation 注解中的房间上下文接入 UE 的托管服务器注册流程；在完成该适配前，不应把此示例当作
已启用加入票据强校验的生产清单。

## 验证

```bash
kubectl describe fleet guiyang-mahjong -n guiyang-mahjong
kubectl logs -n guiyang-mahjong -l agones.dev/fleet=guiyang-mahjong -c game-server --tail=200
kubectl create -f Deploy/Agones/guiyang-mahjong-allocation.yaml -o yaml
```

健康状态以 Agones SDK Sidecar 收到的 UE health ping 为准；容器 TCP/HTTP 探针不能代表 UDP 游戏
进程已完成 Agones 注册。
