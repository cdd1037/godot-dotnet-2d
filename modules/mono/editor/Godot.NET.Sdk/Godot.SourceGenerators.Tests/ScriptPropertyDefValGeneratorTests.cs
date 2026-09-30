using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Xunit;

namespace Godot.SourceGenerators.Tests;

public class ScriptPropertyDefValGeneratorTests
{
    // A getter-only override can inherit a valid setter even though Roslyn's
    // SetMethod is null. The generator runs in exports without TOOLS too.
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void ExportedOverrideWithInheritedSetter(bool tools)
    {
        var parseOptions = new CSharpParseOptions(LanguageVersion.CSharp12,
            preprocessorSymbols: tools ? new[] { "TOOLS" } : Array.Empty<string>());
        var references = ((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!)
            .Split(Path.PathSeparator).Select(path => MetadataReference.CreateFromFile(path)).ToList();
        references.Add(MetadataReference.CreateFromFile(Constants.GodotSharpAssembly.Location));
        var tree = CSharpSyntaxTree.ParseText("""
            using Godot;
            public partial class InheritanceBase : Node
            {
                public virtual int Value { get; set; }
            }
            public partial class InheritanceChild : InheritanceBase
            {
                [Export] public override int Value => 42;
            }
            """, parseOptions, "/project/InheritanceChild.cs");
        var compilation = CSharpCompilation.Create("InheritedSetterRegression", new[] { tree }, references,
            new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
        Assert.DoesNotContain(compilation.GetDiagnostics(), d => d.Severity == DiagnosticSeverity.Error);
        var property = compilation.GetTypeByMetadataName("InheritanceChild")!
            .GetMembers("Value").OfType<IPropertySymbol>().Single();
        Assert.False(property.IsReadOnly);
        Assert.Null(property.SetMethod);
        Assert.NotNull(property.OverriddenProperty!.SetMethod);

        GeneratorDriver driver = CSharpGeneratorDriver.Create(
            new ISourceGenerator[] { new ScriptPropertyDefValGenerator() }, parseOptions: parseOptions);
        var result = driver.RunGenerators(compilation).GetRunResult();
        Assert.Null(result.Results.Single().Exception);
        Assert.DoesNotContain(result.Diagnostics, d => d.Severity >= DiagnosticSeverity.Warning);
        Assert.Contains(result.Results.Single().GeneratedSources,
            source => source.HintName.Contains("InheritanceChild_ScriptPropertyDefVal"));
    }

    [Fact]
    public async Task ExportedFields()
    {
        await CSharpSourceGeneratorVerifier<ScriptPropertyDefValGenerator>.Verify(
            new string[] { "ExportedFields.cs", "MoreExportedFields.cs" },
            new string[] { "ExportedFields_ScriptPropertyDefVal.generated.cs" }
        );
    }

    [Fact]
    public async Task ExportedProperties()
    {
        await CSharpSourceGeneratorVerifier<ScriptPropertyDefValGenerator>.Verify(
            "ExportedProperties.cs",
            "ExportedProperties_ScriptPropertyDefVal.generated.cs"
        );
    }

    [Fact]
    public async Task ExportedProperties2()
    {
        await CSharpSourceGeneratorVerifier<ScriptPropertyDefValGenerator>.Verify(
            "ExportedProperties2.cs", "ExportedProperties2_ScriptPropertyDefVal.generated.cs");
    }

    [Fact]
    public async Task ExportedComplexStrings()
    {
        await CSharpSourceGeneratorVerifier<ScriptPropertyDefValGenerator>.Verify(
            "ExportedComplexStrings.cs",
            "ExportedComplexStrings_ScriptPropertyDefVal.generated.cs"
        );
    }
}
