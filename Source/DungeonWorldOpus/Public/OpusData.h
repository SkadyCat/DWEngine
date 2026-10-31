#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DWRuleWorldSubsystem.h"
#include "OpusData.generated.h"
USTRUCT(BlueprintType)
struct FOpusEntry
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Id;
 UPROPERTY(BlueprintReadOnly) FString Key;
 UPROPERTY(BlueprintReadOnly) FString Kind;
 UPROPERTY(BlueprintReadOnly) FString Name;
 UPROPERTY(BlueprintReadOnly) FString Description;
 UPROPERTY(BlueprintReadOnly) FString Slot;
 UPROPERTY(BlueprintReadOnly) FString Category;
 UPROPERTY(BlueprintReadOnly) FString Rarity;
 UPROPERTY(BlueprintReadOnly) FString Icon;
 UPROPERTY(BlueprintReadOnly) FString Origin;
 UPROPERTY(BlueprintReadOnly) FVector Position=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector SourcePosition=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) int32 CurrencyGrade=0;
 UPROPERTY(BlueprintReadOnly) FString CraftOp;
 UPROPERTY(BlueprintReadOnly) int32 CraftCount=0;
 UPROPERTY(BlueprintReadOnly) int32 Count=0;
 UPROPERTY(BlueprintReadOnly) int32 Tier=0;
 UPROPERTY(BlueprintReadOnly) int64 Price=0;
 UPROPERTY(BlueprintReadOnly) bool Identified=true;
 UPROPERTY(BlueprintReadOnly) bool Equipped=false;
 UPROPERTY(BlueprintReadOnly) bool Locked=false;
 UPROPERTY(BlueprintReadOnly) bool Synthesized=false;
 UPROPERTY(BlueprintReadOnly) int32 FixedAffixCount=0;
 UPROPERTY(BlueprintReadOnly) TArray<FString> AffixIds;
};
USTRUCT(BlueprintType)
struct FOpusHoverLine
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Text;
 UPROPERTY(BlueprintReadOnly) int32 Tier=0;
 UPROPERTY(BlueprintReadOnly) int32 Style=0;
 UPROPERTY(BlueprintReadOnly) int32 Index=-1;
 UPROPERTY(BlueprintReadOnly) int32 Grade=0;
 UPROPERTY(BlueprintReadOnly) FString Description;
};
USTRUCT(BlueprintType)
struct FOpusCraftState
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool Instance=false;
 UPROPERTY(BlueprintReadOnly) int32 BoundCount=0;
 UPROPERTY(BlueprintReadOnly) int32 PoolCount=0;
 UPROPERTY(BlueprintReadOnly) int32 Count=0;
 UPROPERTY(BlueprintReadOnly) FString Operation;
 UPROPERTY(BlueprintReadOnly) FString Blocked;
 UPROPERTY(BlueprintReadOnly) TArray<int32> EligibleAffixes;
};
// Read-only reflection of existing engine data. No inventory, RNG or combat ownership.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusData : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static TArray<FOpusHoverLine> HoverLines(UDWRuleWorldSubsystem* Rules,const FString& Id);
 UFUNCTION(BlueprintPure) static FOpusCraftState CraftState(UDWRuleWorldSubsystem* Rules,const FString& Id,FName Currency,FName Omen,bool VirtualCurrency=false);
 static TArray<FOpusHoverLine> InstanceLines(const DWRules::FDWEquipmentInstance& Item,const DWRules::FDWRuleLibrary& Library);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Inventory(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Skills(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Listings(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Ground(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Merchant(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Presets();
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> EquipmentAffixes(UDWRuleWorldSubsystem* Rules,const FString& Id);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> AffixPool(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Character(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> CharacterEffects(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Summons(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> MapEquipment(UDWRuleWorldSubsystem* Rules,FName Core=NAME_None);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> MapCores(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static FString SkillIcon(UDWRuleWorldSubsystem* Rules,int32 Slot);
 UFUNCTION(BlueprintPure) static FString SkillName(UDWRuleWorldSubsystem* Rules,int32 Slot);
 UFUNCTION(BlueprintPure) static double Dps(UDWRuleWorldSubsystem* Rules);
};
