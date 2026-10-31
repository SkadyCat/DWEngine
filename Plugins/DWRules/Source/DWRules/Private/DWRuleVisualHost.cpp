#include "DWRuleVisualHost.h"
#include "DWWebVFXComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DWRuleAssets.h"
#include "DWRuleWorld.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* const FallbackMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	// Engine sphere is 100 cm across: radius 1 m maps to scale 2.
	const FVector FallbackScalePerMeter(2.0, 2.0, 2.0);
}

ADWRuleVisualHost::ADWRuleVisualHost()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	WebEffects=CreateDefaultSubobject<UDWWebVFXComponent>(TEXT("WebPrefabs"));WebEffects->SetupAttachment(GetRootComponent());
	SetCanBeDamaged(false);
}

UInstancedStaticMeshComponent* ADWRuleVisualHost::EnsureComponent(FName Kind, const UDWRuleWorldConfig* Config)
{
	if (FBatch* Existing = Batches.Find(Kind))
	{
		return Existing->Component;
	}
	const FDWRuleVisual* Visual = Config ? Config->Visuals.Find(Kind) : nullptr;
	UStaticMesh* Mesh = Visual && Visual->Mesh ? Visual->Mesh.Get() : LoadObject<UStaticMesh>(nullptr, FallbackMesh);
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetStaticMesh(Mesh);
	if (Visual && Visual->Material)
	{
		Component->SetMaterial(0, Visual->Material);
	}
	Component->SetupAttachment(GetRootComponent());
	Component->RegisterComponent();
	Components.Add(Component);
	FBatch& Batch = Batches.Add(Kind);
	Batch.Component = Component;
	return Component;
}

void ADWRuleVisualHost::Sync(const DWRules::FDWRuleWorld& World, const UDWRuleWorldConfig* Config)
{
	bUsingWebEffects=SyncWebCarriers(World);
	if(bUsingWebEffects)return;
	const DWRules::FDWProjectiles& Carriers = World.Projectiles();
	for (TPair<FName, FBatch>& Pair : Batches)
	{
		Pair.Value.Transforms.Reset();
	}
	for (int32 Index = 0; Index < Carriers.Num; ++Index)
	{
		if (Carriers.Flags[Index] & DWRules::FDWProjectiles::Dead)
		{
			continue;
		}
		const DWRules::FDWCompiledPlan& Plan = World.GetPlan(World.GetRoot(int32(Carriers.Root[Index])).Plan);
		const FName Kind = Plan.KindNames[Carriers.Kind[Index]];
		EnsureComponent(Kind, Config);
		FBatch& Batch = Batches[Kind];
		const FDWRuleVisual* Visual = Config ? Config->Visuals.Find(Kind) : nullptr;
		const FVector ScalePerMeter = Visual && Visual->Mesh ? Visual->ScalePerMeter : FallbackScalePerMeter;
		const FVector Location = FVector(Carriers.Position[Index]) * 100.0;
		const FRotator Rotation = FVector(Carriers.Velocity[Index]).Rotation();
		Batch.Transforms.Emplace(Rotation, Location, ScalePerMeter * double(Carriers.Radius[Index]));
	}
	for (const DWRules::FDWSummon& Summon : World.GetSummons())
	{
		if (Summon.bStorm || Summon.bDead)
			continue;
		const DWRules::FDWCompiledPlan& Plan = World.GetPlan(World.GetRoot(Summon.Root).Plan);
		const FName Kind = Plan.KindNames[Summon.Kind];
		EnsureComponent(Kind, Config);
		const FDWRuleVisual* Visual = Config ? Config->Visuals.Find(Kind) : nullptr;
		const FVector Scale = Visual && Visual->Mesh ? Visual->ScalePerMeter : FVector(0.8, 0.8, 1.2);
		Batches[Kind].Transforms.Emplace(FVector(Summon.Direction).Rotation(), FVector(Summon.Position) * 100.0, Scale);
	}
	for (TPair<FName, FBatch>& Pair : Batches)
	{
		FBatch& Batch = Pair.Value;
		const int32 Needed = Batch.Transforms.Num();
		if (Needed > Batch.Allocated)
		{
			TArray<FTransform> Extra;
			Extra.Append(Batch.Transforms.GetData() + Batch.Allocated, Needed - Batch.Allocated);
			Batch.Component->AddInstances(Extra, false, true);
			Batch.Allocated = Needed;
		}
		// Unused instances collapse to zero scale instead of being removed (no index churn).
		while (Batch.Transforms.Num() < Batch.Allocated)
		{
			Batch.Transforms.Emplace(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
		}
		if (Batch.Allocated > 0)
		{
			Batch.Component->BatchUpdateInstancesTransforms(0, Batch.Transforms, true, true, true);
		}
	}
}
