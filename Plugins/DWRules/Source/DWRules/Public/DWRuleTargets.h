#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "DWRuleTargets.generated.h"

// Anything the rule runtime can hit. Lives on Mass entities; health is the authoritative value.
USTRUCT()
struct DWRULES_API FDWRuleTargetFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Rules")
	float RadiusCm = 45.f;

	UPROPERTY(EditAnywhere, Category = "Rules")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, Category = "Rules")
	float MaxHealth = 100.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float Shield = 0.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float MaxShield = 0.f;
	UPROPERTY(EditAnywhere, Category = "Rules") bool bElite = false;

	UPROPERTY(EditAnywhere, Category = "Rules")
	uint8 Team = 1;

	// Game entity id reported in damage rows. 0 = derive from the Mass handle.
	UPROPERTY(EditAnywhere, Category = "Rules")
	int32 ExternalId = 0;

	// fire, frost, lightning, poison. Physical/void have no elemental resistance.
	UPROPERTY(EditAnywhere, Category = "Rules")
	float Resistance[4] = { 0.f, 0.f, 0.f, 0.f };
};

UCLASS(meta = (DisplayName = "DW Rule Target"))
class DWRULES_API UDWRuleTargetTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Rules")
	FDWRuleTargetFragment Defaults;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
