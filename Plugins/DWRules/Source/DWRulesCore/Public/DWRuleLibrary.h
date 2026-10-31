#pragma once

#include "DWRuleSheet.h"
#include "DWRuleCollection.h"
#include "DWRuleTags.h"
#include "DWRuleTraitValues.h"
#include "DWRuleMonsters.h"
#include "DWRuleShop.h"
#include "DWRuleWorld.h"

namespace DWRules
{
struct FDWLibraryTrait
{
	FDWTraitSource Source;
	FString DisplayName;
	FString Description;
	FString Path;
	// Authoring category: these traits are supplied through equipment/items, not base skill/status lists.
	bool bHosted = false;
};
struct FDWBuildDefinition
{
	FName Id;
	FString DisplayName;
	FName ActiveId;
	FString Description;
	TArray<FName> Traits;
};
struct FDWContribution
{
	FDWHeldTrait Held;
	FName OriginalId;
	bool bMerged = false;
	bool bExpired = false;
	double Duration = 0.0;
	int32 Tier = 0;
};
// Validated immutable definitions, independent of JSON, engine objects and live world state.
struct DWRULESCORE_API FDWRuleLibrary
{
	TMap<FName, FDWLibraryTrait> Traits;
	TMap<FName, FDWBuildDefinition> Builds;
	TArray<FDWStatusDef> Statuses;
	TMap<FName, FDWCollection> Equipment;
	TMap<FName, FDWCollection> Items;
	FDWTagDocument TagDefinitions;
	FDWTagIndex Tags;
	TMap<FName, FDWTraitValueProfile> ValueProfiles;
	TMap<FName, FDWMonsterDefinition> Monsters;
	FDWShopCatalog Shop;
	bool Validate(FString &Error) const;
	bool SourceFromEntry(const FDWTraitEntry &Entry, FDWTraitSource &Out, FString &Error, FName Rename = NAME_None,
						 int32 Depth = 0) const;
	bool NamedBuildSource(FName Build, const TArray<FName> &Extras, FDWBuildSource &Out, FString &Error) const;
	bool TraitsSource(FName Active, const TArray<FName> &Ids, FDWBuildSource &Out, FString &Error) const;
	bool Aggregate(FName Build, const TArray<FDWCollectionPart> &Parts, double At, FDWBuildSource &Out,
				   TArray<FDWContribution> &Contributions, FString &Error) const;
};
} // namespace DWRules
