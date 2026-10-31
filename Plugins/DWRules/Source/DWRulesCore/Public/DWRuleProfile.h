#pragma once
#include "DWRuleLoadout.h"

namespace DWRules
{
// Presentation metadata only. Ref points at existing ownership; a split never grants an item.
struct FDWInventoryStackPlacement
{
 FString Key, Ref;
 int32 Count = 0;
 bool operator==(const FDWInventoryStackPlacement& Other) const { return Key==Other.Key&&Ref==Other.Ref&&Count==Other.Count; }
};
struct FDWInventoryLayout
{
 TArray<FDWInventoryStackPlacement> Slots;
 int32 NextKey = 1;
 bool operator==(const FDWInventoryLayout& Other) const { return NextKey==Other.NextKey&&Slots==Other.Slots; }
};
// Persistent acquisition state. Combat RNG and inventory RNG are deliberately independent.
struct DWRULESCORE_API FDWRuleProfile
{
	FDWRuleLoadout Loadout;
	TMap<FName, int32> OwnedEquipment;
	TArray<FName> Belt;
 FDWInventoryLayout BagLayout;
	int64 Stones = 0, Earned = 0, Spent = 0, Kills = 0, Battles = 0, Drops = 0;
	bool bInitialized = false, bProgression = false;
	FName StarterWeapon = TEXT("frostthorn_staff");
	FDWEquipmentRandom Random{71};
	void Reset(const FDWRuleLibrary &Library);
	bool StartProgression(const FDWRuleLibrary &Library, FString &Error);
	bool ApplyPreset(const FDWRuleLibrary &Library, const FDWRuleProfile &Preset, FString &Error);
	bool Buy(const FDWRuleLibrary &Library, const FString &Key, FString &Error, TOptional<int64> PriceOverride = {});
	bool Equip(const FDWRuleLibrary &Library, FName Slot, FName Id, FString &Error);
	bool Identify(const FDWRuleLibrary &Library, FName Id, FString &Uid, FString &Error);
	bool Craft(const FDWRuleLibrary &Library, const FString &Uid, FName Currency, FName Omen, int32 Index,
			   FString &Error);
	bool Consume(FName Id);
	void RecordKill(int64 Reward);
	bool AwardKill(const FDWRuleLibrary &Library, double MaxHealth, FString &Error, bool bDropEquipment = true);
	bool SetBeltSlot(const FDWRuleLibrary& Library, int32 Slot, FName Id, FString& Error);
	bool QuoteSale(const FDWRuleLibrary& Library, const FString& Id, int64& Price, FString& Error) const;
	bool Sell(const FDWRuleLibrary& Library, const FString& Id, int64& Price, FString& Error);
	void Discard(const FString &Uid);
};
} // namespace DWRules
