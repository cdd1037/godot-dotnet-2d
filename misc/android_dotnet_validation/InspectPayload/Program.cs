using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using System.Text.Json;

if (args.Length != 2 || args[1] is not ("jit" or "trimmed-jit"))
    throw new ArgumentException("Usage: InspectPayload <publish-directory> <jit|trimmed-jit>");
string directory = Path.GetFullPath(args[0]);
string mode = args[1];
using var file = File.OpenRead(Path.Combine(directory, "AndroidMonoSmoke.dll"));
using var pe = new PEReader(file);
var metadata = pe.GetMetadataReader();
bool entrypoint = false, unused = false;
foreach (var handle in metadata.TypeDefinitions)
{
    var type = metadata.GetTypeDefinition(handle);
    string name = metadata.GetString(type.Name);
    unused |= name == "AndroidTrimUnusedSentinel";
    if (metadata.GetString(type.Namespace) != "GodotPlugins.Game" || name != "Main") continue;
    foreach (var methodHandle in type.GetMethods())
    {
        var method = metadata.GetMethodDefinition(methodHandle);
        if (metadata.GetString(method.Name) == "InitializeFromGameProject")
            entrypoint = method.RelativeVirtualAddress != 0;
    }
}
bool jitFallback = metadata.MemberReferences.Any(handle =>
    metadata.GetString(metadata.GetMemberReference(handle).Name) == "EnableJitConstructorFallback");
if (jitFallback != (mode == "jit")) throw new Exception("Generated callback mode does not match publish mode");
if (!entrypoint) throw new Exception("Hosted native entrypoint was removed or has no body");
if (unused != (mode == "jit")) throw new Exception("Dead-type trimming sentinel has unexpected presence");
foreach (string name in new[] { "libmonosgen-2.0.so", "libSystem.Security.Cryptography.Native.Android.so",
    "libSystem.Security.Cryptography.Native.Android.jar", "System.Private.CoreLib.dll", "GodotSharp.dll" })
    if (!File.Exists(Path.Combine(directory, name))) throw new Exception($"Missing runtime dependency: {name}");
using var config = JsonDocument.Parse(File.ReadAllText(Path.Combine(directory, "AndroidMonoSmoke.runtimeconfig.json")));
var options = config.RootElement.GetProperty("runtimeOptions");
if (options.GetProperty("tfm").GetString() != "net10.0" ||
    options.GetProperty("includedFrameworks")[0].GetProperty("version").GetString() != "10.0.12")
    throw new Exception("Unexpected target framework or runtime version");
var records = Directory.GetFiles(directory, "*", SearchOption.AllDirectories).Select(path => new
{
    name = Path.GetRelativePath(directory, path).Replace('\\', '/'),
    bytes = new FileInfo(path).Length,
    sha256 = Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(path))),
    category = path.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ? "managed" :
        path.EndsWith(".so", StringComparison.OrdinalIgnoreCase) ? "native_runtime" :
        path.EndsWith(".a", StringComparison.OrdinalIgnoreCase) ? "excluded_static_link_input" : "other"
}).OrderBy(entry => entry.name).ToArray();
if (records.Any(entry => entry.name.Contains("coreclr", StringComparison.OrdinalIgnoreCase) ||
    entry.name.Contains("hostfxr", StringComparison.OrdinalIgnoreCase)))
    throw new Exception("Desktop runtime leaked into Mono payload");
Console.WriteLine(JsonSerializer.Serialize(new { mode, rooted_entrypoint = entrypoint, unused_type_present = unused, jit_constructor_fallback = jitFallback,
    android_device_runtime_test = false, files = records }, new JsonSerializerOptions { WriteIndented = true }));
