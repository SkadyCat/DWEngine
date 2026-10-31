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
struct FOpusGameplayState
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) int32 Version=1;
 UPROPERTY(BlueprintReadWrite) TArray<FName> AuraSlots;
 UPROPERTY(BlueprintReadWrite) TArray<bool> AuraEnabled;
 UPROPERTY(BlueprintReadWrite) bool FirstFloorComplete=false;
 UPROPERTY(BlueprintReadWrite) bool RewardClaimed=false;
 UPROPERTY(BlueprintReadWrite) int32 ClaimedChoice=-1;
 UPROPERTY(BlueprintReadWrite) TArray<FString> RewardCandidates;
 UPROPERTY(BlueprintReadWrite) TArray<FOpusStoredStack> Stash;
 UPROPERTY(BlueprintReadWrite) TArray<FOpusStoredStack> Goods;
};
// Serialization and existing inventory transactions; AS owns reward policy and interaction.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusGameplayStore : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
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
