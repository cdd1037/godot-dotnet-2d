using Godot;

public partial class RegressionDynamic : Node
{
    private int _stored;
    public int SetCalls;
    public int GetCalls;
    public int ReadOnlyValue => _stored;
    public int WriteOnlyValue { set => _stored = value; }

    public override bool _Set(StringName property, Variant value)
    {
        SetCalls++;
        if (property == nameof(ReadOnlyValue))
        {
            _stored = value.AsInt32();
            return true;
        }
        return false;
    }

    public override Variant _Get(StringName property)
    {
        GetCalls++;
        if (property == nameof(WriteOnlyValue)) return _stored;
        return default;
    }
}
