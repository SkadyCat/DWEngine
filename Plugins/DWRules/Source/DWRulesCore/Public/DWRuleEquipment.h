#pragma once
#include "DWRuleLibrary.h"

namespace DWRules
{
struct FDWEquipmentAffix
{
	FName Id;
	FString Name;
	TArray<FDWTraitEntry> Traits;
	int32 Tier = 0;
};
struct FDWEquipmentInstance
{
	FString Uid;
	FName BaseId, Slot, Rarity, Skill;
	FString Name;
	bool bIdentified = true;
	int32 Level = 1;
	int32 Tier = TraitTierCount;
	TArray<FDWEquipmentAffix> Affixes;
	TArray<FDWEquipmentAffix> Bases;
	TOptional<FDWEquipmentAffix> Legendary;
	// Fusion provenance/data only. Existing trait evaluation remains unchanged.
	bool bSynthesized = false;
	TArray<FDWEquipmentAffix> AdditionalLegendary;
	// Ordered random-line indices bound to grade 1, 2 and 3 reroll currencies.
	TArray<int32> ReforgeTargets;
};
struct FDWEquipmentRandom
{
	explicit FDWEquipmentRandom(uint32 Seed = 1) : State(Seed ? Seed : 1) {}
	uint32 State;
	double Next()
	{
		State = State * 1664525u + 1013904223u;
		return double(State) / 4294967296.;
	}
};
class DWRULESCORE_API FDWEquipmentRules
{
  public:
	static TArray<FDWEquipmentAffix> Pool(const FDWRuleLibrary &Library);
	static TArray<FDWEquipmentAffix> BasePool(const FDWRuleLibrary &Library, bool Defensive);
	static bool RollBases(const FDWRuleLibrary &Library, const FDWCollection &Template, int32 Tier,
		TFunctionRef<double()> Random, TArray<FDWEquipmentAffix>& Out, FString& Error);
	static bool EnsureBases(const FDWRuleLibrary &Library, FDWEquipmentInstance& Item, FString& Error);
	static bool Conflict(const FDWEquipmentAffix &A, const FDWEquipmentAffix &B);
	static bool RollAffix(const FDWRuleLibrary &Library, const FDWEquipmentAffix &Definition,
						  TFunctionRef<double()> Random, FDWEquipmentAffix &Out, FString &Error,
						  int32 MaxTier = TraitTierCount);
	static bool Sample(const FDWRuleLibrary &Library, const TArray<FDWEquipmentAffix> &Pool, int32 Count,
					   TFunctionRef<double()> Random, const TArray<FDWEquipmentAffix> &Excluded,
					   TArray<FDWEquipmentAffix> &Out, FString &Error, int32 MaxTier = TraitTierCount);
	static bool Roll(const FDWRuleLibrary &Library, const FDWCollection &Template, FName Rarity,
					 TFunctionRef<double()> Random, const FString &Uid, FDWEquipmentInstance &Out, FString &Error,
					 int32 Tier = 0);
	static bool Reroll(const FDWRuleLibrary &Library, const FDWEquipmentInstance &Item, TFunctionRef<double()> Random,
					   FDWEquipmentInstance &Out, FString &Error, int32 Index = INDEX_NONE, FName Family = NAME_None);
	static bool AddAffixes(const FDWRuleLibrary &Library, const FDWEquipmentInstance &Item, int32 Count,
						   TFunctionRef<double()> Random, FDWEquipmentInstance &Out, FString &Error,
						   FName Family = NAME_None);
	static bool RemoveAffixes(const FDWEquipmentInstance& Item, int32 Count, TFunctionRef<double()> Random,
		FDWEquipmentInstance& Out, FString& Error);
	static bool RollAffixes(const FDWRuleLibrary& Library, const FDWEquipmentInstance& Item, int32 Count,
		TFunctionRef<double()> Random, FDWEquipmentInstance& Out, FString& Error, FName Family = NAME_None);
	static FDWCollection Document(const FDWEquipmentInstance &Item, const FDWRuleLibrary *Library = nullptr);
	static FName RandomRarity(TFunctionRef<double()> Random);
	static bool Drop(const FDWRuleLibrary &Library, TFunctionRef<double()> Random, double Chance, const FString &Uid,
					 FDWEquipmentInstance &Out, FString &Error);
	static FName OmenFamily(const FDWRuleLibrary &Library, const FDWCollection &Item);
};
} // namespace DWRules
