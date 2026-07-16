# 阶段 0/1：新工程基线与规则核心

更新时间：2026-07-15

## 工程边界

- 当前唯一开发根目录：`H:\MahjongGame`
- Unreal 工程：`H:\MahjongGame\GuiyangMahjong.uproject`
- Unreal Engine：`F:\UnrealEngine-5.8.0-release`
- 旧工程 `H:\MahjongGame_Test` 仅保留作历史参考，本轮没有写入、移动或删除。
- 当前目录已经承载此前的新工程重建成果，因此不再创建第三份重复工程，也不把代码切回旧目录。

## Git/LFS 基线

- 新增 Unreal 生成目录忽略规则，避免提交 `Binaries`、`Intermediate`、`DerivedDataCache`、`Saved`。
- Unreal 资产、贴图、DCC 文件和音视频通过 Git LFS 跟踪。
- 本机已检测到 Git 2.54.0 与 Git LFS 3.7.1。
- 初始提交需在确认作者身份与待提交资产范围后执行；本轮不擅自把已有用户成果提交为单一无来源提交。

## GuiyangMainstreamV1 规则决策

- 默认牌集：万、条、筒 1-9，每种 4 张，共 108 张。
- 兼容牌集：`Standard136`，在 108 张基础上增加东南西北、中发白 28 张。
- `FMahjongRuleConfig` 新增 `RuleId`、`RuleVersion`、`TileSetMode`。
- 房间配置通过 `FGuiyangRuleSnapshot` 规范化并生成 SHA-1 哈希；相同配置产生相同定义和哈希。
- 快照校验同时验证规范定义与哈希，开局、重连恢复、回放读取时可拒绝被修改的数据。
- 数值规范范围：底分 1-100，鸡分/杠分 0-100，倍率 1-16，重连窗口 15-600 秒。

## 验证证据

Editor 构建命令：

```powershell
F:\UnrealEngine-5.8.0-release\Engine\Build\BatchFiles\Build.bat GuiyangMahjongEditor Win64 Development H:\MahjongGame\GuiyangMahjong.uproject -WaitMutex -NoHotReloadFromIDE
```

结果：`Succeeded`，2026-07-15 实测。

自动化命令：

```powershell
F:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe H:\MahjongGame\GuiyangMahjong.uproject -unattended -nop4 -nullrhi "-ExecCmds=Automation RunTests GuiyangMahjong; Quit" "-TestExit=Automation Test Queue Empty" -log
```

结果：12/12 Success，包含：

- `GuiyangMahjong.Core.Deck.Default108`
- `GuiyangMahjong.Core.Deck.Optional136`
- `GuiyangMahjong.Core.Deck.ShuffleAndDeal`
- `GuiyangMahjong.Rules.SnapshotDeterminism`
- 原有胡牌、七对、碰杠、鸡牌、零和计分、登录生命周期与凭证持久化安全测试

## 已知环境缺口

- Win64 SDK 有效。
- Android SDK 当前被 UE 5.8 判定为 `INVALID r27c`，APK/真机阶段开始前必须修复；本轮不伪报 Android 构建通过。
- 当前自动化使用 NullRHI，不能替代 UI 像素级截图验收。

## 下一阶段入口

1. 将规则快照接入服务端 RoomManager，使房间创建后只能读取、不能原地改写规则。
2. 实现 6 位房间号、6-12 位密码盐化摘要、常量时间比较、失败限流和生命周期清理。
3. 再将快照传入 Table/Turn/ActionResolver，所有出牌与结算只接受服务端权威序列。
