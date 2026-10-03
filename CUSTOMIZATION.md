# 当前定制与兼容边界

接口基线更新：2026-10-01；策略索引更新：2026-10-03。本文只描述当前源码的结构和接口；
[构建命令](BUILDING.md)、[验证结果与待办](PROJECT_RESULTS.md)、[历史归因](PROJECT_HISTORY.md)各自维护。
源码支持、编译通过、静态检查和目标平台运行是不同结论。

后续作者工作流的 [2026-10-03 策略复核](PROJECT_RESULTS.md#41-作者工作流策略复核2026-10-03)
优先建议 Godot + 薄 C# 辅助层；它是待评估建议，不新增已实现功能或改变本文的兼容边界。

## 基线与配对

- 官方基线：`4.7.2-stable` / `ed1daf0bf001b61586d9930840f2f1394092c079`
- 当前工具链：SDK `10.0.401`、runtime `10.0.12`、API/SDK/SourceGenerators `4.7.2-2dtrim.3`
- API、工具、游戏目标 `net10.0`；分析器/生成器宿主保持 `netstandard2.0`
- editor、native/managed ABI、生成绑定、SDK 和目标平台模板必须配对；游戏与脚本库需重编
- .NET 8 冻结包、早期 `.1` / `.2` SDK 和同版本上游包只属于各自检查点，不能混用

## 引擎功能边界

| 范围 | 当前状态 |
|---|---|
| 2D / UI | 场景、动画、2D 物理、音频、资源、文件、通用 Script 与 GDExtension 共享 ABI 保留 |
| 渲染 | canvas-only RenderingDevice compositor；Vulkan，及尚未运行验收的 macOS Metal 源码 |
| 共享渲染能力 | 2D 灯光/遮挡、HDR、MSAA、GPU 粒子、compute；2D 所需 mesh/multimesh/skeleton/texture/material 存储保留 |
| 数学类型 | `Vector3`、`Quaternion`、`Projection` 等共享类型保留，不表示恢复 3D 场景 |
| 已删除 | 3D/XR 场景与管线、GDScript、OpenGL/GLES/ANGLE/D3D12、iOS/Web/visionOS |
| 网络 | 高层多人同步/RPC、ENet/WebRTC/WebSocket/UPnP 及托管 RPC 桥删除；低层 TCP/UDP/HTTP 保留 |
| 其它删除 | 内置视频播放、VideoStreamPlayer、JSONRPC、ZIPReader/ZIPPacker 包装、在线 AssetLibrary、Godot 3→4 转换器 |

`forward_plus` / `mobile` 名称只是 canvas-only RD 的兼容入口，不恢复 3D。
MovieMaker 录制编码在源码中保留，但某个 profile 未必启用其模块。
Windows/Android 没有 OpenGL 或 D3D12 fallback。macOS 源码保留不等于完成 Metal 运行验收。
Android 已重新恢复，不能沿用早期“所有移动平台已删除”的历史状态。

## 托管接口与发布协议

### 实现与归因

- [#116300](https://github.com/godotengine/godot/pull/116300)：选择性移植 native→C# 方法/属性/信号 trampoline 与缓存，按本 fork ABI 配对
- [#116301](https://github.com/godotengine/godot/pull/116301)：提取构造增量；无参构造使用 UnsafeAccessor/函数指针，有参构造保留原选择语义
- [#118932](https://github.com/godotengine/godot/pull/118932)：采用静态元数据/显式注册架构，修正复现问题并补齐实际 trim/AOT 导出闭环
- .NET 10 迁移参考 [#123738](https://github.com/godotengine/godot/pull/123738)、[#123869](https://github.com/godotengine/godot/pull/123869)、[#123934](https://github.com/godotengine/godot/pull/123934)

不是依次应用三个完整 PR，也不是完整同步上游。原始 head、落点、负对照和归因限制见
[历史 .NET 移植](PROJECT_HISTORY.md#dotnet)。不把草稿或局部 probe 描述为完整上游引擎实测。

本地修正涵盖 `_Set` / `_Get` 合法动态回退、闭合 generic collector、空构造 collector、
静态脚本元数据与闭包注册、`NoScriptFileAssociation`、增量生成器相等性、
信号 backing field、nullable 对象转换、typed Array/Dictionary 和对象身份。
初始化/部分注册失败明确报错、清理并非零退出；不再兼容旧 DLL 生成协议。

### 对游戏与插件的约束

- 普通 JIT 保留已验证的游戏 AssemblyLoadContext 动态插件路线，须共享 GodotSharp 身份；不保证任意 `Assembly.LoadFrom`
- trimmed JIT / NativeAOT 的脚本、反射目标、插件和泛型闭包必须在发布时可达；不支持未知晚加载 IL
- `[assembly: Godot.RegisterScriptType(typeof(MyScript<int>))]` 显式注册闭合类型，不创建资源路径关联
- 脚本库使用 `IsGodotLibraryProject=true`；`NoScriptFileAssociation` 排除路径关联但保留合法类型元数据
- 有参构造可直接由 C# 调用，不代表 PackedScene、Inspector 或热重载能提供这些参数
- editor 仍使用 JIT；NativeAOT 没有隐藏 JIT，不承诺 AOT 热卸载；NuGet 依赖需分别验证 trim/AOT 兼容性

入口精确保活 `GodotPlugins.Game.Main.InitializeFromGameProject`，不 root 整个游戏。
NativeAOT 入口为 `godotsharp_game_main_init`。每次导出写 `.godot-dotnet-publish-mode`；
缺失所需 runtime、空或非法标记明确失败，不受旧 runtime 文件或 `DOTNET_ROOT` 误导。
切换模式只警告旧文件，不删除用户数据。详细合约见 [发布模式](misc/dotnet_publish_modes/README.md)。

## Profile 与 ClassDB 边界

Python SCons profile 控制源码构建，不是 `export_presets.cfg`，也不是 JSON `build_profile`。
默认 editor API 可能宽于最小模板；C# 编译成功不证明某个 native class/method 可用。
`GDREGISTER_CUSTOM_INSTANCE_CLASS` 遵守类启用宏，低层直接注册仍可有意绕过；
内部 `exposed=false` 元数据不等于公开 API。`NativeProxyRegistry` 不立即请求缺失类构造函数，
但真正实例化缺失模块的类仍会失败。任意裁剪 profile 都需匹配绑定和项目级回归。

当前 desktop minimal-extra 的 14 模块为：
`astcenc bcdec freetype glslang godot_physics_2d jpg mono mp3 msdfgen ogg svg text_server_adv vorbis webp`。

- 保留 advanced GUI、FreeType、MSDFgen、HarfBuzz、`deprecated=yes`、导航 API 和 2D physics
- **Brotli 已恢复**，内嵌默认 WOFF2 字体依赖它；保留 FreeType/MSDFgen 本身不足以保留默认字体
- `module_navigation_2d` 关闭，导航服务是 dummy；保留节点/API 不代表 NavigationAgent2D/TileMap 导航可用
- TLS、Godot.RegEx、noise、VisualShader、interactive_music、Basis Universal、KTX/DDS 等未启用模块不能直接使用
- `.NET` 网络/密码学是另一实现，仍需验证目标 runtime；共享 PNG/WAV 等不只由 `modules_enabled_by_default` 控制
- `minizip=no` 取消运行时 ZIP 包来源，保留普通/嵌入 PCK，不能称为完全删除第三方 minizip 源码
- Graphite 关闭不等于关闭 HarfBuzz；当前无 mbedtls，`builtin_certs=no` 也不等于关闭系统证书
- Windows 保留 SDL/AccessKit/WinRT；Android 不采用这些平台专属开关

第一组 Windows minimal 只有 13 模块、无 MSDFgen，不要与 extra 混用。
资源、字体、纹理与第三方依赖详细检查见 [profile 兼容清单](misc/build_profiles/windows_release_minimal.md)
和 [extra 差异](misc/build_profiles/windows_release_minimal_extra.md)。

## Android 专属边界

当前是 **arm64-v8a / Mono / Vulkan / APK**，默认普通 JIT，trimmed JIT 为实验性。
Android CoreCLR / NativeAOT 仍拒绝；其它 ABI、AAB、自定义 Gradle 未经同等验收。
游戏 APK 可使用预编译模板并关闭 Gradle；维护者构建模板仍需 Gradle。

- Mono `10.0.12` 与 crypto JAR 必须来自同一官方 runtime 包，native `.so`、Java preload/JNI 不得拆配
- 直接调用 Mono `coreclr_create_delegate`；无需桌面 hostfxr 的 ComponentActivator 保活属性
- 项目正文之后计算 trim 常量；Mono descriptor 保留 runtime 自身的 native→managed 入口
- Android host 未把 runtimeconfig JSON 属性加载给 Mono；带上该文件不证明 runtime 设置生效
- 应用默认最低 API 29 / Vulkan 1.1，native library 兼容底线 API 24；ETC2 基线，ASTC 需设备支持
- 不恢复 Android editor、GL/XR 或 Android 专属 NetSocket 覆盖；低层网络继续走通用 POSIX
- SAF 需要系统 picker 的 content/tree URI 和有效授权，普通路径保留原语义；provider 模拟不代替设备权限验收

SAF 来源为 `5cbce230` / `fdaea11a`，Android 打包包含 [#122774](https://github.com/godotengine/godot/pull/122774)
的静态 `.a` 排除；`.so` 放 ABI 目录，托管文件放 PCK，JAR 去重。
完整验证边界见 [Android 专项说明](misc/android_dotnet_validation/README.md)。

## 上游维护口径

审计固定于 master `2490bf30ec229ef3eeda24befb9d80d2226c8d29` 的 **1,362 项快照**。
原 high：**38 完整、1 部分、4 延期**；`120746` 部分采用，`119123`、`120545`、`122667`、`123693` 延期。
后续 13 项中低优先级补丁在 `4060b758` 发布，是依证据重建的恢复版本，不是找回原始提交，
也不改变原 high 统计。每项来源、状态和验证范围由 [状态账本](misc/upstream_sync/status.json) 与
[历史批次](PROJECT_HISTORY.md#upstream-medium-batch)追溯。
