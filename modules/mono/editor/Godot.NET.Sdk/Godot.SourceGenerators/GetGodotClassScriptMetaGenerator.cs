using System.Linq;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;

namespace Godot.SourceGenerators;

[Generator]
public class GetGodotClassScriptMetaGenerator : IIncrementalGenerator
{
    // ReSharper disable once StructCanBeMadeReadOnly
    private record struct Model(
        string Namespace,
        string FullQualifiedNameOmitGlobal,
        string DisplayStringMinimallyQualifiedFormat,
        GenericInfoModel? GenericInfoModel,
        bool IsSealed,
        ContainingTypeModel[]? ContainingTypeModels,
        string NativeTypeFullQualifiedNameIncludeGlobal
    )
    {
        public readonly bool Equals(Model other)
            => Namespace == other.Namespace
                && FullQualifiedNameOmitGlobal == other.FullQualifiedNameOmitGlobal
                && DisplayStringMinimallyQualifiedFormat == other.DisplayStringMinimallyQualifiedFormat
                && GenericInfoModel == other.GenericInfoModel
                && IsSealed == other.IsSealed
                && NativeTypeFullQualifiedNameIncludeGlobal == other.NativeTypeFullQualifiedNameIncludeGlobal
                && ContainingTypeModel.SequenceEquals(ContainingTypeModels, other.ContainingTypeModels);

        public readonly override int GetHashCode()
        {
            int hash = ContainingTypeModel.SequenceHashCode(ContainingTypeModels);
            hash = unchecked(hash * 31 + Namespace.GetHashCode());
            hash = unchecked(hash * 31 + FullQualifiedNameOmitGlobal.GetHashCode());
            hash = unchecked(hash * 31 + DisplayStringMinimallyQualifiedFormat.GetHashCode());
            hash = unchecked(hash * 31 + GenericInfoModel.GetHashCode());
            hash = unchecked(hash * 31 + IsSealed.GetHashCode());
            hash = unchecked(hash * 31 + NativeTypeFullQualifiedNameIncludeGlobal.GetHashCode());
            return hash;
        }
    }

    private record struct GenericInfoModel(
        string UnboundGenericReflectionNameOmitAssembly,
        string AssemblyName
    );

