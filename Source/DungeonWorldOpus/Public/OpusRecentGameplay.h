#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpusRecentGameplay.generated.h"
class UDWRuleWorldSubsystem;
USTRUCT(BlueprintType)
struct FOpusGroundView
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FName Type;
 UPROPERTY(BlueprintReadOnly) FVector Position=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector Normal=FVector::UpVector;
 UPROPERTY(BlueprintReadOnly) double Radius=0;
 UPROPERTY(BlueprintReadOnly) double Remaining=0;
 UPROPERTY(BlueprintReadOnly) int32 Element=-1;
};
USTRUCT(BlueprintType)
struct FOpusAuraChoice
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FName Id;
 UPROPERTY(BlueprintReadOnly) FString Name;
 UPROPERTY(BlueprintReadOnly) FString Description;
 UPROPERTY(BlueprintReadOnly) double Reservation=0;
};
USTRUCT(BlueprintType)
struct FOpusMonsterEmission
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FName Kind;
 UPROPERTY(BlueprintReadOnly) int32 Count=0;
 UPROPERTY(BlueprintReadOnly) double Duration=0;
 UPROPERTY(BlueprintReadOnly) double Radius=0;
 UPROPERTY(BlueprintReadOnly) double Angle=0;
 UPROPERTY(BlueprintReadOnly) FVector Origin=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) bool Invulnerable=false;
};
// Typed transport only: AS owns toggles, slots, actor lifetime and player interaction.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusRecentGameplay : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) static TArray<FOpusAuraChoice> AuraChoices(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintCallable) static bool ApplyAuras(UDWRuleWorldSubsystem* Rules,const TArray<FName>& Ids,FString& Error);
 UFUNCTION(BlueprintCallable) static TArray<FOpusMonsterEmission> DrainMonsterEmissions(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintCallable) static void ReleaseTemporaryMonster(UDWRuleWorldSubsystem* Rules,int32 TargetId);
 UFUNCTION(BlueprintCallable) static void SetTargetInvulnerable(UDWRuleWorldSubsystem* Rules,int32 TargetId,bool Enabled);
 UFUNCTION(BlueprintPure) static double ManaReservation(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static double UsableMana(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static bool SelfTarget(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static bool PointTarget(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static double SkillCooldown(UDWRuleWorldSubsystem* Rules,int32 Slot);
 UFUNCTION(BlueprintPure) static int32 Mirrors(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static int32 Captures(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusGroundView> Grounds(UDWRuleWorldSubsystem* Rules);
};
