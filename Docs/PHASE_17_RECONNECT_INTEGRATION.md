# 阶段 17：四客户端断线重连集成验证

## 完成内容

- 新增 `Scripts/RunReconnectIntegration.ps1`，自动拉起一个 Editor Dedicated Server 和四个独立客户端进程。
- 四个客户端使用确定性的集成测试身份完成登录、快速匹配和准备，服务器实际开始四人牌局。
- 0 号客户端在收到私有手牌后主动关闭网络连接，随后通过现有重连子系统重新连接同一端点。
- 服务端按玩家身份恢复原座位并重新发送公共牌桌、私有手牌和当前回合快照。
- 脚本逐项验证开桌、四份私有状态、断线、服务端恢复和客户端恢复标记；任一条件不满足都会返回失败。
- 每次运行均在 `Saved/Integration/Phase17Reconnect/<RunId>/` 归档 `result.json`、服务端日志和四份客户端日志。

## 安全边界

- 集成登录和主动断线入口只在非 Shipping 构建中编译生效。
- 入口还必须显式携带 `-MahjongEnableIntegrationHooks`，普通开发运行默认拒绝调用。
- 集成身份限定为 `integration-client-*`；不会写入登录 SaveGame，也不会覆盖真实用户资料。
- 新增自动化测试 `GuiyangMahjong.Security.IntegrationHooksDisabledByDefault`，锁定“未传开关时入口不可用”的默认行为。

## 验证结果

- `GuiyangMahjongEditor Win64 Development`：编译成功。
- 四客户端集成运行：通过，耗时 16.38 秒，端口 17778。
- 服务端恢复证据：`Player=integration-client-0 Seat=0 Online=4 Hand=14 Round=1 Remaining=119`。
- 客户端恢复证据：`Client=0 Seat=0 Hand=14 Round=1 Remaining=119`。
- 证据索引：`Saved/Integration/Phase17Reconnect/20260717-100028/result.json`。
- `GuiyangMahjong` 自动化：31/31 Success。
- 自动化报告：`Saved/Automation/Phase17ReconnectIntegration/index.html`。

## 运行命令

```powershell
PowerShell -ExecutionPolicy Bypass -File .\Scripts\RunReconnectIntegration.ps1
```

可通过 `-Port`、`-StartupTimeoutSeconds`、`-TestTimeoutSeconds` 和 `-OutputRoot` 调整运行环境。

## 尚未覆盖

- 当前证据使用 Editor Dedicated Server 模式；独立 Game/Server Target 的编译、Cook 和打包运行仍待验证。
- 网络切换、进程被杀、Android 前后台切换等平台级中断仍需真机覆盖。
- UI 截图捕获环境仍输出黑图，下一阶段修复可见渲染截图与多分辨率验收。
