using Godot;
using Godot.NativeInterop;

partial class GenericClass<T>
{
partial class NestedClass
{
#pragma warning disable CS0109 // Disable warning about redundant 'new' keyword
    /// <summary>
    /// Cached StringNames for the methods contained in this class, for fast lookup.
    /// </summary>
    public new class MethodName : global::Godot.GodotObject.MethodName {
    }
    protected new static partial class GodotInternal
    {
        /// <summary>
        /// Get the method information for all the methods declared in this class.
        /// This method is used by Godot to register the available methods in the editor.
        /// Do not call this method.
        /// </summary>
        public static
#nullable enable
            global::System.Collections.Generic.List<global::Godot.Bridge.MethodInfo>?
#nullable restore
            GetGodotMethodList()
        {
            return null;
        }
        private static unsafe void GetGodotMethodTrampolines(global::Godot.Bridge.MethodTrampolineCollector collector)
        {
        }
        private static unsafe void GetGodotConstructorTrampolines(global::Godot.Bridge.ConstructorTrampolineCollector collector)
        {
            static global::Godot.GodotObject trampoline_0(global::System.IntPtr ptr, NativeVariantPtrArgs args)
            {
                if (args.Count != 0) throw new global::System.ArgumentException("Expected no constructor arguments.");
                return global::Godot.Bridge.ScriptManagerBridge.Accessors.CreateKnownGenericScriptInstance<global::GenericClass<T>.NestedClass>(ptr);
            }
            collector.TryAdd(0, new(&trampoline_0));
        }
    }
#pragma warning restore CS0109
}
}
