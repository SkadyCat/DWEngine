#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OpusDeliveryPolicy.generated.h"
class UDWRuleWorldSubsystem;
DECLARE_DYNAMIC_DELEGATE_RetVal(bool,FOpusDamagePermission);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOpusProjectileMotion,int32,Index,float,DeltaSeconds);
USTRUCT(BlueprintType)
struct FOpusProjectileState
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool Valid=false;
 UPROPERTY(BlueprintReadOnly) bool Hostile=false;
 UPROPERTY(BlueprintReadOnly) FVector Position=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector Velocity=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) int32 Id=0;
 UPROPERTY(BlueprintReadOnly) FName Kind;
 UPROPERTY(BlueprintReadOnly) double Age=0;
 UPROPERTY(BlueprintReadOnly) int32 GroundTint=-1;
};
// Generic serial host-policy bridge. Ability decisions, targeting and timing live in AS.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusDeliveryPolicy : public UActorComponent
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) bool BindPolicy(UDWRuleWorldSubsystem* InRules,UObject* Observer,FName DamageFunction,FName MotionFunction);
 UFUNCTION(BlueprintCallable) double LiveStat(FName Path);
 UFUNCTION(BlueprintPure) FName SelectedSkill() const;
 UFUNCTION(BlueprintPure) FName SkillAt(int32 Slot) const;
 UFUNCTION(BlueprintPure) int32 ProjectileCount() const;
 UFUNCTION(BlueprintPure) FOpusProjectileState Projectile(int32 Index) const;
 UFUNCTION(BlueprintCallable) void SetVelocity(int32 Index,FVector Velocity);
private:
 UPROPERTY() TObjectPtr<UDWRuleWorldSubsystem> Rules;
 UPROPERTY() FOpusDamagePermission Permission;
 UPROPERTY() FOpusProjectileMotion Motion;
 double SheetClock=-1;
 TArray<FName> Granted;
 bool Bound=false;
};
