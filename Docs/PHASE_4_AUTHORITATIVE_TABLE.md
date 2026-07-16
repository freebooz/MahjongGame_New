# 阶段 4：服务端权威牌桌状态机

更新时间：2026-07-16

## 已实现

- `UMahjongTableEngine` 只在服务端创建并持有完整牌墙和四家手牌。
- 从房间锁定的 `FGuiyangRuleSnapshot` 初始化默认 108 张或可选 136 张牌墙。
- 固定种子 Fisher-Yates 洗牌，可在自动化、回放和故障定位中复现。
- 庄家 14 张、闲家 13 张；无人声明时按座位轮转并由服务端摸牌。
- 请求必须匹配 `RoundId`、`TurnId` 和单调递增的 `ClientSequence`。
- 服务端维护独立 `ServerActionSequence`，拒绝请求不会推进该序号。
- 出牌只接受当前座位真实手牌中的 `UniqueId`，客户端不能凭空构造牌。
- 出牌后服务端计算其他座位的胡、明杠、碰候选；无候选时自动进入下一家回合。
- 有候选时等待所有相关座位响应，优先级为胡 > 明杠 > 碰 > 过。
- 默认开启一炮多响时保留全部合法胡家；关闭时选择出牌者后的最近胡家。
- 碰/明杠从服务端手牌移除实际牌，生成公开副露并标记弃牌已被声明。
- GameMode 在第四人准备后启动牌桌，将房间切换到 `Playing`。
- GameState 仅复制公共状态；每个 PlayerController 只接收所属座位的私有手牌。

## 安全边界

- 公共状态包含牌数、弃牌、副露、当前座位、剩余牌数和赢家座位，不包含任何手牌数组或牌墙顺序。
- `Server_RequestAction` 仅传递动作意图，最终合法性和优先级均由服务端决定。
- 旧 `Server_RequestPlayTile` 作为 UI 兼容入口保留，但也会转换为请求并进入同一个权威引擎。
- 测试专用手牌注入函数只在 `WITH_DEV_AUTOMATION_TESTS` 下编译，不是 Blueprint/RPC 接口。

## 验证

- `GuiyangMahjongEditor Win64 Development`：Succeeded。
- `GuiyangMahjong`：16/16 Success。
- 新增自动化：
  - `GuiyangMahjong.Table.AuthoritativeTurnAndReplayGuard`
  - `GuiyangMahjong.Table.ReactionPriorityAndMultiHu`
- 覆盖固定种子牌序、初始牌数、出牌/摸牌轮转、重复请求、伪造牌 ID、候选动作、胡牌优先级和一炮多响。

## 尚未完成

- 暗杠、补杠、抢杠胡以及杠后补牌的完整地方规则结算。
- 自摸检测、流局、鸡牌翻取、冲锋鸡/责任鸡/乌骨鸡和最终积分接入牌桌状态机。
- 操作倒计时、超时自动过/自动出牌、掉线托管。
- 断线重连的 Round/Turn/ActionSequence 恢复和未决反应窗口恢复。
- 下一局、总局数结束、房间关闭和战绩持久化。

## 下一阶段

1. 将胡牌和流局接入 `MahjongScoreCalculator` 与鸡牌计算器。
2. 实现暗杠、补杠、抢杠胡和杠后补牌响应链。
3. 增加服务端操作计时器与断线重连快照。
