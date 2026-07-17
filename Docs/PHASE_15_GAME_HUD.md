# 阶段 15：牌桌 GameHUD 完整状态展示

## 完成内容

- HUD 根据本地玩家座位旋转四方显示，本地玩家始终位于底部。
- 显示房间号、剩余牌数、牌局阶段、当前行动玩家和服务端倒计时。
- 四方玩家区域显示昵称、手牌数量、分数、在线状态和当前行动标记。
- 四方弃牌池按相对座位渲染，已被碰杠的弃牌不再显示，最新有效弃牌高亮。
- 四方副露区域显示吃、碰、明杠、暗杠和补杠；暗杠使用公开状态中的背面占位牌，不泄露牌面。
- 显示翻鸡牌和冲锋鸡、责任鸡事件。
- 仅在本地玩家的出牌回合启用手牌点击。
- 手牌采用二次确认交互：首次点击抬起并选中，切换牌时自动取消旧选择，再次点击已选牌才提交出牌请求。
- 私有状态首次确定本地座位时立即重绘公共牌桌，避免等待下一次公共状态广播。

## 状态来源

- 公共牌桌、座位、弃牌、副露、鸡牌事件：`AGuiyangMahjongGameState::PublicTableState`。
- 本地座位和私有手牌：所属 `AGuiyangMahjongPlayerController` 的私有状态事件。
- 房间号：`AGuiyangMahjongGameState::RoomState`。
- 倒计时：服务端截止时间与同步后的服务器世界时间差值。

## 验证结果

- `GuiyangMahjongEditor Win64 Development`：编译成功。
- UMG 第二次幂等生成：17/17，0 error，0 warning。
- `GuiyangMahjong` 自动化：29/29 Success。
- 新增测试：`GuiyangMahjong.UI.HudSeatMapping`。
- 自动化报告：`Saved/Automation/Phase15GameHUD/index.html`。

## 下一阶段

- 将重连状态接入 `WBP_ReconnectOverlay`，完成剩余时间、重试和返回连接页交互。
- 增加实际 UMG 页面导航、点击和多分辨率截图自动化。
- 在 Android DeviceProfile/Cook 环境验证触控热区、字体和背景显存。
