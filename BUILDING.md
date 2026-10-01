# 构建、配对与导出

所有命令从仓库根目录执行，除非单独注明。本文是常用构建/导出命令的维护入口；
专项测试、CI 自动化和历史实验通过链接引用，不在多份文档中复制命令。
[当前结果](PROJECT_RESULTS.md)说明哪些平台/模式真正验收过；命令存在不等于验收通过。

## 1. 工具链与干净配对

- .NET SDK `10.0.401`、runtime `10.0.12`；当前 fork 包 `4.7.2-2dtrim.3`
- Python / SCons、目标平台 C++ 工具链与依赖；Windows CI 固定 SCons `4.10.1`
- Android：JDK 17、Gradle `8.11.1`、SDK API 36、build-tools `36.1.0`、NDK `29.0.14206865`
- Windows native CI 使用 MSVC；历史 MinGW LTO 参数不可套到 MSVC
- [global.json](global.json)允许 `latestPatch`，复现实验仍应核对 `dotnet --info` 的实际 SDK

顺序始终是 **editor → 生成 glue → GodotSharp/SDK → 游戏/脚本库 → 匹配模板 → 导出运行**。
不要混用官方 Godot、旧 fork、旧 .NET 8 包或不同 ABI 的产物。
每个 publish mode 使用全新输出目录；不得靠清空既有用户数据来规避旧文件。
总并行度最多 8 jobs，受内存限制时降低；不要让 native 和 managed 大构建叠加超限。

### Linux editor 与绑定示例

```sh
scons platform=linuxbsd target=editor arch=x86_64 module_mono_enabled=yes -j8
bin/godot.linuxbsd.editor.x86_64.mono --generate-mono-glue modules/mono/glue
python modules/mono/build_scripts/build_assemblies.py --godot-output-dir=bin
```

输出的本地 NuGet feed 为 `bin/GodotSharp/Tools/nupkgs`。游戏和脚本库应使用此 feed，
并设置隔离的 `NUGET_PACKAGES`；四个 Godot 包须解析到本 fork，不能命中同版本上游缓存。
本地 feed / 双精度构建的高级选项见 [Mono 模块说明](modules/mono/README.md)。
Release template 没有 editor 的 tools 能力，不能用于生成绑定。

## 2. 桌面 Release 模板

