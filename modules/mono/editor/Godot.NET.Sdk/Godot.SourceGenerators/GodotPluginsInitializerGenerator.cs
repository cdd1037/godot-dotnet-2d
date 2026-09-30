using System;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.Text;

namespace Godot.SourceGenerators
{
    [Generator]
    public class GodotPluginsInitializerGenerator : ISourceGenerator
    {
        public void Initialize(GeneratorInitializationContext context)
        {
        }

        public void Execute(GeneratorExecutionContext context)
        {
            bool isLibrary = context.TryGetGlobalAnalyzerProperty("IsGodotLibraryProject", out string? libraryFlag) &&
                string.Equals(libraryFlag, "true", StringComparison.OrdinalIgnoreCase);
            if (isLibrary || context.IsGodotToolsProject() || context.IsGodotSourceGeneratorDisabled("GodotPluginsInitializer"))
                return;

            bool supportLegacyNonTrimSafeApis = IsGodotSupportLegacyNonTrimSafeApisEnabled(context);

            var source = new StringBuilder();

            source.Append(
                    @"using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using Godot.Bridge;
using Godot.NativeInterop;

namespace GodotPlugins.Game
{
    internal static partial class Main
    {
        public static partial void RegisterScriptTypes();

#if TOOLS
        [RequiresUnreferencedCode(""TOOLS build of Godot project is not compatible with trimming"")]
#endif
        internal static godot_bool Initialize(IntPtr godotDllHandle, IntPtr outManagedCallbacks,
            IntPtr unmanagedCallbacks, int unmanagedCallbacksSize)
        {
            DllImportResolver dllImportResolver = new GodotDllImportResolver(godotDllHandle).OnResolveDllImport;

            var coreApiAssembly = typeof(global::Godot.GodotObject).Assembly;

            NativeLibrary.SetDllImportResolver(coreApiAssembly, dllImportResolver);

            NativeFuncs.Initialize(unmanagedCallbacks, unmanagedCallbacksSize);

#if TOOLS
            ManagedCallbacks.CreateForToolsBuild(outManagedCallbacks);
#elif !GODOT_AOT && !GODOT_TRIMMED
            // Ordinary JIT retains reflection-based argument constructor semantics.
            // This does not enable the legacy metadata resolver.
            ManagedCallbacks.CreateIncludingLegacyCallbacks(outManagedCallbacks);
#else
")
                .Append("            ")
                .Append(supportLegacyNonTrimSafeApis
                    ? "ManagedCallbacks.CreateIncludingLegacyCallbacks(outManagedCallbacks);"
                    : "ManagedCallbacks.CreateExcludingLegacyCallbacks(outManagedCallbacks);"
                )
                .Append(
                    @"
#endif

            ScriptManagerBridge.InitializeNativeClassConstructors();
#if !TOOLS && !GODOT_AOT && !GODOT_TRIMMED
            ScriptManagerBridge.EnableJitConstructorFallback();
                ScriptManagerBridge.ConfigureJitGenericMetadataResolver(static (providerName, scriptType) =>
                {
                    // Resolve against the actual script assembly, which may be collectible.
                    // Type.GetType from GodotSharp would resolve in the wrong load context.
                    int separator = providerName.IndexOf(',');
                    string typeName = separator < 0 ? providerName : providerName.Substring(0, separator);
                    if (separator >= 0 && new System.Reflection.AssemblyName(providerName.Substring(separator + 1)).Name
                        != scriptType.Assembly.GetName().Name)
                        throw new InvalidOperationException(""The metadata provider must belong to the script assembly."");
                    var providerDefinition = scriptType.Assembly.GetType(typeName)
                        ?? throw new InvalidOperationException(""The nested metadata provider was not found in the script assembly."");
                    var ownerDefinition = scriptType.GetGenericTypeDefinition();
                    var owner = providerDefinition.DeclaringType;
                    while (owner != null && owner != ownerDefinition) owner = owner.DeclaringType;
                    if (owner == null)
                        throw new InvalidOperationException(""The metadata provider is not nested within the script type."");
                    var arguments = scriptType.GetGenericArguments();
                    if (!providerDefinition.IsGenericTypeDefinition ||
                        providerDefinition.GetGenericArguments().Length != arguments.Length)
                        throw new InvalidOperationException(""The nested metadata provider has incompatible generic arguments."");
                    var closedProvider = providerDefinition.MakeGenericType(arguments);
                    if (!typeof(global::Godot.IScriptTypeMetaProvider).IsAssignableFrom(closedProvider))
                        throw new InvalidOperationException(""The nested metadata provider does not implement IScriptTypeMetaProvider."");
                    var method = closedProvider.GetMethod(""GetGodotClassScriptMeta"",
                        System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static,
                        null, Type.EmptyTypes, null)
                        ?? throw new InvalidOperationException(""The nested metadata provider has no metadata method."");
                    return (ScriptTypeMeta)method.Invoke(null, null)!;
                });
#endif

");

            if (supportLegacyNonTrimSafeApis)
            {
                source.Append(
                    @"
            // Use of legacy obsolete API manually enabled with $(GodotSupportLegacyNonTrimSafeAPIs) in MSBuild.
#pragma warning disable CS0618 // Type or member is obsolete
            ScriptManagerBridge.EnableLegacyScriptTypeMetaResolver();
#pragma warning restore CS0618 // Type or member is obsolete
");
            }

            source.Append(
                @"
            RegisterScriptTypes();

            return godot_bool.True;
        }

// The editor loads projects through GodotPlugins, not this native game entrypoint.
// .NET 10 rejects RequiresUnreferencedCode on an unmanaged entrypoint (IL2123).
#if !TOOLS
        [UnmanagedCallersOnly(EntryPoint = ""godotsharp_game_main_init"")]
        private static godot_bool InitializeFromGameProject(IntPtr godotDllHandle, IntPtr outManagedCallbacks,
            IntPtr unmanagedCallbacks, int unmanagedCallbacksSize)
        {
            try
            {
                return Initialize(godotDllHandle, outManagedCallbacks, unmanagedCallbacks, unmanagedCallbacksSize);
            }
            catch (Exception e)
            {
                global::System.Console.Error.WriteLine(e);
                return false.ToGodotBool();
            }
        }
#endif
    }
}
");

            context.AddSource("GodotPlugins.Game.generated",
                SourceText.From(source.ToString(), Encoding.UTF8));
        }

        private static bool IsGodotSupportLegacyNonTrimSafeApisEnabled(GeneratorExecutionContext context)
            => context.TryGetGlobalAnalyzerProperty("GodotSupportLegacyNonTrimSafeAPIs", out string? toggle) &&
               toggle != null &&
               toggle.Equals("true", StringComparison.OrdinalIgnoreCase);
    }
}
