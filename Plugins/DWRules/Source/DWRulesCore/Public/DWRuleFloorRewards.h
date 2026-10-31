#pragma once
#include "DWRuleProfile.h"

namespace DWRules
{
// Port of RuleStudio floor-rewards.mjs. The acquisition streams never consume combat RNG.
struct DWRULESCORE_API FDWFloorLoot
{
 FString Key, Name, Description;
 FName Kind, Id, Rarity, Category;
 FDWEquipmentInstance Item;
 int32 Count = 1, Tier = 0;
 int64 Price = 0;
 FVector2d Position = FVector2d::ZeroVector;
 // Presentation metadata only; never read by reward rolls or acquisition.
 FVector2d SourcePosition = FVector2d::ZeroVector;
 FName Origin = TEXT("drop");
 bool bClaimed = false, bMerchant = false;
};
struct DWRULESCORE_API FDWRuleFloorRewards
{
 explicit FDWRuleFloorRewards(uint32 InSeed = 1, int32 InFloor = 1, int32 Visit = 1, int32 InMaxTier = 1);
 static constexpr double MerchantChance = .35;
 static constexpr double PickupRadiusCm = 200;
 int32 Floor = 1, MaxTier = 1;
 uint32 Seed = 1;
 TArray<FDWFloorLoot> Ground, Stock;
 bool bCompleted = false, bHasMerchant = false;
 double EquipmentChance = .3, ItemChance = .3, MerchantProbability = MerchantChance;
 void OnKill(const FDWRuleLibrary& Library, FVector2d PositionCm, bool bBoss);
 void Complete(const FDWRuleLibrary& Library);
 bool Pickup(const FString& Key, FDWRuleProfile& Profile, FVector2d HeroCm, bool bDead, FString& Error);
 bool DropInventory(const FDWRuleLibrary& Library,FDWRuleProfile& Profile,FName Kind,const FString& Id,int32 Count,FVector2d HeroCm,bool bDead,FString& Key,FString& Error);
 bool Buy(const FString& Key, FDWRuleProfile& Profile, FString& Error);
 int32 Remaining() const;
 int64 KillStones(double MaxHealth,int32 TowerLevel=1);
 private:
 int32 NextId = 1;
 FDWEquipmentRandom Random, MerchantRandom, StonesRandom;
 FString Uid(const TCHAR* Prefix);
 static bool Grant(FDWRuleProfile& Profile, FDWFloorLoot& Loot, FString& Error);
};
}
