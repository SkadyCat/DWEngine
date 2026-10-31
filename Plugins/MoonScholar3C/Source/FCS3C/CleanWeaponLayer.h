#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CleanWeaponLayer.generated.h"
class UAnimSequence;

// Presentation only: selecting a stance grants no inventory item or combat effect.
UCLASS(BlueprintType)
class FCS3C_API UCleanWeaponLayer : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName StanceName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> IdlePose;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float UpperBodyWeight = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UAnimSequence>> LightAttacks;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> HeavyAttack;
};
