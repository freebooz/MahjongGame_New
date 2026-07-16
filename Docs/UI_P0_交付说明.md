# 贵阳捉鸡麻将 P0 UMG 交付说明

## 1. 交付结论

- 工程根目录：`H:\MahjongGame`。
- 已创建并编译 10 个 P0 Widget Blueprint，全部以 C++ Widget 类为父类。
- UMG 生成器已通过二次幂等执行验证：`10/10`、`0 error(s)`、`0 warning(s)`。
- 所有页面根节点统一为 `SafeZone_Root -> Scale_Design1920x1080 -> Canvas_Root`。
- 所有动态文字均为 `TextBlock` 或 `EditableTextBox`，没有文字烘焙图片。
- 所有可点击操作均使用 `Button`；按钮包含 Normal、Hovered、Pressed、Disabled 四态。
- 按钮和弹窗底图使用 `DrawAs=Box`、`Margin=0.25` 的九宫格 Brush，可直接替换为正式切图。
- UI 不写牌局权威状态，只调用 `AGuiyangMahjongPlayerController` 请求函数。
- 公共牌桌状态由 `AGuiyangMahjongGameState::PublicTableState` Replication 下发。
- 私有手牌、操作列表、结算和错误分别由所属 PlayerController 的 Client RPC 下发。
- PC 鼠标点击和 Android 触控共用 UMG Button 点击事件。
- 当前目录未发现 PNG、JPG、TGA、PSD、WebP 或 SVG 视觉源文件，因此没有导入外部纹理；当前颜色、牌面文字和图标槽为可运行占位表现，不冒充最终视觉稿。

## 2. 已创建 Widget 列表

| Widget | 资产路径 | C++ 基类 | 数据刷新/显示函数 | 点击事件 |
|---|---|---|---|---|
| WBP_ConnectServer | `/Game/UI/Screens/WBP_ConnectServer` | `UMobileConnectServerWidget` | `SetConnecting` | 连接服务器 |
| WBP_Lobby | `/Game/UI/Screens/WBP_Lobby` | `UMobileLobbyWidget` | `RefreshPlayerInfo` | 快速开始、创建房间、加入房间、设置 |
| WBP_Room | `/Game/UI/Screens/WBP_Room` | `UMobileRoomWidget` | `RefreshRoomState` | 准备、离开房间 |
| WBP_GameHUD | `/Game/UI/Screens/WBP_GameHUD` | `UMobileMahjongHUDWidget` | `RefreshTableState`、`RefreshPrivateHand` | 由手牌和操作面板承接 |
| WBP_HandTile | `/Game/UI/Components/WBP_HandTile` | `UMobileHandTileWidget` | `SetTile` | 首次选中、再次点击请求出牌 |
| WBP_DiscardTile | `/Game/UI/Components/WBP_DiscardTile` | `UMobileDiscardTileWidget` | `SetDiscard` | 无，弃牌只展示 |
| WBP_ActionButtonPanel | `/Game/UI/Components/WBP_ActionButtonPanel` | `UMobileActionButtonPanel` | `ShowActions` | 胡、杠、碰、过 |
| WBP_Settlement | `/Game/UI/Dialogs/WBP_Settlement` | `UMobileSettlementWidget` | `SetSettlementResult` | 再来一局、返回大厅 |
| WBP_ErrorToast | `/Game/UI/Components/WBP_ErrorToast` | `UMobileErrorToastWidget` | `ShowToast` | 无，定时隐藏 |
| WBP_ReconnectOverlay | `/Game/UI/Dialogs/WBP_ReconnectOverlay` | `UMobileReconnectOverlayWidget` | `RefreshReconnectState` | 重新连接、返回连接界面 |

## 3. 控件层级与关键命名

所有 Widget 都具有以下公共根结构：

```text
SafeZone_Root
└─ Scale_Design1920x1080（ScaleToFit）
   └─ Canvas_Root（1920x1080 设计坐标）
      └─ Background_ComponentSlot（可替换背景组件/九宫格 Brush）
```

### WBP_ConnectServer

```text
Canvas_Root
├─ Title
├─ Txt_ServerIP
├─ Txt_ServerPort
├─ Txt_PlayerName
├─ Chk_RememberAddress
├─ Btn_Connect
│  └─ Txt_ConnectButton
└─ Txt_Version
```

### WBP_Lobby

```text
Canvas_Root
├─ Txt_PlayerName
├─ Txt_PlayerId
├─ Txt_OnlineCount
├─ Btn_QuickStart
├─ Btn_CreateRoom
├─ Btn_JoinRoom
└─ Btn_Setting
```

### WBP_Room

```text
Canvas_Root
├─ Txt_RoomId
├─ Txt_RuleSummary
├─ Seat_Top / Seat_Left / Seat_Right / Seat_Bottom
├─ Btn_Ready
├─ Btn_LeaveRoom
└─ Txt_StartTip
```

