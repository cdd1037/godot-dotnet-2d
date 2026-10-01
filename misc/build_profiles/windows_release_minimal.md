# Windows 2D .NET 最小模块 Release 模板

这是显式启用的 SCons 源码构建配置，不是项目的 `export_presets.cfg`，
也不是 JSON `build_profile`。不修改 editor 默认配置，也不替换已有 Linux 交付包。

## 构建入口

当前构建与导出命令统一维护在 [BUILDING.md](../../BUILDING.md)。
本文件只维护第一组 minimal 的资源与模块兼容清单；当前 extra 与它有差异。
MinGW 非 LTO / full LTO 的精确历史命令、工具链与失败尝试见
[历史 Windows LTO](../../PROJECT_HISTORY.md#windows)，不作为每次构建要求。

## 实际模块清单

`modules_enabled_by_default=False`；只开启下面 13 项：

```text
astcenc bcdec freetype glslang godot_physics_2d jpg mono mp3
ogg svg text_server_adv vorbis webp
```

`freetype` 是确认后补充的一项，用于普通 TTF/OTF 等动态字体。
`msdfgen` 没有开启。没有自动追加其他可选模块。

真实依赖规则：

- `text_server_adv/config.py` 中 FreeType、MSDFgen、SVG 都是可选依赖；
  `optional=True` 只参与排序，不会自动开启模块
- 不启用 FreeType 仍可能编译，但没有常规动态字体能力，不能当作正常 UI 字体配置
- MSDF 字体需要 `module_msdfgen_enabled=yes`，而 MSDFgen 硬依赖 FreeType
- Vorbis 硬依赖 Ogg，当前二者都已开启
- Release 模板的 Mono 不要求 RegEx；Mono editor 才要求
- 2D 物理模块的准确名称是 `godot_physics_2d`，同时保留 `disable_physics_2d=False`
- glslang 仅在 Vulkan / Metal / D3D12 条件下可构建；本 fork 的 Windows 实际只用 Vulkan

## 仍然保留什么

`modules_enabled_by_default` 只控制 `modules/`，不是“禁用所有引擎组件”。
PNG 在 `drivers/png` 中无条件编入，基础 UI、2D 场景、动画、音频/WAV、
文件与资源系统仍由 core / scene / servers / drivers 构建。
本配置没有再打开 `disable_advanced_gui` 等功能裁剪开关。
此前已物理移除的 3D、XR、GDScript、高层多人网络、视频能力不会因此恢复。
底层 HTTP/TCP 等仍在，不能把“移除高层多人网络”理解为“没有任何网络 API”。

## 项目使用前检查

- **字体**：TTF/OTF 路径使用已开启的 FreeType；所有字体资源的 MSDF 设置须关闭。
  `text_server_adv` 并不代替字体文件，中文需要资源中确实包含相应字形
- **寻路**：未开 `navigation_2d`。API/节点仍可能可见，但服务会退回 dummy，
  NavigationAgent2D、NavigationRegion2D、TileMap 导航等不能视为可用。
  仅用 AStar2D/AStarGrid2D 不等于使用该导航服务
- **噪声**：未开 `noise`，项目不能依赖 FastNoiseLite / NoiseTexture2D
- **正则**：未开 Godot 的 `regex`，不能使用 Godot.RegEx；这与
  .NET 的 `System.Text.RegularExpressions` 是两个独立实现
- **TLS/密码学**：未开 `mbedtls`，Godot 的 HTTPS、TLS/DTLS、相关 Crypto 功能缺少实现。
  C# 的 HttpClient/密码学是否可用取决于最终 .NET 发布和系统环境，不能凭模板编译保证
- **纹理**：本源码 Import 面板的 `compress/mode` 精确取值为
  `0 Lossless / 1 Lossy / 2 VRAM Compressed / 3 VRAM Uncompressed / 4 Basis Universal`。
  未开 `basis_universal`，不要带入第 4 种已有导入资源；需重新导入成其他适合项目的模式，
  或明确开启此模块。普通第 2 种原生 BC/S3TC/BPTC 压缩不是 Basis Universal，
  不应笼统禁用。当前 `CompressedTexture2D` 的 Basis 分支会直接调用未注册的解码回调，
  因而不能指望“不支持时总能安全返回错误”
- **KTX/DDS/其他图片格式**：未开 `ktx`、`dds`、BMP/TGA/HDR/EXR 等资源加载模块。
  `ktx` 还硬依赖 `basis_universal`。预先导入的原生 BC 压缩纹理不要求运行时 DDS 文件加载器；
  运行时直接读取原始 `.dds`/`.ktx` 文件则是另一回事
- **可视化 Shader**：未开 `visual_shader`，已有 VisualShader 资源要检查；
  普通文字 shader 和 glslang 编译能力不等于 VisualShader 资源类型存在
- **交互式音乐**：未开 `interactive_music`，不能使用 AudioStreamInteractive、
  AudioStreamPlaylist、AudioStreamSynchronized 等；普通 WAV/MP3/Vorbis 不受这一项影响
- **显卡兼容性**：这是 Vulkan-only Windows 模板；OpenGL/D3D12 已从 fork 删除，
  不会作为旧显卡或驱动不支持 Vulkan 时的备用方案

按需增加模块必须先确定项目确实使用它。当前配置没有替用户作这些选择。

## .NET API 与最终游戏包

此 profile 只构建原生 `.NET` 模板。它不自动构建 GodotSharp、Godot.NET.Sdk、
游戏程序集、.NET runtime，也不自动执行 JIT/Trim/NativeAOT 发布。
原生 EXE 体积不能当成最终可运行游戏目录的体积。

使用同一 fork 的 editor、生成的 GodotSharp 与本地 SDK 包；不要混入上游同版本包。
本 fork 当前包版本与上游重合时，按 `misc/2d_dotnet_validation/README.md`
使用隔离 NuGet 缓存和 fork 的本地 feed。
绑定生成和 SDK 配对步骤见 [统一构建说明](../../BUILDING.md#1-工具链与干净配对)。

不要运行 Release 模板来替代具备工具能力的 editor 生成绑定，也不要覆盖已有交付的
GodotSharp 目录。完整 editor 的 C# API 可比精简模板更宽；“C# 能编译”不保证对应
native class/method 仍存在。项目必须避免引用本模板未包含的模块类型，并在 Windows
实际运行后验证。Linux 结果不自动变成 Windows 结果；后续 .NET 10 Windows 三模式 CI 的精确范围见
[当前结果](../../PROJECT_RESULTS.md)，不回写本文件的历史交叉编译结论。

## 历史验证范围

保留原生模板编译、实际 SCons 配置、启用模块头、PE 格式/导入依赖、SHA-256、
字节数及相同条件 LTO 对比。最终结果以本轮构建记录为准。
Linux 交叉编译成功不等于 Windows 真机运行成功；尚未在 Windows 验证启动、
Vulkan 画面、输入、音频、字体或 .NET 游戏发布运行。