    public void Initialize(IncrementalGeneratorInitializationContext context)
    {
        var modelsProvider = context.SyntaxProvider
            .CreateSyntaxProvider(
                predicate: (s, _) => s is ClassDeclarationSyntax { BaseList: not null },
                transform: (ctx, _) =>
                {
                    var symbol = Common.GetIfIsGodotScriptClass(ctx);

                    if (symbol == null || symbol.GetGodotScriptNativeClass() is not { } nativeClass)
                        return (Model?)null;

                    return new Model(
                        Namespace: symbol.ContainingNamespace is { IsGlobalNamespace: false } namespaceSymbol
                            ? namespaceSymbol.FullQualifiedNameOmitGlobal()
                            : string.Empty,
                        FullQualifiedNameOmitGlobal: symbol.FullQualifiedNameOmitGlobal(),
                        DisplayStringMinimallyQualifiedFormat: symbol.ToDisplayString(SymbolDisplayFormat
                            .MinimallyQualifiedFormat),
                        GenericInfoModel: symbol.IsGenericType
                            ? new GenericInfoModel(
                                UnboundGenericReflectionNameOmitAssembly: symbol
                                    .GetReflectionName(includeAssembly: false),
                                AssemblyName: symbol.ContainingAssembly.Identity.Name
                            )
                            : null,
                        IsSealed: symbol.IsSealed,
                        ContainingTypeModels: ContainingTypeModel.GetContainingTypesFor(symbol),
                        NativeTypeFullQualifiedNameIncludeGlobal: nativeClass.FullQualifiedNameIncludeGlobal()
                    );
                })
            .Where(model => model.HasValue)
            .Select((model, _) => model!.Value)
            // Type identity deduplication is separate from semantic cache equality.
            // Stable ordering also makes multiple matching partial filenames deterministic.
            .Collect().SelectMany((items, _) => items
                .GroupBy(item => item.FullQualifiedNameOmitGlobal)
                .Select(group => group.First()));

        context.RegisterSourceOutput(modelsProvider, (spc, model) =>
        {
            bool hasNamespace = model.Namespace.Length != 0;

            string uniqueHint = model.FullQualifiedNameOmitGlobal.SanitizeQualifiedNameForUniqueHint()
                                + "_GetGodotClassScriptMeta.g.cs";

            var sb = new StringBuilder();

            sb.Append("using Godot;\n");
            sb.Append("using Godot.Bridge;\n");
            sb.Append("\n");

            if (hasNamespace)
            {
                sb.Append("namespace ");
                sb.Append(model.Namespace);
                sb.Append(";\n\n");
            }

            if (model.ContainingTypeModels != null)
            {
                foreach (var containingType in model.ContainingTypeModels)
                {
                    sb.Append("partial ");
                    sb.Append(containingType.DeclarationKeyword);
                    sb.Append(" ");
                    sb.Append(containingType.DisplayStringMinimallyQualifiedFormat);
                    sb.Append("\n{\n");
                }
            }


            if (model.GenericInfoModel is { } genericInfoModel)
            {
                // The assembly name is required for fully qualified names annotated with DynamicallyAccessedMembers.
                sb.Append("[global::Godot.GenericScriptTypeMetaProvider(\"")
                    .Append(genericInfoModel.UnboundGenericReflectionNameOmitAssembly)
                    .Append("+GodotInternal+MetaProvider, ")
                    .Append(genericInfoModel.AssemblyName)
                    .Append("\")]\n");
            }
            else
            {
                sb.Append("[global::Godot.ScriptTypeMetaProvider<")
                    .Append(model.FullQualifiedNameOmitGlobal).Append(".GodotInternal.MetaProvider>()]\n");
            }

            sb.Append("partial class ");
            sb.Append(model.DisplayStringMinimallyQualifiedFormat);
            sb.Append(" : global::Godot.IScriptTypeMetaProvider\n{\n");
            sb.Append("    static global::Godot.Bridge.ScriptTypeMeta global::Godot.IScriptTypeMetaProvider.GetGodotClassScriptMeta()\n");
            sb.Append("        => GodotInternal.CreateScriptTypeMeta();\n\n");

            sb.Append("#pragma warning disable CS0109 // Disable warning about redundant 'new' keyword\n");

            sb.Append("    ").Append(model.IsSealed ? "private " : "protected ")
                .Append(
                    "new static partial class GodotInternal\n    {\n");

            // Keep the reflection/attribute adapter separate from the statically
            // rooted implementation. Directly calling an interface implementation
            // on a generic nested provider can leave an orphan MethodImpl after
            // ILLink removes its otherwise-unused InterfaceImpl metadata.
            sb.Append("        internal class MetaProvider : global::Godot.IScriptTypeMetaProvider\n        {\n");
            sb.Append("            public static global::Godot.Bridge.ScriptTypeMeta GetGodotClassScriptMeta()\n");
            sb.Append("                => CreateScriptTypeMeta();\n        }\n\n");

            string classFullNameOmitGlobal = model.FullQualifiedNameOmitGlobal;

            const string ScriptTypeMetaType = "global::Godot.Bridge.ScriptTypeMeta";

            sb.Append("        internal static ")
                .Append(ScriptTypeMetaType)
                .Append(" CreateScriptTypeMeta()\n        {\n");

            sb.Append("                return new ").Append(ScriptTypeMetaType).Append("(\n");

            sb.Append("                    Type: ");

            // Metadata describes this exact script type, even when its constructor
            // collector is empty and an ancestor happens to expose CachedType.
            sb.Append("typeof(global::");
            sb.Append(classFullNameOmitGlobal);
            sb.Append("),\n");

            sb.Append("                    NativeType: ")
                .Append(model.NativeTypeFullQualifiedNameIncludeGlobal).Append(".CachedType,\n");
            sb.Append("                    NativeName: ")
                .Append(model.NativeTypeFullQualifiedNameIncludeGlobal).Append(".NativeName\n");
            sb.Append("                )\n                {\n");

            sb.Append("                    GetGodotClassTrampolines = GodotInternal.GetGodotClassTrampolines,\n");
            sb.Append("                    GetGodotMethodList = GodotInternal.GetGodotMethodList,\n");
            sb.Append("                    GetGodotSignalList = GodotInternal.GetGodotSignalList,\n");
            sb.Append("                    GetGodotPropertyList = GodotInternal.GetGodotPropertyList,\n");

            sb.Append("#if TOOLS\n");
            sb.Append(
                "                    GetGodotPropertyDefaultValues = GodotInternal.GetGodotPropertyDefaultValues\n");
            sb.Append("#endif\n");

            sb.Append("                };\n");
            sb.Append("            }\n");

            sb.Append("    }\n"); // partial class GodotInternal

            sb.Append("#pragma warning restore CS0109\n");

            sb.Append("}\n"); // partial class

            if (model.ContainingTypeModels != null)
            {
                foreach (var _ in model.ContainingTypeModels)
                    sb.Append("}\n"); // outer class
            }

            spc.AddSource(uniqueHint, SourceText.From(sb.ToString(), Encoding.UTF8));
        });
    }
}