### WBP_GameHUD

```text
Canvas_Root
├─ Txt_RoomId / Txt_RemainingTileCount / Txt_CurrentPhase
├─ Txt_CurrentTurnPlayer / Txt_Countdown
├─ Seat_Self / Seat_Top / Seat_Left / Seat_Right
├─ Panel_SelfHandTiles
├─ Panel_SelfDiscards / Panel_TopDiscards
├─ Panel_LeftDiscards / Panel_RightDiscards
├─ ActionButtonPanel（WBP_ActionButtonPanel 实例）
└─ PopupLayer（结算、错误、重连弹层）
```

### WBP_HandTile / WBP_DiscardTile

```text
WBP_HandTile:    Btn_Tile -> Txt_TileName
WBP_DiscardTile: Border_Tile -> Txt_TileName
```

### WBP_ActionButtonPanel

```text
Panel_Actions
├─ Btn_Hu
├─ Btn_Gang
├─ Btn_Peng
└─ Btn_Pass
```

### WBP_Settlement

```text
Panel_Dialog9Slice
└─ Panel_SettlementContent
   ├─ Txt_ResultTitle
   ├─ Txt_HuType
   ├─ Txt_JiResult
   ├─ Panel_PlayerScores
   ├─ Btn_NextRound
   └─ Btn_BackLobby
```

### WBP_ErrorToast / WBP_ReconnectOverlay

```text
WBP_ErrorToast:
└─ Border_Toast -> Txt_Message

WBP_ReconnectOverlay:
└─ Panel_Reconnect9Slice
   └─ Panel_ReconnectContent
      ├─ Txt_ReconnectStatus
      ├─ Txt_RemainingTime
      ├─ Btn_Reconnect
      └─ Btn_BackConnect
```

## 4. 控件绑定清单

Blueprint 不重复实现业务逻辑。控件名与 C++ `UPROPERTY(meta=(BindWidget))` 同名，由 Blueprint 编译器自动绑定；本次生成命令在保存前还会按属性名称和类型进行显式校验。

- ConnectServer：`Txt_ServerIP`、`Txt_ServerPort`、`Txt_PlayerName`、`Chk_RememberAddress`、`Btn_Connect`、`Txt_ConnectButton`、`Txt_Version`。
- Lobby：`Btn_QuickStart`、`Btn_CreateRoom`、`Btn_JoinRoom`、`Btn_Setting`、`Txt_PlayerName`、`Txt_PlayerId`、`Txt_OnlineCount`。
- Room：`Txt_RoomId`、`Txt_RuleSummary`、四个 `Seat_*`、`Btn_Ready`、`Btn_LeaveRoom`、`Txt_StartTip`。
- GameHUD：五个状态 `Txt_*`、四个 `Panel_*Discards`、`Panel_SelfHandTiles`、四个 `Seat_*`、`ActionButtonPanel`、`PopupLayer`。
- HandTile：`Btn_Tile`、`Txt_TileName`。
- DiscardTile：`Border_Tile`、`Txt_TileName`。
- ActionButtonPanel：`Btn_Hu`、`Btn_Gang`、`Btn_Peng`、`Btn_Pass`。
- Settlement：`Txt_ResultTitle`、`Txt_HuType`、`Txt_JiResult`、`Panel_PlayerScores`、`Btn_NextRound`、`Btn_BackLobby`。
- ErrorToast：`Border_Toast`、`Txt_Message`。
- ReconnectOverlay：`Txt_ReconnectStatus`、`Txt_RemainingTime`、`Btn_Reconnect`、`Btn_BackConnect`。

## 5. C++ 与 UMG 数据流

```text
服务端权威牌桌流程
  -> AGuiyangMahjongGameState::SetPublicTableStateAuthority
  -> PublicTableState Replication / OnRep_PublicTableState
  -> WBP_GameHUD::RefreshTableState

服务端私有数据
  -> Client_UpdatePrivateHand
  -> OnPrivateHandUpdated
  -> WBP_GameHUD::RefreshPrivateHand
  -> 动态创建 WBP_HandTile

服务端可操作列表
  -> Client_ShowAvailableActions
  -> OnAvailableActionsUpdated
  -> WBP_ActionButtonPanel::ShowActions

服务端结算
  -> Client_ShowSettlement
  -> OnSettlementShown
  -> PopupLayer 动态创建 WBP_Settlement

服务端错误
  -> Client_ShowErrorMessage
  -> OnErrorShown
  -> PopupLayer 动态创建 WBP_ErrorToast

UMG Button 点击
  -> Widget Handle* 函数
  -> AGuiyangMahjongPlayerController::Server_Request*
  -> 服务端校验、执行、更新 GameState/Client RPC
```

