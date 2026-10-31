#pragma once
#include "DWRuleProgram.h"

namespace DWRules
{
struct FDWRuleLibrary;
struct FDWTagFamily
{
	FName Id;
	FString Name, Description;
};
struct FDWTagDefinition
{
	FName Id, Family;
	FString Name;
};
struct FDWTagDocument
{
	TArray<FDWTagFamily> Families;
	TMap<FName, FDWTagDefinition> Tags;
	TMap<FName, TArray<FName>> TraitTags;
};
struct DWRULESCORE_API FDWTagIndex
{
	FDWTagDocument Document;
	TMap<FName, FDWTagDefinition> ByName;
	TMap<FName, TArray<FName>> TraitTags, TraitFamilies, Unmapped;
	bool Build(const FDWRuleLibrary &Library, const FDWTagDocument &Doc, FString &Error);
	const FDWTagDefinition *Tag(const FString &Name) const;
	TArray<FName> FamiliesOf(FName Trait) const;
	TArray<FName> CollectionFamilies(const TArray<FString> &Tags, TArray<FString> *Unknown = nullptr) const;
	FString FamilyName(FName Id) const;
	static TArray<FName> DeriveTraitTags(const FDWTraitSource &Trait);
	static double RollValue(FName Name, double Base, double Random);
};
} // namespace DWRules
