#nullable enable
using Godot;
public partial class NullableSignals : Node
{
    [Signal]
    public delegate void TickEventHandler(int value);
}
