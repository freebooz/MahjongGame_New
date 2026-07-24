# 麻将房间运行时灯光修改研究与执行计划

日期：2026-07-24  
适用范围：Unreal Engine 5.8，Windows Client、Android Client、Linux/Windows Dedicated Server

## 1. 结论

当前问题不是 `MahjongRoomVisualPreviewMap` 没有保存灯光，而是运行时不加载该预览关卡。客户端进入 `MahjongRoomMap` 后，使用 `AMahjongRoomPresentationActor::StaticClass()` 动态创建原生 C++ 默认实例，因此：

- 预览关卡中新加的 Directional Light、Sky Light、Spot Light 不会进入运行时世界。
- 对预览关卡中已放置 Actor 所做的实例覆盖，不会自动写回 C++ Class Default Object。
- 运行时最终只得到 C++ 构造函数中的两个低强度 Spot Light。
- 摄像机固定手动曝光且 `AutoExposureBias=-0.7`，会继续压低最终亮度。
- Android 工程没有显式声明移动本地灯光支持策略；移动 Forward 路径下动态 Spot Light 可能因 Shader Permutation/设备路径不同而不渲染或表现不一致。

推荐结构是：

1. 保持 `MahjongRoomMap` 为客户端/服务器共享、无客户端视觉资源的网络关卡。
2. 使用客户端专属 `BP_MahjongRoomPresentation` 作为桌面、摄像机、灯光的唯一视觉源。
3. 预览关卡放置同一个蓝图；人工调整必须写入蓝图类默认值，不保留关卡实例覆盖。
4. 客户端通过 `TSoftClassPtr` 和异步加载生成该蓝图。
5. 用客户端专属 Primary Asset Label 或 Asset Manager 规则确保蓝图及依赖只进入客户端 Cook。
6. Dedicated Server 继续只 Cook `MahjongRoomMap`，不加载客户端模块、预览关卡或灯光资源。

## 2. Epic 官方依据

