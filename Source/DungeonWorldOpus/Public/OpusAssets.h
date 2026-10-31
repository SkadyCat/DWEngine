#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/StreamableManager.h"
#include "OpusAssets.generated.h"

// Resource lifetime only. Encounter planning and frame scheduling live in AS.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusAssetCache : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 UPROPERTY() TMap<FString,TObjectPtr<UObject>> Objects;
 UPROPERTY() TObjectPtr<UClass> HUD;
 TArray<FString> Queue;
 TMap<FString,TSharedPtr<FStreamableHandle>> Loading;
 TSet<FString> Failed;
 FStreamableManager Streamer;
 uint64 LastPump=MAX_uint64;
 virtual void Deinitialize() override;
};

UCLASS()
class DUNGEONWORLDOPUS_API UOpusAssets : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) static void Request(UObject* Context,const TArray<FString>& Paths);
 UFUNCTION(BlueprintCallable) static void Pump(UObject* Context);
 UFUNCTION(BlueprintPure) static bool Ready(UObject* Context,const TArray<FString>& Paths);
 UFUNCTION(BlueprintPure) static FString Error(UObject* Context,const TArray<FString>& Paths);
 UFUNCTION(BlueprintPure) static UObject* Find(UObject* Context,const FString& Path);
 static UClass* HUDClass(UObject* Context);
};
