#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpusData.h"
#include "OpusMapEvents.generated.h"

USTRUCT(BlueprintType)
struct FOpusMapEventDefinition
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Id;
 UPROPERTY(BlueprintReadOnly) FString Kind;
 UPROPERTY(BlueprintReadOnly) FString Name;
 UPROPERTY(BlueprintReadOnly) FString Description;
 UPROPERTY(BlueprintReadOnly) int32 Weight=1;
 UPROPERTY(BlueprintReadOnly) int32 MinFloor=1;
 UPROPERTY(BlueprintReadOnly) int32 Guards=0;
 UPROPERTY(BlueprintReadOnly) int32 CandidateCount=3;
 UPROPERTY(BlueprintReadOnly) int32 ChooseCount=1;
 UPROPERTY(BlueprintReadOnly) FName Rarity;
 UPROPERTY(BlueprintReadOnly) TArray<FName> Currencies;
};
USTRUCT(BlueprintType)
struct FOpusMapEventConfig
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 ChancePercent=75;
 UPROPERTY(BlueprintReadOnly) int32 MinEvents=1;
 UPROPERTY(BlueprintReadOnly) int32 MaxEvents=2;
 UPROPERTY(BlueprintReadOnly) TArray<FOpusMapEventDefinition> Definitions;
};
UCLASS()
class DUNGEONWORLDOPUS_API UOpusEventOffer : public UObject
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintReadOnly) bool Claimed=false;
 UPROPERTY(BlueprintReadOnly) int32 ChooseCount=1;
 UFUNCTION(BlueprintCallable) bool Generate(UDWRuleWorldSubsystem* Rules,const TArray<FName>& Allowed,int32 Seed,int32 Tier,FName Rarity,int32 Count,int32 Choose,FString& Error);
 UFUNCTION(BlueprintCallable) bool PrepareFusion(UDWRuleWorldSubsystem* Rules,const FString& Left,const FString& Right,const FString& Token,const TArray<int32>& LeftIndices,const TArray<int32>& RightIndices,FString& Error);
 UFUNCTION(BlueprintPure) TArray<FOpusEntry> Entries() const;
 UFUNCTION(BlueprintPure) TArray<FOpusHoverLine> Lines(int32 Index) const;
 UFUNCTION(BlueprintCallable) bool Claim(const TArray<int32>& Indices,FString& Error);
private:
 UPROPERTY() TObjectPtr<UDWRuleWorldSubsystem> OwnerRules;
 TArray<DWRules::FDWEquipmentInstance> Items;
 TArray<FString> Consumed;
 FString LockToken;
};
UCLASS()
class DUNGEONWORLDOPUS_API UOpusMapEvents : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) static bool ReadConfig(const FString& Json,FOpusMapEventConfig& Config,FString& Error);
 UFUNCTION(BlueprintPure) static FString LocalConfig();
 UFUNCTION(BlueprintPure) static TArray<FName> EquipmentCandidates(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static int32 FusionLineCount(UDWRuleWorldSubsystem* Rules,const FString& Uid);
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Equipment(UDWRuleWorldSubsystem* Rules);
};
