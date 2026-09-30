using Godot;
using Godot.NativeInterop;

partial class NullableSignals
{
#pragma warning disable CS0109 // Disable warning about redundant 'new' keyword
    /// <summary>
    /// Cached StringNames for the signals contained in this class, for fast lookup.
    /// </summary>
    public new class SignalName : global::Godot.Node.SignalName {
        /// <summary>
        /// Cached name for the 'Tick' signal.
        /// </summary>
        public new static readonly global::Godot.StringName @Tick = "Tick";
    }
    protected new static partial class GodotInternal
    {
        /// <summary>
        /// Get the signal information for all the signals declared in this class.
        /// This method is used by Godot to register the available signals in the editor.
        /// Do not call this method.
        /// </summary>
        public static
#nullable enable
            global::System.Collections.Generic.List<global::Godot.Bridge.MethodInfo>?
#nullable restore
            GetGodotSignalList()
        {
            var signals = new global::System.Collections.Generic.List<global::Godot.Bridge.MethodInfo>(1);
        signals.Add(new(name: SignalName.@Tick, returnVal: new(type: (global::Godot.Variant.Type)0, name: "", hint: (global::Godot.PropertyHint)0, hintString: "", usage: (global::Godot.PropertyUsageFlags)6, exported: false), flags: (global::Godot.MethodFlags)1, arguments: new() { new(type: (global::Godot.Variant.Type)2, name: "value", hint: (global::Godot.PropertyHint)0, hintString: "", usage: (global::Godot.PropertyUsageFlags)6, exported: false),  }, defaultArguments: null));
            return signals;
        }
        private static unsafe void GetGodotRaiseSignalTrampolines(global::Godot.Bridge.RaiseSignalTrampolineCollector collector)
        {
            static void trampoline_1_Tick(object godotObject, NativeVariantPtrArgs args, ref godot_variant_call_error callError)
            {
                if (args.Count != 1) {
                    callError = godot_variant_call_error.CreateInvalidArgumentCountError(expected: 1, provided: args.Count);
                    return;
                }
                ((global::NullableSignals)godotObject).backing_Tick?.Invoke(global::Godot.NativeInterop.VariantUtils.ConvertTo<int>(args[0]));
            }
            var aux_delegate_1_Tick = trampoline_1_Tick;
            collector.TryAdd(new(SignalName.@Tick, 1), new(aux_delegate_1_Tick.Method.MethodHandle.GetFunctionPointer()));
        }
    }
#pragma warning restore CS0109
#nullable enable
    private global::NullableSignals.TickEventHandler? backing_Tick;
#nullable restore
    /// <inheritdoc cref="global::NullableSignals.TickEventHandler"/>
    public event global::NullableSignals.TickEventHandler @Tick {
        add => backing_Tick += value;
        remove => backing_Tick -= value;
    }
    protected void EmitSignalTick(int @value)
    {
        EmitSignal(SignalName.Tick, [@value]);
    }
}
