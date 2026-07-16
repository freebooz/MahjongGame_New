# UI 纹理导入规范

`Scripts/ImportUIAssets.py` 是幂等导入入口。

小图标、按钮、面板、控件、头像和麻将牌：Compression=`UserInterface2D`，LOD Group=`UI`，sRGB=true，NoMipmaps，NeverStream=true。

背景：Compression=`UserInterface2D`，LOD Group=`UI`，sRGB=true，保留纹理组 Mip，NeverStream=false。

执行：

```powershell
F:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe H:\MahjongGame\GuiyangMahjong.uproject -unattended -nop4 -nosplash -nullrhi -ExecutePythonScript=H:\MahjongGame\Scripts\ImportUIAssets.py
```

Android Cook 中背景 MaxTextureSize 建议 2048；图标与牌面后续合图集，不改变逻辑注册表键。