- Epic 建议将设计师可编辑的默认属性保存在 Blueprint 派生类中；加载该 Blueprint 后，其默认属性和引用资源会随类一起使用：[Referencing Assets in Unreal Engine 5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/referencing-assets-in-unreal-engine)
- `TSoftClassPtr`/软引用适合避免不必要的硬依赖，并允许按需加载类；异步加载可避免同步加载造成的帧卡顿：[Referencing Assets](https://dev.epicgames.com/documentation/en-us/unreal-engine/referencing-assets-in-unreal-engine)、[Asynchronous Asset Loading](https://dev.epicgames.com/documentation/unreal-engine/asynchronous-asset-loading-in-unreal-engine)
- Asset Manager 和 Primary Asset Label 可明确控制资产 Cook、依赖归属和 Chunk，并可用 Asset Audit、Size Map、Reference Viewer 检查结果：[Asset Management](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine)、[Cooking Content and Creating Chunks](https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-and-creating-chunks-in-unreal-engine)
- Dedicated Server 是无渲染的独立目标，应分别构建和 Cook Server/Client 内容：[Setting Up Dedicated Servers in Unreal Engine 5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine)
- 灯光 Mobility 必须按是否动态生成及目标平台选择；Movable 灯可以运行时生成，但成本取决于影响范围和阴影：[Light Types and Their Mobility](https://dev.epicgames.com/documentation/unreal-engine/light-types-and-their-mobility-in-unreal-engine)、[Movable Light Mobility](https://dev.epicgames.com/documentation/en-us/unreal-engine/movable-light-mobility-in-unreal-engine)
- Epic 建议使用物理灯光单位和 EV100/曝光体系，避免依赖没有语义的“魔法强度值”：[Physical Lighting Units](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-physical-lighting-units-in-unreal-engine)、[Auto Exposure](https://dev.epicgames.com/documentation/unreal-engine/auto-exposure-in-unreal-engine)
- 移动端应先确定 Lighting Tier；动态本地灯光需要对应的移动 Shader Permutation，且 PC 与 Android 必须分别验证：[Performance Guidelines for Mobile Devices](https://dev.epicgames.com/documentation/en-us/unreal-engine/performance-guidelines-for-mobile-devices-in-unreal-engine)、[Mobile Rendering Features](https://dev.epicgames.com/documentation/en-us/unreal-engine/mobile-rendering-features-in-unreal-engine)

## 3. 当前工程证据

- 实际客户端关卡检测：`Source/GuiyangMahjongClient/Private/Game/GuiyangClientControllerBridgeImpl.cpp:54`
- 当前直接生成 C++ 原生类：`Source/GuiyangMahjongClient/Private/Game/GuiyangClientControllerBridgeImpl.cpp:126`
- 当前主灯/补光灯强度为 800/260：`Source/GuiyangMahjongClient/Private/Game/MahjongRoomPresentationActor.cpp:30`
- 当前手动曝光补偿为 -0.7：`Source/GuiyangMahjongClient/Private/Game/MahjongRoomCameraActor.cpp:41`
- Windows/Android Client 只 Cook `MahjongRoomMap`，未 Cook 预览关卡：
  - `Config/Windows/WindowsGame.ini:6`
  - `Config/Android/AndroidGame.ini:6`
- Server Target 不包含 `GuiyangMahjongClient`，现有模块隔离方向正确：
  - `Source/GuiyangMahjongServer.Target.cs`
  - `GuiyangMahjong.uproject`

## 4. 分阶段执行计划

### 阶段 0：建立视觉与性能基线

1. 固定同一局牌、同一摄像机和同一桌面状态。
2. 保存 Windows 1080p、Android 手机、Android 平板三组截图。
3. 记录当前实际生成的灯光数量、类型、强度、曝光值和 Feature Level。
4. 记录桌面场景 GPU 时间、帧率和是否出现闪烁/黑屏。

验收：能够用相同场景复现“预览亮、运行时暗”，并形成修改前对照。

### 阶段 1：建立可继承的客户端展示基类

修改：

- `AMahjongRoomPresentationActor` 去掉 `final`，声明为 `Blueprintable`。
- 保留客户端模块归属和非复制属性。
- 将桌子、摄像机、灯光组件整理为可在 Blueprint Class Defaults 中调整的组件。
- 将 C++ 当前值保留为安全回退值，不再作为正式视觉参数源。

新增：

- 客户端专属 `UMahjongRoomPresentationSettings`，提供
  `TSoftClassPtr<AMahjongRoomPresentationActor> PresentationClass`。
- 默认指向 `/Game/Client/Room/Presentation/BP_MahjongRoomPresentation`。

验收：Server Target 不引用新增设置类或 Blueprint 路径；Client 编译通过。

### 阶段 2：创建唯一视觉蓝图

1. 创建 `BP_MahjongRoomPresentation`，父类为 `AMahjongRoomPresentationActor`。
2. 将当前预览关卡中的有效灯光、桌子和摄像机参数迁入蓝图类默认值。
3. 不把额外灯光作为预览关卡独立 Actor 保留。
4. `MahjongRoomVisualPreviewMap` 只放置该蓝图，用于人工构图和调光。
5. 如果在关卡实例上调整，必须执行“Apply Instance Changes to Blueprint”，随后清除实例覆盖。

验收：删除并重新放置蓝图后，灯光、镜头和桌面参数保持一致；预览关卡中不存在只在运行时缺失的独立关键灯光。

### 阶段 3：改造运行时加载和无黑帧切换

1. 创建房间进度界面出现后立即开始异步加载 `PresentationClass`。
2. 进入 `MahjongRoomMap` 后：
   - 已加载：生成 `BP_MahjongRoomPresentation`。
   - 正在加载：保持进度/遮罩，加载完成后生成。
   - 加载失败：记录明确错误并回退到 C++ 默认类，不能永久黑屏。
3. 确保只生成一个本地、非复制的 Presentation Actor。
4. 桌子和摄像机准备完成后再切换 View Target，并淡出加载遮罩。
5. 离开房间时销毁本地 Presentation Actor、释放异步句柄。

验收：进入房间不会短暂黑屏或显示大厅；日志明确记录实际生成的类路径。

### 阶段 4：重新校准 PC/Android 灯光与曝光

1. 所有灯光显式指定 Intensity Units，使用 Lumens/Lux 等物理单位。
2. 先将曝光补偿恢复到中性基线，再依据牌面白色树脂不过曝、深色桌布保留细节进行调整。
3. 保持固定曝光以避免此前的亮度闪烁，但将固定 EV/曝光补偿作为蓝图默认参数，而不是硬编码。
4. 优先采用低成本共同基线：
   - 一个主要动态光；
   - 必要时增加一个无阴影低成本补光；
   - 限制 Attenuation Radius，只覆盖麻将桌；
   - Android 不启用实验性 Lumen 作为基础方案。
5. 若保留移动 Spot Light，在项目设置中显式启用对应的 Mobile Local Light Shader 支持，并重启编辑器、重编 Shader。
6. 为 Windows、Android 高/中档设备设置明确的 Scalability/Device Profile 差异，不依赖编辑器默认值。

验收：

- 所有牌面纹理可辨认且白色牌身不大面积过曝。
- PC 和 Android 构图亮度差异可接受。
- 静止画面无曝光跳变或灯光闪烁。
- Android 桌面场景 GPU 时间满足项目帧率预算。

### 阶段 5：客户端 Cook 与服务器隔离

1. 为 `/Game/Client/Room/Presentation` 创建客户端专属 Primary Asset Label，规则设为 Client Always Cook。
2. 显式包含蓝图及其桌子、材质、纹理等依赖。
3. `MahjongRoomVisualPreviewMap` 保持 Editor/开发预览用途，不加入 Shipping MapsToCook。
4. Windows/Android Client Cook 中验证蓝图存在。
5. Linux/Windows Server Cook 中验证以下内容不存在：
   - `BP_MahjongRoomPresentation`
   - `MahjongRoomVisualPreviewMap`
   - Mahjong UI、声音、材质、纹理和客户端展示模块
6. 使用 Asset Audit、Reference Viewer、Cook Manifest 和 Pak 列表四重检查。

验收：客户端可生成蓝图；服务器包大小不因本次视觉修改显著增加，且隔离测试通过。

### 阶段 6：自动化回归

新增或修改测试：

1. 客户端 Presentation 软类路径可解析。
2. 运行时生成类必须是配置的 Blueprint 类，而不是意外回退 C++ 类。
3. `MahjongRoomMap` 不序列化客户端展示 Actor。
4. 预览关卡放置的 Presentation 类必须与运行时配置类相同。
5. Presentation 只能生成一次，并拥有有效桌子、摄像机和必要灯光。
6. Client/Server Package Isolation 测试覆盖新增资产路径。

验收：编辑器自动化、Client Cook、Server Cook、Package Isolation 全部通过。

### 阶段 7：人工验收和发布

1. 一键部署独立服务器，确认仍加载 `MahjongRoomMap`。
2. 启动 4 个 Windows 客户端，由人工完成登录、创建房间、准备、开局、出牌、碰杠、返回大厅、再次进入。
3. Android 手机和平板分别安装最新包，检查：
   - 进入房间亮度；
   - 刘海/全面屏布局；
   - 牌面清晰度；
   - 发热和帧率；
   - 横屏旋转/恢复。
4. 对照阶段 0 截图，签署视觉验收。

验收：4 客户端人工完整游戏无黑屏、断线遮罩误报和光照不足；手机、平板达到同一视觉基准。

## 5. 预计修改范围

- `Source/GuiyangMahjongClient/Public/Game/MahjongRoomPresentationActor.h`
- `Source/GuiyangMahjongClient/Private/Game/MahjongRoomPresentationActor.cpp`
- `Source/GuiyangMahjongClient/Private/Game/GuiyangClientControllerBridgeImpl.cpp`
- 新增客户端 Presentation Settings 类
- `Config/DefaultEngine.ini` 或客户端平台配置
- `Config/Windows/WindowsGame.ini`
- `Config/Android/AndroidGame.ini`
- `Content/Client/Room/Presentation/BP_MahjongRoomPresentation.uasset`
- 客户端专属 Primary Asset Label
- `Content/Maps/MahjongRoomVisualPreviewMap.umap`
- `Source/GuiyangMahjongEditorTools/Private/Tests/MahjongCoreTests.cpp`
- `Scripts/Test-PackageIsolation.ps1`

不应修改服务器玩法逻辑、房间协议、牌局复制状态或 Agones 生命周期。

## 6. 风险与控制

| 风险 | 控制措施 |
|---|---|
| 软引用资产未 Cook，Shipping 回退或黑屏 | Primary Asset Label、Cook Manifest 测试、明确回退日志 |
| 人工只改预览实例，运行时仍不一致 | 蓝图作为唯一源、清除实例覆盖、自动测试类一致性 |
| Android Spot Light 不渲染 | 显式 Mobile Local Light 设置和真机验证 |
| 动态灯增加移动端 GPU 成本 | 限制数量、范围和阴影；Device Profile 分级 |
| 自动曝光再次引起闪烁 | 固定曝光基线并把参数移入蓝图 |
| 客户端资产污染 Server 包 | Target/模块隔离、Server Cook 清单和 Pak 审计 |

## 7. 推荐执行顺序

严格按“架构可编辑化 → 蓝图迁移 → 异步运行时加载 → 灯光校准 → Cook 隔离 → 自动化 → 四客户端/真机人工验收”执行。不要先把预览关卡加入 Shipping Cook；这会复制世界内容、污染服务器边界，并不能解决实例参数与运行时类默认值分离的问题。
