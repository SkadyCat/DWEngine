#include "DungeonGenerator.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"

namespace
{
	uint32 HashParams(const FDungeonGenerationParams& P)
	{
		uint32 H = GetTypeHash(P.Seed);
		H = HashCombine(H, GetTypeHash(P.GridSize));
		H = HashCombine(H, GetTypeHash(P.MaxRooms));
		H = HashCombine(H, GetTypeHash(P.MinRoomSize));
		H = HashCombine(H, GetTypeHash(P.MaxRoomSize));
		H = HashCombine(H, GetTypeHash(P.ExtraConnectionChance));
		return H;
	}
}

ADungeonGenerator::ADungeonGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Ground = CreateDefaultSubobject<UBoxComponent>(TEXT("Ground"));
	Ground->SetupAttachment(Root);
	Ground->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Ground->SetHiddenInGame(true);
#if WITH_EDITORONLY_DATA
	Ground->bVisualizeComponent = false;
#endif
	// Only generated floor meshes define walkable navigation. The safety slab
	// must not create paths through the invisible space outside the dungeon.
	Ground->SetCanEverAffectNavigation(false);
	Ground->ShapeColor = FColor(255, 140, 40);

	PCG = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
	PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnLoad;

	UpdateGround();
}

void ADungeonGenerator::UpdateGround()
{
	const double Size = FMath::Max(12, Params.GridSize) * FDungeonLayout::CellSize;
	Ground->SetBoxExtent(FVector(Size * 0.5, Size * 0.5, 25.0), false);
	Ground->SetRelativeLocation(FVector(Size * 0.5, Size * 0.5, -25.0));
}

void ADungeonGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateGround();
}

void ADungeonGenerator::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// In the editor, make sure a freshly opened level shows the dungeon without pressing "Generate".
	// Deferred one frame so the PCG subsystem of the editor world is guaranteed to exist.
	UWorld* World = GetWorld();
	if (World && !World->IsGameWorld() && !IsRunningCommandlet() && !IsTemplate() && PCG && PCG->GetGraph() && !PCG->bGenerated)
	{
		TWeakObjectPtr<ADungeonGenerator> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (ADungeonGenerator* Self = WeakThis.Get())
			{
				if (Self->PCG && !Self->PCG->bGenerated && !Self->PCG->IsGenerating())
				{
					Self->PCG->GenerateLocal(/*bForce=*/false);
				}
			}
			return false;
		}));
	}
}

#if WITH_EDITOR
void ADungeonGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bCacheValid = false;
	UpdateGround();
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		Regenerate();
	}
}
#endif

void ADungeonGenerator::Regenerate()
{
	bCacheValid = false;
	if (!PCG)
	{
		return;
	}
	if (PCG->IsGenerating())
	{
		PCG->CancelGeneration();
	}
	PCG->GenerateLocal(/*bForce=*/true);
}

void ADungeonGenerator::RandomizeSeed()
{
	Params.Seed = FMath::Rand() ^ (FMath::Rand() << 15);
	bCacheValid = false;
	Regenerate();
}

void ADungeonGenerator::SetupGraph(UPCGGraphInterface* Graph)
{
	if (PCG)
	{
		PCG->SetGraphLocal(Graph);
	}
}

const FDungeonLayout& ADungeonGenerator::GetLayout() const
{
	const uint32 Hash = HashParams(Params);
	if (!bCacheValid || Hash != CachedHash)
	{
		CachedLayout.Generate(Params);
		CachedHash = Hash;
		bCacheValid = true;
	}
	return CachedLayout;
}

FVector ADungeonGenerator::ToWorld(const FVector& Local) const
{
	FTransform T = GetActorTransform();
	T.SetScale3D(FVector::OneVector);
	return T.TransformPosition(Local);
}

FVector ADungeonGenerator::GetPlayerStartLocation() const
{
	return ToWorld(GetLayout().GetStartLocation());
}

FVector ADungeonGenerator::GetDungeonCenter() const
{
	return ToWorld(GetLayout().GetLocalBounds().GetCenter() * FVector(1.0, 1.0, 0.0));
}

FVector ADungeonGenerator::GetDungeonExtent() const
{
	return GetLayout().GetLocalBounds().GetExtent();
}

TArray<FDungeonRoomInfo> ADungeonGenerator::GetRooms() const
{
	TArray<FDungeonRoomInfo> Out;
	const FDungeonLayout& Layout = GetLayout();
	for (const FDungeonRoom& Room : Layout.Rooms)
	{
		if (Room.Area() <= 0)
		{
			continue;
		}
		FDungeonRoomInfo& Info = Out.AddDefaulted_GetRef();
		Info.Center = ToWorld(Layout.GetRoomCenter(Room.Id));
		Info.Size = FVector2D(Room.Size.X, Room.Size.Y) * FDungeonLayout::CellSize;
		Info.Theme = Room.Theme;
	}
	return Out;
}

bool ADungeonGenerator::IsGenerationInProgress() const
{
	return PCG && (PCG->IsGenerating() || !PCG->bGenerated);
}

ADungeonGenerator* ADungeonGenerator::FindInWorld(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADungeonGenerator> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}
