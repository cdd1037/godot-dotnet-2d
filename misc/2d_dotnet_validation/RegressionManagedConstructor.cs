using Godot;
using System.IO;

// A valid managed-only constructor need not have a Variant-marshallable signature.
public partial class RegressionManagedConstructor : RefCounted
{
    public Stream ManagedDependency { get; }
    public RegressionManagedConstructor(Stream dependency) => ManagedDependency = dependency;
}
