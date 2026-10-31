#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DWRuleTargetComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDWRuleDamagedSignature, float, Amount, int32, Element, bool, bKilled);

// Makes an Actor hittable by rule plans (player heroes, bosses, props). Mass entities use
// FDWRuleTargetFragment instead. Health here is authoritative and only changed by the rule world.
UCLASS(ClassGroup = (Rules), meta = (BlueprintSpawnableComponent))
class DWRULES_API UDWRuleTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDWRuleTargetComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	float RadiusCm = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Rules")
	float Health = 100.f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Rules") float Shield = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") float MaxShield = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules") bool bElite = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	uint8 Team = 0;

	// Game entity id reported in damage rows (e.g. the Lua EntityID). 0 = auto slot id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	int32 ExternalId = 0;

	// fire, frost, lightning, poison. Physical/void have no elemental resistance.
	UPROPERTY(EditAnywhere, Category = "Rules")
	float Resistance[4] = { 0.f, 0.f, 0.f, 0.f };

	UPROPERTY(BlueprintAssignable, Category = "Rules")
	FDWRuleDamagedSignature OnRuleDamaged;

	// Rule-world external id of this target (valid after BeginPlay).
	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetTargetId() const
	{
		return TargetId;
	}

	UFUNCTION(BlueprintPure, Category = "Rules")
	bool IsAlive() const
	{
		return Health > 0.f;
	}

	UFUNCTION(BlueprintCallable, Category = "Rules")
	void ResetHealth();

	int32 TargetId = 0;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
