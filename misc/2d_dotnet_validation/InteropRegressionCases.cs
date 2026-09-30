using Godot;
using System;

public static class InteropRegressionCases
{
    public static void Run(Node host)
    {
        int failures = 0;
        void Expect(bool condition, string name, string actual)
        {
            GD.Print($"{(condition ? "PASS" : "FAIL")}: {name}; {actual}");
            if (!condition) failures++;
        }
        using var stream = new System.IO.MemoryStream();
        using var managedOnly = new RegressionManagedConstructor(stream);
        Expect(ReferenceEquals(managedOnly.ManagedDependency, stream),
            "managed-only constructor remains usable", "dependency identity preserved");
        var dynamicTarget = new RegressionDynamic();
        host.AddChild(dynamicTarget);
        dynamicTarget.Set("name", "native-fallback");
        Expect(dynamicTarget.Name == "native-fallback" && dynamicTarget.SetCalls > 0,
            "_Set(false) preserves native property fallback", $"name={dynamicTarget.Name}, calls={dynamicTarget.SetCalls}");
        dynamicTarget.Name = "native-get-fallback"; // Isolate this check from a failed dynamic setter.
        var nativeName = dynamicTarget.Get("name").AsString();
        Expect(nativeName == "native-get-fallback" && dynamicTarget.GetCalls > 0,
            "_Get(nil) preserves native property fallback", $"name={nativeName}, calls={dynamicTarget.GetCalls}");
        dynamicTarget.Set(nameof(RegressionDynamic.ReadOnlyValue), 123);
        Expect(dynamicTarget.ReadOnlyValue == 123,
            "read-only property can fall back to dynamic _Set", $"value={dynamicTarget.ReadOnlyValue}");
        dynamicTarget.WriteOnlyValue = 456;
        var dynamicValue = dynamicTarget.Get(nameof(RegressionDynamic.WriteOnlyValue)).AsInt32();
        Expect(dynamicValue == 456,
            "write-only property can fall back to dynamic _Get", $"value={dynamicValue}");
        var genericTarget = new RegressionGeneric<int>();
        host.AddChild(genericTarget);
        var result = genericTarget.Call("Multiply", 6, 7).AsInt32();
        Expect(result == 42, "closed generic script method native dispatch", $"result={result}");
        dynamicTarget.QueueFree();
        genericTarget.QueueFree();
        if (failures != 0) throw new Exception($"{failures} managed interop behavior regressions");
    }
}
