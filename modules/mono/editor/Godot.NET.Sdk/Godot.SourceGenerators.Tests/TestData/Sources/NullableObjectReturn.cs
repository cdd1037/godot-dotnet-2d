#nullable enable
using Godot;
public partial class NullableObjectReturn : Node
{
    public Texture2D? GetOptionalTexture() => null;
}
