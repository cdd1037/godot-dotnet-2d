using Godot;
using Godot.NativeInterop;

partial class ManagedOnlyConstructor
{
#pragma warning disable CS0109 // Disable warning about redundant 'new' keyword
    /// <summary>
    /// Cached StringNames for the methods contained in this class, for fast lookup.
    /// </summary>
    public new class MethodName : global::Godot.GodotObject.MethodName {
    }
    protected internal new static partial class GodotInternal
    {
        public static void GetGodotMethodTrampolines(global::Godot.Bridge.MethodTrampolineCollector collector)
        {
        }
        public new static unsafe void GetGodotConstructorTrampolines(global::Godot.Bridge.ConstructorTrampolineCollector collector)
        {
        }
    }
#pragma warning restore CS0109
}
