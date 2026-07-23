# Client / Dedicated Server 包体隔离

## 模块边界

| 模块 | Target | 内容 |
|---|---|---|
| `GuiyangMahjongCore` | Client + Server | 纯规则、牌型、登录与大厅传输类型 |
| `GuiyangMahjong` | Client + Server | 复制状态、PlayerState、共享 PlayerController/RPC 接口 |
| `GuiyangMahjongOnline` | ClientOnly | 登录、HTTP Auth、登录存档 |
| `GuiyangMahjongClient` | ClientOnly | UMG、声音、3D 麻将桌、摄像机、大厅、重连、历史 |
| `GuiyangMahjongServer` | ServerOnly | GameMode、房间权威、票据验证、Agones 生命周期 |
| `GuiyangMahjongEditorTools` | Editor | UI/地图生成命令与自动化测试 |

共享 PlayerController 不直接依赖客户端或服务端实现。客户端模块注册表现桥接，服务端
GameMode 实现权威请求接口，因此两端 Target 不会因为 C++ 链接关系互相拖入专属模块。

## 插件策略

工程启用 `DisableEnginePluginsByDefault`。客户端只显式启用 `EnhancedInput`；Agones 只允许
`Server` 和 `Editor` Target；Python 与 Editor Scripting 只允许 `Editor` Target。

## Cook 策略

- Windows/Android Client：Cook `/Game/UI`、`/Game/Art/Mahjong`、客户端编辑用
  `MahjongRoomMap` 和联网共享的 `MahjongNetMap`。
- LinuxServer/WindowsServer：只 Cook 无渲染、无目标专属类依赖的 `MahjongNetMap`，并将 `/Game/UI`、
  `/Game/Art` 加入 NeverCook。
- `DefaultGame.ini` 不再使用全局 AlwaysCook，防止相同规则污染所有 Target。

## 构建与门禁

```powershell
./Scripts/Build-Client.ps1 -Platform Win64 -Configuration Shipping
./Scripts/Build-LinuxServer.ps1 -Configuration Shipping
./Scripts/Test-TargetModuleGraph.ps1
./Scripts/Test-PackageIsolation.ps1
```

`Test-PackageIsolation.ps1` 检查 Target receipt 和 Cook 配置：客户端禁止 Agones/Server；服务端
禁止 Client/Online、NNERuntimeORT、NNEDenoiser、MsQuic，以及项目 UI/美术运行依赖。
UE 5.8 的 `Engine`/`Launch` 本身仍链接基础 Slate/UMG/NNE 代码；项目侧保证不加载这些客户端
实现，也不把 `/Game/UI`、`/Game/Art` 资源 Cook 到服务端。若要求连引擎基础代码也移除，必须
维护 UE 源码分支，不能仅靠项目 Target 安全实现。
`Test-TargetModuleGraph.ps1` 使用 UBT 导出最终链接图，直接阻断客户端包含服务端模块、服务端
包含客户端/Online 模块或 NNERuntimeORT、NNEDenoiser、MsQuic 的情况。
