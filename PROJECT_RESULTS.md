# 项目交付与待办索引

更新：2026-10-01。当前状态入口；完整实现、历史测量与归因见 [CUSTOMIZATION.md](CUSTOMIZATION.md)。
本次整理只更新文档/机读结果与清理可重建缓存，没有修改生产源码、profile、CI 或重新构建产物。

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

## 4. 待办与暂缓

1. **Android 发布前验收**：真实项目的自动 trimmed 导出、签名安装、冷启动、Vulkan/ETC2、JNI/crypto、生命周期与 SAF 权限；当前没有真机结果
2. **音频**：等用户 PC 上普通 AudioStreamPlayer / AudioStreamPlayer2D 对比数据，再决定窄范围优化；保留空间位置、衰减和 bus 语义
3. **上游维护**：沿用已安排的每周检查。审计固定于 `2490bf30ec229ef3eeda24befb9d80d2226c8d29`、1,362 项快照；见 [状态账本](misc/upstream_sync/status.json)。`120746` 部分采用；`119123`、`120545`、`122667`、`123693` 仍按既有依据延期
4. **未实施**：size_extra、Android relocation packing 等额外体积建议；未经新的实施决定，不修改 profiles，不增加构建矩阵
5. **仍不推广**：其它 Android ABI、AAB、自定义 Gradle、Android CoreCLR/NativeAOT，以及未经运行验收的 macOS

## 5. 清理与复现环境

本轮仅清理可重建对象、Gradle 中间文件/缓存、NuGet HTTP 缓存、工具下载及已闲置的 NDK/JDK17/Gradle 安装。
保留全部 Git 历史/工作树、源码、最终 APK/模板/SDK 包、完整证据、音频 harness/runtime、桌面 .NET 10 SDK、NuGet 包缓存与 Android build-tools。

- 释放已分配磁盘 **8,659,181,568 B（8.0645 GiB）**；文件逻辑大小合计 8,560,469,370 B
- 清理采样时可用空间 **17,964,617,728 → 26,623,799,296 B**；磁盘值是当时快照
- 2,660 个保留文件的 SHA-256 前后一致；23 个 APK/ZIP/AAR/NuGet 压缩包 CRC 前后通过
- 所有 Git refs 清理前后一致，`git fsck --full` 通过；既有 dangling objects 未清除
- 详细清单、hash、校验与工具许可证/版本信息：`../cleanup-records/20261001-android-trim/`

`../android-trim-tools/env.sh` 保留的是原复现环境设置；其中 JDK17/NDK/Gradle 安装路径已清理，
重新构建 Android native/template 前须按保留 provenance 恢复相同官方版本。APK 检查所需 build-tools 仍在。
共享 Git 实际位于本仓库 `.git/`，关联 `../godot-phase3-recovery/`；不要按旧工作区说明删除任何 Git 容器。
