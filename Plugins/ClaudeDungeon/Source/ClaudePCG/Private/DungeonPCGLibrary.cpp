#include "DungeonPCGLibrary.h"

#include "PCGNode.h"
#include "Elements/PCGSpawnActor.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "MeshSelectors/PCGMeshSelectorByAttribute.h"

#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/BrushComponent.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#include "BSPOps.h"
#endif

bool UDungeonPCGLibrary::ConfigureNavigationBounds(ANavMeshBoundsVolume* Volume, FVector Extent)
{
#if WITH_EDITOR
	if (!Volume || !Volume->GetWorld() || Extent.GetMin() <= 0.0) return false;
	UCubeBuilder* Builder = NewObject<UCubeBuilder>();
	Builder->X = Extent.X * 2.0;
	Builder->Y = Extent.Y * 2.0;
	Builder->Z = Extent.Z * 2.0;
	if (!Builder->Build(Volume->GetWorld(), Volume)) return false;
	// CubeBuilder updates polygons, but the old BSP nodes still describe the
	// default cube. Rebuild those nodes before deriving the cooked convex hull.
	FBSPOps::csgPrepMovingBrush(Volume);
	Volume->GetBrushComponent()->BuildSimpleBrushCollision();
	Volume->GetBrushComponent()->UpdateBounds();
	Volume->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UDungeonPCGLibrary::ConfigureMeshSpawner(UPCGSettings* Settings, FName MeshAttribute, bool bCollision, bool bCastShadow, float CullDistance)
{
	UPCGStaticMeshSpawnerSettings* Spawner = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
	if (!Spawner)
	{
		return false;
	}

	Spawner->SetMeshSelectorType(UPCGMeshSelectorByAttribute::StaticClass());
	UPCGMeshSelectorByAttribute* Selector = Cast<UPCGMeshSelectorByAttribute>(Spawner->MeshSelectorParameters);
	if (!Selector)
	{
		return false;
	}

	Selector->AttributeName = MeshAttribute;
	FSoftISMComponentDescriptor& Desc = Selector->TemplateDescriptor;
	Desc.bUseDefaultCollision = false;
	Desc.BodyInstance.SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Desc.bCastShadow = bCastShadow;
	Desc.bCastDynamicShadow = bCastShadow;
	Desc.bCastStaticShadow = false;
	if (CullDistance > 0.f)
	{
		Desc.InstanceStartCullDistance = int32(CullDistance * 0.8f);
		Desc.InstanceEndCullDistance = int32(CullDistance);
	}
	Spawner->bApplyMeshBoundsToPoints = true;
	Spawner->MarkPackageDirty();
	return true;
}

bool UDungeonPCGLibrary::ConfigureActorSpawner(UPCGSettings* Settings, TSubclassOf<AActor> ActorClass)
{
	UPCGSpawnActorSettings* Spawner = Cast<UPCGSpawnActorSettings>(Settings);
	if (!Spawner || !ActorClass)
	{
		return false;
	}
	Spawner->Option = EPCGSpawnActorOption::NoMerging;
	Spawner->bForceDisableActorParsing = true;
	Spawner->AttachOptions = EPCGAttachOptions::Attached;
	Spawner->SetTemplateActorClass(ActorClass);
	Spawner->MarkPackageDirty();
	return true;
}

void UDungeonPCGLibrary::SetNodePosition(UPCGNode* Node, int32 X, int32 Y)
{
#if WITH_EDITOR
	if (Node)
	{
		Node->SetNodePosition(X, Y);
	}
#endif
}

void UDungeonPCGLibrary::SetNodeTitle(UPCGNode* Node, FName Title)
{
	if (Node)
	{
		Node->NodeTitle = Title;
	}
}

bool UDungeonPCGLibrary::UseComplexAsSimpleCollision(UStaticMesh* Mesh)
{
#if WITH_EDITOR
	if (!Mesh)
	{
		return false;
	}
	if (!Mesh->GetBodySetup())
	{
		Mesh->CreateBodySetup();
	}
	UBodySetup* Body = Mesh->GetBodySetup();
	if (!Body)
	{
		return false;
	}
	Body->Modify();
	Body->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}
