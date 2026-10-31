#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DWRuleCombat.h"
#include "DWRuleVisualHost.generated.h"

class UInstancedStaticMeshComponent;
class UDWRuleWorldConfig;
class UDWWebVFXComponent;
namespace DWRules { struct FDWRuleLibrary; class FDWCombatRuntime; }

namespace DWRules
{
	struct FDWCompiledPlan;
	struct FDWProjectiles;
	class FDWRuleWorld;
}

// One ISM per carrier visual; whole-frame batch transform upload. No per-carrier Actor or component.
UCLASS(NotPlaceable, Transient)
class DWRULES_API ADWRuleVisualHost : public AActor
{
	GENERATED_BODY()

public:
	ADWRuleVisualHost();

	void Sync(const DWRules::FDWRuleWorld& World, const UDWRuleWorldConfig* Config);
	void SyncEffects(TArray<DWRules::FDWCombatEffect> Effects, float Dt, const UDWRuleWorldConfig* Config);
	void ClearWebPresentation();
	void SyncWebCombat(const DWRules::FDWRuleWorld& World, const DWRules::FDWCombatRuntime* Combat, const DWRules::FDWRuleLibrary& Library);

private:
	struct FBatch
	{
		UInstancedStaticMeshComponent* Component = nullptr;
		TArray<FTransform> Transforms;
		int32 Allocated = 0;
	};

	UInstancedStaticMeshComponent* EnsureComponent(FName Kind, const UDWRuleWorldConfig* Config);
	bool SyncWebCarriers(const DWRules::FDWRuleWorld& World);
	UPROPERTY(Transient) TObjectPtr<UDWWebVFXComponent> WebEffects;
	bool bUsingWebEffects=false;
	float WebClock=0, WebDelta=0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;

	TMap<FName, FBatch> Batches;
	struct FEffect { DWRules::FDWCombatEffect Data; float Age=0; uint64 VisualId=0; FName MappingId; };
	TArray<FEffect> LiveEffects;
	uint64 NextEffectVisualId = 1;
	// Recent presentation identities survive the rule carrier's compact/remove.
	TMap<uint32, float> IceCarrierSeen;
};
