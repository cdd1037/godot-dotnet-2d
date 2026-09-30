using System.Linq;
using Microsoft.CodeAnalysis;

namespace Godot.SourceGenerators;

internal record struct ContainingTypeModel(
    string DeclarationKeyword,
    string DisplayStringMinimallyQualifiedFormat
)
{
    internal static bool SequenceEquals(ContainingTypeModel[]? left, ContainingTypeModel[]? right)
        => ReferenceEquals(left, right) || (left != null && right != null && left.SequenceEqual(right));

    internal static int SequenceHashCode(ContainingTypeModel[]? items)
    {
        int hash = 0;
        if (items != null)
            foreach (var item in items)
                hash = unchecked(hash * 31 + item.GetHashCode());
        return hash;
    }

    public static ContainingTypeModel[]? GetContainingTypesFor(INamedTypeSymbol symbol)
    {
        if (symbol.ContainingType == null)
            return null;

        int containingTypeCount = 0;
        INamedTypeSymbol? containingType = symbol.ContainingType;

        while (containingType != null)
        {
            containingTypeCount++;
            containingType = containingType.ContainingType;
        }

        var containingTypeModels = new ContainingTypeModel[containingTypeCount];

        containingType = symbol.ContainingType;

        while (containingType != null)
        {
            containingTypeModels[--containingTypeCount] = new ContainingTypeModel(
                containingType.GetDeclarationKeyword(),
                containingType.ToDisplayString(SymbolDisplayFormat.MinimallyQualifiedFormat)
            );
            containingType = containingType.ContainingType;
        }

        return containingTypeModels;
    }
}
