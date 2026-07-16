# UI 九宫格规范

| 资源 | Margin(px) | Normalized | 推荐尺寸范围 | Draw As |
|---|---:|---:|---|---|
| Main GreenGold | 40 | 0.1563 | 420×240–1600×900 | Box |
| Dialog CreamGold | 44 | 0.1719 | 520×360–1400×980 | Box |
| Dialog DarkGreen | 44 | 0.1719 | 520×360–1400×980 | Box |
| PlayerInfo | 36 | 0.1406 | 280×120–720×280 | Box |
| RoomRule | 34 | 0.1328 | 360×180–960×720 | Box |
| Toast | 30 | 0.1172 | 320×80–1000×220 | Box |
| ScoreRow | 28 | 0.1094 | 420×72–1100×140 | Box |
| InputBox | 28 | 0.1094 | 280×72–900×120 | Box |
| Tab | 28 | 0.1094 | 160×64–480×112 | Box |
| Notice | 30 | 0.1172 | 420×120–1400×320 | Box |
| NetworkStatus | 26 | 0.1016 | 180×52–520×96 | Box |

原图透明，四角不进入拉伸区。导入脚本把 Margin 写入 `/Game/UI/Data/DA_PanelStyles`，并在回读阶段核对纹理存在与 UI 压缩设置。
