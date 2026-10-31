#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DWWebVFXActor.generated.h"
class UDWWebVFXComponent;
class UDWWebVFXPrefab;

// Placeable, presentation-only prefab. Combat uses the shared batched component
// instead of spawning one of these actors for every particle or projectile.
UCLASS(Blueprintable)
class DWRULES_API ADWWebVFXActor : public AActor
{
	GENERATED_BODY()
public:
	ADWWebVFXActor();
	virtual void Tick(float Dt) override;
	virtual bool ShouldTickIfViewportsOnly() const override {return true;}
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effect") TObjectPtr<UDWWebVFXPrefab> Prefab;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effect") bool bLoop=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effect") float PlayRate=1;
private:
	UPROPERTY(VisibleAnywhere,Category="Effect") TObjectPtr<UDWWebVFXComponent> Renderer;
	float Age=0;
};
