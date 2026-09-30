using System;

namespace Godot;

/// <summary>
/// Declares a build-time-known script type whose metadata must be registered by
/// this assembly's generated startup code. Use a closed constructed type for
/// generic scripts. This does not associate a script file path with the type.
/// </summary>
[AttributeUsage(AttributeTargets.Assembly, AllowMultiple = true)]
public sealed class RegisterScriptTypeAttribute : Attribute
{
    /// <summary>The exact script type to register, including closed generic arguments.</summary>
    public Type ScriptType { get; }

    /// <summary>Declares a script type that is known when this assembly is compiled.</summary>
    public RegisterScriptTypeAttribute(Type scriptType) => ScriptType = scriptType;
}
