# 《贵阳捉鸡麻将》系统架构图

日期：2026-07-18  
适用范围：UE 5.8 PC/Android/平板客户端、独立 Lobby、Allocator、一桌一进程 Dedicated Server。

> 图中“已实现”表示当前仓库已有对应代码和自动化验证；“待接入/发布目标”表示架构边界已经确定，但仍属于发布门禁。

## 1. 应用架构图

```mermaid
flowchart LR
    subgraph Client["UE 5.8 客户端（PC / Android / 平板）"]
        UI["移动端全屏 UI\n大厅 / 房间 / 牌桌 / 结算 / 重连"]
        Login["LoginSubsystem\n登录会话与玩家档案"]
        LobbySDK["LobbySubsystem + ILobbyBackend\nLocalLegacy / RemoteLobby"]
        Reconnect["ReconnectSubsystem\n重连窗口与非敏感牌桌提示"]
        NetClient["PlayerController\nClientTravel / RPC / 私有手牌"]

        UI --> Login
        UI --> LobbySDK
        UI --> Reconnect
        LobbySDK --> NetClient
        Reconnect --> LobbySDK
    end

    Auth["独立 Auth 应用（待接入）\n签发 / 刷新 / 吊销玩家 Bearer Token"]

    subgraph ControlPlane["控制面"]
        Lobby["ASP.NET Core Lobby（已实现）\n房间目录 / 玩家映射 / 路由 / 票据 / 结算"]
        Allocator["ASP.NET Core Allocator（已实现）\n端口租约 / 进程生命周期 / 健康监测 / 回收"]
        Outbox[("本地结算 outbox\n原子 JSON / 无凭据")]
        Store["ILobbyStore\nInMemory（开发）\nRedis + PostgreSQL（生产适配器）"]
        EventHub["WebSocket Event Hub\nlobby.updated / room.updated\nserver.assigned / room.closed"]

        Lobby --> Store
        Lobby --> EventHub
        Lobby -->|"分配 / Drain"| Allocator
        Allocator --> Outbox
    end

    subgraph DataPlane["牌桌数据面（一桌一进程）"]
        Bridge["GameServerBridge\n注册 / 心跳 / 票据校验 / 结算补报"]
        GameMode["GameMode + RoomManager\n房间与玩家权威"]
        Engine["MahjongTableEngine\n回合 / 动作 / 超时 / 计分"]
        Replication["GameState + Client RPC\n公开状态 / 私有手牌 / 可操作项"]

        Bridge --> GameMode
        GameMode --> Engine
        Engine --> Replication
    end

    LocalLegacy["LocalLegacy RPC\n兼容回退链路"]
    LocalHistory["客户端本地战绩缓存"]

    UI -.-> LocalLegacy
    Auth -->|"玩家 Token"| Login
    LobbySDK -->|"HTTPS Lobby API v1"| Lobby
    Lobby -->|"权威路由 + 短期 JoinTicket"| LobbySDK
    EventHub -->|"WebSocket 事件"| LobbySDK
    Allocator -->|"启动 / 停止 UE 进程"| Bridge
    Bridge -->|"内部注册 / 心跳 / 幂等结算"| Lobby
    Bridge -->|"先写盘，确认后删除"| Outbox
    Outbox -->|"崩溃后内部身份恢复补报"| Allocator
    NetClient -->|"UDP 连接 + JoinTicket\n可靠 RPC / 状态复制"| Replication
    Replication --> NetClient
    NetClient --> LocalHistory
```

### 核心职责边界

| 组件 | 权威数据 | 明确禁止 |
|---|---|---|
| Auth | 玩家身份、Token 生命周期 | 客户端自签身份；Lobby 暴露公开签发接口 |
| Lobby | 玩家到房间映射、房间生命周期、GameServer 路由、最终战绩 | 承载实时牌局；信任客户端规则或结算 |
| Allocator | 端口租约、GameServer 进程和实例状态 | 承载玩家 UI 或牌局规则 |
| GameServer | 座位、手牌、牌墙、动作、超时、单局和最终结算 | 单进程承载多张牌桌；接受客户端权威状态 |
| UE Client | 输入、展示、本人的临时会话状态 | 保存签名密钥；复用过期 JoinTicket；提交权威结算 |

## 2. 部署架构图

