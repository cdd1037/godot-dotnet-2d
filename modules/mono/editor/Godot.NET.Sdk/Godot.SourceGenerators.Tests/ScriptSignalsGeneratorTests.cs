using System.Threading.Tasks;
using Xunit;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace Godot.SourceGenerators.Tests;

public class ScriptSignalsGeneratorTests
{
    [Fact]
    public async Task EventSignals()
    {
        await CSharpSourceGeneratorVerifier<ScriptSignalsGenerator>.Verify(
            "EventSignals.cs",
            "EventSignals_ScriptSignals.generated.cs"
        );
    }
    [Fact]
    public async Task NullableSignalBackingField()
    {
        var test = CSharpSourceGeneratorVerifier<ScriptSignalsGenerator>.MakeVerifier(
            new[] { "NullableSignals.cs" }, new[] { "NullableSignals_ScriptSignals.generated.cs" });
        test.SolutionTransforms.Add((solution, projectId) =>
            solution.WithProjectCompilationOptions(projectId,
                ((CSharpCompilationOptions)solution.GetProject(projectId)!.CompilationOptions!)
                    .WithNullableContextOptions(NullableContextOptions.Enable)));
        await test.RunAsync();
    }
}
