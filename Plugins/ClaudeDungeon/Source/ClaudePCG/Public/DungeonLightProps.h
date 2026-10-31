#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonLightProps.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UParticleSystemComponent;

/** Base for PCG-spawned light sources: owns a mesh and a softly flickering point light. */
UCLASS(Abstract)
class CLAUDEPCG_API ADungeonLightProp : public AActor
{
	GENERATED_BODY()

public:
	ADungeonLightProp();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	float BaseIntensity = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light", meta = (ClampMin = "0", ClampMax = "1"))
	float FlickerAmount = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	float FlickerSpeed = 7.f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	FVector BaseLightLocation = FVector::ZeroVector;
	float Phase = 0.f;
};

UCLASS()
class CLAUDEPCG_API ADungeonTorch : public ADungeonLightProp
{
	GENERATED_BODY()

public:
	ADungeonTorch();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UParticleSystemComponent> Fire;
};

UCLASS()
class CLAUDEPCG_API ADungeonCandelabra : public ADungeonLightProp
{
	GENERATED_BODY()

public:
	ADungeonCandelabra();
};