UI 只保存显示所需的短期数据，例如当前可操作列表、选中手牌和 Toast 计时器；不保存或改写权威房间、牌墙、回合、分数状态。

## 6. 已导入 UI 资产列表

### 已生成的 UE 资产

- `Content/UI/Screens/WBP_ConnectServer.uasset`
- `Content/UI/Screens/WBP_Lobby.uasset`
- `Content/UI/Screens/WBP_Room.uasset`
- `Content/UI/Screens/WBP_GameHUD.uasset`
- `Content/UI/Components/WBP_HandTile.uasset`
- `Content/UI/Components/WBP_DiscardTile.uasset`
- `Content/UI/Components/WBP_ActionButtonPanel.uasset`
- `Content/UI/Components/WBP_ErrorToast.uasset`
- `Content/UI/Dialogs/WBP_Settlement.uasset`
- `Content/UI/Dialogs/WBP_ReconnectOverlay.uasset`

### 外部纹理资产

当前为 **0 个**。原因是 `H:\MahjongGame` 内没有视觉稿源图片，当前会话也没有收到可读取的设计图附件。正式切图到位后按以下规则导入：

- 所有 UI Texture：`Texture Group = UI (UserInterface2D)`。
- 图标、牌面、按钮：关闭有损压缩，优先无损 UI 压缩；根据像素稿决定是否关闭 MipMap。
- 大背景可单独评估尺寸和压缩；按钮、面板、头像框必须保留透明边缘。
- 面板和按钮底图设置 `Draw As = Box`，按设计标注填写 Margin。
- 文案不进入图片，统一保留为 UMG TextBlock。

## 7. PC 客户端 UI 测试步骤

1. 用 UE 5.8 打开 `H:\MahjongGame\GuiyangMahjong.uproject`。
2. 在 Widget Blueprint 编辑器逐个打开 10 个 WBP，确认父类、控件树和编译状态均正常。
3. 以 1920x1080 启动 Standalone Client，检查连接、Lobby、Room、HUD、结算、错误和重连层。
4. 在连接页输入 IP、端口、中文昵称，点击连接；确认按钮进入 Disabled 状态并显示“连接中……”。
5. 验证大厅四个按钮、房间两个按钮、操作面板四个按钮、结算两个按钮、重连两个按钮均能产生中文日志。
6. 用 2 至 4 客户端 PIE + Dedicated Server 验证：公共状态对所有客户端一致，私有手牌只到所属客户端。
7. 注入 `Client_ShowAvailableActions`，确认只显示服务器提供的胡/杠/碰/过选项。
8. 注入 `Client_ShowSettlement` 和 `Client_ShowErrorMessage`，确认弹层从 `PopupLayer` 创建。
9. 依次用 1920x1080、1280x720、2400x1080、2340x1080 窗口检查：不裁切、按钮可点、文字不溢出、超宽屏内容保持在 SafeZone 内。
10. 查看 `Saved/Logs/GuiyangMahjong.log`，确认 UI 与网络日志为中文。

## 8. Android UI 适配步骤

1. 安装 UE 5.8 要求的 Android SDK/NDK；当前环境平台验证报告为 `Android INVALID r27c`，需先修复 SDK 工具链才能打包。
2. 保留所有 WBP 的 `SafeZone_Root`，不要将控件移出安全区。
3. 使用项目已配置的 `ShortestSide` DPI 曲线；720 短边为 0.666、1080 短边为 1.0。
4. 在 Android Preview 分别选择 2400x1080 和 2340x1080，并检查刘海、圆角和手势导航区。
5. 触控目标建议不小于 44x44 逻辑像素；关键牌面和操作按钮应留出手指间距。
6. 在真机验证单击、快速连击、断线重连、前后台切换和横屏恢复。
7. 导入正式贴图后确认所有 UI Texture 为 UserInterface2D，小图标和按钮未使用有损压缩。
8. 使用 Profile/Shipping 包检查纹理内存、Slate Batch、字体回退和中文字符完整性。

## 9. 验证记录

- `GuiyangMahjongEditor Win64 Development`：编译成功。
- 自动化测试：7 成功、0 失败、0 警告。
- UMG 二次生成：10/10 成功、0 错误、0 警告。
- 自动化测试报告：`Saved/Automation/All/index.html`。
- UMG 生成日志：`Saved/Logs/GuiyangMahjong.log`。

## 10. 重新生成 UMG 资产

```powershell
F:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe `
  H:\MahjongGame\GuiyangMahjong.uproject `
  -run=GenerateMahjongUI -unattended -nop4 -nosplash -nullrhi
```

生成器源码：`Source/GuiyangMahjong/Private/Editor/GenerateMahjongUICommandlet.cpp`。
