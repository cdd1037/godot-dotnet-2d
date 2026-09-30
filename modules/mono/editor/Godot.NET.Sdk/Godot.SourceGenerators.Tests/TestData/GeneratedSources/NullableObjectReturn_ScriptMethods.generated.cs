using Godot;
using Godot.NativeInterop;

partial class NullableObjectReturn
{
#pragma warning disable CS0109 // Disable warning about redundant 'new' keyword
    /// <summary>
    /// Cached StringNames for the methods contained in this class, for fast lookup.
    /// </summary>
    public new class MethodName : global::Godot.Node.MethodName {
        /// <summary>
        /// Cached name for the 'GetOptionalTexture' method.
        /// </summary>
        public new static readonly global::Godot.StringName @GetOptionalTexture = "GetOptionalTexture";
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
            var methods = new global::System.Collections.Generic.List<global::Godot.Bridge.MethodInfo>(1);
        methods.Add(new(name: MethodName.@GetOptionalTexture, returnVal: new(type: (global::Godot.Variant.Type)24, name: "", hint: (global::Godot.PropertyHint)0, hintString: "", usage: (global::Godot.PropertyUsageFlags)6, exported: false, className: new global::Godot.StringName("Texture2D")), flags: (global::Godot.MethodFlags)1, arguments: null, defaultArguments: null));
            return methods;
        }
        private static unsafe void GetGodotMethodTrampolines(global::Godot.Bridge.MethodTrampolineCollector collector)
        {
            static godot_variant trampoline_0_GetOptionalTexture(object godotObject, NativeVariantPtrArgs args, ref godot_variant_call_error callError)
            {
                if (args.Count != 0) {
                    callError = godot_variant_call_error.CreateInvalidArgumentCountError(expected: 0, provided: args.Count);
                    return default;
                }
                var callRet = ((global::NullableObjectReturn)godotObject).@GetOptionalTexture();
                return global::Godot.NativeInterop.VariantUtils.CreateFromGodotObject(callRet);
            }
            var aux_delegate_0_GetOptionalTexture = trampoline_0_GetOptionalTexture;
            collector.TryAdd(new(MethodName.@GetOptionalTexture, 0), new(aux_delegate_0_GetOptionalTexture.Method.MethodHandle.GetFunctionPointer(), isStatic: false));
        }
        [global::System.Diagnostics.CodeAnalysis.DynamicallyAccessedMembers(global::System.Diagnostics.CodeAnalysis.DynamicallyAccessedMemberTypes.PublicConstructors | global::System.Diagnostics.CodeAnalysis.DynamicallyAccessedMemberTypes.NonPublicConstructors)]
        public static global::System.Type CachedType { get; } = typeof(global::NullableObjectReturn);
        private static partial class Accessors
        {
            [global::System.Runtime.CompilerServices.UnsafeAccessor(global::System.Runtime.CompilerServices.UnsafeAccessorKind.Method, Name = ".ctor")]
            public extern static void CtorAsMethod(global::NullableObjectReturn godotObject);
        }
        private static unsafe void GetGodotConstructorTrampolines(global::Godot.Bridge.ConstructorTrampolineCollector collector)
        {
            static global::Godot.GodotObject trampoline_0(global::System.IntPtr godotObjectPtr, NativeVariantPtrArgs args)
            {
                if (args.Count != 0) {
                    throw new global::System.MissingMemberException($"Invalid argument count for constructor of class 'NullableObjectReturn'. Expected 0, but got {args.Count}.");
                }
                var godotObject = (global::NullableObjectReturn)global::System.Runtime.CompilerServices.RuntimeHelpers.GetUninitializedObject(global::NullableObjectReturn.GodotInternal.CachedType);
                global::Godot.Bridge.ScriptManagerBridge.Accessors.UnsafeSetGodotObjectNativePtr(godotObject, godotObjectPtr);
                global::NullableObjectReturn.GodotInternal.Accessors.CtorAsMethod(godotObject);
                return godotObject;
            }
            collector.TryAdd(0, new(&trampoline_0));
        }
    }
#pragma warning restore CS0109
}
