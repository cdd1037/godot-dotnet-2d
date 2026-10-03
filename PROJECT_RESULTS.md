# 当前验证结果与后续计划

更新：2026-10-03。本页维护最新验证、交付物和待办；[构建导出](BUILDING.md)、
[当前定制](CUSTOMIZATION.md)、[历史实验与来源](PROJECT_HISTORY.md)分别维护操作、接口和旧记录。
此次整合仅修改 Markdown；不更新机读记录，不修改生产源码、profile、CI 或重建产物。

## 0. 本轮阶段性结论与接续点（2026-10-03）

**当前阶段可以按“已形成可继续开发的 2D/.NET 10 定制基线，Android trimmed JIT
仍处实验验证阶段”收口。接下来回到 Godot 魔改；具体第一项源码任务尚未指定。**

- **已有成果**：2D/UI/C# 裁剪、桌面托管发布协议、.NET 10 迁移与 Android arm64/Mono
  普通 JIT 路线已有实现和相应历史验证；上游原 high 清单为 38 完整 / 1 部分 / 4 延期，
  另有 13 项选择性回移。它们是可接续基线，不表示所有平台、功能和上游变更均已覆盖。
- **实验结论**：Android 匹配测试重打包体积减少 29.1143%，证明该 payload 与打包方案的
  静态体积收益；尚不能替代真实自动 trimmed 导出、签名安装及设备运行验收。
- **作者工作流结论**：普通 C# 核心的可移植性已得到局部探针支持；现有比较不足以证明
  自研引擎在完整游戏工时、AI 上下文或总维护成本上更优。近期以现有 Godot 基线继续，
  薄 C# 辅助层仅是候选方向，详细证据与边界见 [策略复核](#41-作者工作流策略复核2026-10-03)。
- **验证边界**：音频仍缺用户 PC 真机数据；macOS Metal、对应桌面硬件与 Android 设备
  验收缺口仍在。文档归档不补齐这些验证，也不重建或重新认证历史二进制。
- **下一阶段起点**：本轮先整理三个工程、保存项目包并同步代码，再准备回到 Godot。
  尚未指定的新功能、深层重构、对照实验或构建矩阵不在本次归档内；选定首个任务后再确定验收条件。

