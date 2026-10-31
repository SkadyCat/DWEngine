#pragma once
#include "DWRuleProgram.h"

namespace DWRules
{
enum class ECollectionKind : uint8
{
	Equipment,
	Item
};
struct FDWCollection
{
	FName Id;
	FString Name;
	FString Description;
	FName Slot;
	FName Category;
	FName Rarity;
	int32 Level = 1;
	int32 Tier = 1;
	int32 Stack = 1;
	TArray<FString> Tags;
	TArray<FName> Skills;
	TArray<FDWTraitEntry> Traits;
	// Ordinary templates author one base and roll a second; legendary templates author the complete fixed list.
	TArray<FDWTraitEntry> Bases;
	TOptional<FDWTraitEntry> Legendary;
	FName CraftOp = TEXT("reroll");
	int32 CraftCount = 0;
	int32 CraftGrade = 1;
};
struct FDWCollectionPart
{
	FDWCollection Collection;
	bool bEquipment = false;
	FString Label;
};

// Definition ids with per-pawn document overrides; overrides never mutate the shared catalogue.
struct FDWPawnEquipment
{
	TMap<FName, FName> Ids;
	TMap<FName, FDWCollection> Overrides;
};
} // namespace DWRules
