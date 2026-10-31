#pragma once
#include "CoreMinimal.h"
#include "OpusData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpusGameplayState.generated.h"
USTRUCT(BlueprintType)
struct FOpusStoredStack
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FString Key;
 UPROPERTY(BlueprintReadWrite) FString Ref;
 UPROPERTY(BlueprintReadWrite) FString Payload;
 UPROPERTY(BlueprintReadWrite) int32 Count=0;
 UPROPERTY(BlueprintReadWrite) int32 Cell=0;
};
USTRUCT(BlueprintType)
struct FOpusVendorOffer
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FString Key;
 UPROPERTY(BlueprintReadWrite) FString Payload;
 UPROPERTY(BlueprintReadWrite) int64 Price=0;
 UPROPERTY(BlueprintReadWrite) bool Claimed=false;
};
USTRUCT(BlueprintType)
struct FOpusGameplayState
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) int32 Version=1;
 UPROPERTY(BlueprintReadWrite) TArray<FName> AuraSlots;
 UPROPERTY(BlueprintReadWrite) TArray<bool> AuraEnabled;
 UPROPERTY(BlueprintReadWrite) bool FirstFloorComplete=false;
 UPROPERTY(BlueprintReadWrite) bool QuestTrackerCollapsed=false;
 UPROPERTY(BlueprintReadWrite) bool RewardClaimed=false;
 UPROPERTY(BlueprintReadWrite) int32 ClaimedChoice=-1;
 UPROPERTY(BlueprintReadWrite) TArray<FString> RewardCandidates;
 UPROPERTY(BlueprintReadWrite) TArray<FOpusStoredStack> Stash;
 UPROPERTY(BlueprintReadWrite) TArray<FOpusStoredStack> Goods;
 UPROPERTY(BlueprintReadWrite) int32 MerchantWorld=0;
 UPROPERTY(BlueprintReadWrite) int64 MerchantPeriod=-1;
 UPROPERTY(BlueprintReadWrite) TArray<FOpusVendorOffer> MerchantStock;
};
// Serialization and existing inventory transactions; AS owns reward policy and interaction.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusGameplayStore : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 // Placement metadata only; ownership and item transactions remain in DWRules.
 UFUNCTION(BlueprintPure) static TArray<FOpusStoredStack> InventoryCells(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static int32 InventoryNextKey(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintCallable) static bool SetInventoryCells(UDWRuleWorldSubsystem* Rules,const TArray<FOpusStoredStack>& Cells,int32 NextKey,FString& Error);
 UFUNCTION(BlueprintCallable) static bool Decode(const FString& Profile,FOpusGameplayState& State,FString& Error);
 UFUNCTION(BlueprintCallable) static FString Encode(const FString& Profile,const FOpusGameplayState& State);
 UFUNCTION(BlueprintCallable) static bool RollEquipment(UDWRuleWorldSubsystem* Rules,FName Slot,int32 Tier,int32 Count,int32 Seed,TArray<FString>& Candidates,FString& Error);
 UFUNCTION(BlueprintPure) static FOpusEntry Entry(UDWRuleWorldSubsystem* Rules,const FString& Payload);
 UFUNCTION(BlueprintPure) static FString SkillIcon(FName Id);
 UFUNCTION(BlueprintPure) static TArray<FOpusHoverLine> Lines(UDWRuleWorldSubsystem* Rules,const FString& Payload);
 UFUNCTION(BlueprintCallable) static bool Grant(UDWRuleWorldSubsystem* Rules,const FString& Payload,FString& Error);
 UFUNCTION(BlueprintCallable) static bool Store(UDWRuleWorldSubsystem* Rules,const FString& Key,int32 Count,FOpusStoredStack& Stack,FString& Error);
 UFUNCTION(BlueprintCallable) static bool Restore(UDWRuleWorldSubsystem* Rules,const FOpusStoredStack& Stack,FString& Error);
};
