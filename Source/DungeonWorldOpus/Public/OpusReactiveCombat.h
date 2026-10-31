#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OpusReactiveCombat.generated.h"
class UDWRuleWorldSubsystem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOpusLandedAttack,double,OriginalDamage,bool,Dot,bool,Dead);

// Transport only: AS owns capabilities, wave timing, amounts and hit deduplication.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusReactiveCombat : public UActorComponent
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintAssignable) FOpusLandedAttack OnLandedAttack;
 UFUNCTION(BlueprintCallable) bool BindPlayer(UDWRuleWorldSubsystem* InRules);
 UFUNCTION(BlueprintCallable) TArray<int32> ReflectedPulse(FVector Origin,double RadiusCm,double Amount,const TArray<int32>& Excluded);
private:
 UPROPERTY() TObjectPtr<UDWRuleWorldSubsystem> Rules;
 int32 ReflectionPlan=INDEX_NONE;
};
