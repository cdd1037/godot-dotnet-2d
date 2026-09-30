using Godot;
using System;

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
