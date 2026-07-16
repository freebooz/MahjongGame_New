# 阶段 13：UI 主流程 P0 修复

## 完成内容

- RootHUD 已订阅登录、房间复制状态和重连快照。
- 页面可按房间生命周期自动切换：登录、大厅、房间、牌局。
- 登录完成后会向服务端提交当前 Session 认证。
- 手牌与胡、碰、杠、过统一通过 PlayerController 生成请求。
- 请求统一携带当前 `RoundId`、`TurnId` 和单调递增的 `ClientSequence`。
- 创建房间改为使用 `WBP_CreateRoomDialog`，支持局数、公开房和可选密码。
- 加入房间改为使用 `WBP_JoinRoomDialog`，支持房间号和可选密码。
- 快速开始会优先加入可用公开房；没有可用房间时自动创建公开房。
- 结算“返回大厅”会先请求离开房间，再由 RootHUD 路由回大厅。

## 新增资产

- `/Game/UI/Dialogs/WBP_CreateRoomDialog`
- `/Game/UI/Dialogs/WBP_JoinRoomDialog`

## 验证结果

- `GuiyangMahjongEditor Win64 Development`：编译成功。
- UMG 幂等生成：15/15，0 error，0 warning。
- `GuiyangMahjong` 自动化：27/27 Success。
- 自动化报告：`Saved/Automation/Phase13UIFlowFinal/index.html`。

## 下一阶段

- 创建 `WBP_RuleConfig` 和 `WBP_RuleSummary`。
- 完成 GameHUD 弃牌、副露、鸡牌事件和本地座位旋转。
- 接入重连遮罩、断线倒计时和返回连接页。
- 增加 UMG 页面导航与点击自动化。
