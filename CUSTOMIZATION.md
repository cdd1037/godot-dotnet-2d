# 2D / .NET 定制说明与维护计划

更新：2026-09-30。本文是本 fork 的统一维护入口，记录引擎修改、构建配对、已验证产物和分级待办。

- **冻结包实现检查点**：`36914aeaa5d291f417afc83c70eb3d03feea19b3`；当前开发源码另包含 [第 9 节首批上游正确性回移](#upstream-first-batch)，冻结包未覆盖
- **当前目标**：2D / .NET 桌面工具链；全平台 .NET 10 升级方向已确认，但尚未完成或合入。Android 目标仍为 .NET 10（`net10.0`）/CoreCLR 普通预编译 APK 模板导出，最新决定暂缓集成，尚未加入桌面分支
- **迁移状态**：生产源码、已封存桌面包和现有 CI 仍以 .NET 8 为已配置/验证基线；独立迁移草稿保留、不合并，本轮不继续桌面或 Android 迁移。暂缓不等于永久取消 .NET 10 目标
- **验证状态**：Linux 已完成下述场景的三模式运行；Windows 仅完成交叉编译和静态检查；Android 仅有独立源码补丁与局部测试
- **交付状态**：产物本地保留，未上传/推送；本次不覆盖、重打或重新发布已冻结的包
- **规划归并**：原独立 `long-term-planning/LONG_TERM_PLAN.md` 的全部技术规划已并入本文；旧文件保留为历史快照，后续只维护这里

此前“不将长期规划放入源码”的安排已经由本次统一文档提交取代，但旧归档本身没有改变。
旧包 README 中“ClassDB 暂缓”“SDL 仍有两条警告”“长期规划独立保存”等文字，须按各包的提交和时间理解。

## 导航

1. [版本与项目布局](#versions)
2. [引擎裁剪与保留能力](#engine)
3. [.NET 移植、自有修正与发布流程](#dotnet)
4. [ClassDB 与 build profile 边界](#classdb)
5. [Windows 最终配置与真实 LTO](#windows)
6. [验证、体积和产物索引](#evidence)
7. [分级待办与验收](#roadmap)
8. [长期研究细节与参考](#research)
9. [首批上游正确性回移](#upstream-first-batch)

<a id="versions"></a>
## 1. 版本与项目布局

| 层次 | 不可变检查点 | 含义 |
|---|---|---|
| 官方基线 | `ed1daf0bf001b61586d9930840f2f1394092c079` | 官方 `4.7.2-stable` |
| 纯 2D/.NET 裁剪 | `177d7f65e468d8f3d749980e13b93c8c6db069cb` | 原生/托管裁剪、Linux 基础验证 |
| .NET 优化冻结版 | `1c26d4743e678d66db23c369c043ea738ea8bfe9` | interop、构造、静态元数据、三发布模式、typed collection 修正 |
| Linux 当前冻结包 | `4c802b3b40e174f6aee45cc3d7b8c27e93960f8f` | 在 1c 上修复 custom-instance 注册宏，重建并回归 |
| 桌面源码 / Windows 当前包 | `36914aeaa5d291f417afc83c70eb3d03feea19b3` | 在 4c 上加入 Windows 修正、可选 profile、真实 LTO 路径、SDL 两行回移 |
| 首批上游正确性源码 | `d00abd3f`（含 `3c063387`、`b7989fd3`、`0f9e13ce`） | 第 9 节 12 项小修复，本地批量回归；冻结包不变 |
| Windows-only CI 本地配置 | `3632efa16d7e90303eff0c243f8a35fbabf0f573` | 仅修改 `.github`；真实远程运行与发布仍未验证 |
| Android 独立分支 | `5cbce23002b89339cc77118fdc2394d95ea67284` → `fdaea11a1f2bc877b7f102f177f078c645cb9b7e` | 直接基于官方 ed1daf0bf 的 `android-saf-diraccess`，未合入本 fork |

相对路径约定：本文位于 `godot-dotnet-opt/` 根目录；`../godot-*` 指同一工作区的配套交付或证据目录。
这些目录不在本仓库内，单独拿到源码时需要另取相应交付包；引用它们不代表把二进制或实验缓存提交进 Git。

- `godot-dotnet-opt/`：当前桌面开发仓库；代码/文档维护入口
- `godot-delivery-classdb-4c802b3b/`：冻结的 Linux 源码、编辑器、模板、SDK 与回归证据
- `godot-windows-minimal/`：最新 Windows ZIP、源码归档、manifest、复现脚本与结果日志；旧工具链、重复源码和历次 EXE 已回收
- `android-saf-diraccess` Git 分支、`godot-android-saf-delivery/`：保留独立 SAF 源码历史与按序补丁；原 `godot-android-saf/` 工作树已移除
- `godot-pr-research/`：固定 PR 快照、归因与最小复现；研究快照不是当前上游状态承诺
- 其余 `godot-*-validation`、`godot-*-probe`、`godot-build-logs`、`godot-metrics`：保留验证源码、报告、日志、结果与截图；旧运行目录、发布物、托管缓存及大工具下载已回收，不能直接当作现成可运行安装

2026-09-30 按“只保留当前版本”完成旧资料清理。最新 Linux/Windows 冻结交付及 Android 最终补丁不变；历史证据与恢复入口见 [清理记录](../cleanup-records/current-only-20260930/README.md)。
`../godot-2d-dotnet/` 现在是持有共享 `.git/` 的小型 sparse-checkout 壳，**不可删除此目录或其中 `.git/`**，否则当前主树及完整 Git 历史都会损坏。

<a id="engine"></a>
## 2. 引擎裁剪与保留能力

### 2.1 已经修改原生引擎，而非仅禁用菜单

| 范围 | 已完成的改变 | 代表提交 |
|---|---|---|
| 脚本语言 | 删除 GDScript 运行时、工具及专属编辑器集成，默认使用 C# | `9eb1bdd8` |
| 3D / XR | 物理删除 3D scene、editor/import、physics/navigation、XR、相关模块与专属依赖；移除 3D 动画轨道、压缩和 root motion，保留 2D track ID 兼容 | `1e21a595`、`9b89662e`、`9a20df13` |
| 渲染 | 删除 OpenGL/GLES3/ANGLE/D3D12 与 Forward+/Mobile 的实际 3D 管线、场景剔除、GI/天空/雾、环境/相机后处理；采用 canvas-only RenderingDevice compositor | `c59f3e24`、`08f633a1`、`bbfc041c`、`94408491` |
| 平台 | 删除 Android/iOS/Web/visionOS、嵌入式 Apple driver 和对应 .NET 导出/runtime 分支；保留 Linux/Windows/macOS 源码 | `6bb38438`、`0b9d6923` |
| 高层网络 | 删除 Godot 多人同步/RPC、ENet/WebRTC/WebSocket/UPnP 及托管 RPC 桥；同步 native/managed ABI | `7dbd729a` |
| 视频与附加 API | 删除内置视频播放、VideoStreamPlayer、Theora 播放侧，以及 JSONRPC、ZIPReader/ZIPPacker 游戏包装 | `fc439bf3`、`d49e8d68` |
| 编辑器旧功能 | 删除在线 AssetLibrary 浏览器、Godot 3→4 转换器；修复 2D 快捷键、已删功能 profile、命令行导出退出等残留路径 | `d49e8d68`、`a76ce926`、`91468024` |

删除范围同时反映到 ClassDB 和生成的 C# API；后续 PR 移植没有重新引入 RPC 或 3D 包装。

### 2.2 有意保留的基础

- 2D 场景/UI/动画、2D 物理、音频、文件与资源系统、C#、通用 Script 和 GDExtension 共享 ABI
- Vulkan / Metal 源码；Canvas RD、2D 灯光/遮挡、HDR、MSAA、GPU 粒子和 compute
- 2D 使用的共享 mesh/multimesh/skeleton/texture/material 存储；通用 `Vector3`、`Quaternion`、`Projection` 和 compute 体纹理
- PNG 在 `drivers/png`，WAV 和基础音频在 core/scene 等层；不是仅靠 `modules_enabled_by_default` 控制
- 低层 TCP/UDP/HTTP 等网络；Linux 常规模块构建还包含 TLS、regex、noise、2D 导航和 VisualShader 等
- 原裁剪分支保留 MovieMaker 的录制编码，包括共享 Theora/Ogg/Vorbis；“无视频播放”不等于“无录制编码”
- `forward_plus` / `mobile` 配置名作为兼容入口保留，实际渲染为 canvas-only RD，不代表恢复 3D

共享数学/存储类型不是漏删的 3D 场景实现。不得为了清除所有含“3D”的名字破坏 2D 或 GDExtension 合约。
“源码保留”和“某个模板实际编入”也要分开：Windows extra profile 另做了模块裁剪，例如没有导航后端、TLS、noise、VisualShader 或 Theora 模块。
Windows Vulkan-only 没有 OpenGL/D3D12 fallback；Metal 仅为 macOS 保留源码，尚未完成运行验证。

<a id="dotnet"></a>
## 3. .NET 移植、自有修正与发布流程

### 3.1 三个 PR 的选择性移植

下面的上游状态均为 2026-09-30 研究快照，不把未合并草稿描述为已合并官方功能。

| 来源 | 固定来源 / 本地落点 | 实际使用与边界 |
|---|---|---|
| [PR #116300](https://github.com/godotengine/godot/pull/116300) | 上游合并 `1d797566ee4f2dafe0a016ef3b7842f97f01da58`；本地 `8318d843` | native→C# 方法/属性/信号 trampoline 与缓存；按 2D/RPC 裁剪后的 ABI 成对适配 |
| [PR #116301](https://github.com/godotengine/godot/pull/116301) | 草稿 head `5608aca6d7f5666112eb78734d07c5aba479ee5a`；本地 `9e85ed10` | 只提取 `7f32e9fd`、`d4afb648`、`5608aca6` 的构造增量；.NET 8 UnsafeAccessor 与函数指针用于无参构造，有参构造保留原反射选择语义 |
| [PR #118932](https://github.com/godotengine/godot/pull/118932) | 草稿 head `c0b714b07cfff7187435fce4cf77aab0231b345e`；本地 `2b373597` 起 | 采用静态脚本元数据、显式注册与 editor/runtime 回调拆分的架构，改写已复现问题，并自建真正的 trim/AOT 导出闭环 |

PR 之间有依赖和重叠 diff，不能将三个完整 patch 依次机械套用。PR #118932 的手动 `OutputType=Exe` 实验未实现游戏导出，其 4.1 MB 示例不含原生引擎，不能当作本项目完整游戏大小。
详细历史见 [移植方案](../godot-pr-research/DOTNET_PORT_PLAN.md)，其中“尚未实施”等状态属于实施前快照。

### 3.2 自有正确性修正

- `bb04236d`：恢复合法 `_Set` / `_Get` 动态回退。`_Set=false` 不能被当作已处理；只读/只写 CLR 属性也不能吞掉同名动态回调
- `f6479453`：普通 JIT 反射调用前正确闭合 generic trampoline collector
- `a505287b`：构造生成器按当前 compilation 判断 unsafe 模式
- `ea2ddceb`：无 Variant-compatible 构造时仍生成空 collector，修复 dispatcher 请求不存在方法的 `CS0117`
- `2b373597`、`4e63d969`：静态闭包注册、可静态追踪的脚本元数据与 provider 分离；无旧 Godot DLL 的非 trim-safe 生成协议兼容负担，项目/插件需重编
- `30487d49`、`612b33a1`：managed handshake / 部分元数据注册失败时明确报错并完整清理、非零退出
- `be5f9699`、`e1c4641a`：生成信号 backing field 与 nullable 对象转换修正
- `ba6c4649`、`f0b6f9cf`、`8c684524`：显式发布模式优先于残留 runtime 文件；非法/空标记不隐式 fallback；标记读写使用 BCL 文本 IO
- `1c26d474`：生成原生 wrapper 的静态类型元数据，覆盖 typed Array/Dictionary 中 native key/value、脚本/闭合泛型及对象身份

删除旧 DLL 协议不授权破坏新源码正常的动态属性语义。普通 JIT 的未知插件路线仍保留。

### 3.3 已复现的生成器问题及归因边界

[PR #118932 审查](../godot-pr-research/REVIEW_118932.md)保存了以下三个独立复现及源码来源：

1. **泛型 provider**：`.NET 8` 对 `typeof(Outer<int>).GetNestedType(...)` 返回开放嵌套类型，原属性逻辑错误拒绝它。原样属性实现的局部 probe 使用不影响反射路径的 Godot 返回类型 stub；不是完整上游引擎运行。JIT 显式 `MakeGenericType` 也不足以证明 AOT 闭包可达，当前方案通过生成的 closed-type 静态注册处理
2. **`NoScriptFileAssociation`**：原注册生成器遗漏 opt-out，仍生成路径注册和 `ScriptPathAttribute`。已用原样 generator、真实候选筛选和具有正确 GodotSharp 身份的类型模型复现；没有把它量化成未经测量的体积损失。当前修正排除路径关联但保留合法类型元数据
3. **增量 model 相等性**：仅比较类名忽略输出相关语义。复用 GeneratorDriver，将同名脚本基类 Node 改为 Resource 时仍输出旧 Node 元数据，clean driver 才正确。原直接实证是 NativeType 变化；后续本地回归扩大到增量缓存、文件/类型变化与 nullable 等，不能倒推成所有上游场景已复现

另有 PR #116301 的空 collector 问题：[原生成器完整复现](../godot-pr-research/116301-original-generator/README.md)
对固定 head 的 24 个原始 C# generator 文件逐字核验，使用真实本地 GodotSharp metadata reference，
`RefCounted` 的唯一 `System.IO.Stream` 构造产生 `CS0117`；抽象 Stream 构造基类也复现。
直接 C# `new` 已验证可工作，未在已有有效无参构造的具体类型上复现这个缺方法错误。

[2026-09-30 讨论](https://github.com/godotengine/godot/pull/116301#issuecomment-5909210375)
澄清缺少无参构造不应阻止 C# 编译；引擎无法创建时应正确报错。该参与者意见不是 PR 作者最终裁定或上游已修复证明。
本地空 collector 修复解决编译一致性，不让 Stream 变成 Variant-compatible，也不保证 PackedScene、Resource/Inspector 或 editor 热重载能凭空提供构造参数。

PR #116300 的 `_Set` / `_Get` 来源已逐字对比官方合并函数，且在本地移植候选复现；
[语义审查](../godot-pr-research/REVIEW_116300_SEMANTICS.md)没有声称编译运行完整官方 4.8。

### 3.4 editor、绑定、SDK、模板必须配对

当前冻结优化包 SDK 为 **`4.7.2-2dtrim.1`**。官方同基础版本、早期 `4.7.2` 裁剪包、interop/constructor 实验包和旧 DLL 不可混用。

1. 从指定提交构建 `.NET editor`；由它运行 `--generate-mono-glue modules/mono/glue`
2. 用 `modules/mono/build_scripts/build_assemblies.py --godot-output-dir=bin` 构建匹配 GodotSharp、工具、SDK/SourceGenerators
3. 游戏与脚本库采用这个 editor 的 `GodotSharp/Tools/nupkgs` 本地源和隔离 `NUGET_PACKAGES` 缓存，全部重新编译
4. 构建/选择同 fork、同平台、适用 profile 的 Release template，再由匹配 editor 导出；Release template 不能代替 tools editor 生成绑定
5. 清洁输出目录分别发布、真实运行，再测 native、managed/runtime 或 AOT payload、资源、符号与完整目录大小

参考 [Mono 构建说明](modules/mono/README.md)、[验证项目](misc/2d_dotnet_validation/README.md)和
[三模式协议](misc/dotnet_publish_modes/README.md)。早期验证说明中“同为 4.7.2”的文字是历史缓存风险背景；当前包以本节和具体产物为准。

### 3.5 三种真正的导出模式

本节记录桌面发布协议及其验证边界；Android 当前目标为 .NET 10（`net10.0`）/CoreCLR 普通预编译 APK 模板导出，不直接继承桌面三模式支持声明，见 A3。

| `dotnet/publish_mode` | 发布物与执行方式 | 脚本/插件边界 |
|---|---|---|
| `0` 普通 JIT（默认） | self-contained 未 trim 的 managed/runtime payload | 可走已验证的当前游戏 AssemblyLoadContext 动态插件路线，须共享 GodotSharp 身份；不保证任意 `Assembly.LoadFrom` |
| `1` trimmed JIT | self-contained trimmed managed/runtime payload，仍有 JIT | 脚本、反射目标、插件及泛型闭包必须在发布时可达 |
| `2` NativeAOT | 游戏共享库，由 `godotsharp_game_main_init` 初始化 | 构建时已知脚本/插件与闭包；没有隐藏 JIT，不能运行未知 IL；不承诺 AOT 热卸载 |

SDK 精确保活 `GodotPlugins.Game.Main.InitializeFromGameProject`，而非增加游戏 console Main 或 root 所有方法。
trimmed JIT 还使用 .NET 8 内部属性 `_EnableConsumingManagedCodeFromNativeHosting`；升级 SDK 必须重新验证。
已知泛型/插件用 `[assembly: Godot.RegisterScriptType(typeof(MyScript<int>))]` 注册；它不创建资源路径关联。
库项目设置 `IsGodotLibraryProject=true`，避免第二个引擎入口。缺闭合类型注册要明确失败。

每次导出写 `.godot-dotnet-publish-mode`；模板按它选择模式，不受 `DOTNET_ROOT` 或旧 JIT 文件误导。
切换模式会提示旧文件但不删除用户数据；空/非法标记或缺少所需 runtime 应失败退出，不静默换模式。
editor 仍为 JIT，热重载与发布 AOT 是不同路径。普通 NuGet 包是否 trim/AOT-safe 仍须逐包验证。

<a id="classdb"></a>
## 4. ClassDB 与 build profile 边界

`4c802b3b` 新增 `GDREGISTER_CUSTOM_INSTANCE_CLASS(T)`，以 `if constexpr (GD_IS_CLASS_ENABLED(T))` 守卫注册，
迁移全部 8 个现存 core 调用：HTTPClient、X509Certificate、CryptoKey、HMACContext、Crypto、StreamPeerTLS、PacketPeerDTLS、DTLSServer。
上游另外 4 个调用属于本 fork 早已删除的 UPNP/WebRTC/WebSocket 模块。

- 低层 `register_custom_instance_class<T>()` 维持原语义；直接调用它仍是有意绕过宏层策略，新的生产调用应使用宏
- 禁用类不会经这些宏被公开，但内部使用可能产生 `exposed=false` 元数据；不能统一要求所有禁用类 `class_exists=false`
- 未额外关闭这 8 类的默认能力；这是 profile 正确性修复，不是体积裁剪
- 普通构建 4 tests / 90,148 assertions、随机顺序同结果；实际 disabled profile 3 tests / 40 assertions
- 负对照：绕过宏产生预期 7 个失败；deleted factory 的 helper 直接调用按预期编译失败
- 独立 profile 测试是非 .NET `template_debug`；生产 4c 包另按正常 profile 重建与回归，不能据此保证任意裁剪 profile 配同一 GodotSharp 都安全

详见 [ClassDB 报告](../godot-delivery-classdb-4c802b3b/reports/VALIDATION.md)及其
[生产说明](../godot-delivery-classdb-4c802b3b/README.md)。前者的“生产包未重建”是隔离测试阶段描述，后者记录了随后完成的重建。

<a id="windows"></a>
## 5. Windows 最终配置与真实 LTO

### 5.1 代码与配置的演进

- `6212553b`：删除 Vulkan-only Windows 路径残留的 WGL include
- `adf3f7b2`：第一组可选 Windows minimal profile，13 modules，无 MSDFgen
- `0f56101c`、`3573f417`、`5dfe8dce`：默认关闭的 `mingw_lto_plugin`、普通 COFF 输入与经验证的 partition=none 构建命令
- `a65b9138`：第二组 extra profile，补 MSDF 并裁指定功能
- `2451c8c2`：撤回 `deprecated=no`，当前 `deprecated=yes`
- `b1258033`：撤回完整 2D 导航 API 裁剪，保留 API，模块后端仍关闭
- `36914aea`：仅回移 SDL 上游 `55acc0b82954a50758cf4774365cc1e2877816c6` 的两行返回声明，`int` → `bool`，匹配已有实现；无警告抑制或 ICU/WinRT 生产改动

### 5.2 最终 extra profile（14 modules）

源配置：[windows_release_minimal_extra.py](misc/build_profiles/windows_release_minimal_extra.py)。
`modules_enabled_by_default=False`，明确开启：

```text
astcenc bcdec freetype glslang godot_physics_2d jpg mono mp3
msdfgen ogg svg text_server_adv vorbis webp
```

- `minizip=no`、`brotli=no`、`graphite=no`、`builtin_certs=no`
- `deprecated=yes`、`disable_navigation_2d=no`；`module_navigation_2d` 关闭，服务为 dummy，不代表可用寻路
- Vulkan/volk、2D physics、advanced GUI、SDL、AccessKit、WinRT 保留；FreeType、MSDFgen、HarfBuzz 保留
- `minizip=no` 取消运行时 ZIP 资源包来源，普通/嵌入 PCK 保留；当前 SCsub 仍编译 minizip 第三方源，不能宣称整库已删
- Brotli 关闭意味着没有 WOFF2 / `Compression.MODE_BROTLI`，普通 TTF/OTF 保留；Graphite 关闭不等于关闭 HarfBuzz
- 当前无 mbedtls；内置 CA 无消费者，不期待其额外明显体积收益。以后若打开 mbedtls，系统 ROOT 证书与内置 CA 是不同路径

使用前检查项目是否依赖 Godot.RegEx、TLS/HTTPS、noise、navigation backend、VisualShader、interactive_music、KTX/DDS 或 Basis Universal 等未启用模块。
.NET 对应库是独立实现，仍需在最终 payload/系统环境中验证。Basis Universal 导入模式 `compress/mode=4` 不可直接沿用；
普通原生 BC/VRAM 压缩不等于 Basis，预导入 BC 纹理也不等于运行时读取 `.dds`。

[第一组说明](misc/build_profiles/windows_release_minimal.md)和[第二组差异](misc/build_profiles/windows_release_minimal_extra.md)
提供更完整的依赖限制；第一组的 13-module 清单、无 MSDF 字体要求不能套到最终 extra 上。

### 5.3 历史实验的可复现 MinGW 真 LTO 路径

以下两模式命令用于重现已经结束的体积实验，不是以后每次 CI 的构建矩阵。
后续 Windows CI 导出模板只保留 LTO；最新 SDL-fixed ZIP 内保留最终 none/full 对照，历史结果和日志保留，其余旧二进制已回收，不定期重复构建对照。

历史对照使用同一 Windows x86_64、MinGW GCC 14.2.0 POSIX / binutils 2.44、`-Os`、Release、无 native debug symbols、相同 strip 政策：

```sh
scons profile=misc/build_profiles/windows_release_minimal_extra.py platform=windows target=template_release arch=x86_64 use_mingw=yes use_llvm=no optimize=size debug_symbols=no production=no lto=none -j8
# 另存上次输出，再构建；两个模式输出同名。
scons profile=misc/build_profiles/windows_release_minimal_extra.py platform=windows target=template_release arch=x86_64 use_mingw=yes use_llvm=no optimize=size debug_symbols=no production=no lto=full mingw_lto_plugin=yes "linkflags=-v -flto-partition=none" -j8
```

两份主 EXE 均另行用同一 `x86_64-w64-mingw32-strip --strip-all` 处理。
依赖和环境准备、保存输出与配置的完整步骤见最终包的 `reproduce-minimal-templates.sh`（选择 extra）。
构建需要实际保持 AccessKit/WinRT/SDL 开启；缺依赖时自动关闭不能算同一配置。

不能仅凭 `lto=full` 或命令里出现 `-flto` 就说完成 LTO。
本仓库原 MinGW 路径使用 `-fno-use-linker-plugin -fwhole-program`；早期那组名义 full 构建没有证明实际全程序 LTO，不用于最终收益结论。
仅打开 plugin 后，强制 PE-bigobj 最小复现仍报 `plugin needed to handle lto object`，正常平衡分区也触发编译器内部错误。
最终实验路径去掉 `-Wa,-mbig-obj`，使用普通 COFF + linker plugin + `-flto-partition=none`，日志确认实际执行 `lto1` 全程序优化。
`none` 直接执行不写分区中间对象的路径，与仍写一个分区的 `one` 不同；不是“关闭 LTO”，也未把 `-Os` 偷换为 `-O2`。

此方案限于已验证的工具链/配置；普通 COFF 节数上限、大工程变化和工具链升级均需重验。
主要 LTO 阶段不具备 8 路分区并行，本轮最终链接约 11 分钟；总构建并发保持最多 8 jobs。

### 5.4 最终 SDL 修复后的结果与警告

| 模式 | 主原生 EXE B | MiB | 构建警告 |
|---|---:|---:|---|
| 非 LTO | 33,670,144 | 32.1104 | 0 |
| 真 LTO | 28,708,352 | 27.3784 | 3：ICU 1 + WinRT 2（`-Wodr`） |

同配置 LTO 节省 **4,961,792 B / 14.7365%**；SDL 修复前后大小相同，hash 改变。
原有 5 条 LTO 警告中的 SDL 两条已消失；ICU/WinRT 没有用 `-Wno-odr`、`-Wno-lto-type-mismatch` 或全局关闭 aliasing 来隐藏。
新二进制里 56-byte ICU stub 的唯一性、16-byte 对齐与 `.rdata` RVA `0x1671a80` 也已复核，但这仍非 ODR 正确性或 Windows 运行证明。

当前只得到原生模板，不包含 GodotSharp、游戏程序集或 .NET runtime，也未运行 Windows EXE。
Windows 启动、Vulkan、输入/手柄、音频、字体、WinRT 和 .NET 三模式仍在发布前待办中。

### 5.5 Windows-only CI 与发布策略

工作流已在本地改为 Windows-only，独立提交 **`3632efa16d7e90303eff0c243f8a35fbabf0f573`**。
实现入口：[windows.yml](.github/workflows/windows.yml)、[CI 操作与限制](.github/ci/README.md)。
它替换继承的多平台 workflow 和不适用的 D3D12/ANGLE 等旧构建链，不改变 Linux/macOS 源码支持。

| 触发 | 运行范围 | 交付 |
|---|---|---|
| 普通 push / PR | Python/SCons 语法、格式/style、workflow/helper checks、4 个保留 shader-generator fixture；任何非文档变更保守追加 editor、匹配 glue/managed packages、.NET source-generator tests 与 editor headless C# smoke | 有构建时保存日志；不构建模板或发行 ZIP，不创建 Release |
| 纯 Markdown/RST/issue-template 变更 | 仅上述轻量检查；不运行 native/managed build；未知 diff base 采用保守构建 | 不创建 Release |
| 手动 workflow_dispatch | Windows editor、匹配 NuGet、最终 extra Release **LTO-only** template、实际 C# 导出并在 Windows runner 执行 | 完整 CI artifacts，不创建 Release |
| `v<major>.<minor>.<patch>-custom.<revision>` tag push | 完整 Windows 构建/测试和打包，例如 `v4.7.2-custom.1` | 全部成功后创建 GitHub Release **草稿**（标记 prerelease），用户审核后手动发布 |

轻量 shader-generator fixture 与 C# SourceGenerators 是两组测试：后者依赖 editor 生成的绑定，只在 build job 运行。
Linux/macOS/Android/iOS/Web 不在 CI 中运行；现存源码及 Linux 已验证历史保留。
完整构建交付三个 ZIP：`godot-windows-x64-editor-dotnet.zip`、`godot-windows-x64-fork-nuget.zip`、
`godot-windows-x64-release-template-extra-lto.zip`，另有 `manifest.json`、`SHA256SUMS.txt`。
editor ZIP 与 NuGet ZIP 分离；原生 template ZIP 不包含游戏/.NET runtime payload。

CI 使用 `windows-2022` / MSVC 14.3 工具链族、SCons 4.10.1、Python 3.12.10、.NET SDK 8.0.425。
CI 原生并行度为 `min(4, runnerCPU)`，editor/template 独立缓存各限 2 GiB；历史人工构建的最多 8 jobs 不是 CI 默认。
缓存键包含 runner image revision，避免 MSVC servicing 更新后重用不兼容的 LTO 对象。
editor 使用 `optimize=speed lto=none`，最终 extra Release 模板使用 `optimize=size lto=full`，核对 `/GL`、`/LTCG` 与实际 linker code generation。
模板实验已结束，以后不固定构建非 LTO 对照，也没有 template_debug 发布任务；保留最新最终对照 ZIP 与历史报告，过时包已回收。
MSVC hosted runner 的 servicing revision 会变化；日志记录实际配置，历史 MinGW 的字节/hash与警告数不能套用到这里。

headless C# smoke 覆盖 Node2D、Control、World2D/physics、TileMapLayer、2D shape、生成信号和模板模块排除，
需明确成功 marker 与退出 0。完整导出 smoke 目前仅 **untrimmed JIT（mode 0）**；不等于 Windows trim/AOT 或硬件 Vulkan 已验证。
editor API 比 extra 模板宽，测试通过也不会让未包含模块变得可用。

tag 只命名源码，不自动修改 NuGet 版本。当前 `4.7.2-2dtrim.1` 写入 manifest/草稿说明；
以后向共享 feed 分发不同源码前必须审查并分配独立包版本，不能覆盖已发布版本或移动已发布 tag。
草稿步骤核验 repository、tag commit 和文件校验；拒绝修改已发布 Release，或覆盖来源不同/校验不符的已有草稿附件。
普通 jobs 只读；仅 tag 草稿 job 使用短期内置 token 的 `contents: write`，没有外部 PAT 或 NuGet.org 凭证。

本次本地通过：actionlint、YAML/Windows-only/权限策略检查、6 个 helper 单测、4 个保留 shader fixture、Python AST/Ruff。
另复用已冻结 Linux editor/API 通过 64/64 .NET generator tests、新 C# smoke 编译（0 警告）与成功 marker 运行；这些不是新 Windows 验证。

这是为用户自己的未来 GitHub 仓库准备的本地配置。当前 `origin` 仍指官方 Godot，未向它推送，
没有用户自有远程上的 Actions run / artifact / Release 成功证据。
首轮 Windows/MSVC build、导出运行、缓存、artifact 上传与 tag 草稿创建仍须真实观察；
本地脚本测试以及复用 Linux editor 的 smoke 不能替代这些验收。
Windows hosted runner 的 headless smoke 也不能替代硬件 Vulkan、输入/音频/字体/WinRT 和三模式设备验收。

NuGet 目前仅构建并随 artifacts/草稿包交付，**不发布 NuGet.org**。
其账号、版本/包名、发布权限与安全凭证由用户以后准备再接入，见 C1；本次不创建、填写或保存发布凭证。

<a id="evidence"></a>
## 6. 验证、体积和产物索引

### 6.1 Linux 当前冻结包 4c802b3b

工具链 GCC 14.2、Python 3.12.14、SCons 4.11.1、.NET SDK 8.0.425 / runtime 8.0.31。
editor 默认 `optimize=speed_trace`（`-O2`），Release template 默认 `optimize=speed`（`-O3`）；无 LTO、无 native debug symbols、链接时 strip。
正常模块配置不等同 Windows extra。环境缺 Wayland scanner / AccessKit，原版与本 fork 使用同样检测结果。

| 部分 | 实测 B | 口径 |
|---|---:|---|
| native editor | 124,428,568 | 4c，大小与 1c 相同 |
| native Release template | 60,838,968 | 58.02056 MiB，大小与 1c 相同 |
| GodotSharp / Tools / 当前 SDK 包 | 64,981,767 | 比 1c 的 64,981,699 B 多 68 B；非游戏 runtime |

同一小场景、同一 native template、同一 1,728 B PCK、self-contained、正常 globalization、无独立调试符号，三模式真实运行并验证 `fr-FR` / `zh-CN`：

| 模式 | 游戏/.NET payload B | 完整运行目录 B | 完整目录 MiB |
|---|---:|---:|---:|
| JIT | 78,111,617 | 138,952,313 | 132.52 |
| trimmed JIT | 21,105,543 | 81,946,239 | 78.15 |
| NativeAOT | 3,863,148 | 64,703,844 | 61.71 |

完整目录包含共同的 60,838,968 B 引擎；小场景结果不承诺任意游戏的体积。测量为普通文件表观字节总和，非压缩包大小。
Linux 的 **58.02 MiB → Windows 27.38 MiB** 同时改变平台、编译优化、模块与 LTO，绝不能作为“LTO 节省”比例。
Linux map 归属分析见 [体积分析](../godot-size-analysis/README.md)：输入节归属近似值不能把模块收益简单相加。

### 6.2 已验证到哪一步

| 检查点 | 已完成 | 未涵盖 |
|---|---|---|
| 纯裁剪 177d | editor/Release/C# 构建；ClassDB/C# 删除负断言；2D 物理、导航、动画/noise；窗口与独立 PCK 导出 Vulkan | 全引擎套件、真实复杂游戏、Windows/macOS、实体 GPU |
| .NET 1c | 64/64 generator；三模式严格窗口 Vulkan；typed Array/Dictionary 每模式 25 断言；跨程序集 int/string 闭包、Generic Script.new、属性/信号、NuGet 样例、Resource 保存重载 | 任意第三方依赖和任意动态加载方式 |
| .NET 1c 生命周期 | v1→v2→v3 editor 热重载、两次 ALC 卸载、同一 native 节点、导出值 91、信号一次、typed Array 读回 closed generic；普通 JIT 未预引用 DLL；初始化失败非零退出；模式标记负用例 | NativeAOT 热卸载 |
| ClassDB 4c | 上述宏专项/负对照；生产 editor/Release/SDK；64/64 generator；724 个 API 文件与 1c 逐字一致；三模式小/完整场景导出、headless 和严格窗口 Vulkan | 未把 1c 全部热重载、动态插件和错误退出用例再跑一次；任意 profile 兼容性 |
| Windows 36914aea | 最终 14-module 两模式原生构建、配置一致性、PE/导入依赖、`lto1`、字节/hash、SDL 诊断消失、ICU blob 静态检查 | Windows 运行、.NET payload 发布、硬件 GPU |
| Android fdaea11a | 70 项 Robolectric/JUnit 执行；真实 Android API Kotlin 编译；native/JNI/路径局部检查 | 完整 SDK/NDK/Gradle 构建、APK、模拟器/真机、真实 provider |

4c 窗口回归环境为 **Linux x86_64 + Mesa llvmpipe 软件 Vulkan 1.4.305**，每模式各 4 张帧图，共 12 张；
JIT 69、trimmed JIT 63、AOT 63 条 pass assertions，退出均为 0，生产窗口日志 diagnostics 为空。
覆盖 compute 读回 42、HDR16F > 1、灯光/遮挡像素差、可见 GPU 粒子与 2D MSAA。
这是软件 Vulkan 的真实渲染验证，不是实体 GPU 性能或兼容性测试；headless 跳过图形断言，不能替代窗口验证。
严格成功须有 `FORK_VALIDATION_OK` 和退出 0，超时/强制退出不是通过。

微基准是阶段样例：interop 信号 787.27→141.52 ns/call（6 次/variant），
构造阶段普通 GC 隐式构造 3387.55→2216.11 ns、managed 296→104 B；其余路径存在波动和退化样例。
NoGC control 单列，不当作普通游戏平均；不把这些数据换算成统一 FPS 提升。
原始完整 case 分布见 [interop](../godot-interop-stage1-artifacts/benchmark/summary.json)、
[普通 GC 构造](../godot-constructor-stage2-artifacts/benchmark-normal-gc/summary.json)和
[NoGC 对照](../godot-constructor-stage2-artifacts/benchmark-nogc-control/summary.json)。

保留诊断：trimmed JIT 的 .NET 8 native-host 一条 IL2026；glue 文档 53 条已删 API 交叉引用诊断；
历史 verbose editor export 退出中一个 Node StringName 残留，尚未归因为持续运行泄漏。
不以局部日志无警告宣称所有输出零诊断；也不以成功编译代替运行。

4c 源码统计采用固定 1c 全树统计加 Git blob 增量复核：非第三方 878,331、第三方 1,320,486、总 2,198,817 code 行，
相对官方总 2,964,308 下降 25.824%；不是对 36914aea 重新全树计数，更不是二进制或维护成本指标。
未变 C# 文件的 cloc 391 行误归类异常及证据保留在生产包报告。

### 6.3 Android 独立补丁的现状

已实现 SAF 目录打开/枚举、文件目录区分、相对导航、存在性、单级/递归 mkdir、文件/空目录删除、
受 provider 限制的同父目录 rename 与同名跨父目录 move、显式单文件 copy、共享 FileAccess 只读路径解析。
保留 opaque URI、授权树边界和 JNI Unicode；目录子项元数据批量查询，避免每子项 N+1 查询，不承诺所有路径延迟已解决。
读取不隐式创建；显式 FileAccess 写保留建父目录兼容行为。`.json` 筛选由调用者完成。

70 项测试覆盖 API 24/36 路径/contract 与 API 28/36 真实 ContentResolver adapter + 测试 provider。
另有两种官方 JNI header 下各 17 断言、真实 OpenJDK JNI 13 次 Unicode/BOM roundtrip、native 路径/copy 15 断言。
Kotlin 使用真实 Android 16 API 和官方 4.7.2 AAR 依赖编译；这不是完整 Android build。
SAF `copy_dir` 当前在目标变更前返回 `ERR_UNAVAILABLE`；无跨 provider 原子 rename、覆盖 rename 或隐式 copy/delete 模拟。

[Android 交付说明](../godot-android-saf-delivery/README.md)记录编码约定和所有边界。
这些实现尚未进入桌面精简源码；“合并安卓改动并定制导出”是第 7 节暂缓待办，目标仍为 .NET 10 / CoreCLR 普通预编译 APK 模板。

后续独立的 [CoreCLR 包与 host 调查](../godot-android-coreclr-research/AUDIT.md)已保存：
确认了官方 Android CoreCLR runtime pack 的内容和原生 host 机制线索，但现有 Godot Android 的 Mono 专用加载接口仍需适配，
不能只改 TFM 或 `UseMonoRuntime`。这是包/源码/静态布局调查，**没有构建或运行 APK，也没有真机验收**。
`integrate/net10-android` 分支保留，迁移草稿未合入主树。原临时工作树已移除；28 个修改/新增文件、完整 patch、base、status 和校验清单保存在 [迁移草稿档案](../cleanup-records/current-only-20260930/net10-draft/README.md)。研究记录与已逐文件校验的一套 .NET 10 SDK 安装保留，重复下载包已回收。

### 6.4 简洁产物索引

| 用途 | 相对目录 / 入口 | 选择规则 |
|---|---|---|
| 当前 Linux 冻结包 | [godot-delivery-classdb-4c802b3b](../godot-delivery-classdb-4c802b3b/README.md) | `source-4c802b3b.tar.gz`、editor、Release、三模式小场景、build-linux.sh、reports/logs |
| 最新已封存 Windows 实验包 + SDL 修复 | [manifest-36914aea.json](../godot-windows-minimal/delivery/manifest-36914aea.json) | `godot-windows-extra-sdl-fixed-36914aea.zip`，另附 `source-36914aea.tar.gz` |
| Windows 精确结果 | [results.json](../godot-windows-minimal/artifacts/sdl-bool-fix-36914aea/results.json) | 主 EXE 及 console wrapper 仅保留在最新 ZIP 的 `none/`、`true_lto/` 中，散装重复副本已回收；wrapper 不计主 EXE 大小 |
| Windows 复现/警告 | [最终证据目录](../godot-windows-minimal/artifacts/sdl-bool-fix-36914aea/) | `reproduce-minimal-templates.sh`、`controlled-comparison-check.json`、`warning-diff.json`、probes/日志；大 PE dump 的精确原件在清理证据归档 |
| Android CoreCLR 调查 | [AUDIT.md](../godot-android-coreclr-research/AUDIT.md) | 包/host 可行性线索与静态证据；非 APK 构建或运行结果，独立迁移草稿暂缓且未合并 |
| 独立 Android 源码补丁 | [godot-android-saf-delivery](../godot-android-saf-delivery/README.md) | 官方基线按 5cbce230 → fdaea11a 顺序应用；没有 APK 或 template |
| 历史报告（旧包已回收） | [纯裁剪](../godot-delivery-2d-dotnet/README.md)、[1c 优化报告](../godot-delivery-dotnet-optimized/README.md) | 仅保留报告、结果、日志及小型增量 bundle；旧源码/编辑器/模板/导出 tar 已删除。原 README 中的旧包列表属于历史记录，恢复源码使用 Git |

Git bundle 是增量历史：Linux 优化 bundle 依赖 `177d7f65`，Windows bundle 依赖 `4c802b3b`；
纯裁剪 bundle 依赖官方基线。已做各自恢复检查，不把它们称作无前提的完整仓库。
产物完整 hash 以各目录 manifest / `SHA256SUMS` 为准；下列为本次从冻结清单读取的关键值。

| Windows 交付文件 | 压缩文件 B | SHA-256 |
|---|---:|---|
| `godot-windows-extra-sdl-fixed-36914aea.zip` | 28,292,938 | `b5512e2e926e018c1071c96eb8278f1340e18a3b34ca803c9e188343d7202a1f` |
| `source-36914aea.tar.gz` | 64,015,138 | `897ab8156675c087231442c8fa273f31b9f0dd9946b009791ff447d562c6933a` |

主 EXE 校验：

- 非 LTO：`f63fa1b861e2bf3db313ca7008baa44b2176dbe83e004adc04016ccaf20a5fd0`
- 真 LTO：`483b0b13c3d25931d8ccddd1fe661a47e71f2b8622bfb93ad62ea68311473643`

Linux 整包校验：[SHA256SUMS](../godot-delivery-classdb-4c802b3b/SHA256SUMS)；
逐二进制、三模式文件清单与帧图校验分别在 `reports/binary-sizes.json`、`reports/three-mode-sizes.json`、`reports/production-validation.json`。
压缩传输大小与运行目录大小不得混算。源码 archive 不带 Git 元数据；跨机器重建不承诺字节一致。

<a id="roadmap"></a>
## 7. 分级待办与验收

**优先级是建议顺序，不代表所有待办均应立即实施。** Android A1/A2/A3 曾获授权开展独立集成，
完成包/host 调查后，用户最新决定暂缓；迁移草稿不合并，APK 构建和真机验收未完成。
全平台 .NET 10 升级目标保留，但当前整体未完成、未合入，本轮也不继续桌面迁移；生产仍以 .NET 8 为基线。
恢复前重新确认范围与优先级。其余未完成项仍按各自状态保留，既有冻结包不覆盖；Windows CI 状态见第 5.5 节。
重新启动任一项时，先确认用户优先级、源码/工具链版本及届时上游状态。
P0 是相应平台发布前的验证/风险门槛，P1 是功能与平台完善，P2 是候选精简/性能优化，P3 是远期架构研究。

### 7.1 已完成与当前不再开放的事项

- 已完成：2D/.NET 桌面物理裁剪、三 PR 选择性移植和本地正确性修正、Linux 三模式闭环
- 已完成：ClassDB custom-instance 宏及八处迁移、4c 正常 profile 生产包重建
- 已完成：Windows WGL include 清理、最终 extra profile、真实 MinGW LTO 复现与匹配比较
- 已完成：SDL 两行声明修复及两模式重建，5→3 条警告；不再把 SDL 作为待修警告
- 已完成本地配置：Windows-only / LTO-only template CI 与 tag 草稿发布逻辑；真实 GitHub/MSVC 首跑尚未完成
- 已完成但隔离：Android SAF 基础源码与局部测试；完整构建/设备/桌面合并仍未完成
- 已撤回并暂缓：`deprecated=no` 与完整 2D 导航 API 裁剪；当前分别为 `deprecated=yes`、导航 API 保留

### 7.2 P0：发布前验证与类型边界

| ID / 状态 | 为什么优先 / 依赖 | 验收条件 |
|---|---|---|
| W1 Windows 原生与 .NET 运行：待验证 | 现有结果只有交叉编译；依赖可运行 Windows、Vulkan 设备、匹配 fork SDK/绑定、self-contained payload | 启动/退出、Vulkan/2D、SDL 手柄热插拔、输入/音频、FreeType/MSDF/字体、WinRT；JIT/trim/AOT 各自真实发布运行、资源/泛型/信号/生命周期回归，分别记录不支持和未验证项 |
| W2 ICU 类型/数据边界：暂缓设计与验证 | 剩余 1 条真实 ODR；依赖统一存储/声明与解析方案、W1 平台和可控 ICU 数据 | 空 stub 与 editor 完整数据、外部 dat、CJK/泰文分词换行、整形、缺失/错误数据；非 LTO/LTO 行为一致，保留全数据/对齐，不以布局吻合或压警告宣称修复 |
| W3 WinRT MinGW 桥接：暂缓设计与验证 | 剩余 2 条真实 ODR；依赖真实 WinAPI 原型/显式转换方案、W1 和依赖维护策略 | activation、MTA usage、DLL fallback、TTS、HDR/显示及可用 emoji UI；包含顺序/C++20/异常/LTO 和目标架构检查，不将 x64 probe 推广到全部 ABI |
| A1 Android 完整构建：最新决定暂缓，未验收；Android 发布 P0 | 局部编译不能证明产物可用；依赖 JDK/Android SDK/build tools/NDK、许可、所选 ABI 与匹配 .NET 10 SDK/CoreCLR/JAR，先独立 SAF 分支后适配分支 | 维护端完整 native + Gradle tests/模板构建；JNI/SCU/普通构建发现与链接正确；生成匹配 net10.0 的预编译 APK 模板，runtime/managed payload 按 ABI 正确打包；最多 8 jobs |
| A2 Android 设备/provider：最新决定暂缓，未验收；Android 发布 P0 | 无真实 binder/授权/provider 或 .NET 10 CoreCLR 模板导出运行实证；依赖 A1、目标 ABI 设备和可丢弃测试树 | net10.0 游戏经普通预编译模板导出 APK，无需游戏侧 Gradle build；真机安装、CoreCLR 初始化、2D/.NET 运行/退出及资源/SAF 可用；ExternalStorage、Downloads、SD 卡与云 provider 的授权/失败/并发/特殊文件名、延迟和查询次数均留证据 |

W2/W3 尚无造成现有 EXE 运行故障的实证，也没有 Windows 运行通过证据。
完成类型修复或有证据的风险评估与运行验收之前，不把 LTO 构建成功作为发布安全结论。
A1/A2 是 Android 的发布门槛，不阻塞仅桌面范围的源码整理。
Windows-only CI 的真实首跑、产物下载与 tag 草稿发布链验收也是 W1 的交付前置；配置检查成功不代替 GitHub Actions 运行。
细节见 [ICU/WinRT 研究](#research-windows)与 [SAF 研究](#research-android)。

### 7.3 P1：平台扩展与 SAF 功能完善

| ID / 状态 | 为什么 / 依赖 | 验收条件 |
|---|---|---|
| U1 全平台 .NET 10 升级：目标已确认，未完成/未合入，本轮暂不继续 | 用户已明确受支持及计划支持平台统一到 .NET 10；依赖各平台 SDK/runtime/绑定配对和回归，不能以 Android 调查代替桌面验收 | 在独立工作中迁移并验证 editor、GodotSharp/SDK、模板和发布模式；更新对应 CI 后实际运行通过；当前生产 .NET 8 保持原状，草稿不合并，恢复前重新确认 |
| A3 合并安卓改动并定制 .NET 10 CoreCLR APK 导出：最新决定暂缓，草稿未合并/未验收 | 当前目标为 net10.0 + CoreCLR 的普通预编译 APK 模板；fork 已物理删 Android 和 .NET 移动路径，不能直接 cherry-pick 即宣称支持；依赖 A1/A2 与匹配 runtime/JAR/ABI/模板 | 恢复 Android、Java/Kotlin/JNI、构建与 editor 导出链，适配两 SAF commit；生成并配对 .NET 10 SDK/绑定/CoreCLR/JAR/预编译 APK 模板；游戏作者无需走 Gradle 即可导出普通 APK；目标 ABI 真机验证、桌面回归不退化 |
| A4 SAF 安全递归 copy_dir：暂缓 | 当前有明确功能缺口但安全失败；依赖 URI-aware join、冲突/覆盖语义、provider 能力和失败安全设计、A1/A2 | 编码/opaque ID 保真；拒绝自身/子目录；明确部分成功、取消/撤权/离线/满盘；只清理由本次创建且允许删除的内容；既有目标和树外内容不损坏，非 SAF 不退化 |
| A5 SAF 路径性能：待测量、候选优化 | 已消除子项 N+1，祖先解析与 provider 延迟仍在；依赖 A2 真实查询/延迟基线 | 大目录/深路径/不同 provider 的查询数和 p50/p95 等实测；缓存若引入需覆盖撤权、rename ID 变化、并发修改/过期；减少查询不牺牲授权边界和正确性 |
| A6 .NET 11 GA 后跟进：条件待办 | 先完成 A3 的 .NET 10/CoreCLR APK 路线；依赖 .NET 11 正式发布及可用的 Android CoreCLR/依赖支持 | 跟进适配评估，核验 SDK/runtime/JAR、TFM、ABI、APK 打包与真机兼容；通过后升级配套预编译模板，不以 GA 本身当作验证通过，不因 GA 自动启用 Android NativeAOT 或把桌面升级到 .NET 11 |

A3 的技术方向已明确：**提供基于 .NET 10（TFM `net10.0`）/CoreCLR 的普通预编译 APK 模板导出**。
先恢复 Android 平台与导出链、适配合并 SAF，配对 .NET 10 SDK、CoreCLR runtime、Java/JAR 依赖、
引擎/绑定与各目标 ABI 的预编译 APK 模板，再验收游戏导出、安装和真机运行。
游戏作者使用模板导出普通 APK，**不要求每次导出都走 Gradle 项目构建**；这不代表维护者构建/测试模板时不用 Gradle，
也不代表游戏作者可以省去实际导出所需的 .NET/Android 工具和签名配置。
Android 插件、自定义 Gradle 构建或 AAB 如有需要，属于另行选择和验收的可选路线，不默认扩大本轮普通 APK 目标。

**这是一项待实现、待验证的定制目标，不是现成稳定能力声明。** .NET 10 Android CoreCLR 的成熟度、
最低 Android 版本、目标 ABI、runtime/JAR 配对、host/JNI 加载、生命周期和第三方依赖兼容性都须实际验证。
先建立未裁剪的可用导出，trimming 另行评估；不能只改项目 TFM 或放宽校验就宣布支持 .NET 10。
用户随后明确了**全平台升级 .NET 10** 的方向，取代之前“Android 单独升级、桌面维持 .NET 8”的计划限制。
但该迁移尚未完成或合入；最新暂停决定后，本轮不继续桌面或 Android 迁移。
当前生产与 CI 仍配置 .NET 8 是实施现状，不是永久拒绝桌面升级的决定，见 U1。

后续版本路线见 A6：**.NET 11 正式发布（GA）后跟进 Android CoreCLR 适配**。
核对 SDK/runtime/JAR、目标 TFM/ABI 和 APK 打包链，完成真机验证后再升级相应预编译模板。
正式发布不是兼容性或运行通过证明；Android NativeAOT 仍按其独立成熟度条件延期，不随 .NET 11 GA 自动启用。

上游现状仅作迁移背景：官方 `4.7.2-stable` / `ed1daf0bf` 的
[Android exporter](https://github.com/godotengine/godot/blob/ed1daf0bf001b61586d9930840f2f1394092c079/platform/android/export/export_plugin.cpp#L2935-L2952)
在非 Gradle 路径硬检查 `net9.0`，注释说明预编译模板的 JAR 可能只兼容 .NET 9；
[项目生成器](https://github.com/godotengine/godot/blob/ed1daf0bf001b61586d9930840f2f1394092c079/modules/mono/editor/GodotTools/GodotTools.ProjectEditor/ProjectGenerator.cs#L26-L31)
也为 Android 设置 `net9.0`。这些固定基线事实不是本 fork 的最终 .NET 10 目标，也不证明修改两处字符串便能完成升级。

**Android NativeAOT 等微软支持就绪后再评估，当前不投入适配，也不作为近期平行路线。**
重新评估的外部前提是微软对所需 Android 目标/ABI 提供明确且可核验的 NativeAOT 支持；
届时另行确认范围与优先级，不因上游状态变化自动启动实施，
全平台 .NET 10 迁移另按 U1 管理，不以等待或尝试 Android NativeAOT 作为升级完成的依据。
此决定不改变现有桌面 NativeAOT 路线及已记录的验证状态。
分层恢复平台必需内容，不顺手恢复 3D/GDScript/RPC 等已删除功能；native/managed API 与 template/SDK 要配对。
独立 Android 分支的完整上游背景和桌面裁剪冲突都需审查，保留旧 SAF 交付作为基线。

### 7.4 P2：候选精简、兼容与性能

| ID / 状态 | 为什么 / 依赖 | 验收条件 |
|---|---|---|
| N1 完整裁剪 2D 导航 API：用户暂缓 | 有精简空间但会破坏现有 MethodBinds 静态初始化；依赖 editor 允许/支持的匹配 API 生成设计和 GodotSharp/SDK 重建 | 无已删类/方法引用；全局注册、Node2D、World2D 普通物理查询、TileMapLayer、场景/资源/泛型/信号正常；误用清晰失败；同平台同配置测增益，Windows 三模式实跑 |
| D1 deprecated 兼容裁剪：已撤回，待评估 | 不应在未完成 API/资源兼容分析时继续缩小模板；依赖旧资源/API 使用清单、绑定策略 | 明确兼容取舍，匹配绑定、旧资源/当前项目与三模式回归；收益单独量化，不与其他开关混算 |
| R1 首次 PCK 全目录快照 #122438：用户暂缓 | 影响首次同步阻塞；依赖文件量/隐藏目录/忽略目录、首次和后续加载及目录枚举基线 | 磁盘/PCK 合并视图、replace_files、.godot/imported、UID、加载后新增文件都正确；不得简单跳过 .godot/隐藏目录或回滚可见性修复；性能与正确性同时验收 |
| C1 NuGet.org 接入：以后单独配置 | 当前只构建/交付本地 NuGet 包；依赖用户以后准备账号、包名/版本策略、发布权限和安全凭证，并再次授权接入 | 用户自己的发布配置就绪后，验证版本/依赖配对、预发布或受控发布与失败处理；不能把生成 nupkg、CI artifact 或 Release 附件当作已发布 NuGet.org |
| Q1 已记录诊断清理：候选维护项 | 改善升级与审查质量；依赖明确区分文档、退出残留和 native-host 契约 | 清理 53 条过期文档引用；复现并归因 Node StringName 退出残留；SDK 升级重验 native-host 内部属性/IL2026；不全局屏蔽、不无证据宣布泄漏或普遍 trim-safe |
| B1 保留功能的编译耗时优化：已列入后续计划，尚未实施 | 优先分析 `core/variant/variant_call.cpp` 与 `modules/godot_physics_2d/godot_collision_solver_2d_sat.cpp`；先固定工具链与构建配置，采集编译器阶段计时，区分头文件解析、模板实例化、优化与代码生成瓶颈 | 评估头文件依赖整理、模板复用、适当移出头文件的实现、谨慎拆分翻译单元及缓存/增量构建；同机同配置比较干净构建与典型增量构建耗时，并复验运行性能、二进制体积与行为，无功能裁剪或静默降低优化质量 |

N1 细节见 [导航研究](#research-navigation)，R1 见 [PCK 研究](#research-pck)。
Q1 是对已知诊断的维护候选，不改变 frozen binaries 或当前运行结论。
B1 先以编译器阶段数据定位原因，再决定具体改动；已有单文件耗时只作为调查线索，不能据此承诺优化收益。
本次仅记录计划，不实施构建优化，也不为此中断正在运行的 CI。

### 7.5 P3：远期架构

| ID / 状态 | 为什么 / 依赖 | 验收条件 |
|---|---|---|
| L1 libgodot + godot-dotnet 的 .NET-first：长期研究、未实现 | 目标是减少单人长期维护的 native 补丁；依赖上游成熟度、固定配对版本、用户重新确认方向 | Linux 上由真正 .NET Main 在进程内启动 libgodot、注册 Node2D/运行/退出，验证生命周期/资源/PCK/泛型/信号与干净发布；小型 2D 游戏比较性能/内存/大小和补丁面；JIT先行、trim/AOT单独门槛 |
| L2 单文件发布：远期独立候选 | 是打包/加载设计，不是 L1 首轮必要条件；依赖稳定的 native/runtime/资源发布布局与平台加载约束 | 明确嵌入/提取、PCK/原生库查找、离线无 SDK 启动、更新/失败清理和权限行为，分别按平台/模式验证，不用“一个 ZIP”冒充单文件 |

不重写完整编辑器，不要求首轮原生窗口嵌入、进程内反复重启或 AOT 热卸载，不替换/丢弃当前已验证分支。
详见 [架构研究](#research-libgodot)。

### 7.6 后续维护规则

- 先固定版本、平台、profile、compiler/SDK、LTO、优化和 strip，再比较；模块裁剪、API 裁剪、JIT trimming 与 native LTO 分别归因
- 生产补丁分主题独立提交；native callbacks、managed ABI、generator 和匹配绑定共同审查
- 保留旧包和失败负对照；新结果写新提交专属目录，产物 manifest 记录 source SHA、工具链、配置、字节/hash、已跑/未跑测试；历史实验结束后不默认再建非 LTO 模板
- 总并行度最多 8 jobs；native 与 managed 大构建不叠加超限，MSBuild 使用单节点/受控并行
- 构建或静态检查不能代替运行；软件 Vulkan 不能冒充硬件 GPU，Linux 结果不能自动变成 Windows/Metal/Android 结果
- 保留 Godot MIT 许可、`COPYRIGHT.txt` 和实际随包第三方/runtime notices；不把源码量统计当法律归属或维护成本
- 文档与源码的新提交不会自动更新冻结 archive；只有另行决定新版本发布时才重建、回归和打包

<a id="research"></a>
## 8. 长期研究细节与参考

以下从原独立长期规划完整迁入，保留研究证据、约束和链接；优先级/状态以第 7 节为准。
来源均截至 2026-09-30 的调查，重新启动时须重查上游，不把静态阅读或 probe 写成完整平台验证。

<a id="research-libgodot"></a>
### 8.1 libgodot 与 godot-dotnet 的 .NET-first 工具链

更新：2026-09-30

#### 状态

长期方向，暂不实施。研究内容已归并到本文，不替换当前已验证分支；新平台和优化按第 7 节分别推进。

#### 目标

探索一个能够由单人长期维护的 .NET-first 2D 桌面开发工具链。重点是减少需要自行维护的原生引擎补丁，而不是重写整个 Godot 编辑器。

#### 候选架构

- .NET 可执行程序拥有 Main、配置、日志、业务逻辑与测试。
- libgodot 提供窗口、渲染、输入、2D 物理、音频与场景树。
- godot-dotnet 提供 GDExtension API 绑定；仅添加必要的宿主适配与构建发布工具。
- 尽量跟随官方原生引擎，使用清晰的构建配置和少量必要补丁。
- 继续复用 Godot 编辑器、资源导入流程及 PCK 打包，不在首版重写这些部分。
- 业务数据尽量保持为普通 .NET 类型；与 Godot 交互的边界保持明确。

#### 已知现状与风险

这是源码与文档评估，尚未编译或运行此组合。

- 官方 libgodot 核心已经合入，自 Godot 4.6 起有基础支持；这不等于完整窗口嵌入或进程内反复停止、重启已经完善。
- godot-dotnet 仍为 WIP，API 尚可能破坏兼容。此次查看的提交为 965a09db8b42f79d7987270063eb19abb985ceb6，使用 .NET SDK 10.0.103，默认目标 net10.0。
- 现有主要文档路径是 Godot 加载 .NET GDExtension；让 .NET Main 主导引擎生命周期仍需适配，不是两个现成包直接组合便完成。
- 引擎、API dump、绑定生成器和 SDK 必须固定为验证过的配对；本次分别查看的版本不构成兼容保证。
- GC、RefCounted、QueueFree、信号回调、主线程与异步恢复需要专项验证。
- trimming/AOT 兼容声明不等于所有泛型、集合和外部依赖均已安全。现有开放问题与修复提案仅作验证线索，不能当作已复现结论。
- 如果仍要求物理删除全部 3D 原生源码，这部分分叉维护成本不会因更换绑定方式消失。

#### 未来重新启动时的最小验证范围

须先重新确认届时上游状态与用户优先级，再开始实现。

1. 先选 Linux 单平台，固定版本，从干净环境可重复构建。
2. 由 .NET Main 真正启动 libgodot，注册自定义 Node2D、运行若干帧并干净退出；不能仅启动另一个 Godot 可执行程序来代替验证。
3. 验证自定义类、属性、Resource、集合、信号及场景保存加载。
4. 压力测试对象释放、原生与托管生命周期、异步主线程恢复。
5. 无 SDK、编辑器和开发缓存的发布环境，能够加载图片、字体、音频与 PCK。
6. 用一个小型 2D 游戏比较启动、内存、发布体积、调用成本和实际维护改动面。
7. 普通 JIT 发布先成立，trimming、NativeAOT 各自设置单独验收门槛，不以声明或编译成功代替运行验证。

#### 首轮不做

- 不重写完整编辑器。
- 不要求原生窗口嵌入到其他 UI 框架。
- 不要求同一进程内反复重启引擎或 AOT 热卸载。
- 不把单文件发布作为必要条件；它属于未来独立的打包与加载设计。
- 不替换或丢弃当前已验证的 2D/.NET 精简分支。

#### 参考

- godot-dotnet：https://github.com/godotengine/godot-dotnet
- 官方 libgodot Core：https://github.com/godotengine/godot/pull/110863
- godot-dotnet 使用说明：https://github.com/godotengine/godot-dotnet/tree/965a09db8b42f79d7987270063eb19abb985ceb6/src/Godot.Bindings
- .NET single-file：https://learn.microsoft.com/en-us/dotnet/core/deploying/single-file/overview

此架构研究不代表当前阶段新增实施任务。

<a id="research-pck"></a>
### 8.2 首次加载 PCK 的全目录扫描（#122438）

记录：2026-09-30。状态：用户暂缓，已合入统一待办 R1；本次不实施。

- 问题：散文件项目首次调用 load_resource_pack() 时，先同步递归扫描整个 res:// 并建立目录快照，再挂载目标 PCK。开销随项目文件数量增长，隐藏目录也被扫描，且此路径不按 .gdignore 过滤。
- 当前核查：精简分支 1c26d474 及 ClassDB 修复 4c802b3b 均保留该路径。issue 中约 7.38 秒为用户历史测量，本轮没有重新进行性能复测。
- 原因背景：原修复为解决挂载 PCK 后散文件目录不可见的问题；全目录快照同时带来首次阻塞及后续新增文件的枚举局限。
- 未来先做：固定小 PCK，对比不同项目文件量、隐藏目录及忽略目录，测量首次/后续加载和目录枚举行为，建立可重复基线。
- 候选方向：评估按需合并磁盘与 PCK 的目录视图，避免首次遍历整个项目；若采用短期过滤优化，须先明确排除规则的语义。
- 验收约束：保留散文件可见性、同名文件及 replace_files 覆盖规则、.godot/imported 导入资源、UID 和加载后新增文件的正确行为。不能简单回滚旧修复，也不能整体排除隐藏目录或 .godot。
- 重启条件：重新确认上游进展及用户优先级后，再决定实现范围。

参考：
- 用户 issue：https://github.com/godotengine/godot/issues/122438
- 原修复 PR：https://github.com/godotengine/godot/pull/90425
- 原目录可见性问题：https://github.com/godotengine/godot/issues/19815


<a id="research-android"></a>
### 8.3 Android SAF 剩余能力与设备验证

记录：2026-09-30。最新状态：A1/A2/A3 集成暂缓，保留 .NET 10（net10.0）/CoreCLR 普通预编译 APK 模板目标；调查和迁移草稿已保留但未合并，没有 APK 或真机验收。全平台 .NET 10 目标见 U1，当前生产仍为 .NET 8；A4/A5 仍为后续事项。

#### 平台导出方向

先恢复精简 fork 的 Android 平台和导出支持、适配合并 SAF，再配对 .NET 10 SDK/CoreCLR/JAR 与目标 ABI 的预编译 APK 模板。
普通 net10.0 游戏的 APK 导出不要求游戏作者走 Gradle；维护端构建/测试模板仍可使用 Gradle。
Android 插件/AAB 等 Gradle 路线按需要另行设计，不默认纳入当前目标。
.NET 10 Android CoreCLR 的成熟度与兼容性须以实际构建、导出和真机运行验收，不视为现成稳定支持；
官方 4.7.2 非 Gradle 路径的 net9.0 检查仅为上游迁移背景，具体见 A3。
Android NativeAOT 等微软所需支持就绪后另行评估，当前不做适配；
全平台 .NET 10 目标已确认但本轮迁移暂停，尚未合入，现有桌面 .NET 8 源码/CI/冻结包保持原状；
桌面 NativeAOT 不受这项 Android 决策影响。
.NET 11 GA 后按 A6 跟进 Android CoreCLR 适配，runtime/JAR/ABI/APK 真机验证通过后升级配套模板；
不会因 GA 自动启用 Android NativeAOT 或把桌面升级到 .NET 11。具体依赖和验收以 U1/A1/A2/A3/A6 为准。

#### 已完成的基础

- 独立完整上游分支 android-saf-diraccess 已提交 5cbce230、fdaea11a，基于官方 4.7.2-stable ed1daf0bf；未改动桌面精简分支及原交付包。
- 已实现 SAF 目录打开、枚举、文件/目录区分、相对导航与存在性检查，具备遍历目录、按文件名筛选 .json 并构造可交给 FileAccess 的正确路径所需的基础能力；应用自己的 .json 筛选逻辑仍由调用方完成。
- 已实现单级/递归 mkdir、文件及空目录删除、受 provider 能力限制的同父目录重命名和同名跨父目录移动。保留 URI 编码与授权树边界，读取/查询不隐式创建，显式 FileAccess 写入保留原有建父目录兼容行为。
- 现有验证：70 项 Robolectric/JUnit 多 API 测试通过，受影响 Kotlin 源码对真实 Android API 编译通过，并完成 native 语法、JNI Unicode 与路径分发验证。这些是本地验证，不能替代完整 Android 构建或真实设备/provider 验证。

#### 待办一：安全的 SAF 递归目录复制

目前 SAF copy_dir 在修改目标之前明确返回 ERR_UNAVAILABLE；显式单文件 copy 已支持正确解析 SAF 路径。

未来实施前须明确并覆盖：

1. 统一 URI-aware 路径连接，保留 opaque document ID 与基址编码；正确处理原始枚举名称和绝对路径的编码边界，覆盖字面 %2F、%、#、Unicode/emoji、反斜线以及异常 provider 文件名，不能重复解码或退回物理文件路径。
2. 明确同名目标、文件/目录类型冲突、覆盖、跳过与报错策略；不得默认删除既有目标或把未授权覆盖作为成功路径。
3. 拒绝复制到自身或自身子目录，并考虑 provider 别名、多父目录及 document ID 变化；不能只靠 URI 字符串前缀判断。
4. 明确取消、撤权、离线、空间不足、provider 报错和部分成功时的行为。跟踪本次操作产生的半成品并报告；清理只能针对可确认由本次操作创建且允许删除的内容，不能扩大到既有文件或授权树外。
5. 保留非 SAF 目录复制行为，补查询规模、失败传播与跨后端回归测试。不得用复制后删除来静默模拟原子 rename。

#### 待办二：完整 SDK/NDK/APK 构建

- 按届时仓库要求配置并验证 JDK、Android SDK、build tools、NDK 及匹配的 .NET 10 SDK/CoreCLR/JAR；固定验证过的版本配对，涉及新的许可或权限时先完成必要确认。全平台 .NET 10 配套升级仍需按 U1 单独验证；本轮暂停期间不继续修改现有桌面 .NET 8。
- 在独立分支执行完整 native Android 构建、维护端 Gradle 测试与 net10.0/CoreCLR 预编译 APK 模板打包，构建并行度保持最多 8 jobs；普通游戏模板导出不得要求作者再走 Gradle build。
- 验证实际 Android JNI、SCU/普通构建的测试发现与链接、Java/Kotlin/native 接口匹配；适配后的普通 APK 导出须验证 .NET 10 CoreCLR runtime、JAR、managed payload、模板与目标 ABI 匹配及真机运行。不得把现有 host 语法检查、官方 AAR 依赖编译或仅修改 TFM 校验标记为完整支持。

#### 待办三：真机与真实 DocumentsProvider 验证

- 使用可丢弃的测试目录，在真实 Android 设备上验证 ExternalStorageProvider、DownloadsProvider、SD 卡树及至少一个云端 provider；按可用设备覆盖不同 Android 版本。
- 验证目录枚举、导航、.json 文件筛选与 FileAccess 读写，以及 mkdir、重命名后 document ID 变化、受支持的同名移动、空目录删除和显式单文件复制。
- 覆盖只读授权、撤销授权、持久授权后的进程/设备重启、离线或加载中的 provider、空/失败查询、特殊文件名和大目录；实测延迟与查询次数，不把本地减少 N+1 查询等同于已解决所有性能问题。
- 专项验证并发目录变化与失败安全。SAF 无原子的“仅为空才删除”接口，现有删除前检查不能消除 provider 侧竞态；不得承诺与本地文件系统完全相同的原子语义。

#### 明确的语义边界

- 跨 provider 原子 rename 没有通用保证，不列为必须强行实现的功能，也不承诺通过复制/删除补出原子性。
- 若未来需要跨 provider 传输，应单独设计、明确请求的复制操作及其冲突、部分完成和清理策略；是否删除源内容属于另行明确的操作，不能隐含在 rename 中。
- 用户最新已暂缓 A1/A2/A3 的独立 Android 集成、构建与导出验证，草稿不合并，既有冻结包不覆盖；全平台 .NET 10 目标未取消但尚未完成。恢复以及 SAF 递归复制、后续性能优化等事项仍须按范围重新确认。

参考：
- Android DocumentsContract：https://developer.android.com/reference/android/provider/DocumentsContract
- 原 SAF 支持与 rename 返回 URI 的限制：https://github.com/godotengine/godot/pull/112215
- SAF DirAccess 文档问题：https://github.com/godotengine/godot-docs/issues/11699
- SAF 访问性能问题：https://github.com/godotengine/godot/issues/122726


<a id="research-navigation"></a>
### 8.4 彻底裁导航 API 与匹配 GodotSharp 生成验证

记录：2026-09-30。状态：用户明确暂缓，已纳入 N1/D1。

- 当前 Windows 第二组精简配置保留 `disable_navigation_2d=no`，但 `module_navigation_2d` 仍关闭；导航 API/节点存在，后端退回 dummy，不等于寻路可用。
- 待做：在独立配置中彻底删除 2D 导航 API、节点、资源和 TileMap 导航相关代码，并配套生成、构建和验证匹配的 GodotSharp/SDK，而不是复用较完整 API 包后仅要求业务代码“不调用导航”。
- 已确认的兼容风险：`World2D.get_navigation_map` 和 `TileMapLayer` 的导航方法会被 `NAVIGATION_2D_DISABLED` 裁掉，旧生成绑定却在同一嵌套 `MethodBinds` 静态类型初始化时一并请求；缺失方法可能牵连普通 World2D 物理查询或 TileMapLayer 方法。
- 先核查绑定生成路径：现有 editor 不允许直接使用 `disable_navigation_2d=yes`，需设计明确、可维护的匹配 API 生成方案，不能绕过限制后假定所有 editor 代码仍可用。
- 验收至少覆盖：生成绑定不引用已删 native 类/方法；全局 NativeProxyRegistry/构造注册正常；普通 Node2D、World2D 物理查询、TileMapLayer、场景/资源加载、泛型集合和信号正常；意外使用已删 API 时有明确失败。
- 在相同平台、编译器、优化与符号政策下测量真实新增体积收益，并分别验证 JIT、Trim、NativeAOT；编译通过不代替 Windows 运行。
- `deprecated=no` 本轮同样已撤回，保持 `deprecated=yes`；旧 API 兼容裁剪及其绑定改造先不纳入本轮实现。
- 重新开始前确认届时用户优先级、源码版本和绑定策略。

源码线索：`scene/resources/world_2d.cpp`、`scene/2d/tile_map_layer.cpp`、`modules/mono/editor/bindings_generator.cpp`、生成的 `World2D.cs`。


<a id="research-windows"></a>
### 8.5 Windows LTO 的 ICU / WinRT 类型边界与运行验证

记录：2026-09-30。状态：原先同意单独记录并继续验证；已纳入 W1/W2/W3。本次只归并设计和后续验收，不修改 ICU/WinRT 生产实现，不屏蔽警告。

#### 本轮已确定与已批准的小修复

- Windows x86_64、MinGW GCC 14.2.0 POSIX / binutils 2.44、`-Os`、真实 linker plugin + 普通 COFF + `-flto-partition=none` 的最终 extra 配置原有 5 条警告。
- SDL 两条为已确认的声明错误：头文件返回 `int`，C17 实现返回 `bool` / `_Bool`。上游 SDL #14804 由 55acc0b82954a50758cf4774365cc1e2877816c6 修复，仅需把两行声明改为 `bool`。
- 本 fork 已最小回移并独立提交 36914aeaa5d291f417afc83c70eb3d03feea19b3；匹配非 LTO / 真 LTO 重建的结果以该提交专属构建报告为准。忽略返回值降低当前实际影响，不能消除原来的语言级类型不兼容。
- 此提交最终匹配重建已完成：非 LTO 33,670,144 bytes（32.1104 MiB，0 条警告）；真 LTO 28,708,352 bytes（27.3784 MiB，仅剩 ICU/WinRT 3 条 ODR）。LTO 节省 4,961,792 bytes / 14.7365%；两模式大小与修复前相同。新结果和旧包均在本地保留，未上传，仍未完成 Windows 运行验证。

#### 待办一：ICU 数据入口的声明、存储与解析边界

- `thirdparty/icu4c/common/udata.cpp:646` 把 `icudt_godot78_dat` 声明为 `DataHeader`；`modules/text_server_adv/icu_data/icudata_stub.cpp:44` 把相同 `extern "C"` 符号定义为不同的 `ICU_data_header`。
- 目标工具链的独立静态断言已确认 `DataHeader` 为 24 字节、stub 为 56 字节，`info` 位于偏移 4、padding 24、count 32、reserved 36、目录占位 40。这是有意的数据包前缀视图；空目录 count=0，不会读取实际目录条目。
- 修复前实测 LTO EXE（SHA-256 `c1b924ec1b7048f68174cd5bd03a6b0069306892281c20c07a36c2de058ed74d`）含唯一完整 stub，`.rdata` RVA `0x1671a80`，16 字节对齐，magic、headerSize=32、ToCP 格式与版本正确。该事实只对应这一个产物，不保证未来编译器/布局。
- 类型名不同和相同外部符号的声明不一致仍然是真实类型契约问题；前缀布局相同不等于 C++ ODR 或别名规则安全。不能仅重命名类型、把 56 字节数据截为 24 字节，或用关闭 LTO/警告冒充修复。
- 未来先评估统一的存储/声明类型及显式字节数据解析边界，保留完整数据、对齐与内部数据读取语义；同时覆盖 editor 的完整数据数组和 template 的空 stub，不能只消掉其中一种配置的诊断。
- 验收覆盖空 stub、外部 `icudt_godot.dat`、中文/日文/韩文及泰文等分词/换行、字体整形、缺失或错误数据的处理，以及相同工具链非 LTO / LTO 的行为一致性。

#### 待办二：WinRT 的 MinGW 类型准确桥接

- WinRT `base.h:555/557` 的 `void*` 声明经 `__asm__("LoadLibraryExW")` / `__asm__("GetProcAddress")` 接到 WinAPI；SDK 原型分别返回严格句柄 `HMODULE` 与函数指针 `FARPROC`。本工具链的 `DWORD` 与 `uint32_t` 也不是同一 C++ 类型。
- 微软当前 cppwinrt 上游仍采用这个有意的 ABI 适配。独立三文件 probe 在同一 GCC14.2、`-Os`、plugin、partition=none 下重现两条警告；最终汇编显示 SDK/WinRT 路径的参数寄存器与 import 跳转一致，64 位指针返回值完整保留。
- 这证明本次 probe 的机器调用约定匹配，不能证明任意优化均安全，也不能仅凭指针同宽宣布无 UB 或误优化风险。
- 未来评估 MinGW 专用的真实 WinAPI 原型与显式转换包装层，避免不兼容原型直接别名到同一外部符号；保留 WinRT 功能，评估生成头和依赖更新后的可维护性，不直接临时修改生成依赖后即称永久修复。
- 验收覆盖 WinRT activation、`CoIncrementMTAUsage` 与 DLL activation fallback、OneCore TTS、显示/HDR信息及可用的 emoji UI；核对不同包含顺序、C++20/异常设置、目标架构及 LTO 开关，不盲目扩展 x64 的结论到 x86/ARM64。

#### 共通验收与边界

- 保留原始警告与修复前后日志；仍存在的 ICU / WinRT 三条应明确记录，不用 `-Wno-odr`、`-Wno-lto-type-mismatch` 或全局 `-fno-strict-aliasing` 隐藏问题。
- 原生构建和静态 probe 不能替代 Windows 运行。后续在 Windows 验证启动、Vulkan画面、SDL手柄热插拔、音频、字体、WinRT 和同 fork .NET 项目；JIT/Trim/NativeAOT 分别验收。
- 维持总并行度最多 8 jobs，保留旧测量包；固定配置、工具链与 strip 政策，以主 EXE 字节数和 SHA-256报告。原生模板体积不包含 GodotSharp、游戏程序集或 .NET runtime。
- 本轮未发现后三条已导致现有 EXE 运行故障的实证，也没有完成 Windows 真机运行；重启实现前重新确认用户优先级和上游进展。

参考：
- SDL 已确认问题：https://github.com/libsdl-org/SDL/issues/14804
- SDL 最小修复：https://github.com/libsdl-org/SDL/commit/55acc0b82954a50758cf4774365cc1e2877816c6
- Godot ICU 前缀设计说明：https://github.com/godotengine/godot/issues/45179#issuecomment-760184103
- ICU stub 结构：https://github.com/unicode-org/icu/blob/main/icu4c/source/stubdata/stubdata.h
- 微软 WinRT 声明：https://github.com/microsoft/cppwinrt/blob/master/strings/base_extern.h
- GCC LTO 不兼容类型说明：https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Optimize-Options.html
- Windows x64 调用约定：https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention


#### SDL 修复后最终产物补充

上述 ICU 旧 EXE hash `c1b924ec...` 是修复前的布局证据，保留作历史对照。
`36914aea` 最终 LTO 主 EXE hash 为 `483b0b13c3d25931d8ccddd1fe661a47e71f2b8622bfb93ad62ea68311473643`，
[最终 blob 检查](../godot-windows-minimal/artifacts/sdl-bool-fix-36914aea/icu-final-blob-check.json)
同样找到唯一 56-byte stub、`.rdata` RVA `0x1671a80`、16-byte 对齐。
这仍不证明语言层 ODR/aliasing 正确，不改变 Windows 尚未运行的事实。


<a id="upstream-first-batch"></a>
## 9. 首批上游正确性回移

记录：2026-09-30。基于本地 `dbd171fd3ec2e13bc9624ea161d57aefe5053b11`，按完整上游审计的首批 12 项进行小型正确性修复。保持当前 2D 裁剪、自有静态 C# metadata、.NET 8、JIT/Trim/NativeAOT 架构；不包含 .NET 10 或大规模 GDType/ClassDB 迁移。

### 9.1 来源与适配

| 上游 PR | 本地处理 |
|---|---|
| [#98396](https://github.com/godotengine/godot/pull/98396) | 只取 `ScriptPropertyDefValGenerator` 缺失的一处继承 setter 检查；其他 helper/metadata 已有本地实现，不重复覆盖 |
| [#121277](https://github.com/godotengine/godot/pull/121277) | `Dictionary::set_typed(ContainerType, ContainerType)` 正确使用 value script，保留本地 C# 静态类型元数据路径 |
| [#112813](https://github.com/godotengine/godot/pull/112813) | embedded PCK 起点和尾部用 `_get_pad(8, ...)` 补齐；本地额外将 helper 的位置参数拓宽至 `uint64_t`，避免超过 2 GiB 时窄化为负数导致补齐为 0 |
| [#120731](https://github.com/godotengine/godot/pull/120731) | 在较旧的 Tree 析构函数中移除第二次 `custom_ci` 释放；不引入本地不存在的 sticky-header 成员 |
| [#118229](https://github.com/godotengine/godot/pull/118229) | 普通 Callable 无效 target 检查不再仅限 DEBUG，release 返回 `CALL_ERROR_INSTANCE_IS_NULL` |
| [#121265](https://github.com/godotengine/godot/pull/121265) | Animation 向上/下移动轨道时分别限制负索引和零边界 |
| [#123546](https://github.com/godotengine/godot/pull/123546) | ClassDB default-value cache 入口加已有递归读写锁的 write guard；不迁移其他 ClassDB 实现 |
| [#120126](https://github.com/godotengine/godot/pull/120126) | shader 常量数组节点通过 `alloc_node` 纳入解析器生命周期 |
| [#121163](https://github.com/godotengine/godot/pull/121163) | mat4 × vec4 常量折叠从 matrix 参数取系数 |
| [#122653](https://github.com/godotengine/godot/pull/122653) | Canvas RD 析构前清除全部已登记 canvas texture 的失效 callback |
| [#121958](https://github.com/godotengine/godot/pull/121958) | dummy shader/material/mesh/multimesh/texture 的五个 RID owner 启用内部线程安全 |
| [#123448](https://github.com/godotengine/godot/pull/123448) | 三个 native Error-return interop 统一 `int64_t`，匹配本 fork 生成的 C# `Error : long` |

### 9.2 验证范围

本轮按批构建和回归，不对每个 PR 重跑一套完整构建。新增回归随源码提交；日志与临时导出位于工作区 `../godot-first-batch-validation/`，不纳入 Git。

- .NET 8 source-generator 全量测试：66/66 通过；新增 getter-only override 继承 base setter 的 TOOLS / 无 TOOLS 两种测试。此问题发生在生成阶段，不能描述成所有 AOT 导出都会失败
- 原生聚合构建：GCC 14 Linux editor + Mono + tests 构建通过；minimal-extra release + Mono + tests 构建通过。验证构建使用 `optimize=none lto=none`，不是新的 shipping/LTO 体积测量
- 新增原生回归：editor 8/8、随机顺序重复 8/8；release 6/6（内存计数仅 DEBUG 可用，PCK helper 测试仅 editor 可用，故 release 不伪报这两项）。覆盖释放后 Callable、Tree 析构、Animation 两侧边界、matrix/vector 常量折叠、解析器节点释放、ClassDB 四线程冷缓存与 constructor 重入、五类 dummy RID owner 并发分配/释放
- 扩大后的相关原生集合初轮为 319/320，发现一个既有失败：`Animation::add_track` 收到已删除类型时没有新增轨道，却返回 0 而非 -1。用户随后单独批准修复；按第 9.4 节修改实现并扩充断言后，当前集合为 **320/320，通过 115,242 项断言**。未降低断言或把旧故障归因于本批 move-up/down 补丁
- Managed editor smoke：35 项断言通过，包含 binary/JSON typed Dictionary 保持不同 C# key/value scripts、继承属性 native Set/Get、Error 返回值、signal awaiter 和释放后的 native Callable
- PCK 宽度边界回归：196 个真实 helper 的 alignment/residue 组合覆盖 2^31、2^32、2^40 与接近 2^63 的位置。独立编译原 helper 得到 75 个失败，修复后为 0；持久化原生测试直接调用生产 helper（protected static，仅测试 accessor 暴露），不分配巨型文件
- 三模式真实 embedded release export：JIT、Trimmed JIT、NativeAOT 各 35 项断言通过；AOT 在移除 `DOTNET_ROOT` 后再次通过（`dynamic_code=False`）。模板副本分别设置长度余数 3/5/7，实际 PCK magic 起点、footer 和 embedded region 长度校验通过；另有覆盖起始/载荷全部 8×8 余数组合的模型检查，此模型不冒充 64 次实际导出
- 已保留三模式 hidden MSBuild logs：JIT 和 NativeAOT issues CSV 为空；Trimmed JIT 有 1 条现存的 .NET 8 native-hosting `IL2026`（`ComponentActivator.GetFunctionPointer` → `InternalGetFunctionPointer`），并非零警告。先前 `static-registration-clean-trimmed-jit-msbuild.log` / `typed-collections-trimmed-jit-msbuild.log` 中已有相同警告，本批未新增。Runtime 故意调用 `Array.Resize(-1)`，预期的 `ERR_INVALID_PARAMETER` 日志被精确区分，不将其计为未预期错误
- RD canvas teardown：已准备实际 Vulkan/lavapipe 的泄漏 CanvasTexture shutdown runner，但尚未执行到 engine。当前容器的 `socket(AF_UNIX)` 返回 EPERM，Xvfb 无法启动；申请提升后的相同命令及最小 socket probe 仍失败。未修改安全/网络设置，也未把 headless dummy 运行当作 RD 验证。#122653 只有源码审阅与编译覆盖，RD 运行和 sanitizer 仍待具备图形环境时补验
- 不重新发布或覆盖冻结产物；本地 Linux 验证不替代 Windows/macOS 运行或全平台 ABI 验证


### 9.3 本地提交与复现入口

- `3c063387`：四项 core/scene 修复及原生回归（#118229、#120731、#121265、#123546）
- `b7989fd3`：四项 shader/rendering 修复及原生回归（#120126、#121163、#121958、#122653）
- `0f9e13ce`：四项 managed/export/collection 修复及 generator 回归（#98396、#121277、#112813、#123448）
- `d00abd3f`：review 后的 #112813 64-bit helper 适配与边界回归；review 通过后用修正后的 editor 重新执行全部三模式实际导出（无旧导出复用）

工作区工具链环境由 `../godot-build-tools/env.sh` 提供；其他机器应先配置等价的 .NET 8 SDK、GCC 14 与 SCons。批量验证命令：

```sh
scons platform=linuxbsd target=editor module_mono_enabled=yes tests=yes dev_build=no debug_symbols=no optimize=none lto=none accesskit=no wayland=no -j8
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*FirstFixBatch*'
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*FirstFixBatch*' --order-by=rand --rand-seed=17
scons profile=misc/build_profiles/linux_release_minimal_extra.py target=template_release tests=yes lto=none optimize=none extra_suffix=first_batch_validation accesskit=no wayland=no -j8
bin/godot.linuxbsd.template_release.x86_64.first_batch_validation.mono --headless --test --test-case='*FirstFixBatch*'
dotnet test modules/mono/editor/Godot.NET.Sdk/Godot.SourceGenerators.Tests -c Release -m:1 -p:BuildInParallel=false
```

具体 source snapshot、上游 merge/constituent SHA、完整命令日志和 smoke runner 保存在本工作区验证目录。首轮 native binary 在提交前从相同工作树源码构建，显示的 Git 版本标签可仍为 base HEAD；这不是针对最终提交的远程 CI 验证，也不与冻结交付包混用。


### 9.4 单独授权的 Animation 无效轨道类型修复

记录：2026-09-30。此项是首批回归发现后另行批准的本地修复，不算作额外上游 PR，也不改动第一批 12 项的来源范围。

- `Animation::add_track` 的 default 分支改为 `ERR_FAIL_V_MSG(-1, ...)`，无效类型立即返回失败，不创建轨道，也不再触发虚假的 `changed` 通知。有效类型的 insertion / append 路径不变
- 扩充现有测试，覆盖负值转换、已删除的类型 1–4、未知值 9/127/255，以及空/非空 Animation、不同插入位置、所有五种保留类型；检查既有轨道顺序、路径、key 数据和有效返回索引
- Editor / release 均完成增量构建；editor animation-focused **12/12**（530 断言），相关 aggregate **320/320**（115,242 断言）；release 的 animation + first-batch 集合 **17/17**（568 断言）
- Read-only review 通过。没有为这一行 runtime 错误返回修复再次执行三套导出；第 9.2 节 JIT/Trim/AOT 导出证据仍明确属于先前 12 项批次，图形环境与平台验证边界不变
- 日志：`../godot-first-batch-validation/animation-enum-{editor-build,focused,aggregate,release-build,release-tests}.log`


<a id="upstream-second-batch"></a>
## 10. 第二批小型性能回移

记录：2026-09-30。基于 `a043f328`，仅处理已批准的五项局部优化；不引入 .NET 10、PhysicsServer enum 重构或 #123968。未测量 FPS、加载耗时或 Apple shader 编译速度，不把上游的性能数字当成本 fork 的结果。

### 10.1 来源与适配

| 上游 PR / merge SHA | 本地处理 |
|---|---|
| [#110402](https://github.com/godotengine/godot/pull/110402) / `bb0e47633af893227bde5a06f5d00a765e908a24` | 删除仍会求值和构造字符串的 `print_bl` 调试宏及调用；保留 `use_real64` header 字段的读取，避免改变文件游标 |
| [#123809](https://github.com/godotengine/godot/pull/123809) / `90148f1c9448e3de59e103611957dd682bbd1852` | `VariantInternalAccessor<Ref<T>>::get` 先转为 `T *`，直接调用 pointer constructor，避免先隐式构造临时 Variant；保留旧参数名。此 internal API 原本就要求已经验证的兼容类型，不用它处理任意不匹配对象；公开 `Ref` 类型检查未改变 |
| [#121446](https://github.com/godotengine/godot/pull/121446) / `d238a74a36ebea170d37f1ec9c0793508cb6ae1e` | 无 CCD 的 kinematic body 不扩大整段移动/传送路径的 broadphase AABB，并在最终位置更新 bounds；ray/shape CCD 保留 sweep。只将上游 `PS2DE` 名称适配为本分支的 `PhysicsServer2D` |
| [#123739](https://github.com/godotengine/godot/pull/123739) / `9197671a5660c94efdd356985ec6ca7d76073209` | 锁内复用 `StringBuilder`，批量 `ToString/Clear` 与 `AppendLine`；锁和 deferred 调度边界不变。跟随上游采用平台换行，Windows 为 CRLF；未声称 Windows 运行验证 |
| [#123319](https://github.com/godotengine/godot/pull/123319) / `8c4486ad3e5476aedf073b39c854b6210b3586f0` | 原样回移 bounded token `memcpy`、vendor patch `0003` 和 README；本分支已有 `0002`，没有补入额外 glslang 版本升级 |

### 10.2 验证结果与边界

- GCC 14 Linux Mono editor 与 minimal-extra release + tests 增量构建通过；使用 `optimize=none lto=none`，release 复用隔离的 `first_batch_validation` 后缀，不覆盖 shipping/LTO 产物
- 新增 native focused：editor **4/4、278 断言**，随机顺序再次通过；release 与首批组合 **10/10、323 断言**。二进制资源测试覆盖普通/压缩 × little/big endian 四种组合、嵌套资源、Unicode、64-bit 整数及 Variant 数据；Ref 测试覆盖 derived/base、null、正确引用计数、提取后的存活与最终释放，另验公开 API 拒绝不兼容类型
- 物理测试调用真实 `GodotBody2D` force/velocity integration 与 broadphase/direct-space collision query，覆盖无 CCD 的连续两次远距离传送、终点碰撞、旧位置/中途排除，以及 ray/shape CCD sweep 的保留与下一静止 step 清除。不是完整游戏场景 benchmark
- 扩展相关 native aggregate：editor **381/381、124,568 断言**；release **362/362、86,368 断言**。包含 Variant、Resource、GodotPhysics2D、首批回归、Animation、Callable、ClassDB、Dictionary、Array、Tree 和 Shader 名称匹配集合。沿用旧 GUI 测试的预期非 `_draw()` 绘制诊断，断言全通过；不是整个引擎测试套件
- .NET 8 source-generator 全量 **66/66**；既有 managed editor smoke **35 断言**再次通过。GodotTools 与新增日志测试项目均 **0 warnings / 0 errors**
- MSBuild 实际 headless editor/panel 测试 **11 断言**：null/空/空白/内嵌 LF 和 CRLF、实际 deferred queue、重复 flush、后续批次、builder 复用、20,000 条并发 stdout/stderr 完整且各流顺序不乱。原始 buffer 按平台换行检查，RichTextLabel 的 CR 归一化另行检查。格式整理后用本轮重建的 editor 再次通过
- glslang 实际生产 header 的 ASan/UBSan 边界测试：**1,283** 种长度、显式 prefix、精确分配且无 NUL 的 source、`SIZE_MAX` 截断、NUL 终止、尾部/相邻 guard 与 metadata 保持。LeakSanitizer 在 ptrace 环境不可用，仅关闭 leak 检查；未将其计为 leak pass
- glslang 八个受 header 影响的 translation unit 独立重编译；四种实际 preprocess + original/expanded parse/link 场景通过：macro replay/punctuation、identifier/operator paste、stringification、1024-character identifier。旧 archive 上同一 expected-expansion 集合也通过；源码上游 patch 和 vendor patch 逆向应用检查通过
- 本轮未重复三套 JIT/Trim/NativeAOT 导出；之前第 9 节导出证据仍属于首批。未进行 GPU render、Apple、Windows/macOS 真机运行或性能测量，不重新发布冻结交付包

### 10.3 可复现入口

原生回归已纳入 `tests/core/io/test_resource.cpp`、`tests/core/variant/test_variant.cpp` 和 `tests/servers/test_godot_physics_2d.cpp`。两个独立 runner 随源码保留：`misc/msbuild_log_validation/` 和 `misc/glslang_validation/`，详见各自 README。

工具链环境同第 9 节。主要命令：

```sh
scons platform=linuxbsd target=editor module_mono_enabled=yes tests=yes dev_build=no debug_symbols=no optimize=none lto=none accesskit=no wayland=no -j8
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*SecondFixBatch*'
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*SecondFixBatch*' --order-by=rand --rand-seed=17
scons profile=misc/build_profiles/linux_release_minimal_extra.py target=template_release tests=yes lto=none optimize=none extra_suffix=first_batch_validation accesskit=no wayland=no -j8
bin/godot.linuxbsd.template_release.x86_64.first_batch_validation.mono --headless --test --test-case='*SecondFixBatch*,*FirstFixBatch*'
# 对 editor 和 release 分别运行此 aggregate filter：
# --test-case='*Variant*,*Resource*,*GodotPhysics2D*,*FirstFixBatch*,*Animation*,*Callable*,*ClassDB*,*Dictionary*,*Array*,*Tree*,*Shader*'
dotnet test modules/mono/editor/Godot.NET.Sdk/Godot.SourceGenerators.Tests -c Release -m:1 -p:BuildInParallel=false
misc/msbuild_log_validation/run.sh "$PWD/bin/godot.linuxbsd.editor.x86_64.mono" /absolute/isolated-output
ASAN_OPTIONS=detect_leaks=0 GLSLANG_ARCHIVE="$PWD/bin/obj/modules/libmodule_glslang.linuxbsd.editor.x86_64.a" misc/glslang_validation/run.sh /absolute/isolated-output
```

本工作区日志和上游审计 snapshot 摘录：`../godot-second-batch-validation/`（不纳入 Git）。这是本地工作树验证，远程 CI 必须另外核对最终提交；native binary 的版本标签可能仍显示构建时 HEAD。


<a id="constant-registration"></a>
## 11. 常量注册去重（#123968）

记录：2026-09-30。基于 `96caefacf36fa83b5cf8f1639ca0cfa47bde3c18`，单独适配 [上游 #123968](https://github.com/godotengine/godot/pull/123968)；merge SHA `cd9c5d57fb9795886f3bfed8e2003062e1378178`，原补丁 `6bca64ff473998f9268c838e6873237af9407df3`。

### 11.1 适配范围

- 将全局常量宏展开中的重复容器写入收敛到一个 `_NO_INLINE_` helper；enum / bitfield 的编译期限定名称保存在 `GetTypeInfo`，不再为每次注册创建整份 `PropertyInfo`
- 将限定名称转换移到独立 `core/variant/type_info.cpp`；在旧 `GDType` 上增加 raw-name wrapper，随后仍调用原来的 `bind_integer_constant`。保留现有常量/enum map、继承、主线程与初始化状态检查、重复注册检查和 ClassDB API/hash 路径
- 保留原来的 `get_slice("::", 1)` 命名规则、64-bit 数值、enum / bitfield 与文档 metadata。未移入新 GDType Member 架构、上游较新的 EXT 注册宏、#123984 / #124025 或 .NET 10
- 已核对[上游 review](https://github.com/godotengine/godot/pull/123968#pullrequestreview-5357046011)及三个 `BitField<>` 一致性建议；审计补丁已含建议的最终形式。上游报告的体积收益不是本 fork 的实测值

### 11.2 按风险精简验证

- 仅构建一个 GCC 14 Linux Mono editor + tests 配置：`optimize=none lto=none`。首次测试文件缺少 `variant_caster.h` 导致编译失败，补齐 include 后增量续建成功；最终构建日志没有 compiler warning/error。这是一种构建配置，不是一次失败被省略的全绿首跑
- 三项聚焦 native 测试 **3/3、3,199 项断言通过**：限定名称与属性 enum/bitfield metadata、旧 GDType raw/direct 注册与继承等价、所有全局常量的索引/map/enum membership；含 64-bit 极值、多层限定名及自定义公开名称
- 以修改前已有的第二批 editor 获取 baseline，修改后 editor 再导出完整 `--dump-extension-api-with-docs`；两份 JSON **逐字节相同**，SHA-256 均为 `73806db750b20ce1c5749306b996299d414b2dd69bd7aa1c598ddbec3ea73722`。覆盖 687 类、11 个普通全局常量、22 个全局 enum / 517 个值、142 个普通类常量、542 个类 enum / 3,629 个值，以及其余完整 API 和文档
- ClassDB hash 保持：core `131315370`，editor `1704154020`。Baseline editor 版本标签仍显示其构建开始时的 `a043f328e`；不是另外从 `96caefac` 重建的 LTO 基线。前一批源码构建日志和本轮修改前 dump 保留，不用版本标签冒充新的独立 baseline 构建
- 可复现工具：`misc/constant_registration_validation/`；原生测试：`tests/core/object/test_constant_registration.cpp`。比较脚本经 Ruff、Python 编译检查、自比较和修改 enum 值的负对照检查
- 响应减少测试压力的要求，本轮未重复 release / JIT / Trimmed JIT / NativeAOT 矩阵、source-generator 全套或跨平台运行。没有覆盖冻结交付包或发布新产物

**体积尚未实测**：匹配 LTO baseline 的 dry-run 仍需近完整重编译，故将严格 A/B 延至下一交付检查点；保留不可变 before commit `96caefac` 供未来同工具链/profile/strip/LTO 比较。旧 29.89 MiB 模板还缺少前两批改动，不能直接相减并归因给 #123968，也不能把 debug/editor 文件大小当作 shipping 收益。

工作区证据：`../godot-constant-registration-validation/`，包括 before/after API、hash 日志、构建/测试日志和来源记录；不纳入 Git。主要命令：

```sh
scons platform=linuxbsd target=editor module_mono_enabled=yes tests=yes dev_build=no debug_symbols=no optimize=none lto=none accesskit=no wayland=no -j8
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*ConstantRegistration*'
python3 misc/constant_registration_validation/compare_api.py /absolute/before /absolute/after
```

<a id="upstream-third-batch"></a>
## 12. 第三批：基础类型与 2D 正确性

记录：2026-09-30。基于 `5031c516`，本地持续回移；未推送，也未覆盖冻结交付物。

| 上游 PR | 实际改动与适配 |
|---|---|
| [#113204](https://github.com/godotengine/godot/pull/113204) | NodePath 在 simplify / prepend_period 修改共享数据前执行 copy-on-write；本地额外避免复制尚未有效的未初始化 hash_cache |
| [#121525](https://github.com/godotengine/godot/pull/121525) | 空 initializer_list 的 CowData 不再分配并写空存储；当前基线不存在 Span 构造器，因此未为了补丁引入新 API |
| [#121786](https://github.com/godotengine/godot/pull/121786) | 空 UTF-32 span 的 unchecked append 直接返回，避免向 memcpy 传空指针，也不产生无意义的 COW detach |
| [#120128](https://github.com/godotengine/godot/pull/120128) | Variant change_and_reset 在新建类型后不再重复初始化；local storage 使用值初始化，保留同类型 reset 与 packed array 路线 |
| [#121100](https://github.com/godotengine/godot/pull/121100) | 2D 点速度按旋转后的自定义质心计算；body origin 的平移不改变该相对坐标语义 |
| [#122578](https://github.com/godotengine/godot/pull/122578) | tscn node tag 缺少 name 时返回 ERR_FILE_CORRUPT，不把非法索引继续送入场景实例化 |

验证按风险合批：复用 Linux Mono editor + tests 配置，未重复 release、跨平台、source-generator 或 JIT/Trim/AOT 导出矩阵。没有修改公开 ClassDB 方法、属性、信号或托管 SDK。

原生回归：NodePath 预热/未预热 cache、原副本隔离、simplified helper；CowData 空/非空及后续 resize；UTF-32 空 append 不 detach 与非 BMP 字符；Variant 全类型初始化上游测试；2D 自定义/旋转/零质心；根和子节点缺名的坏场景、正常场景加载。

工作区证据：`../godot-third-batch-validation/`。固定 merge/constituent SHA 与 patch SHA-256 在 provenance.json；审计清单保留原始时点，不改写为当前合入清单。

验证结果：聚焦 **6/6 test cases、275 断言通过**；相关 NodePath / String / CowData / Variant / PackedScene / GodotPhysics2D aggregate **181/181、16,114 断言通过**。首次构建有两个测试忽略 nodiscard 返回值的警告，改为检查返回值后增量构建干净；生产代码没有因此改变行为。仅一种 editor 配置，第二次是收尾的增量编译，不是额外测试矩阵。

```sh
scons platform=linuxbsd target=editor module_mono_enabled=yes tests=yes dev_build=no debug_symbols=no optimize=none lto=none accesskit=no wayland=no -j8
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*ThirdFixBatch*,*VariantInitialization*'
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*NodePath*,*String*,*CowData*,*Variant*,*PackedScene*,*GodotPhysics2D*,*ThirdFixBatch*'
```

<a id="upstream-fourth-batch"></a>
## 13. 第四批：shader parser 与编辑器稳定性

记录：2026-09-30。基于 `c09c6ec6`，六项小修正继续仅在本地合入，冻结产物与托管 SDK 不变。

- [#121409](https://github.com/godotengine/godot/pull/121409)：struct 的有效比较不再进入仅支持 scalar 的常量求值；回归将原 spatial 复现改为保留的 canvas_item，覆盖 `==` / `!=`、非法比较与运算
- [#121785](https://github.com/godotengine/godot/pull/121785)：初始化 shader 函数参数的五个字段；poisoned raw storage placement-construction 和实际解析结果均检查默认值
- [#121784](https://github.com/godotengine/godot/pull/121784)：dock 目标不是 DockTabContainer 时不解引用空 cast；适配当前旧版嵌套条件，未引入新 dock layout API
- [#122254](https://github.com/godotengine/godot/pull/122254)：延期滚动改传 TreeItem 的 ObjectID，调用时再取活对象；保留 center_on_item 参数。旧 header 的相邻方法不同，签名手工适配
- [#121403](https://github.com/godotengine/godot/pull/121403)：用 display server 名称识别 headless，不再把暂时不能绘制的普通窗口当作命令行模式
- [#122935](https://github.com/godotengine/godot/pull/122935)：只有实际成功加载或卸载 GDExtension 才触发 reload 信号；不存在的扩展连续加载三次，不产生 reload 信号

验证：同一个 Linux Mono editor 配置增量构建成功，无编译 warning/error；聚焦含既有 shader 回归 **5/5 cases、68 断言通过**。Shader / Tree / Object / Callable 相关 aggregate **254/254、11,915 断言通过**，运行中有三条既有绘制上下文 guard 报错，不能描述成完全无错误输出。

真实 headless editor import：缺少 Linux 库的 GDExtension 正常报出 unsupported-platform 错误后退出（exit 0，未超时），project.godot SHA-256 不变。首次 smoke fixture 使用了不再允许的 compatibility_minimum=4.0，随后改成 4.1 重跑，最终确实到达缺平台库路径。可复现脚本 `misc/editor_stability_validation/run.sh` 已单独跑通。

**验证边界**：dock 浮窗/移动、普通窗口最小化和关闭全部场景的真实 GUI 交互未专项自动化复现；本批有对应源码审查、编译与相关测试，并不冒称 UI 全路径通过。未重复托管 generator / 三发布模式 / release / Windows / Metal 矩阵。

```sh
scons platform=linuxbsd target=editor module_mono_enabled=yes tests=yes dev_build=no debug_symbols=no optimize=none lto=none accesskit=no wayland=no -j8
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*FourthFixBatch*,*ShaderLanguage*'
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*FourthFixBatch*,*Shader*,*Tree*,*Object*,*Callable*'
misc/editor_stability_validation/run.sh "$PWD/bin/godot.linuxbsd.editor.x86_64.mono" /absolute/new-output-directory
```

证据：`../godot-fourth-batch-validation/`，包括 provenance.json、构建/原生测试/导入日志与 project.godot 前后摘要。该 shell smoke fixture 针对 Linux 环境设计。

<a id="upstream-fifth-batch"></a>
## 14. 第五批：原生整数溢出与调试器循环

记录：2026-09-30。基于 `8cb70345`，本地回移 [#121740](https://github.com/godotengine/godot/pull/121740) 与 [#121232](https://github.com/godotengine/godot/pull/121232)。虽然前者标题含 GDScript，本批只修改保留的 Math / Vector2i / Vector3i / Vector4i / Variant 运算，没有恢复 GDScript。

- `INT32_MIN / -1`、`INT64_MIN / -1` 明确定义为原最小值，余数为 0，避免 quotient overflow 引起硬件 trap。普通正负数和边界结果保留原语义
- Variant checked、validated、ptr 三种 evaluator 都使用新 helper；checked division/modulo by zero 仍返回 invalid 与错误字符串。raw/validated 路线继续要求分母非零，没有借此改变调用约定
- 当前 math_funcs.h 尚未采用上游的大范围 constexpr 化；仅插入四个独立 helper，没有牵入其他数学重构。C# 自身的纯托管算术与异常语义不在本次变更范围
- 调试器资源图标回退的祖先遍历由永真 OR 条件改为 AND，遇到 Resource 或空基类正确终止

验证：一个 Linux Mono editor + tests 构建，无编译 warning/error。聚焦 **5/5 cases、236 断言通过**；Math / integer-vector / Variant aggregate **155/155、2,532 断言通过**。上游测试覆盖常量表达式和运行时整数边界；本地补充 native Variant 三条 evaluator 路线与 checked 零分母。

另以 `-O2 -fsanitize=undefined -fno-sanitize-recover=undefined` 直接编译当前真实头文件并运行 volatile 边界输入，UBSan 通过。初始独立 probe 缺平台 include 路径和 `UBSAN_ENABLED` define，补齐编译参数后成功；不是完整引擎 UBSan 构建。可复现源码与脚本在 `misc/integer_math_validation/`。

```sh
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*FifthFixBatch*,*division_no_overflow*,*Division and modulo by -1*'
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*Math*,*Vector2i*,*Vector3i*,*Vector4i*,*Variant*'
misc/integer_math_validation/run.sh /absolute/output-directory
```

调试器 GUI 的图标回退未专项交互测试。没有重复 release / .NET generator / JIT-Trim-AOT / 跨平台矩阵；未推送、未更改冻结产物。证据：`../godot-fifth-batch-validation/`。

<a id="upstream-sixth-batch"></a>
## 15. 第六批：UTF-8 生命周期与平台防护

记录：2026-09-30。基于 `f2f95400`，保持本地提交、不推送、不改冻结产物。

- **部分回移** [#120746](https://github.com/godotengine/godot/pull/120746)：仅修正 ASAP error reporting、PulseAudio 输出/输入设备名称的临时 UTF-8 悬空指针，令 owning CharString 覆盖使用周期。上游 input hunk 误用 output_device_name，本地保留正确的 input_device_name。未移入 lifetime attribute 体系、Image/StreamPeerBuffer/Polygon2D/NavigationPolygon 的返回引用 API 或已删除 MeshDataTool；不能把本 PR 标记为完整移植
- [#120087](https://github.com/godotengine/godot/pull/120087)：Windows file-ID 查询检查错误、为不支持 Ex API 的卷回退 legacy API、比较卷序号及完整 ID，并在比较期间保留两个打开句柄。不同句柄使用不同查询能力时保守返回 false；这不是所有文件系统上完美等价性判断的承诺
- [#121995](https://github.com/godotengine/godot/pull/121995)：Metal extension texture 无需创建 view 时 retain 借入纹理，与释放路径配对
- [#123439](https://github.com/godotengine/godot/pull/123439)：metal-cpp SharedPtr 用 nil-safe Objective-C message 调用代替对空 C++ 对象调用方法；同步保留 vendor patch 和更新脚本的 patch 目录处理
- [#123580](https://github.com/godotengine/godot/pull/123580)：Wayland popup 缩放后的尺寸 clamp 到至少 1×1，避免提交无效协议尺寸

验证按平台分开说明：

- Linux Mono editor 增量构建通过，包含 error/PulseAudio 修改，无编译 warning/error。ASAP 回归在 callback 中分配同长度 UTF-8 buffers 后核对完整非 ASCII 文本：**1/1 case、2 断言通过**；String / CowData aggregate **127/127、14,867 断言通过**。运行时打印的一条 WARNING 是测试的显式输入
- 当前 Windows 生产函数原文被提取到 portable mock Win32 harness，在 `-O2` + UBSan 下覆盖 **13 条路径**：Ex / legacy 相同与不同卷及高低 ID、混合能力、打开失败、查询失败、base fallback，以及所有路径的句柄关闭。测试在 `misc/platform_fix_validation/`；这是控制流与比较逻辑检查，**不是 Windows 编译或真实外置盘/网络盘测试**
- metal-cpp 两份 vendor patch 均通过 `git apply --reverse --check --directory=thirdparty/metal-cpp`，更新脚本通过 Bash 语法检查；Metal retain/release 与 nil 调用路线已源码审查
- **未验证**：macOS/Metal 编译与设备运行、真实 Windows 文件系统、Wayland popup 运行、PulseAudio 设备连接。本次 editor 为 `wayland=no`，因此不能用它冒称 Wayland 编译验证。没有安装额外环境或重跑 .NET 三模式矩阵

```sh
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*SixthFixBatch*'
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*SixthFixBatch*,*String*,*CowData*'
misc/platform_fix_validation/run_windows_file_id.sh /absolute/output-directory
```

证据：`../godot-sixth-batch-validation/`。这一批不改变 ClassDB / C# SDK 公共签名；平台行为的真实验证仍需适用环境。

<a id="upstream-seventh-batch"></a>
## 16. 第七批：RenderingDevice graph 修正

记录：2026-09-30。基于 `3ce6266c`，成对回移 [#120555](https://github.com/godotengine/godot/pull/120555) 与 [#121891](https://github.com/godotengine/godot/pull/121891)。不恢复 Forward+/Mobile 3D renderer，不改 public API。

- graph 先标记当前命令的所有资源，full texture 已在同一命令且 usage 相同时，跳过对应 slice 的重复依赖；不同 usage 仍拒绝。command/usage 索引按 frame 重置
- NVIDIA driver 启用窄 workaround：没有绑定过 pipeline 的 draw list，其 discardable attachment 使用 STORE；普通驱动和已绑定 pipeline 的路线保持原 DONT_CARE 行为。每个 draw-list begin 重置绑定记录

验证不依赖假 GPU：新增私有 friend test accessor 只准备 graph 的 CPU recording state，不初始化 driver，不提交命令。直接运行真实 command recording，检查 full texture/slice 两种顺序均无 self-edge、后续写命令仍产生正确前后依赖、跨帧索引重置，以及 workaround on/off 与 bound→unbound→bound 的实际 recorded store ops。聚焦 **2/2 cases、24 断言通过**；graph + shader aggregate **14/14、102 断言通过**。

Linux Vulkan editor 编译通过。首次测试声明使用了本基线未定义的 TESTS_ENABLED guard，导致 friend 不生效；改成与现有 ProjectSettings 测试 accessor 一致的私有 friend 后增量构建干净。没有修改类布局或公开方法。**未进行 NVIDIA GPU 执行、driver crash 复现或真实 RD submit 验证**，CPU recording 通过不等于硬件通过。

```sh
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*SeventhFixBatch*'
bin/godot.linuxbsd.editor.x86_64.mono --headless --test --test-case='*RenderingDeviceGraph*,*Shader*'
```

证据：`../godot-seventh-batch-validation/`。未重跑 .NET / release / 跨平台矩阵，未推送或改写冻结包。

### 16.1 持续回移状态

可机读台账：[misc/upstream_sync/status.json](misc/upstream_sync/status.json)。原 43 项 high 中 **35 项已移植、1 项部分移植、7 项待深入验证**；“已移植”指源码集成和本文明确范围的验证，不代表全部平台均运行通过。台账另记录前面已移植的 medium/low 项，不把它们加进 43 项分母。

剩余 high：#115557 nested local-to-scene、#120354 深层重排复制、#119123/#120545 并行 shader cache 与 Metal 锁、#121835 SPIR-V reflection lookup、#122667 Bezier undo/clipboard、#123693 inspector/connection 生命周期。各自的具体待验证点见台账；不为清零计数机械套用补丁。后续可先处理证据充分的小型正确性/性能改进，并继续研究这些剩余项。
