#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DWRuleWorldSubsystem.h"
#include "OpusNative.generated.h"
DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(float,FOpusSweep,FVector,Start,FVector,End,float,Radius);
USTRUCT(BlueprintType)
struct FOpusMonsterData
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FName Id;
 UPROPERTY(BlueprintReadWrite) bool Enabled=true;
 UPROPERTY(BlueprintReadWrite) double Weight=0;
 UPROPERTY(BlueprintReadWrite) double Health=0;
 UPROPERTY(BlueprintReadWrite) double Speed=0;
 UPROPERTY(BlueprintReadWrite) double StopDistance=0;
 UPROPERTY(BlueprintReadWrite) double Scale=1;
 UPROPERTY(BlueprintReadWrite) double FollowDistance=0;
 UPROPERTY(BlueprintReadWrite) int32 MinFloor=1;
 UPROPERTY(BlueprintReadWrite) FName Rarity;
};
USTRUCT(BlueprintType)
struct FOpusSkillData
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FName Id;
 UPROPERTY(BlueprintReadWrite) double Range=150;
 UPROPERTY(BlueprintReadWrite) double Cooldown=1.2;
 UPROPERTY(BlueprintReadWrite) double Power=10;
};
USTRUCT(BlueprintType)
struct FOpusMapData
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) double LifeBase=0;
 UPROPERTY(BlueprintReadOnly) double LifeIncreased=0;
 UPROPERTY(BlueprintReadOnly) double LifeMore=1;
 UPROPERTY(BlueprintReadOnly) double DamageIncreased=0;
 UPROPERTY(BlueprintReadOnly) double DamageMore=1;
 UPROPERTY(BlueprintReadOnly) double Packs=0;
 UPROPERTY(BlueprintReadOnly) int32 Tier=1;
};
USTRUCT(BlueprintType)
struct FOpusLootData
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Key;
 UPROPERTY(BlueprintReadOnly) FVector Position=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) bool Claimed=false;
};
// Reflection and file adapters only. Encounter policy lives in Script/Opus.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusNative : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) static UDWRuleWorldSubsystem* GetRules(UObject* Context);
 UFUNCTION(BlueprintPure) static TArray<FOpusMonsterData> Monsters(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintCallable) static bool MapData(UDWRuleWorldSubsystem* Rules,FOpusMapData& Out,FString& Error);
 UFUNCTION(BlueprintCallable) static bool MapCoreData(UDWRuleWorldSubsystem* Rules,FName Core,FOpusMapData& Out,FString& Error);
 UFUNCTION(BlueprintCallable) static void SetMapRewardTier(UDWRuleWorldSubsystem* Rules,int32 Tier);
 UFUNCTION(BlueprintCallable) static FString ReadMapCore();
 UFUNCTION(BlueprintCallable) static bool WriteMapCore(const FString& Core);
 UFUNCTION(BlueprintCallable) static bool RegisterEnemy(UDWRuleWorldSubsystem* Rules,AActor* Actor,int32 Id,FOpusMonsterData Data,FName Rarity,int32 Floor,float Radius,const FString& Inherited,const TArray<FOpusSkillData>& AddedSkills,FString& Error);
 UFUNCTION(BlueprintCallable) static bool HitPower(UDWRuleWorldSubsystem* Rules,int32 Id,float Power,FString& Error);
 UFUNCTION(BlueprintPure) static bool IsElite(UDWRuleWorldSubsystem* Rules,int32 Id);
 UFUNCTION(BlueprintPure) static int32 ActivePlan(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static int32 Selected(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintCallable) static void CarryPools(UDWRuleWorldSubsystem* Rules,FDWRulePlayerPools Pools);
 UFUNCTION(BlueprintCallable) static void SetEnabled(UDWRuleWorldSubsystem* Rules,bool Enabled);
 UFUNCTION(BlueprintCallable) static void BeginRewards(UDWRuleWorldSubsystem* Rules,int32 Seed,int32 Floor,int32 Visit,int32 Tier);
 UFUNCTION(BlueprintCallable) static void CompleteRewards(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static TArray<FOpusLootData> Loot(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static double PickupRadius();
 UFUNCTION(BlueprintCallable) static bool Pickup(UDWRuleWorldSubsystem* Rules,const FString& Key,FVector Position,FString& Error);
 UFUNCTION(BlueprintCallable) static bool BuyMerchant(UDWRuleWorldSubsystem* Rules,const FString& Key,FString& Error);
 UFUNCTION(BlueprintCallable) static bool ApplyPreset(UDWRuleWorldSubsystem* Rules,FName Id,FString& Error);
 UFUNCTION(BlueprintPure) static FName Belt(UDWRuleWorldSubsystem* Rules,int32 Slot);
 UFUNCTION(BlueprintPure) static bool Flag(const FString& Name);
 UFUNCTION(BlueprintPure) static int32 IntOption(const FString& Name,int32 Fallback);
 UFUNCTION(BlueprintPure) static FString StringOption(const FString& Name);
 UFUNCTION(BlueprintCallable) static FString ReadProfile();
 UFUNCTION(BlueprintCallable) static bool WriteProfile(const FString& Json);
 UFUNCTION(BlueprintCallable) static void WriteLog(const FString& Message);
 UFUNCTION(BlueprintCallable) static void Quit(bool Success);
 UFUNCTION(BlueprintCallable) static bool LuaSmoke();
 UFUNCTION(BlueprintCallable) static void SetGeometry(UDWRuleWorldSubsystem* Rules,FOpusSweep Sweep);
 UFUNCTION(BlueprintCallable) static void Velocity(AActor* Actor,FVector Value);
 UFUNCTION(BlueprintCallable) static void Tint(UPrimitiveComponent* Component,FLinearColor Color);
 UFUNCTION(BlueprintCallable) static void TargetPoint(UDWRuleWorldSubsystem* Rules,int32 Owner,FVector Point);
 UFUNCTION(BlueprintCallable) static void StepIfNeeded(UDWRuleWorldSubsystem* Rules,float Dt);
 UFUNCTION(BlueprintPure) static int64 Stones(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static int32 SummonCount(UDWRuleWorldSubsystem* Rules);

};