```mermaid
flowchart TB
    subgraph Devices["用户设备"]
        PC["Windows PC 客户端"]
        Android["Android 手机 / 平板"]
    end

    subgraph Edge["公网接入层（发布目标）"]
        DNS["正式域名 + DNS"]
        Gateway["TLS 反向代理 / WAF / 限流"]
    end

    subgraph Services["服务区"]
        AuthSvc["Auth Service（独立应用，待接入）"]
        LobbySvc["Lobby Service\n可横向扩展"]
        AllocatorSvc["Allocator Service\n每区域一个控制器"]
        OutboxDisk[("每区域本地 outbox\nServerInstanceId.json")]
        Observability["日志 / 指标 / 告警（Phase 7）"]
    end

    subgraph Persistence["数据区"]
        Redis[("Redis\n在线房间热缓存 / 事件状态")]
        Postgres[("PostgreSQL\n房间快照 / match_results\n唯一键 MatchId + ResultSequence")]
        SecretStore["密钥服务（发布目标）\nToken / JoinTicket / 内部服务密钥"]
    end

    subgraph GameHosts["GameServer 主机池"]
        HostA["主机 A\nUE Server：桌 1 / 端口 P1\nUE Server：桌 2 / 端口 P2"]
        HostB["主机 B\nUE Server：桌 3 / 端口 P3\nUE Server：桌 4 / 端口 P4"]
    end

    PC -->|"HTTPS / WSS"| DNS
    Android -->|"HTTPS / WSS"| DNS
    DNS --> Gateway
    Gateway --> AuthSvc
    Gateway --> LobbySvc

    AuthSvc --> Postgres
    LobbySvc --> Redis
    LobbySvc --> Postgres
    LobbySvc -->|"内部 HTTPS + 服务凭据"| AllocatorSvc
    AllocatorSvc -->|"启动参数 + 环境变量密钥"| HostA
    AllocatorSvc -->|"启动参数 + 环境变量密钥"| HostB
    HostA -->|"原子写入，不含凭据"| OutboxDisk
    HostB -->|"原子写入，不含凭据"| OutboxDisk
    OutboxDisk -->|"恢复扫描"| AllocatorSvc

    PC -->|"UDP 游戏连接 + 短期 JoinTicket"| HostA
    PC -->|"UDP 游戏连接 + 短期 JoinTicket"| HostB
    Android -->|"UDP 游戏连接 + 短期 JoinTicket"| HostA
    Android -->|"UDP 游戏连接 + 短期 JoinTicket"| HostB

    HostA -->|"注册 / 心跳 / 结算补报"| LobbySvc
    HostB -->|"注册 / 心跳 / 结算补报"| LobbySvc
    LobbySvc -->|"结算确认后 Drain"| AllocatorSvc

    SecretStore -.-> AuthSvc
    SecretStore -.-> LobbySvc
    SecretStore -.-> AllocatorSvc
    LobbySvc -.-> Observability
    AllocatorSvc -.-> Observability
    HostA -.-> Observability
    HostB -.-> Observability
```

### 部署约束

- 公网只开放 Auth、Lobby 的 HTTPS/WSS 入口和已分配的 GameServer UDP 端口；内部注册、心跳、结算、Allocator API 不直接暴露公网。
- 每张牌桌对应一个 UE Dedicated Server 进程、一个端口和一个 `ServerInstanceId`，进程内不承载第二张桌。
- 开发环境可以使用单机 `Lobby + Allocator + 多个 UE Server`；生产环境使用 Redis/PostgreSQL，并把密钥改为密钥服务注入。
- GameServer 只通过环境变量接收一次性注册凭据和签名材料，敏感值不进入命令行或日志。
- 当前实现要求 Allocator 与其启动的 GameServer 共享本地 outbox 文件系统；扩展到多主机时，应在每台主机部署 Allocator Agent 或使用具备相同原子语义的持久卷。

## 3. 运行流程图

