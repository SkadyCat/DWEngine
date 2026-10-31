#pragma once

#include "CoreMinimal.h"
#include "DungeonTypes.h"

struct CLAUDEPCG_API FDungeonRoom
{
	int32 Id = INDEX_NONE;
	FIntPoint Min = FIntPoint::ZeroValue;
	FIntPoint Size = FIntPoint(1, 1);
	EDungeonRoomTheme Theme = EDungeonRoomTheme::Storage;

	FIntPoint Max() const { return Min + Size; }
	FIntPoint CenterCell() const { return Min + Size / 2; }
	int32 Area() const { return Size.X * Size.Y; }
	bool Contains(const FIntPoint& P) const { return P.X >= Min.X && P.Y >= Min.Y && P.X < Min.X + Size.X && P.Y < Min.Y + Size.Y; }
};

/**
 * Grid based dungeon layout: rooms scattered on a macro grid, connected with a minimum spanning tree
 * plus a few loops, corridors carved with a turn-penalised Dijkstra. Pure data, deterministic per seed.
 */
class CLAUDEPCG_API FDungeonLayout
{
public:
	static constexpr double CellSize = 400.0;
	static constexpr double TileSize = 200.0;
	static constexpr double TierHeight = 200.0;
	static constexpr int32 Empty = -2;
	static constexpr int32 Corridor = -1;

	static const FIntPoint Dirs[4];

	int32 Width = 0;
	int32 Height = 0;
	TArray<int32> Cells;
	TArray<FDungeonRoom> Rooms;
	TArray<FIntPoint> Connections;
	TSet<uint64> Doorways;
	int32 StartRoom = INDEX_NONE;
	int32 TreasureRoom = INDEX_NONE;

	void Generate(const FDungeonGenerationParams& Params);

	bool InBounds(const FIntPoint& P) const { return P.X >= 0 && P.Y >= 0 && P.X < Width && P.Y < Height; }
	int32 Get(const FIntPoint& P) const { return InBounds(P) ? Cells[P.Y * Width + P.X] : Empty; }
	bool IsFloor(const FIntPoint& P) const { return Get(P) != Empty; }
	bool IsRoom(const FIntPoint& P) const { return Get(P) >= 0; }

	uint64 EdgeKey(const FIntPoint& A, const FIntPoint& B) const;
	bool IsDoorway(const FIntPoint& A, const FIntPoint& B) const { return Doorways.Contains(EdgeKey(A, B)); }

	/** True when a wall or doorway separates the two cells. */
	bool HasBoundary(const FIntPoint& A, const FIntPoint& B) const;

	/** Local-space (actor space) helpers. */
	static FVector CellCenter(const FIntPoint& P) { return FVector((P.X + 0.5) * CellSize, (P.Y + 0.5) * CellSize, 0.0); }
	FVector GetRoomCenter(int32 RoomIndex) const;
	FVector GetStartLocation() const;
	FBox GetLocalBounds() const;

private:
	void Set(const FIntPoint& P, int32 Value) { Cells[P.Y * Width + P.X] = Value; }
	void PlaceRooms(const FDungeonGenerationParams& Params, FRandomStream& Rng);
	void BuildConnections(const FDungeonGenerationParams& Params, FRandomStream& Rng);
	bool CarveCorridor(int32 RoomA, int32 RoomB);
	void AssignThemes(FRandomStream& Rng);
};
