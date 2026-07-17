# 阶段 25：三维麻将牌模型、PBR 纹理与桌面布局

## 交付范围

- 使用本机 Blender 5.1.2 程序化生成单一可复用麻将牌低模。
- 标准源尺寸为 32 × 22 × 44 mm，底部中心枢轴，1.8 mm 四段倒角。
- 三个材质槽分别承载象牙牌身、动态牌面和绿色牌背。
- 27 张万/条/筒牌面由既有规则纹理生成不透明 3D 版本，保留原 UMG 纹理不变。
- 输出 `.blend`、FBX、GLB、USD 和 SHA-256 清单；UE 运行时使用 FBX 导入的静态网格。
- 桌面层按参考图布置本地明牌手牌、三家暗牌、四向弃牌区、副露区和双层牌墙。

## 源资产

- 生成脚本：`Scripts/Blender/GenerateMahjongTileAssets.py`
- 导入脚本：`Scripts/ImportMahjong3DAssets.py`
- Blender 工程：`SourceArt/3D/MahjongTiles/SM_MahjongTile.blend`
- 交换格式：`SM_MahjongTile.fbx`、`SM_MahjongTile.glb`、`SM_MahjongTile.usdc`
- PBR 纹理：`SourceArt/3D/MahjongTiles/Textures`
- 资产清单：`SourceArt/3D/MahjongTiles/MahjongTileAssetManifest.json`
- UE 静态网格：`/Game/Art/Mahjong/Tiles/SM_MahjongTile`

## 可复现命令

```powershell
& 'D:\Program Files\Blender Foundation\Blender 5.1\blender.exe' `
  --background --factory-startup `
  --python H:\MahjongGame\Scripts\Blender\GenerateMahjongTileAssets.py `
  -- H:\MahjongGame

& 'F:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  H:\MahjongGame\GuiyangMahjong.uproject `
  -unattended -nop4 -nosplash -nullrhi `
  -ExecutePythonScript=H:\MahjongGame\Scripts\ImportMahjong3DAssets.py
```

## 验证门禁

- Blender 日志必须包含 `MAHJONG_TILE_ASSETS_GENERATED` 和 `BLENDER_VERSION=5.1.2`。
- UE 导入日志必须包含 `verified mesh` 和 `MAHJONG_3D_IMPORT_OK`。
- UE 导入包围盒必须约为半尺寸 `(1.600, 1.150, 2.200)` cm。
- `GuiyangMahjong.UI.ThreeDTableLayout` 必须确认 `WBP_GameHUD` 内存在 1580 × 840 的 `UViewport`。
- Editor 编译、UI 生成命令和项目自动化测试全部成功后，才允许进入四客户端人工验收。

## 性能策略

- 108 张牌共享同一个静态网格，避免按牌面复制模型。
- 牌面仍由现有规则纹理映射，运行时只切换牌面图，不生成新网格。
- 网格禁用运行时碰撞；交互继续复用透明 UMG 点击层，移动端无需逐牌射线检测。
- 若导入资产缺失，代码回退到基础立方体，不中断房间状态同步。