归档前策略文档检查点是 `ead9c4c49626e836c63afc3429151ca0918e78e7`，
[CI 37118388302](https://github.com/cdd1037/godot-dotnet-2d/actions/runs/37118388302)
的静态/生成器与 CI helper 检查通过；Windows editor/.NET 构建和 tag release job 均跳过。
这次文档 CI 不能表述为重新完成 Windows full build 或目标平台运行。

本轮源码归档以 **`dotnet-trim`** 为准，保留该提交树中的全部跟踪源码（包括 `thirdparty`）、
构建配置、CI 与阶段文档；默认 `master` 保持上游用途。归档随附精确 commit/tree、完整文件清单、
校验信息与恢复说明。源码快照不等于完整 Git 历史，也不包含本页列举的仓库外历史 APK、
SDK、音频 runtime 和证据目录；恢复这些交付物须另取其原始配套包，不得由同名路径推定存在。

## 1. 当前可用范围

- **原始 2D/.NET 裁剪与桌面优化**：保留 2D/UI/C#，裁剪 3D、GDScript 和其它历史列明功能；历史 Linux/Windows 冻结包以各自源码、配套 SDK 和验证说明为准
- **.NET 10 / Android 恢复**：桌面工具链和 GodotSharp 已迁移；Android arm64、Mono、SAF、Vulkan/ETC2 与关闭 Gradle 的预编译模板普通 JIT APK 导出已验证
- **Android Mono trimming**：实验性 trimmed JIT 已通过 CI publish/IL 验证与匹配测试重打包的静态/体积检查；默认仍是普通 JIT，CoreCLR / NativeAOT 不支持
- **上游修复**：原 high 清单 38 完整、1 部分、4 延期；另有选择性 13 项回移已恢复和发布，不表示全面同步上游所有 PR
- **音频调查**：内部 2D 启动调度测量与复现 harness 已保存；等待用户 PC 真机数据，未实施音频优化

## 2. 最新源码与 CI

| 用途 | 精确检查点 |
|---|---|
| Android trimming 已发布源码 | `1ffed16b32f0ca41b8fe3dc09144ce861fdbb0fa` |
| 树等价本地冻结源码 | `a4e385baa2ef83b4dd3f369123faaa2e1dbe2239` |
| 两者 tree | `8fb4f8106ceb45231baaf4ce13165afdf36043d1` |
| Android native 构建源码 | `6f49a280b9846e93f69627c010744527ab2b16ae` |
| Linux editor 构建源码 | `be5c554b8f9922e167d2f58ce22f18853a1a36dd` |
| 13 项回移已发布检查点 | `4060b758fa3d613ae1a66f19b00ca61de3c0a12e` |
| 13 项回移本地恢复工作树 | `1ab7b3c93b855c4ff84b20b2014ad9d895b66608` |

- [Android trimming full CI 36810309486](https://github.com/cdd1037/godot-dotnet-2d/actions/runs/36810309486)：通过，Android 两模式发布/IL 检查与 payload 来自此运行
- [.NET 10 Windows full CI 36795328988](https://github.com/cdd1037/godot-dotnet-2d/actions/runs/36795328988)：`0630831` 的 Windows full LTO 与三发布模式历史验证
- 文档整理提交不重写上述二进制来源，也不代表再次运行 CI 或真机验收

## 3. Android 最新结果与交付物

**完整匹配 APK：31.7218 → 22.4862 MiB，减少 9.2356 MiB / 29.1143%。**
精确字节数为 33,262,746 → 23,578,521，差 9,684,225 B。
托管程序集原始大小 28,017,248 → 2,989,056 B；Mono native runtime 两侧均为 4,789,424 B。

两侧均为未签名测试重打包，使用同一真实 no-Gradle JIT APK 与经校验的 CI payload。
native/JAR/DEX/非托管资源、sparse-PCK size/MD5、SHA512 manifest、ZIP CRC、manifest 与 16 KiB 对齐验证通过。
这不是自动 trimmed APK 导出或 Android 设备运行验收。

仓库内可随源码阅读：
- [最新机读比较与 APK SHA-256](misc/android_dotnet_validation/trim_ci_apk_results.json)
- [构建、重打包与验收步骤](misc/android_dotnet_validation/README.md)
- [历史本地构建/受限检查点](misc/android_dotnet_validation/trim_local_results.json)，原样保留 CI 前状态
- [Android 构建与平台说明](platform/android/README.md)

工作区独立交付（不提交二进制到 Git；仅克隆仓库时需另取）：
- `../android-trim-evidence/matched-ci-apks/jit.apk`、`trimmed-jit.apk`：最终匹配 APK；同目录保存检查、重打包与对齐报告
- `../android-trim-evidence/postmerge-jit-game.apk`：真实导出器 baseline，33,263,019 B；不是匹配重打包大小
- `../android-trim-evidence/android_monoRelease.postmerge.apk`：匹配 Release 模板，17,594,966 B
- `../android-trim-evidence/ci-1ffed16b/`：CI payload、日志与独立 IL 复核
- `../android-trim-evidence/Android-Mono-Trimming-Evidence-1ffed16b.zip`：已交付的精简报告/复现脚本包，29,648 B；不含 APK/大 payload。SHA-256 `8c599e89a3f0faf5a016a43bec0ff275af3310d3418e8068f0499797a521922c`
- `../audio-latency-evidence/`：音频报告、519 次最终试验、harness 与配套 runtime；60 Hz 下 2D 内部调度中位 16.694 ms，不是扬声器物理延迟
- `../phase3-recovery-evidence/`：13 项恢复补丁、测试日志与 Git bundle

旧交付路径只作为历史引用；其缺席不能用当前文件替代或假定已重新验证。

## 4. 后续计划：门槛、等待与暂缓

**优先级是相应发布/决策的顺序，不是立即执行授权，也不自动创建新任务或 CI 矩阵。**
此前未启动的新实验继续按下表保留；用户本轮已明确准备回到 Godot 魔改，
但尚未指定下一项源码工作。下列新构建、自动导出 host、设备验收及优化不由本次归档自动启动。

| 优先级 / 状态 | 下一步与完成条件 |
|---|---|
| P0 Android 发布前门槛，暂缓执行 | 在允许 ILLink IPC 的 host 用真实项目验证自动 trimmed 导出，再签名安装；验收冷启动、Vulkan/ETC2、JNI/crypto、生命周期、SAF grants。当前只有 publish/IL、静态打包与体积证据 |
| P0 桌面发布验收，按目标平台补充 | Windows 已有 MSVC full LTO / 三模式 CI smoke；硬件 Vulkan、字体、音频/手柄/WinRT 仍需对应平台实测，macOS 未运行验收。历史 MinGW ICU/WinRT ODR 问题不能被 MSVC 成功覆盖 |
| P1 音频，等待用户数据 | 先在用户 PC 比较普通 AudioStreamPlayer / AudioStreamPlayer2D；内部 60 Hz 约 16.7 ms 不是扬声器延迟。获数据后再决定窄优化，保留位置、距离衰减与 bus 语义 |
| P1 上游维护，沿用既有安排 | 检查新增上游变化，按适用性与风险选择回移；不重写固定 1,362 项审计快照，不因上游合并自动实施 |
| P2 体积/构建性能，未实施 | size_extra、Android relocation packing、保留功能的编译耗时优化。先同配置测量，再确认范围；不更改 profile 或新增矩阵 |
| P2 兼容/功能研究，暂缓 | 完整导航 API / deprecated 裁剪、首次 PCK 目录扫描 #122438、SAF provider 性能、NuGet.org 发布接入；各自重新确认需求与验收，不延用旧 CoreCLR 规划 |
| P3 平台/架构，另行评估 | 其它 Android ABI、AAB、自定义 Gradle、Android CoreCLR/NativeAOT、.NET 11 后续路线、libgodot/.NET-first、单文件发布；外部成熟或 GA 不是自动启动条件 |

原 high 仍为 38 完整 / 1 部分 / 4 延期；`120746` 部分采用，`119123`、`120545`、`122667`、`123693`
沿用既有延期依据。13 项恢复回移在 `4060b758` 发布，属于依证据重建版本，不是原始提交恢复。
详见 [状态账本](misc/upstream_sync/status.json)；历史逐项研究与旧验收设计见
[计划附录](PROJECT_HISTORY.md#roadmap)和[研究附录](PROJECT_HISTORY.md#research)，以本表的当前状态为准。

### 4.1 作者工作流策略复核（2026-10-03）

**近期建议：继续以现有 Godot 成品为基线，优先评估 Godot + 薄 C# 作者辅助层；
自研引擎保持冻结，不做整款游戏迁移，也不继续深改 Godot 的场景、资源或对象体系。**
本节记录审计结论和建议优先级，不代表已同意实施、永久放弃自研路线或解冻任何工程。
既有发布验收门槛和上表的等待/暂缓状态保持有效。

#### 证据支持到哪里

- 对比对象是已交付的《法界天书》`phase10r2-20261003-final` 与自研引擎冻结版本对应的公开提交
  [`f8e334d6`](https://github.com/cdd1037/dotnet-2d-engine/commit/f8e334d65ae2a67ab8e5ab3819026ee5b4f11326)，
  以当前公开 API 和现有交付范围为准；没有重做整款游戏或同等 Godot 场景版。
- 游戏的 17 个 `Scripts/Core` 文件、4,584 个物理行已不依赖 Godot；原样放进普通 .NET
  控制台程序后，伤害、抵消和法力断言通过。这证明逻辑可移植，也说明当前 Godot 方案已经能获得
  普通 C# 核心的好处。文件数和行数是归属清单，不能换算为效率百分比或省下的工时。
- 自研生成式 UI 契约已减少重复 schema、命令 ID 和分派接线；坏字段探针实际得到包含
  RML 位置与 C# 声明位置的 `DUI005`。这是可复核的局部收益，没有证明完整游戏更快、
  AI token/上下文更少或总维护成本更低。应用适配、资源管线、引擎维护与测试都需计入总成本。
- 冻结自研版本的 255 UTF-8 字节文本、禁止换行、每文档 32 个独立图片路径，是当前封装的
  人为契约/预算边界；真实长文本和 14 图标 × 3 状态的 42 张图会碰到它们。
  这些是该版本的适配成本，不能据此断言 SDL/RmlUi 架构失败。
- 关闭 JSON 反射后，原游戏序列化探针确实失败；这只是 JIT 下关闭反射的窄验证，
  不是整游戏 NativeAOT 发布实验。普通 C#、生成器或已有引擎 AOT smoke 均不能替代项目级验收。
- 字体、SVG/遮罩、图像预处理、音频格式、窗口与模态生命周期仍需实际适配。
  没有完整 UI 像素/输入对照、配对作者工时或多平台性能数据，不能给出整体速度结论。

公开机制可查自研的
[生成式 UI 契约](https://github.com/cdd1037/dotnet-2d-engine/blob/f8e334d65ae2a67ab8e5ab3819026ee5b4f11326/docs/GENERATED_UI_CONTRACTS.md)、
[UI 模型边界](https://github.com/cdd1037/dotnet-2d-engine/blob/f8e334d65ae2a67ab8e5ab3819026ee5b4f11326/docs/UI_MODELS.md)
和 [API 收敛](https://github.com/cdd1037/dotnet-2d-engine/blob/f8e334d65ae2a67ab8e5ab3819026ee5b4f11326/docs/PUBLIC_API_CONSOLIDATION.md)。
上述游戏观察来自 2026-10-03 只读代码审计与两个隔离探针；本仓库只记录结论，
不附私人 Flash 素材、剧情正文、原始审计报告或游戏资产。

#### Godot 辅助层的建议顺序

以下都是候选工作，尚未实施；先在应用侧证明收益，只有实际导出/装载缺口才进入引擎源码。

1. **类型化节点/资源引用与编译诊断。** 优先利用已有 C# 导出类型、明确引用和窄契约，
   对缺失、错类型及字段/命令不匹配尽早定位。若引入分析器/生成器，必须写清可检查的输入和边界；
   不承诺自动重构所有 `.tscn`、`NodePath` 或其它场景字符串。
2. **轻量 UI 状态/命令绑定。** 用已有 Theme、场景实例和 C# 组合组织静态结构与复用，
   保留普通 C# 权威模型，减少重复投影与接线。不要把这次全 C# 构造 UI 当成 Godot 唯一写法，
   也不预设绑定会自动处理所有刷新、事务或游戏规则。
3. **事件生命周期与模态焦点/输入恢复。** 明确订阅 owner、清理时机、旧命令失效、
   弹层关闭后的焦点/Disabled/Editable 恢复，以及底层时间轴是否继续。
   Godot 通常会自动断开关联信号，但捕获变量的 lambda、自定义信号或普通 C# 事件仍可能需要显式清理；
   不把节点释放等同于全部闭包安全。
4. **可复现资源与 CLI 工作流。** 固定转换、导入、资源映射、版本配对、构建和回归入口，
   让资源错误能定位到原始声明；验证冷导入和已有缓存两种情况，不把资源适配工时隐藏起来。
5. **真实 trim/AOT 导出与装载检查。** 用真实项目逐项核对反射/JSON、脚本和泛型注册、
   资源加载、插件、native/managed 配对与目标平台运行。只对可复现且应用层无法合理解决的
   引擎缺口做窄源码修改；既有 Android CoreCLR/NativeAOT 禁止项及平台验收缺口仍有效。
6. **最后才是体积与构建耗时优化。** 在功能和作者工作流稳定后，同配置测量冷/增量构建、
   完整分发包各部分和回归代价，再决定优化；不由源码更少或 API 更窄推导整体收益。

Godot 已有能力依据：[C# Node/Resource 导出](https://docs.godotengine.org/en/stable/tutorials/scripting/c_sharp/c_sharp_exports.html)、
[场景实例化](https://docs.godotengine.org/en/stable/tutorials/scripting/nodes_and_scene_instances.html)、
[共享 Theme](https://docs.godotengine.org/en/stable/tutorials/ui/gui_using_theme_editor.html)；
清理边界见 [C# 信号说明](https://docs.godotengine.org/en/stable/tutorials/scripting/c_sharp/c_sharp_signals.html)。
这里只采用其运行与数据组织能力，不以可视编辑器操作效率作为比较证据。

#### 保留自研选项与一次有界验证

如果产品目标明确转向 **.NET-first、AOT/trim 可控或小体积分发**，自研仍是可研究的独立选择；
这些目标需要真实项目和目标平台数据支撑。当前结论不足以否定该方向，也不足以为它解冻实现。
同样，不把直接嵌入 RmlUi 当成低成本捷径；渲染、字体、资源、输入、焦点和生命周期整合都需单独验证。

未来若决定补证据，只选**一个真实复杂界面**：法术参考/修炼屏，包含原长说明、
14 个三态图标（42 张状态图）、保留底层状态的模态弹层、关闭/重开以及旧事件失效。
先冻结功能范围与像素容差，允许 Godot 使用合理的场景/Theme/C# 组合。

- 同时记录作者一次状态/字段/命令修改要触及哪些文件和独立规则、资源转换/图集化工作、
  事件 owner 与生命周期代码、实际构建诊断、输入与回归成本
- 需要改引擎时先记录缺口，不用内部 API 绕过边界后宣称应用更简单
- 达到同屏行为、长文本可读、资源完整、模态恢复和旧输入无效即停止；
  不扩展成整游戏迁移、新战役或更高原作保真范围
- 只有仍需比较作者成本时，再做同一个小维护修改的配对实现；结论只适用于该界面和修改

本次仅补充策略文档，**不启动上述对照、辅助层实现或新的构建/优化矩阵**。

## 5. 历史清理与复现环境（2026-10-01 记录）

以下是 2026-10-01 原工作区的历史清理记录，不是 2026-10-03 云端归档后的现存文件或磁盘快照。
当时仅清理可重建对象、Gradle 中间文件/缓存、NuGet HTTP 缓存、工具下载及已闲置的 NDK/JDK17/Gradle 安装。
保留全部 Git 历史/工作树、源码、最终 APK/模板/SDK 包、完整证据、音频 harness/runtime、桌面 .NET 10 SDK、NuGet 包缓存与 Android build-tools。

- 释放已分配磁盘 **8,659,181,568 B（8.0645 GiB）**；文件逻辑大小合计 8,560,469,370 B
- 清理采样时可用空间 **17,964,617,728 → 26,623,799,296 B**；磁盘值是当时快照
- 2,660 个保留文件的 SHA-256 前后一致；23 个 APK/ZIP/AAR/NuGet 压缩包 CRC 前后通过
- 所有 Git refs 清理前后一致，`git fsck --full` 通过；既有 dangling objects 未清除
- 详细清单、hash、校验与工具许可证/版本信息：`../cleanup-records/20261001-android-trim/`

`../android-trim-tools/env.sh` 保留的是原复现环境设置；其中 JDK17/NDK/Gradle 安装路径已清理，
重新构建 Android native/template 前须按保留 provenance 恢复相同官方版本。APK 检查所需 build-tools 仍在。
共享 Git 实际位于本仓库 `.git/`，关联 `../godot-phase3-recovery/`；不要按旧工作区说明删除任何 Git 容器。
