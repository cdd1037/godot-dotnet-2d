using Godot;
using System;
using System.Runtime.CompilerServices;

[assembly: RegisterScriptType(typeof(SmokeRef))]
[assembly: RegisterScriptType(typeof(SmokeGeneric<int>))]

public partial class Main : Node2D
{
    [Signal]
    public delegate void AnswerEventHandler(int value);

    public override void _Ready()
    {
        try
        {
            Position = new Vector2(12, 34);
            if (Position != new Vector2(12, 34)) throw new Exception("Node2D binding failed");
            var control = new Control();
            AddChild(control);
            control.Size = new Vector2(32, 16);
            if (control.Size.X != 32) throw new Exception("Control binding failed");
            var world = GetWorld2D();
            if (!world.Space.IsValid) throw new Exception("World2D physics space unavailable");
            _ = world.DirectSpaceState;
            var tiles = new TileMapLayer();
            AddChild(tiles);
            tiles.NavigationEnabled = false;
            var shape = new RectangleShape2D { Size = new Vector2(4, 8) };
            if (shape.Size.Y != 8) throw new Exception("2D shape binding failed");
            shape.Dispose();
            int answer = 0;
            Answer += value => answer = value;
            EmitSignal(SignalName.Answer, 42);
            if (answer != 42) throw new Exception("Generated signal dispatch failed");
            if (ClassDB.ClassExists("Node3D")) throw new Exception("Unexpected Node3D API");
            if (Array.IndexOf(OS.GetCmdlineUserArgs(), "--minimal-template") >= 0)
            {
                if (ClassDB.ClassExists("FastNoiseLite") || ClassDB.ClassExists("RegEx"))
                    throw new Exception("Unexpected optional module in minimal template");
            }
            using var uniform = new RDUniform();
            using var script = new SmokeRef();
            using var generic = new SmokeGeneric<int>();
            var nativeValues = new Godot.Collections.Array<RDUniform> { uniform };
            using var nativeValuesOwner = (Godot.Collections.Array)nativeValues;
            var scriptValues = new Godot.Collections.Dictionary<SmokeRef, SmokeGeneric<int>> { [script] = generic };
            using var scriptValuesOwner = (Godot.Collections.Dictionary)scriptValues;
            if (nativeValues[0].GetInstanceId() != uniform.GetInstanceId() ||
                scriptValues[script].GetInstanceId() != generic.GetInstanceId() ||
                generic.Call("Marker").AsInt32() != 31)
                throw new Exception("Typed native/script collections or closed generic metadata failed");
            bool expectAot = Array.IndexOf(OS.GetCmdlineUserArgs(), "--expect-aot") >= 0;
            if (expectAot && RuntimeFeature.IsDynamicCodeSupported)
                throw new Exception("NativeAOT export unexpectedly contains a JIT runtime");
            GD.Print($"CI_DOTNET_RUNTIME dynamic_code={RuntimeFeature.IsDynamicCodeSupported}");
            GD.Print("CI_DOTNET_SMOKE_OK");
            GetTree().Quit(0);
        }
        catch (Exception exception)
        {
            GD.PushError(exception.ToString());
            GetTree().Quit(1);
        }
    }
}


[NoScriptFileAssociation]
public partial class SmokeRef : RefCounted
{
}

[NoScriptFileAssociation]
public partial class SmokeGeneric<T> : RefCounted
{
    public int Marker() => 31;
}
