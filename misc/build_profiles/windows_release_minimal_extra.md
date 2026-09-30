# 第二组：进一步裁剪，并补上 MSDF 字体

使用 `profile=misc/build_profiles/windows_release_minimal_extra.py`，
输出后缀是 `.minimal_extra.mono.exe`。第一组 profile 和已保存的产物不受影响。

相对第一组，明确修改：

- `minizip=False`：不注册运行时 ZIP 资源包来源；普通 PCK / 嵌入 PCK 保留。
  当前 `core/SCsub` 仍无条件编译 minizip 的第三方源码，不能把这个开关解释成
  从构建图中删除整套库。ZIP 与 zlib/Deflate 也不是同一个开关
- `brotli=False`：不再编入内置 Brotli，禁用 FreeType 的 Brotli 集成；不能再用
  WOFF2 或 `Compression.MODE_BROTLI` 解压。普通 TTF/OTF 字体不因此禁用
- `graphite=False`：移除 SIL Graphite 智能字体引擎和 HarfBuzz 的 Graphite 桥
- `disable_navigation_2d=False`：按最新确认保留导航节点/API/资源，暂缓彻底裁剪。
  `module_navigation_2d` 仍然关闭，因此导航后端仍是 dummy，不能当作寻路可用
- `builtin_certs=False`：不生成内置 CA bundle；当前 mbedtls 未启用，内置 CA 数组
  没有 TLS 消费者，不应预期这里再贡献明显体积节省
- `module_msdfgen_enabled=True`：按后续确认补上 MSDF 字体生成能力；其 FreeType
  硬依赖已启用。MSDF 的新增体积必须计入这组，不能将两组差值全当成“删减收益”
- `deprecated=True`：按最新确认暂缓移除旧资源/API兼容路径。
  advanced GUI、AccessKit、WinRT、SDL 和 `optimize=size` 同样保持原值

HarfBuzz 一直开启，没有 `module_harfbuzz` 这个模块开关。这里显式保留
`builtin_harfbuzz=True`，正常 OpenType 整形仍在；关闭 Graphite 不等于关闭 HarfBuzz。
`builtin_harfbuzz=False` 表示改用外部依赖，不能当作“删除 HarfBuzz”的方法。

`disable_physics_3d` / `disable_navigation_3d` 在此 fork 已硬编码为 True，
相关 3D 源码已移除，不需要重复添加无收益参数。

如果未来重新启用 mbedtls，`builtin_certs=False` 只删除内置证书兜底，
不禁用 TLS，也不禁用 Windows 系统证书：实际读取顺序是项目指定证书、
Windows 系统 ROOT 证书库、最后才是内置 CA bundle。

## .NET 兼容边界

本组最新版本保留导航 API 和 deprecated 兼容路径，以避开现有 GodotSharp 静态方法
绑定与这两种 API 裁剪的已知不匹配；彻底导航裁剪及匹配绑定生成/验证留待后续。
减少模块和完整关闭导航 API 是不同层面的操作：当前 NativeProxyRegistry 只记录
Type 与 StringName，不立即请求缺失类的 native 构造函数；真正实例化未包含模块的类
仍会失败，不能把“注册不失败”当成全部 API 可用。
原生模板编译成功并不证明 C# 最终发布兼容；还需要同 fork 的 SDK/绑定及项目级 Windows
运行验证。这一轮不把原生 EXE 体积冒充完整 .NET 游戏包体积。

构建、字体资源要求、其他未启用模块、许可证和验证边界，参见
`windows_release_minimal.md`。基线和第二组应保持相同 compiler、优化等级及 strip 政策；
同时分别列出模块/功能差异和 LTO 状态。
