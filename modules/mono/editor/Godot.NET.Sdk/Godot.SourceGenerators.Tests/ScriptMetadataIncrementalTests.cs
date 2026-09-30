using System;
using System.Collections.Immutable;
using System.IO;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Diagnostics;
using Xunit;

namespace Godot.SourceGenerators.Tests;

// These tests exercise generator output and reused-driver invalidation. A small, real
// metadata assembly named GodotSharp preserves the production candidate filter. They
// do not assert runtime behavior of the generated bridge (covered by engine tests).
public class ScriptMetadataIncrementalTests
{
    private static readonly CSharpParseOptions Parse = new(LanguageVersion.CSharp12);
    private static readonly ImmutableArray<MetadataReference> References = CreateReferences();

    private static ImmutableArray<MetadataReference> CreateReferences()
    {
        var refs = ((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator)
            .Select(path => (MetadataReference)MetadataReference.CreateFromFile(path)).ToList();
        var stub = CSharpCompilation.Create("GodotSharp", new[] { CSharpSyntaxTree.ParseText(
            "namespace Godot { public class GodotObject {} public class Node : GodotObject {} public class Resource : GodotObject {} [System.AttributeUsage(System.AttributeTargets.Class)] public class NoScriptFileAssociationAttribute : System.Attribute {} [System.AttributeUsage(System.AttributeTargets.Assembly, AllowMultiple=true)] public class RegisterScriptTypeAttribute : System.Attribute { public RegisterScriptTypeAttribute(System.Type type) {} } }", Parse) }, refs,
            new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
        using var stream = new MemoryStream();
        var emitted = stub.Emit(stream);
        Assert.True(emitted.Success, string.Join("\n", emitted.Diagnostics));
        refs.Add(MetadataReference.CreateFromImage(stream.ToArray()));
        return refs.ToImmutableArray();
    }

    private static CSharpCompilation Compilation(string source, string path = "/project/Sample.cs", string assembly = "SampleAssembly")
        => CSharpCompilation.Create(assembly, new[] { CSharpSyntaxTree.ParseText(source, Parse, path) }, References,
            new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

    private static GeneratorDriver Driver(IIncrementalGenerator generator)
        => CSharpGeneratorDriver.Create(new[] { generator.AsSourceGenerator() }, parseOptions: Parse,
            optionsProvider: new Options());

    private static string Output(GeneratorDriver driver)
    {
        var result = driver.GetRunResult();
        Assert.DoesNotContain(result.Diagnostics, d => d.Severity == DiagnosticSeverity.Error);
        return string.Join("\n", result.Results.SelectMany(r => r.GeneratedSources)
            .OrderBy(s => s.HintName, StringComparer.Ordinal).Select(s => s.SourceText.ToString()));
    }

    private static string CheckEdit(IIncrementalGenerator generator, CSharpCompilation before, CSharpCompilation after)
    {
        var driver = Driver(generator).RunGenerators(before);
        var old = Output(driver);
        driver = driver.RunGenerators(after);
        var updated = Output(driver);
        Assert.NotEqual(old, updated);
        Assert.Equal(Output(Driver(generator).RunGenerators(after)), updated);
        return updated;
    }

    [Fact]
    public void NativeBaseEditInvalidatesMetadata()
    {
        var before = Compilation("public partial class Sample : Godot.Node {}");
        var after = before.ReplaceSyntaxTree(before.SyntaxTrees.Single(), CSharpSyntaxTree.ParseText(
            "public partial class Sample : Godot.Resource {}", Parse, "/project/Sample.cs"));
        Assert.Contains("NativeType: global::Godot.Resource.CachedType", CheckEdit(new GetGodotClassScriptMetaGenerator(), before, after));
    }

    [Fact]
    public void SealedEditInvalidatesMetadata()
        => Assert.Contains("private new static partial class GodotInternal", CheckEdit(new GetGodotClassScriptMetaGenerator(),
            Compilation("public partial class Sample : Godot.Node {}"), Compilation("public sealed partial class Sample : Godot.Node {}")));

    [Fact]
    public void ContainingTypeEditInvalidatesMetadata()
        => Assert.Contains("partial struct Outer", CheckEdit(new GetGodotClassScriptMetaGenerator(),
            Compilation("public partial class Outer { public partial class Sample : Godot.Node {} }"),
            Compilation("public partial struct Outer { public partial class Sample : Godot.Node {} }")));

    [Fact]
    public void GenericAssemblyEditInvalidatesMetadata()
    {
        var before = Compilation("public partial class Sample<T> : Godot.Node {}", assembly: "FirstAssembly");
        Assert.Contains(", SecondAssembly", CheckEdit(new GetGodotClassScriptMetaGenerator(), before, before.WithAssemblyName("SecondAssembly")));
    }

    [Fact]
    public void FileMoveInvalidatesScriptPath()
        => Assert.Contains("res://moved/Sample.cs", CheckEdit(new RegisterScriptTypesGenerator(),
            Compilation("public partial class Sample : Godot.Node {}"),
            Compilation("public partial class Sample : Godot.Node {}", "/project/moved/Sample.cs")));

    [Fact]
    public void NoAssociationSuppressesOnlyPathRegistration()
    {
        var before = Compilation("public partial class Sample : Godot.Node {}");
        var after = Compilation("[Godot.NoScriptFileAssociation] public partial class Sample : Godot.Node {}");
        string output = CheckEdit(new RegisterScriptTypesGenerator(), before, after);
        Assert.DoesNotContain("RegisterScriptPathForType", output);
        Assert.DoesNotContain("ScriptPath(", output);
        Assert.Contains("RegisterScriptPathForType", CheckEdit(new RegisterScriptTypesGenerator(), after, before));
        Assert.Contains("GetGodotClassScriptMeta()", Output(Driver(new GetGodotClassScriptMetaGenerator()).RunGenerators(after)));
    }

    [Fact]
    public void PartialDeclarationsAreDeduplicatedDeterministically()
    {
        const string source = "public partial class Sample : Godot.Node {}";
        var first = Compilation(source, "/project/z/Sample.cs");
        var second = CSharpSyntaxTree.ParseText(source, Parse, "/project/a/Sample.cs");
        var both = first.AddSyntaxTrees(second);
        var reversed = both.RemoveAllSyntaxTrees().AddSyntaxTrees(both.SyntaxTrees.Reverse());
        foreach (var generator in new IIncrementalGenerator[] { new GetGodotClassScriptMetaGenerator(), new RegisterScriptTypesGenerator() })
        {
            string output = Output(Driver(generator).RunGenerators(both));
            Assert.Equal(output, Output(Driver(generator).RunGenerators(reversed)));
            if (generator is RegisterScriptTypesGenerator)
            {
                Assert.Contains("res://a/Sample.cs", output);
                Assert.DoesNotContain("res://z/Sample.cs", output);
            }
        }
    }

    [Theory]
    [InlineData("public partial class Sample<T> : Godot.Node {}", "global::Sample<>")]
    [InlineData("namespace Example; public partial class Sample<T, U> : Godot.Node {}", "global::Example.Sample<,>")]
    [InlineData("namespace @event; public partial class Sample<T> : Godot.Node {}", "global::@event.Sample<>")]
    public void OpenGenericRegistrationUsesValidQualifiedSyntax(string source, string expectedType)
    {
        string output = Output(Driver(new RegisterScriptTypesGenerator()).RunGenerators(Compilation(source)));
        Assert.Contains("typeof(" + expectedType + ")", output);
        var syntax = CSharpSyntaxTree.ParseText(output, Parse);
        Assert.DoesNotContain(syntax.GetDiagnostics(), diagnostic => diagnostic.Severity == DiagnosticSeverity.Error);
    }

    [Fact]
    public void MetadataTypeDoesNotDependOnConstructorCachedType()
    {
        const string source = "public partial class Sample : Godot.Node { public Sample(System.IO.Stream stream) {} }";
        string output = Output(Driver(new GetGodotClassScriptMetaGenerator()).RunGenerators(Compilation(source)));
        Assert.Contains("Type: typeof(global::Sample),", output);
        Assert.DoesNotContain("Type: CachedType,", output);
    }

    [Fact]
    public void StaticManifestRootsClosedGenericWithoutAssociatingPath()
    {
        const string source = "[assembly: Godot.RegisterScriptType(typeof(Sample<int>))] [Godot.NoScriptFileAssociation] public partial class Sample<T> : Godot.Node {}";
        var compilation = Compilation(source);
        string output = Output(Driver(new RegisterScriptTypesGenerator()).RunGenerators(compilation));
        Assert.Contains("RegisterScriptTypeMetadata<global::Sample<int>>()", output);
        Assert.DoesNotContain("RegisterScriptPathFor", output);
        string metadata = Output(Driver(new GetGodotClassScriptMetaGenerator()).RunGenerators(compilation));
        Assert.Contains("partial class Sample<T> : global::Godot.IScriptTypeMetaProvider", metadata);
        Assert.Contains("global::Godot.IScriptTypeMetaProvider.GetGodotClassScriptMeta()", metadata);
    }

    [Theory]
    [InlineData("Sample<>")]
    [InlineData("System.String")]
    [InlineData("Godot.Node")]
    public void StaticManifestRejectsInvalidOrOpenTypes(string typeName)
    {
        string source = "[assembly: Godot.RegisterScriptType(typeof(" + typeName + "))] public partial class Sample<T> : Godot.Node {}";
        var result = Driver(new RegisterScriptTypesGenerator()).RunGenerators(Compilation(source)).GetRunResult();
        Assert.Single(result.Diagnostics.Where(diagnostic => diagnostic.Id == "GDT0001"));
        Assert.DoesNotContain(result.Results, item => item.Exception != null);
    }

    [Fact]
    public void StaticManifestDeduplicatesExactClosedTypes()
    {
        const string source = "[assembly: Godot.RegisterScriptType(typeof(Sample<int>))] [assembly: Godot.RegisterScriptType(typeof(Sample<int>))] [assembly: Godot.RegisterScriptType(typeof(Sample<string>))] public partial class Sample<T> : Godot.Node {}";
        string output = Output(Driver(new RegisterScriptTypesGenerator()).RunGenerators(Compilation(source)));
        Assert.Equal(1, output.Split("RegisterScriptTypeMetadata<global::Sample<int>>()").Length - 1);
        Assert.Equal(1, output.Split("RegisterScriptTypeMetadata<global::Sample<string>>()").Length - 1);
    }

    [Fact]
    public void LibraryHasNoHostEntrypointOrPartialDeclarationDependency()
    {
        var options = new Options(library: true);
        var compilation = Compilation("public partial class Sample : Godot.Node {}");
        GeneratorDriver register = CSharpGeneratorDriver.Create(
            new[] { new RegisterScriptTypesGenerator().AsSourceGenerator() }, parseOptions: Parse, optionsProvider: options);
        string output = Output(register.RunGenerators(compilation));
        Assert.Contains("public static void RegisterScriptTypes()", output);
        Assert.DoesNotContain("public static partial void", output);
        GeneratorDriver initialize = CSharpGeneratorDriver.Create(
            new ISourceGenerator[] { new GodotPluginsInitializerGenerator() }, parseOptions: Parse, optionsProvider: options);
        Assert.Equal("", Output(initialize.RunGenerators(compilation)));
    }

    [Fact]
    public void StaticMetadataDoesNotRootGenericNestedInterfaceAdapter()
    {
        string output = Output(Driver(new GetGodotClassScriptMetaGenerator()).RunGenerators(
            Compilation("public partial class Sample<T> : Godot.Node {}")));
        Assert.Contains("=> GodotInternal.CreateScriptTypeMeta();", output);
        Assert.Contains("internal static global::Godot.Bridge.ScriptTypeMeta CreateScriptTypeMeta()", output);
        Assert.DoesNotContain("=> GodotInternal.MetaProvider.GetGodotClassScriptMeta();", output);
        Assert.DoesNotContain(CSharpSyntaxTree.ParseText(output, Parse).GetDiagnostics(), diagnostic => diagnostic.Severity == DiagnosticSeverity.Error);
    }

    private sealed class Options : AnalyzerConfigOptionsProvider
    {
        private readonly Values values;
        public Options(bool library = false) => values = new Values(library);
        public override AnalyzerConfigOptions GlobalOptions => values;
        public override AnalyzerConfigOptions GetOptions(SyntaxTree tree) => values;
        public override AnalyzerConfigOptions GetOptions(AdditionalText textFile) => values;
        private sealed class Values : AnalyzerConfigOptions
        {
            private readonly bool library;
            public Values(bool library) => this.library = library;
            public override bool TryGetValue(string key, out string value)
            {
                value = key switch
                {
                    "build_property.GodotProjectDir" => "/project/",
                    "build_property.GodotRegisterScriptPaths" => "true",
                    "build_property.IsGodotLibraryProject" => library ? "true" : "false",
                    _ => ""
                };
                return value.Length > 0;
            }
        }
    }
}
