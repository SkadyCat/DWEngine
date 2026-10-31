#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CleanMageAnimationLibrary.generated.h"
class UAnimSequence;

UENUM(BlueprintType)
enum class ECleanSpellReleaseHand : uint8
{
    RightHand = 0 UMETA(DisplayName="右手"),
    LeftHand = 1 UMETA(DisplayName="左手")
};

// Stable ordered allowlist: clients send an index, never an arbitrary asset path.
UCLASS(BlueprintType)
class FCS3C_API UCleanMageAnimationLibrary : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UAnimSequence>> Animations;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FString> Labels;
    // Only clips with authored vertical root travel need flying physics during playback.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<bool> Airborne;
    // Empty supports older catalogs; new catalogs explicitly separate preview clips from spells.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<bool> SpellEligible;
    UFUNCTION(BlueprintPure) bool IsSpellAnimation(int32 Index) const;
    UFUNCTION(BlueprintCallable) int32 GetSkillAnimationIndex(FName SkillId) const;
    UFUNCTION(BlueprintCallable) ECleanSpellReleaseHand GetSkillReleaseHand(FName SkillId) const;
    UFUNCTION(BlueprintCallable) FName GetSkillReleaseSocket(FName SkillId) const;
};

// Presentation only. Release timing is authored with the AS animation notify;
// gameplay validation, effects and damage remain in Lua.
UCLASS(BlueprintType)
class FCS3C_API UCleanSkillAnimationSet : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Presentation")
    TMap<FName, TObjectPtr<UAnimSequence>> SkillAnimations;
    // Missing entries preserve the original right-hand release.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill Presentation")
    TMap<FName, ECleanSpellReleaseHand> SkillReleaseHands;
};
