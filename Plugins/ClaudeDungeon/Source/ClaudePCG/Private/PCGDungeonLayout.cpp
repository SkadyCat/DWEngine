#include "PCGDungeonLayout.h"

#include "DungeonDresser.h"
#include "DungeonGenerator.h"
#include "DungeonLayout.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGDungeonLayout"

UPCGDungeonLayoutSettings::UPCGDungeonLayoutSettings()
{
	bUseSeed = true;
	FloorMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ClaudeDungeonKit/Meshes/SM_Floor.SM_Floor")));
	WallMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ClaudeDungeonKit/Meshes/SM_Wall.SM_Wall")));
	ArchMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ClaudeDungeonKit/Meshes/SM_Arch.SM_Arch")));
	ColumnMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ClaudeDungeonKit/Meshes/SM_ColumnMedium.SM_ColumnMedium")));
	TrimMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ClaudeDungeonKit/Meshes/SM_Separator.SM_Separator")));
}

#if WITH_EDITOR
FText UPCGDungeonLayoutSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Dungeon Layout");
}

FText UPCGDungeonLayoutSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Generates rooms, corridors, walls, arches, columns, themed props, debris and light anchors for a top-down dungeon.");
}
#endif

TArray<FPCGPinProperties> UPCGDungeonLayoutSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	for (const FName& Label : { DungeonPCGPins::Floors, DungeonPCGPins::Walls, DungeonPCGPins::Trim, DungeonPCGPins::Props,
		DungeonPCGPins::Clutter, DungeonPCGPins::Debris, DungeonPCGPins::Torches, DungeonPCGPins::Candelabras })
	{
		Pins.Emplace(Label, EPCGDataType::Point);
	}
	return Pins;
}

FPCGElementPtr UPCGDungeonLayoutSettings::CreateElement() const
{
	return MakeShared<FPCGDungeonLayoutElement>();
}

bool FPCGDungeonLayoutElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDungeonLayoutElement::Execute);

	const UPCGDungeonLayoutSettings* Settings = Context->GetInputSettings<UPCGDungeonLayoutSettings>();
	check(Settings);

	UPCGComponent* Component = Context->SourceComponent.Get();
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	const ADungeonGenerator* Generator = Cast<ADungeonGenerator>(Owner);

	FDungeonGenerationParams Params = Settings->Params;
	if (Settings->bUseOwnerParams && Generator)
	{
		Params = Generator->Params;
	}
	else
	{
		Params.Seed = int32(uint32(Params.Seed) ^ (uint32(Context->GetSeed()) * 2654435761u));
	}

	FTransform ActorTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	ActorTransform.SetScale3D(FVector::OneVector);

	FDungeonLayout Layout;
	Layout.Generate(Params);

	FDungeonMeshSet Meshes;
	Meshes.Floor = Settings->FloorMesh.ToSoftObjectPath();
	Meshes.Wall = Settings->WallMesh.ToSoftObjectPath();
	Meshes.Arch = Settings->ArchMesh.ToSoftObjectPath();
	Meshes.Column = Settings->ColumnMesh.ToSoftObjectPath();
	Meshes.Trim = Settings->TrimMesh.ToSoftObjectPath();

	FDungeonDresser Dresser(Layout, Params, Meshes, ActorTransform);
	Dresser.Build();

	auto Emit = [Context](const FName& Pin, const FDungeonPointSink& Sink)
	{
		FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef();
		Out.Data = Sink.Data;
		Out.Pin = Pin;
	};
	Emit(DungeonPCGPins::Floors, Dresser.Floors);
	Emit(DungeonPCGPins::Walls, Dresser.Walls);
	Emit(DungeonPCGPins::Trim, Dresser.Trim);
	Emit(DungeonPCGPins::Props, Dresser.Props);
	Emit(DungeonPCGPins::Clutter, Dresser.Clutter);
	Emit(DungeonPCGPins::Debris, Dresser.Debris);
	Emit(DungeonPCGPins::Torches, Dresser.Torches);
	Emit(DungeonPCGPins::Candelabras, Dresser.Candelabras);

	PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("Summary", "Dungeon: {0} rooms, {1} floor tiles, {2} wall pieces, {3} props, {4} clutter, {5} debris, {6} torches"),
		Layout.Rooms.Num(), Dresser.Floors.Num(), Dresser.Walls.Num(), Dresser.Props.Num(), Dresser.Clutter.Num(), Dresser.Debris.Num(), Dresser.Torches.Num()));

	return true;
}

#undef LOCTEXT_NAMESPACE
