#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OpusCharacterEvents.generated.h"
class UDWRuleWorldSubsystem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOpusResolvedPlayerHit,double,Amount,bool,Dot,bool,Dead);

// Native event/stat adapter only. Equipment policy and timers live in AS.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusCharacterEvents : public UActorComponent
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintAssignable) FOpusResolvedPlayerHit OnResolvedHit;
 UFUNCTION(BlueprintCallable) bool BindPlayer(UDWRuleWorldSubsystem* InRules);
 UFUNCTION(BlueprintPure) double SheetStat(FName Path) const;
 UFUNCTION(BlueprintPure) double SimulationClock() const;
 UFUNCTION(BlueprintCallable) void UseExternalStun(bool Enabled);
 // Existing authoritative damage path, available only to transient diagnostics.
 UFUNCTION(BlueprintCallable) bool TestHit(double Amount,bool Dot=false);
protected:
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UDWRuleWorldSubsystem> Rules;
 bool Bound=false,ExternalStun=false;
 double SingleHitFraction=0;
};
