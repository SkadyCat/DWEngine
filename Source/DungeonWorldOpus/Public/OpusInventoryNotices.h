#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OpusData.h"
#include "OpusInventoryNotices.generated.h"

// HUD ownership notifications. Rule traits, equipment stats and combat are untouched.
UCLASS(BlueprintType)
class DUNGEONWORLDOPUS_API UOpusInventoryNotices : public UObject
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) static UOpusInventoryNotices* Create(bool Persistent);
 UFUNCTION(BlueprintCallable) void Observe(const TArray<FOpusEntry>& Entries);
 UFUNCTION(BlueprintPure) bool IsNew(const FString& Id) const {return Unseen.Contains(Id);}
 UFUNCTION(BlueprintCallable) bool MarkSeen(const FString& Id);
private:
 TMap<FString,int32> Counts;
 TSet<FString> Unseen;
 bool Initialized=false,SaveEnabled=false;
 void Save() const;
};
