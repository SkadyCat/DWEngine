#include "DungeonLayout.h"
#include "Algo/Reverse.h"

const FIntPoint FDungeonLayout::Dirs[4] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };

void FDungeonLayout::Generate(const FDungeonGenerationParams& Params)
{
	FRandomStream Rng(Params.Seed);

	Width = FMath::Max(12, Params.GridSize);
	Height = Width;
	Cells.Init(Empty, Width * Height);
	Rooms.Reset();
	Connections.Reset();
	Doorways.Reset();
	StartRoom = INDEX_NONE;
	TreasureRoom = INDEX_NONE;

	PlaceRooms(Params, Rng);
	if (Rooms.Num() == 0)
	{
		return;
	}

	BuildConnections(Params, Rng);

	// Drop rooms that could not be reached (corridor carving failed) so we never get sealed boxes.
	TArray<bool> Reached;
	Reached.Init(false, Rooms.Num());
	Reached[0] = true;
	for (bool bChanged = true; bChanged;)
	{
		bChanged = false;
		for (const FIntPoint& C : Connections)
		{
			if (Reached[C.X] != Reached[C.Y])
			{
				Reached[C.X] = Reached[C.Y] = true;
				bChanged = true;
			}
		}
	}
	for (FDungeonRoom& Room : Rooms)
	{
		if (!Reached[Room.Id])
		{
			for (int32& CellValue : Cells)
			{
				if (CellValue == Room.Id)
				{
					CellValue = Empty;
				}
			}
			Room.Size = FIntPoint::ZeroValue;
		}
	}

	AssignThemes(Rng);
}

uint64 FDungeonLayout::EdgeKey(const FIntPoint& A, const FIntPoint& B) const
{
	const uint64 IA = uint64(uint32(A.Y * Width + A.X));
	const uint64 IB = uint64(uint32(B.Y * Width + B.X));
	return IA < IB ? (IA << 32) | IB : (IB << 32) | IA;
}

bool FDungeonLayout::HasBoundary(const FIntPoint& A, const FIntPoint& B) const
{
	const bool bFloorA = IsFloor(A);
	const bool bFloorB = IsFloor(B);
	if (bFloorA != bFloorB)
	{
		return true;
	}
	if (!bFloorA)
	{
		return false;
	}
	// Both floor: a boundary exists when they belong to different regions (doorways count as boundary pieces).
	return Get(A) != Get(B);
}

FVector FDungeonLayout::GetRoomCenter(int32 RoomIndex) const
{
	if (!Rooms.IsValidIndex(RoomIndex))
	{
		return FVector(Width * CellSize * 0.5, Height * CellSize * 0.5, 0.0);
	}
	const FDungeonRoom& R = Rooms[RoomIndex];
	return FVector((R.Min.X + R.Size.X * 0.5) * CellSize, (R.Min.Y + R.Size.Y * 0.5) * CellSize, 0.0);
}

FVector FDungeonLayout::GetStartLocation() const
{
	return GetRoomCenter(StartRoom);
}

FBox FDungeonLayout::GetLocalBounds() const
{
	return FBox(FVector(0.0, 0.0, -50.0), FVector(Width * CellSize, Height * CellSize, 800.0));
}

void FDungeonLayout::PlaceRooms(const FDungeonGenerationParams& Params, FRandomStream& Rng)
{
	const int32 MinSize = FMath::Clamp(Params.MinRoomSize, 3, Width / 3);
	const int32 MaxSize = FMath::Clamp(Params.MaxRoomSize, MinSize, Width / 2);
	const int32 Gap = 2;

	for (int32 Attempt = 0; Attempt < 800 && Rooms.Num() < Params.MaxRooms; ++Attempt)
	{
		// The first rooms get a chance to be big so we usually end up with a proper hall.
		const int32 Bias = Attempt < 40 && Rooms.Num() < 2 ? 1 : 0;
		int32 SX = FMath::Min(MaxSize, Rng.RandRange(MinSize, MaxSize) + Bias);
		int32 SY = FMath::Min(MaxSize, Rng.RandRange(MinSize, MaxSize) + Bias);
		if (SX > SY * 2) { SX = SY * 2; }
		if (SY > SX * 2) { SY = SX * 2; }

		const int32 MaxX = Width - SX - 1;
		const int32 MaxY = Height - SY - 1;
		if (MaxX < 1 || MaxY < 1)
		{
			continue;
		}

		FDungeonRoom Room;
		Room.Min = FIntPoint(Rng.RandRange(1, MaxX), Rng.RandRange(1, MaxY));
		Room.Size = FIntPoint(SX, SY);

		bool bOverlaps = false;
		for (const FDungeonRoom& Other : Rooms)
		{
			const bool bSeparatedX = Room.Min.X >= Other.Max().X + Gap || Other.Min.X >= Room.Max().X + Gap;
			const bool bSeparatedY = Room.Min.Y >= Other.Max().Y + Gap || Other.Min.Y >= Room.Max().Y + Gap;
			if (!bSeparatedX && !bSeparatedY)
			{
				bOverlaps = true;
				break;
			}
		}
		if (bOverlaps)
		{
			continue;
		}

		Room.Id = Rooms.Num();
		Rooms.Add(Room);
	}

	for (const FDungeonRoom& Room : Rooms)
	{
		for (int32 Y = Room.Min.Y; Y < Room.Max().Y; ++Y)
		{
			for (int32 X = Room.Min.X; X < Room.Max().X; ++X)
			{
				Set(FIntPoint(X, Y), Room.Id);
			}
		}
	}
}

