# Godot 2D / .NET fork

基于 Godot 4.7.2 的 2D、UI、C# 定制引擎，当前使用 .NET 10。
保留桌面工具链与 Android arm64 / Mono、Vulkan、SAF、预编译模板 APK 导出；
已移除 3D、GDScript、高层多人同步、视频播放等功能。上游通用介绍不代表本 fork 的支持范围。

## 从这里开始

- [构建与导出](BUILDING.md)：工具链、配套 SDK/模板、桌面三模式、Android APK
- [定制与兼容边界](CUSTOMIZATION.md)：保留/删除功能、托管接口、profile 限制、上游来源
- [当前结果与后续计划](PROJECT_RESULTS.md)：精确源码、CI、交付物、验证缺口和暂缓事项
- [作者工作流策略复核](PROJECT_RESULTS.md#41-作者工作流策略复核2026-10-03)：2026-10-03 的证据边界、Godot + 薄 C# 辅助层建议与自研保留条件
- [历史实验与来源附录](PROJECT_HISTORY.md)：旧检查点、测量、失败尝试与详细研究

Android 默认是普通 Mono JIT。实验性 trimmed JIT 的匹配未签名测试重打包
从 **31.7218 降到 22.4862 MiB（29.1143%）**；这不代表自动 trimmed APK 导出或真机验收通过。
桌面历史包与当前 .NET 10 包不能混用；请先核对结果页中的平台、提交和验证范围。

## 上游与许可

Godot 是自由开源引擎。本 fork 保留 [MIT 许可](LICENSE.txt)、[第三方版权](COPYRIGHT.txt)、
[贡献者](AUTHORS.md)和[贡献指南](CONTRIBUTING.md)。分发时同时保留实际随包第三方及 .NET runtime notices。

[上游官网](https://godotengine.org) · [官方文档](https://docs.godotengine.org) ·
[上游源码](https://github.com/godotengine/godot) · [社区](https://godotengine.org/community)

官方二进制、模板及 NuGet 包不包含本 fork 的定制，不能替代匹配的本地构建和交付物。
