#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpusData.h"
#include "OpusNative.h"
#include "OpusMapItems.generated.h"

USTRUCT(BlueprintType)
struct FOpusMapAffix
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FString Id;
 UPROPERTY(BlueprintReadWrite) double Value=0;
 UPROPERTY(BlueprintReadWrite) double Min=0;
 UPROPERTY(BlueprintReadWrite) double Max=0;
};
USTRUCT(BlueprintType)
struct FOpusMapItem
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FString Uid;
 UPROPERTY(BlueprintReadWrite) FString BaseId;
 UPROPERTY(BlueprintReadWrite) FString Rarity;
 UPROPERTY(BlueprintReadWrite) int32 Tier=1;
 UPROPERTY(BlueprintReadWrite) TArray<FOpusMapAffix> Affixes;
};
USTRUCT(BlueprintType)
struct FOpusMapItemState
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) int32 Version=1;
 UPROPERTY(BlueprintReadWrite) int32 Seed=71;
 UPROPERTY(BlueprintReadWrite) int32 NextId=1;
 UPROPERTY(BlueprintReadWrite) TArray<FOpusMapItem> Items;
 UPROPERTY(BlueprintReadWrite) FOpusMapItem Active;
 // References reserve owned items; placing never consumes them.
 UPROPERTY(BlueprintReadWrite) TArray<FString> DeviceSlots;
 UPROPERTY(BlueprintReadWrite) FString AtlasState;
};
USTRUCT(BlueprintType)
struct FOpusMapQuality
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FString Id;
 UPROPERTY(BlueprintReadWrite) int32 Min=0;
 UPROPERTY(BlueprintReadWrite) int32 Max=0;
};

// Reflection, catalogue, persistence and presentation adapters. Generation and
// use policy live in AngelScript. Combat evaluates the existing DWRules sheet.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusMapItems : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> Catalog(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusMapAffix> AffixPool();
 UFUNCTION(BlueprintPure) static TArray<FOpusMapQuality> Qualities();
 UFUNCTION(BlueprintCallable) static bool DecodeProfile(const FString& Profile,FOpusMapItemState& State,FString& Error);
 UFUNCTION(BlueprintPure) static FString EncodeProfile(const FString& Profile,const FOpusMapItemState& State);
 UFUNCTION(BlueprintCallable) static void Publish(UDWRuleWorldSubsystem* Rules,const FOpusMapItemState& State);
 UFUNCTION(BlueprintCallable) static bool PurchaseTemplate(UDWRuleWorldSubsystem* Rules,const FString& Key,FString& Error);
 UFUNCTION(BlueprintCallable) static bool Evaluate(UDWRuleWorldSubsystem* Rules,FName Core,const FOpusMapItem& Item,FOpusMapData& Out,FString& Error);
 static TArray<FOpusEntry> Inventory(UDWRuleWorldSubsystem* Rules);
 static TArray<FOpusHoverLine> Lines(UDWRuleWorldSubsystem* Rules,const FString& Id);
};
