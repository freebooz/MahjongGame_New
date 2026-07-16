# 阶段 8：多局房间生命周期与累计分

## 本阶段交付

- 单局结算回写：房间管理器只接受四个唯一座位且总变化为零的权威结算。
- 累计分：每局 `TotalDelta` 累加到 `FMahjongSeatInfo.Score`，下一局发牌时作为当前分数基线。
- 庄家状态：`FMahjongRoomInfo.DealerSeat` 对所有客户端公开。
- 默认首庄：房主所在的 0 号座位。
- 默认后续庄家：胡牌者坐庄；原庄胡牌自然连庄；一炮多响使用权威结算中的首位赢家。
- 黄庄策略：`bDrawGameDealerContinues` 写入规则快照，默认庄家连庄，关闭时顺移一位。
- 下一局确认：非末局结算后进入 `WaitingNextRound`，四位玩家全部确认后进入 `Starting`。
- 末局封盘：达到配置局数后进入最终 `Settlement`，拒绝继续开局。
- GameMode 集成：结算只回写一次，重置四家准备状态，并在四人确认后复用牌桌引擎开始下一局。
- 防重放：复用同一牌桌引擎，`RoundId` 跨局递增，上一局请求会被公共校验拒绝。

## 生命周期

```text
Starting
  -> Playing
  -> WaitingNextRound（尚有剩余局数）
  -> Starting（四人确认）

Playing
  -> Settlement（达到总局数）
```

## 服务端约束

- `FinishRound` 只允许在 `Playing` 调用。
- 结算必须恰好包含座位 0 至 3，且不得重复。
- 四家 `TotalDelta` 总和必须为零，否则整笔结算拒绝且房间分数不变。
- 同一局完成后房间不再处于 `Playing`，因此重复回写会被拒绝。
- `RequestNextRound` 只允许在 `WaitingNextRound` 调用，末局完成后始终拒绝。
- `GameMode` 使用牌桌状态序号防止同一结算被重复广播或重复入账。

## 验证结果

- UE 5.8 Editor Development 构建成功。
- `GuiyangMahjong` 自动化测试 24 项全部成功。
- 新增 `GuiyangMahjong.Room.MultiRoundLifecycle`，覆盖两局完整房间流程。
- `AuthoritativeTurnAndReplayGuard` 新增跨局 `RoundId` 单调递增和旧请求拒绝断言。

## 下一阶段

- 最终大结算专用数据结构和排行榜 UI。
- 玩家掉线后的座位保留、重连快照和响应窗口恢复。
- 操作倒计时、自动过牌和托管出牌。
