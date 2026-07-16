# 阶段 5：权威结算与当前回合动作

## 本阶段交付

- `UMahjongTableEngine::SubmitTurnAction`：服务器验证当前座位、局号、回合号、客户端序号和候选动作。
- 自摸：服务器重新校验胡牌结构，生成权威结算并进入 `Settlement`。
- 暗杠：服务器校验四张同牌、移入副露、计算杠分、补牌并保持当前玩家回合。
- 点炮与一炮多响：统一由 `CalculateWins` 计算，所有赢家均进入结算结果。
- 流局：牌墙耗尽时生成明确的 `bDrawGame` 结果，并保留已产生的杠分。
- 基础鸡：结算时按各玩家手牌中的一条计算基础鸡差额。
- 结算广播：`GameMode` 只在新结算状态序号出现时向四位玩家发送一次 `Client_ShowSettlement`。

## 隐私边界

- 真实手牌和真实暗杠牌面只存在于服务器及所属玩家的私有快照。
- 公共暗杠副露仅复制类型、来源座位和四张无效牌占位，客户端可渲染牌背但不能推断牌面。
- 结算分数、赢家和牌局阶段只能由服务器生成，客户端请求不能直接指定分数。

## 计分约束

- 自摸：其余三家分别向赢家支付 `BaseScore * ZiMoMultiplier`。
- 点炮：放炮者向每位赢家支付 `BaseScore * DianPaoMultiplier * 3`。
- 鸡分：四家按鸡数两两计算差额，严格零和。
- 暗杠和明杠：其余三家分别支付 `GangScore`，严格零和。
- `FMahjongSettlementResult.WinningSeats` 保存全部赢家；`WinnerSeat` 保留首位赢家以兼容现有 UI。

## 自动化验证

本阶段完整运行 `GuiyangMahjong` 测试集，共 19 项全部通过。新增覆盖：

- `GuiyangMahjong.Table.AuthoritativeSelfDraw`
- `GuiyangMahjong.Table.AuthoritativeConcealedGang`
- `GuiyangMahjong.Rules.MultiWinScoreZeroSum`

编辑器 Development 构建通过。

## 下一阶段

- 补杠及抢杠胡响应窗口。
- 翻鸡牌的服务端抽取、公开快照和结算计数。
- 冲锋鸡、责任鸡、乌骨鸡的事件记录与规则开关。
- 多局房间总分回写、下一局庄家轮转及最终大结算。