这些是原生模板，不包含游戏、GodotSharp 或 .NET runtime。
minimal-extra 适合满足其[功能边界](CUSTOMIZATION.md#profile-与-classdb-边界)的项目，不能用于 editor。

### Linux x86_64 minimal-extra

```sh
scons profile=misc/build_profiles/linux_release_minimal_extra.py use_llvm=no -j4
```

输出 `bin/godot.linuxbsd.template_release.x86_64.minimal_extra.mono`。
配置为 `optimize=size`、full LTO、无 debug symbols；保留 Brotli/默认字体。
平台依赖决定实际 Wayland/AccessKit 等能力，不能仅凭命令认为已编入。
[历史字体与体积验证](misc/build_profiles/linux_release_minimal_extra.md)属于 .NET 8 检查点。

### Windows x86_64 minimal-extra（MSVC）

在配置好 MSVC 工具链及平台依赖的 Windows 环境中执行：

```sh
scons profile=misc/build_profiles/windows_release_minimal_extra.py use_mingw=no use_llvm=no lto=full -j4
```

输出 `bin/godot.windows.template_release.x86_64.minimal_extra.mono.exe`。
核对实际 `/GL`、`/LTCG` 和 linker code generation 日志，不能只根据参数声称真实 LTO。
AccessKit / WinRT 等依赖缺失可能令对应 driver 关闭，应核对最终 SCons 配置。
安装脚本为 [AccessKit](misc/scripts/install_accesskit.py) 和 [WinRT](misc/scripts/install_winrt.py)。
维护用完整 CI 步骤与依赖固定方式见 [.github/ci/README.md](.github/ci/README.md)。

第一组 13-module profile 和 MinGW 非 LTO / plugin / partition 对照是历史实验，
不要求每次重新构建。精确命令与失败原因保留在 [历史 Windows LTO](PROJECT_HISTORY.md#windows)。

## 3. 桌面游戏导出

1. 使用匹配 editor 创建/打开 C# 项目，指向本地 fork NuGet feed，重编所有脚本库
2. .NET 10 创建解决方案时使用 classic `.sln`（`dotnet new sln --format sln`），导出器不以 `.slnx` 替代它
3. 配置目标平台导出预设，选择匹配的 custom Release template
4. 设置 `dotnet/publish_mode`，导出到该模式独立的空目录；按目标平台实际运行验证

| 值 | 模式 | 必须满足 |
|---|---|---|
| `0` | 普通 self-contained JIT（默认） | 包含完整 managed/runtime payload |
| `1` | trimmed JIT | 依赖 trim-safe，脚本/反射目标/泛型闭包发布时可达 |
| `2` | NativeAOT 共享库 | 依赖 AOT-safe，没有未知 IL 或 fallback JIT |

[发布协议](misc/dotnet_publish_modes/README.md)维护入口、模式标记和脚本库规则。
比较体积必须固定场景、native template、RID、globalization、符号及压缩策略，
分别列原生引擎、managed/runtime 或 AOT payload、资源及完整游戏目录。
Windows 三模式 CI smoke 不代替硬件 Vulkan、手柄、音频或所有目标机器验收。

## 4. Android 模板与普通 APK

设置 `ANDROID_HOME`、`JAVA_HOME` 后运行维护脚本：

```sh
bash misc/build_profiles/build_android_2d.sh
```

脚本构建 arm64 Debug 和四线程 ThinLTO Release，再通过 Gradle 打包，输出
`bin/android_monoDebug.apk`、`bin/android_monoRelease.apk`、`bin/android_source.zip`。
完整 full LTO 曾超出验证机器 RAM，ThinLTO 是当前受控配置，并非 Android 禁止 full LTO。
迭代 native 模板时可显式关闭 LTO，随后仍需重新打包模板：

```sh
scons profile=misc/build_profiles/android_release_2d.py target=template_release lto=none -j8
(cd platform/android/java && ./gradlew generateGodotMonoTemplates)
```

游戏导出预设：
- arm64 / Vulkan，类型 **APK**，`gradle_build/use_gradle_build=false`，选择匹配的预编译 Mono 模板
- `dotnet/publish_mode=0` 普通 Mono JIT；`1` 实验性 trimmed JIT；CoreCLR / NativeAOT 拒绝
- SDK/runtime/crypto JAR 固定配对 `10.0.12`；旧模板会拒绝 `trimmed-jit` 标记
- 应用最低 API 29，native floor API 24；target API 36，Vulkan 1.1
- 启用 `rendering/textures/vram_compression/import_etc2_astc` 并重新导入资源；ETC2 为基线，ASTC 仅限已知支持设备
- 正式安装前需正确签名及设备验收；当前体积证据为未签名测试 APK

游戏作者不必使用 Gradle 导出，但仍需 .NET/Android 工具与签名配置；维护者构建模板仍使用 Gradle。
自动 trimmed APK 导出与 Android 设备运行尚未验收，不能将测试重打包脚本当成生产导出替代品。
[Android 平台说明](platform/android/README.md)维护平台边界；
[Android 专项验证](misc/android_dotnet_validation/README.md)维护 guards、publish、检查和 CI payload 重打包命令。

## 5. 验证入口与复现限制

- [Windows CI](.github/ci/README.md)：普通 push / PR 的轻量与 editor 路径；manual/tag full LTO + 桌面三模式 + Android Mono publish/IL 比较。只有成功的规范 tag 创建 Release 草稿，不发布 NuGet.org
- [Android 专项验证](misc/android_dotnet_validation/README.md)：保留失败负对照、CI 前受限结果和最终匹配 APK 结果；本地 ILLink 所需 Unix socket 受限时，不绕过 host 或压制 trim 警告
- [基础 2D 验证 fixture](misc/2d_dotnet_validation/README.md)：该 fixture 仍固定 `.2` 包，保留其原检查点，不直接当作当前 `.3` 一键回归入口
- [扩展 API](misc/extension_api_validation/README.md)、[常量注册](misc/constant_registration_validation/README.md)、[glslang](misc/glslang_validation/README.md)、[MSBuild 日志](misc/msbuild_log_validation/README.md)：各专项说明维护各自复现命令

当前工作区已清理部分 JDK/NDK/Gradle 和构建缓存；重建前按 provenance 恢复工具。
旧 `env.sh` 留存不等于其工具路径仍可执行。证据及清理说明见 [PROJECT_RESULTS.md](PROJECT_RESULTS.md)。
构建、静态检查、headless smoke、软件 Vulkan 和设备硬件运行应分别报告，不能互相替代。
