#pragma once
#include "CoreMinimal.h"
#include "OpusData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpusStudioBridge.generated.h"

// Data and transaction adapters only. Studio acquisition policy is in AngelScript.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusStudioBridge : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static int64 UnixSeconds();
 UFUNCTION(BlueprintPure) static FString UniqueId();
 UFUNCTION(BlueprintPure) static TArray<FOpusEntry> FixedTemplates(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintCallable) static bool Equipment(UDWRuleWorldSubsystem* Rules,FName Base,FName Rarity,int32 Tier,FName Affix,int32 Seed,const FString& Uid,const FString& Name,FString& Payload,FString& Error);
 UFUNCTION(BlueprintPure) static int64 Price(UDWRuleWorldSubsystem* Rules,FName Base,const FString& Payload);
 UFUNCTION(BlueprintCallable) static bool Purchase(UDWRuleWorldSubsystem* Rules,const FString& Payload,int64 Price,FString& Error);
};
