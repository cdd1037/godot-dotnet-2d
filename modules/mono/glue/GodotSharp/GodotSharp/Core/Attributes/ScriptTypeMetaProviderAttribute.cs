using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using Godot.Bridge;
using JetBrains.Annotations;

namespace Godot;

#nullable enable

/// <summary>
/// Interface for types that provide metadata for Godot script types.
/// </summary>
public interface IScriptTypeMetaProvider
{
    /// <summary>
    /// Gets the metadata for the Godot script type.
    /// </summary>
    /// <returns>The metadata for the Godot script type.</returns>
    public static abstract ScriptTypeMeta GetGodotClassScriptMeta();
}

/// <summary>
/// Base attribute for attributes that specify a type that provides metadata for a Godot script type.
/// </summary>
public abstract class ScriptTypeMetaProviderBaseAttribute : Attribute
{
    /// <summary>
    /// Gets the metadata for the Godot script type provided by the type specified in the attribute type parameter,
    /// using the provided script type if necessary (e.g. for generic script types with a nested provider generic type definition).
    /// </summary>
    /// <returns>
    /// The metadata for the Godot script type.
    /// </returns>
    /// <param name="scriptType">The script type for which to get the metadata. This parameter is
    /// provided for potential use in derived classes that need to use the script type to get the
    /// metadata, e.g. for generic script types with a nested provider generic type definition.</param>
    public abstract ScriptTypeMeta GetGodotClassScriptMeta(Type scriptType);
}

/// <summary>
/// Attribute used to specify a type that provides metadata for a Godot script type.
/// </summary>
/// <typeparam name="T">
/// The type that provides metadata for a Godot script type.
/// Must implement <see cref="IScriptTypeMetaProvider"/>.
/// </typeparam>
[AttributeUsage(AttributeTargets.Class, Inherited = false)]
[PublicAPI]
public sealed class ScriptTypeMetaProviderAttribute
    <[DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicMethods)] T>
    : ScriptTypeMetaProviderBaseAttribute
    where T : IScriptTypeMetaProvider
{
    /// <summary>
    /// Gets the metadata for the Godot script type provided by the type specified in the attribute type parameter,
    /// using the provided script type if necessary (e.g. for generic script types with a nested provider generic type definition).
    /// </summary>
    /// <returns>
    /// The metadata for the Godot script type.
    /// </returns>
    /// <param name="scriptType">The script type for which to get the metadata. This parameter is not used in this implementation,
    /// but it is provided for consistency with the base method and for potential use in derived classes.</param>
    public override ScriptTypeMeta GetGodotClassScriptMeta(Type scriptType)
    {
        return T.GetGodotClassScriptMeta();
    }
}

/// <summary>
/// Attribute used to specify a type that provides metadata for a generic Godot script type.
/// </summary>
[AttributeUsage(AttributeTargets.Class, Inherited = false)]
[PublicAPI]
public sealed class GenericScriptTypeMetaProviderAttribute : ScriptTypeMetaProviderBaseAttribute
{
    private readonly string _nestedProviderGenericTypeDefinitionName;

    /// <summary>
    /// Initializes a new instance of the <see cref="GenericScriptTypeMetaProviderAttribute"/> class with
    /// the specified generic type definition name of the nested provider type.
    /// </summary>
    /// <param name="nestedProviderGenericTypeDefinitionName">
    /// The assembly-qualified name (as expected by <see cref="System.Type.GetType(string)"/>) of the
    /// generic type definition of the provider type. This type must implement <see cref="IScriptTypeMetaProvider"/>
    /// to provides metadata for the Godot script type. The type must be nested within the script type
    /// that this attribute is applied to. Neither this nested provider type nor any possible containing
    /// types between it and the script type generic type definition should have any generic parameters.
    /// </param>
    /// <remarks>
    /// <para><paramref name="nestedProviderGenericTypeDefinitionName"/> must the assembly-qualified name
    /// (as expected by <see cref="System.Type.GetType(string)"/>) of the generic type definition of the
    /// provider type. This type must be nested within the script type that this attribute is applied to.
    /// Neither this nested provider type nor any possible containing types between it and the
    /// script type generic type definition should have any additional generic parameters.</para>
    /// <para>The type specified by <paramref name="nestedProviderGenericTypeDefinitionName"/>
    /// must implement the <see cref="IScriptTypeMetaProvider"/> interface.</para>
    /// </remarks>
    public GenericScriptTypeMetaProviderAttribute(
        string nestedProviderGenericTypeDefinitionName)
    {
        _nestedProviderGenericTypeDefinitionName = nestedProviderGenericTypeDefinitionName;
    }

    /// <summary>
    /// Gets the metadata for the Godot script type provided by the type specified in the attribute type parameter,
    /// using the provided script type if necessary (e.g. for generic script types with a nested provider generic type definition).
    /// </summary>
    /// <returns>
    /// The metadata for the Godot script type.
    /// </returns>
    /// <param name="scriptType">The script type for which to get the metadata. This parameter
    /// is used in this implementation to get the nested provider type from the script type,
    /// as the provided generic type definition is expected to be a nested type within the script
    /// type generic type definition, and to invoke the GetGodotClassScriptMeta method on it.</param>
    public override ScriptTypeMeta GetGodotClassScriptMeta(Type scriptType)
    {
        if (!scriptType.IsGenericType)
            throw new InvalidOperationException(
                $"The script type '{scriptType.FullName}' is expected to be a generic type in this context.");

        if (!scriptType.IsConstructedGenericType)
            throw new InvalidOperationException(
                $"The script type '{scriptType.FullName}' is expected to be a constructed generic type in this context.");

        return ScriptManagerBridge.ResolveJitGenericScriptMetadata(
            _nestedProviderGenericTypeDefinitionName, scriptType);
    }
}
