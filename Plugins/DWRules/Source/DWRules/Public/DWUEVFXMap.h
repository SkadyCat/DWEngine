#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DWUEVFXMap.generated.h"

class UNiagaraSystem;

USTRUCT(BlueprintType)
struct DWRULES_API FDWUEVFXEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UNiagaraSystem> System;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Scale = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Offset = FVector::ZeroVector;
	// Optional Niagara User.* overrides, applied before activation. Gameplay never reads these.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> UserFloats;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, FLinearColor> UserColors;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, bool> UserBools;
	// Flight follows its existing rule carrier. Bursts finish independently.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bFollowSource = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05")) float MaxVisualSeconds = 3.f;
};

// Presentation overrides only: no damage, targeting, collision or gameplay lifetime.
UCLASS(BlueprintType)
class DWRULES_API UDWUEVFXMap : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, FDWUEVFXEntry> Effects;
	// New instances exceeding this limit retain the existing default renderer.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="4096")) int32 MaxActiveSystems = 128;
};