void FDungeonLayout::BuildConnections(const FDungeonGenerationParams& Params, FRandomStream& Rng)
{
	const int32 Num = Rooms.Num();
	auto Distance = [this](int32 A, int32 B)
	{
		return FVector2D::Distance(FVector2D(GetRoomCenter(A)), FVector2D(GetRoomCenter(B)));
	};

	// Prim's minimum spanning tree over room centers.
	TArray<bool> InTree;
	InTree.Init(false, Num);
	InTree[0] = true;
	TSet<uint64> Used;
	TArray<FIntPoint> Planned;
	for (int32 Iter = 1; Iter < Num; ++Iter)
	{
		double Best = TNumericLimits<double>::Max();
		FIntPoint BestPair(INDEX_NONE, INDEX_NONE);
		for (int32 A = 0; A < Num; ++A)
		{
			if (!InTree[A]) continue;
			for (int32 B = 0; B < Num; ++B)
			{
				if (InTree[B]) continue;
				const double D = Distance(A, B);
				if (D < Best)
				{
					Best = D;
					BestPair = FIntPoint(A, B);
				}
			}
		}
		if (BestPair.X == INDEX_NONE)
		{
			break;
		}
		InTree[BestPair.Y] = true;
		Planned.Add(BestPair);
		Used.Add((uint64(FMath::Min(BestPair.X, BestPair.Y)) << 32) | uint64(FMath::Max(BestPair.X, BestPair.Y)));
	}

	// A few loops make the dungeon feel less like a tree.
	TArray<TPair<double, FIntPoint>> Extra;
	for (int32 A = 0; A < Num; ++A)
	{
		for (int32 B = A + 1; B < Num; ++B)
		{
			if (!Used.Contains((uint64(A) << 32) | uint64(B)))
			{
				Extra.Add({ Distance(A, B), FIntPoint(A, B) });
			}
		}
	}
	Extra.Sort([](const TPair<double, FIntPoint>& L, const TPair<double, FIntPoint>& R) { return L.Key < R.Key; });
	const int32 MaxExtra = FMath::Max(1, Num / 3);
	int32 Added = 0;
	for (const TPair<double, FIntPoint>& E : Extra)
	{
		if (Added >= MaxExtra || E.Key > CellSize * 14.0)
		{
			break;
		}
		if (Rng.FRand() < Params.ExtraConnectionChance)
		{
			Planned.Add(E.Value);
			++Added;
		}
	}

	for (const FIntPoint& Pair : Planned)
	{
		if (CarveCorridor(Pair.X, Pair.Y))
		{
			Connections.Add(Pair);
		}
	}
}

