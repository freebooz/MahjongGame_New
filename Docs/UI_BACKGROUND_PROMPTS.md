# UI 背景生成提示词记录

执行模式：内置图像生成工具，`stylized-concept`。所有图片最终由 `Scripts/GenerateUIAssets.py` 重采样为 2560×1440。

## 共同提示词

为 Unreal Engine 5.8《贵阳捉鸡麻将》生成最终可用的 16:9 横屏 UMG 背景；现代国风与贵阳地域文化融合；使用 DeepTableGreen `#073F36`、PrimaryGreen `#176B5B`、JadeGreen `#42A58C`、WarmGold `#D9A441` 和少量 CreamWhite；中央主要内容区低细节，装饰限制在四周；无文字、字母、数字、按钮、面板、图标、人物、Logo、水印和随机符号；不复制商业游戏 UI；无颗粒、噪点、压缩伪影、过度锐化和模糊边缘。

## 页面差异

- `T_BG_Login_Guiyang`：甲秀楼、南明河、贵州山地云雾，宁静青绿夜景，中央 55% 留白。
- `T_BG_Lobby_JiaxiuTower`：甲秀楼与桥灯作为外侧焦点，中央和中下部留给大厅卡片。
- `T_BG_Room_GuiyangNight`：河畔亭台、窗格与灯笼构成社交房间氛围，四座位区域低细节。
- `T_BG_GameTable_GreenFelt`：近俯视深绿桌布、深木与暖金框，无遮挡中央牌桌，无麻将牌。
- `T_BG_Settlement_GuiyangRiver`：南明河蓝调时刻、远处甲秀楼和桥灯，中央留给结算面板。
- `T_BG_Rules_GuiyangCulture`：贵州山水、青岩建筑语言、蜡染和苗族银饰边缘装饰，中央 65% 米白低细节。
- `T_BG_Settings_GuiyangPattern`：深绿/靛蓝抽象山水与几何纹样，中央 70% 均匀低细节。
- `T_BG_Loading_GuiyangLandscape`：晨光中的喀斯特山地、河流和远处楼影，中央留给进度 UI。
