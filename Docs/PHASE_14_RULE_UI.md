# 阶段 14：规则配置与规则摘要 UI

## 完成内容

- 新增 `/Game/UI/Components/WBP_RuleConfig`。
- 新增 `/Game/UI/Components/WBP_RuleSummary`。
- 原始需求中的 16 个强制 Widget 已全部存在：16/16。
- 创建房间弹窗已嵌入规则配置和实时规则摘要。
- 创建请求会提交 UI 中选择的牌制、鸡牌、胡牌、分数和超时参数。
- 创建房间前会展示规则版本、规则明细和规则哈希。
- 房间页面会根据服务端下发的不可变规则快照显示相同摘要和哈希。

## 当前可配置字段

- 108 张万筒条 / 136 张标准牌。
- 冲锋鸡、责任鸡、乌骨鸡。
- 抢杠胡、一炮多响、七对。
- 超时自动托管。
- 底分、鸡分、豆分、自摸倍数。
- 出牌超时、操作超时、重连时限。

UI 只暴露当前 `FMahjongRuleConfig` 和服务端规则引擎真实支持的字段，不显示尚未进入规则模型的伪配置。

## 验证结果

- `GuiyangMahjongEditor Win64 Development`：编译成功。
- UMG 幂等生成：17/17，0 error，0 warning。
- 强制 Widget 清单：16/16。
- `GuiyangMahjong` 自动化：28/28 Success。
- 新增测试：`GuiyangMahjong.UI.RuleSummaryConsistency`。
- 自动化报告：`Saved/Automation/Phase14RuleUI/index.html`。

## 下一阶段

- 完成 GameHUD 弃牌、副露、鸡牌事件和本地座位旋转。
- 根据当前回合禁用非当前玩家手牌。
- 接入重连遮罩、倒计时和返回连接页。
- 增加实际 UMG 导航与点击自动化。
