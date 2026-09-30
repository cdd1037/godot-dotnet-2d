using Godot;
using System;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

[Tool]
public partial class TestPlugin : EditorPlugin
{
    private const BindingFlags Private = BindingFlags.Instance | BindingFlags.NonPublic;
    public override void _EnterTree() => Callable.From(RunTests).CallDeferred();
    private static void Check(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception(message);
        }
        GD.Print("PASS: " + message);
    }
    private async void RunTests()
    {
        Node? panel = null;
        Node? output = null;
        RichTextLabel? label = null;
        try
        {
            var assembly = AppDomain.CurrentDomain.GetAssemblies().Single(a => a.GetName().Name == "GodotTools");
            GD.Print("ASSEMBLY: " + assembly.FullName + " MVID=" + assembly.ManifestModule.ModuleVersionId);
            var type = assembly.GetType("GodotTools.Build.MSBuildPanel", true)!;
            panel = (Node)Activator.CreateInstance(type)!;
            var outputType = assembly.GetType("GodotTools.Build.BuildOutputView", true)!;
            output = (Node)Activator.CreateInstance(outputType)!;
            label = new RichTextLabel();
            outputType.GetField("_log", Private)!.SetValue(output, label);
            type.GetField("_outputView", Private)!.SetValue(panel, output);
            var pending = (StringBuilder)type.GetField("_pendingBuildLogText", Private)!.GetValue(panel)!;
            var stdout = (Action<string?>)type.GetMethod("StdOutputReceived", Private)!.CreateDelegate(typeof(Action<string?>), panel);
            var stderr = (Action<string?>)type.GetMethod("StdErrorReceived", Private)!.CreateDelegate(typeof(Action<string?>), panel);
            var flush = (Action)type.GetMethod("UpdateBuildLogText", Private)!.CreateDelegate(typeof(Action), panel);
            string nl = Environment.NewLine;
            stdout(null); stderr(""); stdout("  "); stderr("a\nb"); stdout("tail\n"); stderr("cr\r\nlf");
            string expected = nl + nl + "  " + nl + "a\nb" + nl + "tail\n" + nl + "cr\r\nlf" + nl;
            Check(pending.ToString() == expected, "null, empty, whitespace and embedded newline buffering");
            // RichTextLabel strips carriage returns; buffer bytes are checked separately.
            string renderedExpected = expected.Replace("\r", "");
            Check(label.GetParsedText() == "", "output remains deferred before main-loop flush");
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
            Check(label.GetParsedText() == renderedExpected && pending.Length == 0, "real deferred call flushes whole batch and clears buffer");
            flush();
            flush();
            Check(label.GetParsedText() == renderedExpected, "repeated empty flush does not duplicate output");
            stdout("next");
            Check(pending.ToString() == "next" + nl, "next batch reuses emptied builder");
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
            Check(label.GetParsedText() == renderedExpected + "next\n", "next batch schedules another deferred flush");
            label.Clear();
            const int count = 10000;
            var producers = Task.WhenAll(
                Task.Run(() =>
                {
                    for (int i = 0; i < count; i++)
                    {
                        stdout("out:" + i);
                    }
                }),
                Task.Run(() =>
                {
                    for (int i = 0; i < count; i++)
                    {
                        stderr("err:" + i);
                    }
                }));
            while (!producers.IsCompleted)
            {
                flush();
                await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
            }
            await producers;
            flush();
            var lines = label.GetParsedText().Split("\n", StringSplitOptions.RemoveEmptyEntries);
            Check(lines.Length == 2 * count && lines.Distinct().Count() == 2 * count, "20,000 concurrent stdout/stderr lines retained exactly once across flushes");
            foreach (string prefix in new[] { "out:", "err:" })
            {
                Check(lines.Where(s => s.StartsWith(prefix)).SequenceEqual(Enumerable.Range(0, count).Select(i => prefix + i)), prefix + " per-stream order preserved");
            }
            string final = label.GetParsedText();
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
            Check(label.GetParsedText() == final && pending.Length == 0, "queued deferred flushes after explicit flush remain idempotent");
            Check(ReferenceEquals(pending, type.GetField("_pendingBuildLogText", Private)!.GetValue(panel)), "same StringBuilder retained across batches");
            GD.Print("MSBUILD_LOG_TESTS_PASSED");
            panel.Free();
            panel = null;
            output.Free();
            output = null;
            label.Free();
            label = null;
            GetTree().Quit(0);
        }
        catch (Exception ex)
        {
            GD.PrintErr("MSBUILD_LOG_TESTS_FAILED: " + ex);
            panel?.Free();
            output?.Free();
            label?.Free();
            GetTree().Quit(1);
        }
    }
}