bool FDungeonLayout::CarveCorridor(int32 RoomA, int32 RoomB)
{
	const int32 NumCells = Width * Height;
	TArray<float> Dist;
	Dist.Init(TNumericLimits<float>::Max(), NumCells * 4);
	TArray<int32> Prev;
	Prev.Init(INDEX_NONE, NumCells * 4);

	struct FEntry
	{
		float Cost;
		int32 State;
	};
	TArray<FEntry> Heap;
	auto Pred = [](const FEntry& L, const FEntry& R) { return L.Cost < R.Cost; };

	auto TouchesForeignRoom = [this, RoomA, RoomB](const FIntPoint& Q)
	{
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const int32 C = Get(Q + FIntPoint(DX, DY));
				if (C >= 0 && C != RoomA && C != RoomB)
				{
					return true;
				}
			}
		}
		return false;
	};

	const FIntPoint Start = Rooms[RoomA].CenterCell();
	const int32 StartIdx = Start.Y * Width + Start.X;
	for (int32 D = 0; D < 4; ++D)
	{
		Dist[StartIdx * 4 + D] = 0.f;
		Heap.HeapPush({ 0.f, StartIdx * 4 + D }, Pred);
	}

	int32 Goal = INDEX_NONE;
	while (Heap.Num() > 0)
	{
		FEntry E;
		Heap.HeapPop(E, Pred);
		if (E.Cost > Dist[E.State])
		{
			continue;
		}
		const int32 CellIdx = E.State / 4;
		const int32 Dir = E.State % 4;
		const FIntPoint P(CellIdx % Width, CellIdx / Width);
		if (Get(P) == RoomB)
		{
			Goal = E.State;
			break;
		}

		for (int32 ND = 0; ND < 4; ++ND)
		{
			if (ND == (Dir + 2) % 4)
			{
				continue;
			}
			const FIntPoint Q = P + Dirs[ND];
			if (Q.X <= 0 || Q.Y <= 0 || Q.X >= Width - 1 || Q.Y >= Height - 1)
			{
				continue;
			}
			const int32 C = Get(Q);
			if (C >= 0 && C != RoomA && C != RoomB)
			{
				continue;
			}

			float Step = 1.f;
			if (C == RoomA || C == RoomB)
			{
				Step = 0.25f;
			}
			else if (C == Corridor)
			{
				Step = 0.5f;
			}
			else if (TouchesForeignRoom(Q))
			{
				Step += 4.f;
			}

			if (ND != Dir)
			{
				Step += C >= 0 ? 0.05f : 1.0f;
			}

			// Prefer reusing an existing doorway instead of punching a new hole next to it.
			if (IsRoom(P) != IsRoom(Q) && !IsDoorway(P, Q))
			{
				Step += 3.f;
			}

			const int32 NS = (Q.Y * Width + Q.X) * 4 + ND;
			const float NC = E.Cost + Step;
			if (NC < Dist[NS])
			{
				Dist[NS] = NC;
				Prev[NS] = E.State;
				Heap.HeapPush({ NC, NS }, Pred);
			}
		}
	}

	if (Goal == INDEX_NONE)
	{
		return false;
	}

	TArray<FIntPoint> Path;
	for (int32 S = Goal; S != INDEX_NONE; S = Prev[S])
	{
		const int32 CellIdx = S / 4;
		Path.Add(FIntPoint(CellIdx % Width, CellIdx / Width));
	}
	Algo::Reverse(Path);

	for (int32 i = 0; i < Path.Num(); ++i)
	{
		if (Get(Path[i]) == Empty)
		{
			Set(Path[i], Corridor);
		}
		if (i > 0 && IsRoom(Path[i - 1]) != IsRoom(Path[i]))
		{
			Doorways.Add(EdgeKey(Path[i - 1], Path[i]));
		}
	}
	return true;
}

void FDungeonLayout::AssignThemes(FRandomStream& Rng)
{
	const int32 Num = Rooms.Num();

	// Start room: the one closest to the bottom of the screen (camera looks toward +X).
	StartRoom = 0;
	for (int32 i = 1; i < Num; ++i)
	{
		if (Rooms[i].Area() > 0 && GetRoomCenter(i).X < GetRoomCenter(StartRoom).X)
		{
			StartRoom = i;
		}
	}

	// Treasure room: the furthest room in graph hops from the start.
	TArray<int32> Hops;
	Hops.Init(INDEX_NONE, Num);
	Hops[StartRoom] = 0;
	TArray<int32> Queue = { StartRoom };
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 Cur = Queue[Head];
		for (const FIntPoint& C : Connections)
		{
			const int32 Other = C.X == Cur ? C.Y : (C.Y == Cur ? C.X : INDEX_NONE);
			if (Other != INDEX_NONE && Hops[Other] == INDEX_NONE)
			{
				Hops[Other] = Hops[Cur] + 1;
				Queue.Add(Other);
			}
		}
	}

	TreasureRoom = INDEX_NONE;
	double BestScore = -1.0;
	for (int32 i = 0; i < Num; ++i)
	{
		if (i == StartRoom || Hops[i] == INDEX_NONE)
		{
			continue;
		}
		const double Score = Hops[i] * 100000.0 + FVector::Dist(GetRoomCenter(i), GetRoomCenter(StartRoom));
		if (Score > BestScore)
		{
			BestScore = Score;
			TreasureRoom = i;
		}
	}

	TArray<int32> Remaining;
	for (int32 i = 0; i < Num; ++i)
	{
		if (i != StartRoom && i != TreasureRoom && Rooms[i].Area() > 0)
		{
			Remaining.Add(i);
		}
	}
	Remaining.Sort([this](int32 A, int32 B) { return Rooms[A].Area() > Rooms[B].Area(); });

	TArray<EDungeonRoomTheme> Pool = { EDungeonRoomTheme::Library, EDungeonRoomTheme::Dining, EDungeonRoomTheme::Storage, EDungeonRoomTheme::Crypt };
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		Pool.Swap(i, Rng.RandRange(0, i));
	}

	int32 Halls = 0;
	int32 PoolIndex = 0;
	for (int32 Index : Remaining)
	{
		FDungeonRoom& Room = Rooms[Index];
		if (Halls < 2 && Room.Area() >= 25 && FMath::Min(Room.Size.X, Room.Size.Y) >= 5)
		{
			Room.Theme = EDungeonRoomTheme::Hall;
			++Halls;
		}
		else
		{
			Room.Theme = Pool[PoolIndex++ % Pool.Num()];
		}
	}

	Rooms[StartRoom].Theme = EDungeonRoomTheme::Start;
	if (TreasureRoom != INDEX_NONE)
	{
		Rooms[TreasureRoom].Theme = EDungeonRoomTheme::Treasure;
	}
}