```mermaid
sequenceDiagram
    autonumber
    actor Player as 玩家
    participant Client as UE Client
    participant Auth as Auth Service
    participant Lobby as Lobby Service
    participant Store as Redis/PostgreSQL
    participant Alloc as Allocator
    participant GS as UE Dedicated Server

    rect rgb(235, 245, 255)
        Note over Player,Lobby: 登录与进入大厅
        Player->>Client: 启动并登录
        Client->>Auth: 登录 / 刷新会话
        Auth-->>Client: 玩家 Bearer Token
        Client->>Lobby: GET /v1/lobby/bootstrap
        Lobby-->>Client: 公告、显示名、在线人数、协议版本
    end

    rect rgb(240, 250, 240)
        Note over Client,GS: 创建或加入牌桌
        Client->>Lobby: 创建房间或加入房间
        Lobby->>Store: 写入权威房间与 PlayerId 映射
        Lobby->>Alloc: POST /internal/allocations
        Alloc->>GS: 启动一桌一进程（RoomId / MatchId / Port）
        GS->>Lobby: 注册（一次性 registrationCredential）
        Lobby->>Store: 绑定 ServerInstanceId、路由和结算凭据摘要
        Lobby-->>GS: heartbeatCredential + resultCredential + 规则快照
        GS->>Lobby: 周期心跳
        Lobby->>Store: Waiting → Playing → Settling
        Client->>Lobby: 查询房间路由
        Lobby->>Store: 校验玩家属于该房间
        Lobby-->>Client: Endpoint + 新 JoinTicket
        Client->>GS: ClientTravel + PlayerId + JoinTicket
        GS->>GS: PreLogin 校验签名、作用域、有效期并消费 nonce
        GS-->>Client: 公开状态复制 + 本人手牌 / 可操作项 Client RPC
    end

    rect rgb(255, 248, 235)
        Note over Client,GS: 网络中断与重连
        GS--xClient: 网络中断
        Client->>Client: 打开重连窗口，不复用旧 JoinTicket
        Client->>Lobby: POST /v1/reconnect/route（认证 PlayerId）
        Lobby->>Store: PlayerId → 活动 RoomId → ServerInstanceId
        Store-->>Lobby: 权威牌桌映射
        Lobby-->>Client: 同一牌桌的新 JoinTicket
        Client->>GS: 再次 ClientTravel
        GS->>GS: 消费新票据并恢复原座位
        GS-->>Client: 公共状态 + 本人手牌 + 可操作项 + 剩余时间
    end

    rect rgb(250, 240, 250)
        Note over Client,Alloc: 最终结算、幂等补报与回收
        GS-->>Client: 单局结算与最终结算 UI（不等待 Lobby）
        GS->>Lobby: POST /internal/matches/{matchId}/result<br/>Idempotency-Key = MatchId + ResultSequence
        GS->>GS: 原子写入本地 outbox（不含凭据）
        alt Lobby 暂时不可用但 GameServer 在线
            Lobby--xGS: 超时 / 5xx
            GS->>GS: 进入内存重试队列并指数退避
            GS->>Lobby: 使用相同结果序号再次补报
        else Lobby 不可用且 GameServer 崩溃
            GS--xAlloc: 进程退出，outbox 保留
            Alloc->>Alloc: 延迟扫描并校验 outbox
            Alloc->>Lobby: 内部服务身份调用 result/recovery
        end
        Lobby->>Store: 单事务插入 match_results 并关闭房间
        alt 首次结果
            Store-->>Lobby: Accepted
        else 相同序号且内容一致
            Store-->>Lobby: Duplicate（不重复记分）
        else 相同序号但内容不同
            Store-->>Lobby: Conflict（拒绝）
        end
        Lobby->>Alloc: Drain ServerInstanceId
        Alloc->>GS: 优雅停止进程并归还端口
        Lobby-->>GS: MatchResultAck（进程仍在线时）
        GS->>GS: 删除已确认 outbox
        Alloc->>Alloc: 恢复补报确认后删除遗留 outbox
    end
```

## 4. 关键状态与幂等键

```mermaid
stateDiagram-v2
    [*] --> Creating
    Creating --> Allocating
    Allocating --> Waiting: GameServer 注册成功
    Waiting --> Playing: 权威心跳
    Playing --> Settling: 最终局完成
    Settling --> Closed: 结算持久化成功
    Creating --> Failed
    Allocating --> Failed
    Waiting --> Failed
    Playing --> Failed
    Settling --> Failed
    Failed --> Closed: 故障清理
    Closed --> [*]
```

- 建房/加入请求：`PlayerId + Idempotency-Key`。
- 入场票据：绑定 `PlayerId + RoomId + MatchId + ServerInstanceId + ExpiresAt + Nonce`，nonce 只允许消费一次。
- 最终结算：数据库唯一键 `MatchId + ResultSequence`；相同内容安全重试，不同内容拒绝覆盖。
