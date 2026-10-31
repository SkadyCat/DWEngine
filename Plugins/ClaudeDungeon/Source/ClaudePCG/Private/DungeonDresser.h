#pragma once

#include "CoreMinimal.h"
#include "DungeonLayout.h"
#include "UObject/SoftObjectPath.h"

class UPCGPointData;
template<typename T> class FPCGMetadataAttribute;

/** Writes PCG points (with an optional "Mesh" soft path attribute) in world space. */
struct FDungeonPointSink
{
	UPCGPointData* Data = nullptr;
	FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = nullptr;
	FTransform ActorTransform = FTransform::Identity;

	void Init(const FTransform& InActorTransform, bool bWithMeshAttribute);
	void Add(const FTransform& Local, const FSoftObjectPath& Mesh, const FVector& Extents = FVector(50.0), float Density = 1.f);
	int32 Num() const;
};

struct FDungeonMeshSet
{
	FSoftObjectPath Floor;
	FSoftObjectPath Wall;
	FSoftObjectPath Arch;
	FSoftObjectPath Column;
	FSoftObjectPath Trim;
};

/** Turns an abstract FDungeonLayout into categorized point sets ready for spawning. */
class FDungeonDresser
{
public:
	FDungeonDresser(const FDungeonLayout& InLayout, const FDungeonGenerationParams& InParams, const FDungeonMeshSet& InMeshes, const FTransform& InActorTransform);

	void Build();

	FDungeonPointSink Floors;
	FDungeonPointSink Walls;
	FDungeonPointSink Trim;
	FDungeonPointSink Props;
	FDungeonPointSink Clutter;
	FDungeonPointSink Debris;
	FDungeonPointSink Torches;
	FDungeonPointSink Candelabras;

private:
	struct FFace
	{
		FIntPoint Cell;
		FIntPoint Neighbor;
		FVector Center;
		FVector Normal;
		FVector Along;
		double Yaw = 0.0;
		int32 Tiers = 0;
		bool bDoorway = false;
		bool bArch = false;
		bool bCut = false;
		bool bOwnsMesh = false;
		int32 Region = 0;
	};

	struct FSlot
	{
		FVector Pos;
		FVector Normal;
		FVector Along;
		double Yaw = 0.0;
		bool bTallWall = false;
	};

	struct FCircle
	{
		FVector2D P;
		double R;
	};

	void CollectFaces();
	void EmitFloors();
	void EmitWalls();
	void EmitColumns();
	void EmitTorches();
	void DressRooms();
	void DressCorridors();
	void EmitDebris();

	void DressRoom(const FDungeonRoom& Room);
	void DressStart(const FDungeonRoom& Room, TArray<FSlot>& Slots);
	void DressHall(const FDungeonRoom& Room, TArray<FSlot>& Slots);
	void DressLibrary(const FDungeonRoom& Room, TArray<FSlot>& Slots);
	void DressDining(const FDungeonRoom& Room, TArray<FSlot>& Slots);
	void DressStorage(const FDungeonRoom& Room, TArray<FSlot>& Slots);
	void DressCrypt(const FDungeonRoom& Room, TArray<FSlot>& Slots);
	void DressTreasure(const FDungeonRoom& Room, TArray<FSlot>& Slots);

	// Placement helpers
	bool IsEdgeCut(const FIntPoint& A, const FIntPoint& B) const;
	bool VertexTouchesCut(int32 I, int32 J) const;
	bool HasArch(const FIntPoint& A, const FIntPoint& B) const;
	static double ColumnScale(int32 Tiers);
	int32 EdgeTiers(const FIntPoint& A, const FIntPoint& B) const;
	bool IsFree(const FVector& P, double Radius) const;
	void Reserve(const FVector& P, double Radius) { Occupied.Add({ FVector2D(P), Radius }); }
	bool PlaceProp(const TCHAR* Mesh, const FVector& P, double Yaw, double Radius, bool bCollidable = true, const FVector& Scale = FVector::OneVector, bool bForce = false);
	void AddClutter(const TCHAR* Mesh, const FVector& P, const FRotator& Rot, const FVector& Scale = FVector::OneVector);
	void PlaceCandleCluster(const FVector& P, int32 Count, double Spread = 32.0);
	void PlaceCandelabra(const FVector& P);
	void FillShelf(const FVector& Pivot, const FVector& Along, const FVector& Normal, double Yaw);
	void SetTable(const FVector& Center, double Yaw, bool bDining);
	TArray<FSlot> GatherWallSlots(const FDungeonRoom& Room) const;
	FVector RoomCornerInset(const FDungeonRoom& Room, int32 Corner, double Inset) const;
	FBox2D RoomInner(const FDungeonRoom& Room) const;
	void Shuffle(TArray<FSlot>& Slots);

	static FSoftObjectPath Kit(const TCHAR* Name);
	static double YawFromNormal(const FVector& N) { return FMath::RadiansToDegrees(FMath::Atan2(-N.X, N.Y)); }

	const FDungeonLayout& Layout;
	const FDungeonGenerationParams& Params;
	FDungeonMeshSet Meshes;
	FTransform ActorTransform;
	FRandomStream Rng;

	TArray<FFace> Faces;
	TArray<FCircle> Occupied;
	TArray<FCircle> KeepOut;
	int32 CurrentRoomId = INDEX_NONE;
};
