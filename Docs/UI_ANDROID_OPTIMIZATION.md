# Android UI 优化

- 背景：设备配置最大 2048，允许流送；低端档可降到 1600×900。
- 24 个图标后续合并为 1024×1024 图集；当前独立源图用于审查和稳定命名。
- 27 张牌面后续合并为 2048×2048 图集，UV 映射由 RuleIndex 0–26 派生。
- 面板和按钮 NoMipmaps/NeverStream；背景保留 Mip 并可流送。
- UI Material 不用 SceneTexture；Pulse/Glow 在 Android 采用静态或低频参数更新。
- 目标 UI 常驻显存预算：高档约 48 MiB，中档约 28 MiB，低档约 18 MiB；最终以 Cook 后 ASTC 数据复核。
- 设计基准 1920×1080，宽屏测试 2400×1080 与 2340×1080；根节点必须使用 SafeZone。
