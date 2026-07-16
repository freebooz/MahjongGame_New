# 阶段 12：本地战绩持久化

## 本阶段交付

- 房间创建时生成不可变 `MatchId`，并同步到最终大结算。
- 新增 `FMahjongMatchHistoryRecord`：比赛 ID、记录时间和最终公开结算。
- 新增 `UMahjongMatchHistorySaveGame`：仅保存战绩记录数组。
- 新增 `UGuiyangMatchHistorySubsystem`：加载、记录、查询和清空本地战绩。
- 客户端收到最终大结算时自动记录战绩。
- 重复广播或重连重复收到同一最终结算时按 `MatchId` 去重。
- 最多保留最近 50 场，最新记录位于数组首位。

## 保存内容

- 比赛 ID。
- 房间号。
- 完成局数。
- 排名、座位、玩家昵称和累计总分。
- 客户端记录的 UTC 时间。

明确不保存：

- 任何玩家手牌。
- 弃牌和操作回放。
- 房间密码。
- 登录 Token、重连凭据或其摘要。
- 服务端会话 ID。

## 客户端流程

```text
Client_ShowFinalSettlement
  -> MatchHistorySubsystem::RecordFinalSettlement
  -> 检查 MatchId
  -> 插入最新记录
  -> 限制为 50 条
  -> SaveGameToSlot
  -> OnHistoryChanged
```

蓝图可以调用：

- `GetRecords`
- `ClearHistory`
- `RecordFinalSettlement`

因此大厅战绩列表可以直接绑定该子系统，无需读取文件或解析 JSON。

## 一致性与失败处理

- `MatchId` 在房间创建时生成，整个多局比赛保持不变。
- 同一比赛只允许一条本地记录。
- 写盘失败时恢复修改前的内存记录，避免内存和磁盘状态不一致。
- 清空操作同时更新 SaveGame 并广播历史变化事件。

## 验证结果

- UE 5.8 Editor Development 构建成功。
- `GuiyangMahjong` 自动化测试 26 项全部成功。
- 新增 `GuiyangMahjong.History.FinalSettlementPersistence`，验证：
  - 最终结算成功写入。
  - 重复比赛 ID 被忽略。
  - GameInstance 重建后可重新加载。
  - 比赛 ID 和记录内容保持一致。
  - 清空历史后正确写盘。
- 安全测试确认战绩 SaveGame 不声明手牌或敏感凭据字段。

## 下一阶段

- 大厅战绩列表和详情 UI。
- 战绩分享图片。
- 后端账号战绩同步及跨设备查询。
