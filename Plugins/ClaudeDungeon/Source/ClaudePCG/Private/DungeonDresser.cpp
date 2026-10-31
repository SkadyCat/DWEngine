#include "DungeonDresser.h"

#include "Data/PCGPointData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

namespace
{
	constexpr double Cell = FDungeonLayout::CellSize;
	constexpr double Tile = FDungeonLayout::TileSize;
	constexpr double Tier = FDungeonLayout::TierHeight;
	constexpr double WallHalfThickness = 9.4;

	FTransform MakeXf(const FVector& Loc, double Yaw, const FVector& Scale = FVector::OneVector)
	{
		return FTransform(FRotator(0.0, Yaw, 0.0), Loc, Scale);
	}

	FVector AxisFromYaw(double Yaw)
	{
		const double R = FMath::DegreesToRadians(Yaw);
		return FVector(FMath::RoundHalfFromZero(FMath::Cos(R) * 1000.0) / 1000.0, FMath::RoundHalfFromZero(FMath::Sin(R) * 1000.0) / 1000.0, 0.0);
	}

	struct FWeighted
	{
		const TCHAR* Mesh;
		float Weight;
		double Radius;
		double ZOffset;
	};

	const FWeighted* PickWeighted(const TArray<FWeighted>& Items, FRandomStream& Rng)
	{
		float Total = 0.f;
		for (const FWeighted& I : Items) { Total += I.Weight; }
		float R = Rng.FRand() * Total;
		for (const FWeighted& I : Items)
		{
			R -= I.Weight;
			if (R <= 0.f) { return &I; }
		}
		return &Items.Last();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Point sink
// ---------------------------------------------------------------------------------------------------------------------

void FDungeonPointSink::Init(const FTransform& InActorTransform, bool bWithMeshAttribute)
{
	ActorTransform = InActorTransform;
	Data = NewObject<UPCGPointData>();
	MeshAttribute = bWithMeshAttribute ? Data->Metadata->CreateAttribute<FSoftObjectPath>(TEXT("Mesh"), FSoftObjectPath(), false, false) : nullptr;
}

void FDungeonPointSink::Add(const FTransform& Local, const FSoftObjectPath& Mesh, const FVector& Extents, float Density)
{
	FPCGPoint& Point = Data->GetMutablePoints().Emplace_GetRef();
	Point.Transform = Local * ActorTransform;
	Point.Density = Density;
	Point.BoundsMin = -Extents;
	Point.BoundsMax = Extents;
	Point.Steepness = 1.f;
	Point.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Point.Transform.GetLocation());
	if (MeshAttribute)
	{
		Data->Metadata->InitializeOnSet(Point.MetadataEntry);
		MeshAttribute->SetValue(Point.MetadataEntry, Mesh);
	}
}

int32 FDungeonPointSink::Num() const
{
	return Data ? Data->GetPoints().Num() : 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Dresser
// ---------------------------------------------------------------------------------------------------------------------

FDungeonDresser::FDungeonDresser(const FDungeonLayout& InLayout, const FDungeonGenerationParams& InParams, const FDungeonMeshSet& InMeshes, const FTransform& InActorTransform)
	: Layout(InLayout)
	, Params(InParams)
	, Meshes(InMeshes)
	, ActorTransform(InActorTransform)
	, Rng(InParams.Seed * 7919 + 17)
{
}

FSoftObjectPath FDungeonDresser::Kit(const TCHAR* Name)
{
	return FSoftObjectPath(FString::Printf(TEXT("/Game/ClaudeDungeonKit/Meshes/%s.%s"), Name, Name));
}

void FDungeonDresser::Build()
{
	Floors.Init(ActorTransform, true);
	Walls.Init(ActorTransform, true);
	Trim.Init(ActorTransform, true);
	Props.Init(ActorTransform, true);
	Clutter.Init(ActorTransform, true);
	Debris.Init(ActorTransform, true);
	Torches.Init(ActorTransform, false);
	Candelabras.Init(ActorTransform, false);

	CollectFaces();
	EmitFloors();
	EmitWalls();
	EmitColumns();
	DressRooms();
	DressCorridors();
	EmitTorches();
	EmitDebris();
}

bool FDungeonDresser::IsEdgeCut(const FIntPoint& A, const FIntPoint& B) const
{
	if (!Params.bCutawayFrontWalls || A.X == B.X)
	{
		return false;
	}
	// The camera looks toward +X: a wall whose +X side is walkable sits between the camera and that floor.
	const FIntPoint PlusX = A.X > B.X ? A : B;
	return Layout.IsFloor(PlusX);
}

int32 FDungeonDresser::EdgeTiers(const FIntPoint& A, const FIntPoint& B) const
{
	const int32 Front = FMath::Clamp(Params.FrontWallTiers, 0, Params.WallTiers);
	if (IsEdgeCut(A, B))
	{
		return Front;
	}
	if (!Params.bCutawayFrontWalls || A.X != B.X)
	{
		return Params.WallTiers;
	}

	// Walls running along X step down one tier per cell toward a lowered front wall. Steps always land on
	// grid vertices, where a column closes them off.
	const int32 LineJ = FMath::Max(A.Y, B.Y);
	for (int32 D = 0; D < Params.WallTiers - Front; ++D)
	{
		if (VertexTouchesCut(A.X - D, LineJ) || VertexTouchesCut(A.X + 1 + D, LineJ))
		{
			return Front + D;
		}
	}
	return Params.WallTiers;
}

bool FDungeonDresser::HasArch(const FIntPoint& A, const FIntPoint& B) const
{
	// The arch mesh is two tiers tall: anywhere the wall has been lowered below that, leave an open gap instead.
	return !IsEdgeCut(A, B) && EdgeTiers(A, B) >= 2;
}

double FDungeonDresser::ColumnScale(int32 Tiers)
{
	// Uniform scale keeps the capital's proportions; the top lands flush with the wall-top trim.
	return (Tiers * Tier + 10.0) / 600.0;
}

bool FDungeonDresser::VertexTouchesCut(int32 I, int32 J) const
{
	if (!Params.bCutawayFrontWalls)
	{
		return false;
	}
	// The two edges running along Y through this vertex are the only ones that can be camera-facing cuts.
	const FIntPoint A1(I - 1, J), B1(I, J);
	const FIntPoint A2(I - 1, J - 1), B2(I, J - 1);
	return (Layout.HasBoundary(A1, B1) && IsEdgeCut(A1, B1)) || (Layout.HasBoundary(A2, B2) && IsEdgeCut(A2, B2));
}

void FDungeonDresser::CollectFaces()
{
	Faces.Reset();
	for (int32 Y = 0; Y < Layout.Height; ++Y)
	{
		for (int32 X = 0; X < Layout.Width; ++X)
		{
			const FIntPoint P(X, Y);
			if (!Layout.IsFloor(P))
			{
				continue;
			}
			for (int32 D = 0; D < 4; ++D)
			{
				const FIntPoint Q = P + FDungeonLayout::Dirs[D];
				if (!Layout.HasBoundary(P, Q))
				{
					continue;
				}
				FFace F;
				F.Cell = P;
				F.Neighbor = Q;
				const FVector Dir(FDungeonLayout::Dirs[D].X, FDungeonLayout::Dirs[D].Y, 0.0);
				F.Center = FDungeonLayout::CellCenter(P) + Dir * (Cell * 0.5);
				F.Normal = -Dir;
				F.Yaw = YawFromNormal(F.Normal);
				F.Along = AxisFromYaw(F.Yaw);
				F.bDoorway = Layout.IsFloor(Q) && Layout.IsDoorway(P, Q);
				F.bCut = IsEdgeCut(P, Q);
				F.Tiers = EdgeTiers(P, Q);
				F.bArch = F.bDoorway && HasArch(P, Q);
				F.bOwnsMesh = !Layout.IsFloor(Q) || Layout.IsRoom(P);
				F.Region = Layout.Get(P);
				Faces.Add(F);
			}
		}
	}
}

void FDungeonDresser::EmitFloors()
{
	for (int32 Y = 0; Y < Layout.Height; ++Y)
	{
		for (int32 X = 0; X < Layout.Width; ++X)
		{
			if (!Layout.IsFloor(FIntPoint(X, Y)))
			{
				continue;
			}
			for (int32 J = 0; J < 2; ++J)
			{
				for (int32 I = 0; I < 2; ++I)
				{
					// Random quarter turns break up the tiling pattern of the floor texture.
					const FVector TileCenter(X * Cell + Tile * (I + 0.5), Y * Cell + Tile * (J + 0.5), 0.0);
					const double Yaw = 90.0 * Rng.RandRange(0, 3);
					const FVector Pivot = TileCenter - FRotator(0.0, Yaw, 0.0).RotateVector(FVector(Tile * 0.5, Tile * 0.5, 0.0));
					Floors.Add(MakeXf(Pivot, Yaw), Meshes.Floor, FVector(Tile * 0.5, Tile * 0.5, 1.0));
				}
			}
		}
	}
}

void FDungeonDresser::EmitWalls()
{
	const FVector Up(0.0, 0.0, Tier);
	for (const FFace& F : Faces)
	{
		if (F.bOwnsMesh)
		{
			if (F.bDoorway)
			{
				if (F.bArch)
				{
					Walls.Add(MakeXf(F.Center - F.Along * (Cell * 0.5), F.Yaw), Meshes.Arch, FVector(200.0, 40.0, 200.0));
					for (int32 S = -1; S <= 1; S += 2)
					{
						const FVector Seg = F.Center + F.Along * (Tile * 0.5 * S);
						for (int32 T = 2; T < F.Tiers; ++T)
						{
							Walls.Add(MakeXf(Seg - F.Along * (Tile * 0.5) + Up * T, F.Yaw), Meshes.Wall, FVector(100.0, 10.0, 100.0));
						}
						if (F.Tiers > 2)
						{
							Trim.Add(MakeXf(Seg - F.Along * (Tile * 0.5) - F.Normal * 10.0 + Up * F.Tiers, F.Yaw), Meshes.Trim, FVector(100.0, 10.0, 5.0));
						}
					}
				}
				continue;
			}

			for (int32 S = -1; S <= 1; S += 2)
			{
				const FVector Seg = F.Center + F.Along * (Tile * 0.5 * S);
				for (int32 T = 0; T < F.Tiers; ++T)
				{
					Walls.Add(MakeXf(Seg - F.Along * (Tile * 0.5) + Up * T, F.Yaw), Meshes.Wall, FVector(100.0, 10.0, 100.0));
				}
				if (F.Tiers > 0)
				{
					// Cap the wall top so the cut reads cleanly from above.
					Trim.Add(MakeXf(Seg - F.Along * (Tile * 0.5) - F.Normal * 10.0 + Up * F.Tiers, F.Yaw), Meshes.Trim, FVector(100.0, 10.0, 5.0));
				}
			}
		}

		if (!F.bDoorway)
		{
			// Skirting board on every walkable side of a wall.
			for (int32 S = -1; S <= 1; S += 2)
			{
				const FVector Seg = F.Center + F.Along * (Tile * 0.5 * S);
				Trim.Add(MakeXf(Seg - F.Along * (Tile * 0.5) + F.Normal * WallHalfThickness, F.Yaw), Meshes.Trim, FVector(100.0, 10.0, 5.0));
			}
		}
	}
}

void FDungeonDresser::EmitColumns()
{
	auto EdgeInfo = [this](const FIntPoint& A, const FIntPoint& B, int32& OutTiers) -> bool
	{
		if (!Layout.HasBoundary(A, B))
		{
			return false;
		}
		const bool bDoor = Layout.IsFloor(A) && Layout.IsFloor(B) && Layout.IsDoorway(A, B);
		if (bDoor && !HasArch(A, B))
		{
			return false;
		}
		OutTiers = EdgeTiers(A, B);
		return true;
	};

	for (int32 J = 0; J <= Layout.Height; ++J)
	{
		for (int32 I = 0; I <= Layout.Width; ++I)
		{
			int32 TE = 0, TW = 0, TN = 0, TS = 0;
			const bool E = EdgeInfo(FIntPoint(I, J - 1), FIntPoint(I, J), TE);
			const bool W = EdgeInfo(FIntPoint(I - 1, J - 1), FIntPoint(I - 1, J), TW);
			const bool N = EdgeInfo(FIntPoint(I - 1, J), FIntPoint(I, J), TN);
			const bool S = EdgeInfo(FIntPoint(I - 1, J - 1), FIntPoint(I, J - 1), TS);
			const int32 Count = int32(E) + int32(W) + int32(N) + int32(S);
			if (Count == 0)
			{
				continue;
			}
			const bool bStraight = (Count == 2) && ((E && W && TE == TW) || (N && S && TN == TS));
			if (bStraight)
			{
				continue;
			}
			// The column closes the tallest wall end meeting here (corners, T-junctions, cutaway steps, arch jambs).
			const int32 H = FMath::Max(FMath::Max(TE, TW), FMath::Max(TN, TS));
			if (H <= 0)
			{
				continue;
			}
			const FVector V(I * Cell, J * Cell, 0.0);
			const double Sc = ColumnScale(H);
			Walls.Add(MakeXf(V, 90.0 * Rng.RandRange(0, 3), FVector(Sc)), Meshes.Column, FVector(52.0 * Sc, 52.0 * Sc, 300.0 * Sc));
			Reserve(V, 60.0);
		}
	}
}

void FDungeonDresser::EmitTorches()
{
	for (const FFace& F : Faces)
	{
		if (F.bDoorway || F.bCut || F.Tiers < 2)
		{
			continue;
		}
		const FIntPoint Dir = F.Neighbor - F.Cell;
		const FIntPoint AlongInt(FMath::RoundToInt(F.Along.X), FMath::RoundToInt(F.Along.Y));

		// Never crowd an archway.
		bool bNearDoor = false;
		for (int32 S = -1; S <= 1; S += 2)
		{
			const FIntPoint P2 = F.Cell + AlongInt * S;
			if (Layout.IsFloor(P2) && Layout.IsFloor(P2 + Dir) && Layout.IsDoorway(P2, P2 + Dir))
			{
				bNearDoor = true;
			}
		}
		if (bNearDoor)
		{
			continue;
		}

		const bool bRoom = F.Region >= 0;
		const int32 Index = Dir.X != 0 ? F.Cell.Y : F.Cell.X;
		const int32 Spacing = bRoom ? Params.TorchSpacing : Params.TorchSpacing + 1;
		const int32 Phase = bRoom ? F.Region : (Dir.X + Dir.Y > 0 ? 1 : 0);
		if (((Index + Phase) % FMath::Max(1, Spacing)) != 0)
		{
			continue;
		}

		const FVector Loc = F.Center + F.Normal * WallHalfThickness + FVector(0.0, 0.0, 175.0);
		Torches.Add(MakeXf(Loc, F.Yaw), FSoftObjectPath(), FVector(15.0, 25.0, 50.0));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Placement helpers
// ---------------------------------------------------------------------------------------------------------------------

FBox2D FDungeonDresser::RoomInner(const FDungeonRoom& Room) const
{
	return FBox2D(FVector2D(Room.Min.X * Cell + WallHalfThickness, Room.Min.Y * Cell + WallHalfThickness),
		FVector2D(Room.Max().X * Cell - WallHalfThickness, Room.Max().Y * Cell - WallHalfThickness));
}

FVector FDungeonDresser::RoomCornerInset(const FDungeonRoom& Room, int32 Corner, double Inset) const
{
	const FBox2D B = RoomInner(Room);
	const double X = (Corner == 1 || Corner == 2) ? B.Max.X - Inset : B.Min.X + Inset;
	const double Y = (Corner >= 2) ? B.Max.Y - Inset : B.Min.Y + Inset;
	return FVector(X, Y, 0.0);
}

bool FDungeonDresser::IsFree(const FVector& P, double Radius) const
{
	const FVector2D P2(P);
	if (Layout.Rooms.IsValidIndex(CurrentRoomId))
	{
		// Only the anchor must be inside the room; wall-hugging props are positioned explicitly by their callers.
		const FBox2D B = RoomInner(Layout.Rooms[CurrentRoomId]).ExpandBy(-FMath::Min(Radius, 8.0));
		if (!B.IsInside(P2))
		{
			return false;
		}
	}
	for (const FCircle& C : KeepOut)
	{
		if (FVector2D::Distance(C.P, P2) < C.R + Radius)
		{
			return false;
		}
	}
	for (const FCircle& C : Occupied)
	{
		if (FVector2D::Distance(C.P, P2) < (C.R + Radius) * 0.92)
		{
			return false;
		}
	}
	return true;
}

bool FDungeonDresser::PlaceProp(const TCHAR* Mesh, const FVector& P, double Yaw, double Radius, bool bCollidable, const FVector& Scale, bool bForce)
{
	if (!bForce && !IsFree(P, Radius))
	{
		return false;
	}
	(bCollidable ? Props : Clutter).Add(MakeXf(P, Yaw, Scale), Kit(Mesh), FVector(Radius, Radius, 50.0));
	Reserve(P, Radius);
	return true;
}

void FDungeonDresser::AddClutter(const TCHAR* Mesh, const FVector& P, const FRotator& Rot, const FVector& Scale)
{
	Clutter.Add(FTransform(Rot, P, Scale), Kit(Mesh), FVector(10.0));
}

void FDungeonDresser::PlaceCandleCluster(const FVector& P, int32 Count, double Spread)
{
	static const TCHAR* Candles[] = { TEXT("SM_CandleSmall"), TEXT("SM_CandleMedium"), TEXT("SM_CandleLarge") };
	for (int32 i = 0; i < Count; ++i)
	{
		const double Angle = Rng.FRandRange(0.0, UE_TWO_PI);
		const double Dist = i == 0 ? 0.0 : Rng.FRandRange(Spread * 0.35, Spread);
		const FVector Offset(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.0);
		const double S = Rng.FRandRange(0.9, 1.5);
		AddClutter(Candles[Rng.RandRange(0, 2)], P + Offset, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0), FVector(S, S, S * Rng.FRandRange(0.8, 1.6)));
	}
}

void FDungeonDresser::PlaceCandelabra(const FVector& P)
{
	if (IsFree(P, 40.0))
	{
		Candelabras.Add(MakeXf(P, Rng.FRandRange(0.0, 360.0)), FSoftObjectPath(), FVector(32.0, 32.0, 90.0));
		Reserve(P, 40.0);
	}
}

void FDungeonDresser::FillShelf(const FVector& Pivot, const FVector& Along, const FVector& Normal, double Yaw)
{
	static const TCHAR* Books[] = { TEXT("SM_BookA"), TEXT("SM_BookB"), TEXT("SM_BookC") };
	static const double Boards[] = { 9.6, 45.4, 82.4, 120.4, 158.4 };
	for (double Board : Boards)
	{
		const float Roll = Rng.FRand();
		if (Roll < 0.12f)
		{
			continue;
		}
		if (Roll < 0.22f)
		{
			// A skull or a few candles instead of books.
			const FVector P = Pivot + Along * Rng.FRandRange(-40.0, 40.0) + Normal * 16.0 + FVector(0.0, 0.0, Board);
			if (Rng.FRand() < 0.5f)
			{
				AddClutter(TEXT("SM_Skull"), P, FRotator(0.0, Yaw + Rng.FRandRange(-30.0, 30.0), 0.0));
			}
			else
			{
				PlaceCandleCluster(P, 3, 14.0);
			}
			continue;
		}

		double X = -63.0 + Rng.FRandRange(0.0, 8.0);
		while (X < 63.0)
		{
			if (Rng.FRand() < 0.08f)
			{
				X += Rng.FRandRange(8.0, 22.0);
				continue;
			}
			const double Lean = Rng.FRand() < 0.1f ? Rng.FRandRange(-14.0, 14.0) : Rng.FRandRange(-2.0, 2.0);
			const double Height = Rng.FRandRange(0.85, 1.08);
			const FVector P = Pivot + Along * X + Normal * Rng.FRandRange(14.0, 18.0) + FVector(0.0, 0.0, Board + 11.6 * Height);
			AddClutter(Books[Rng.RandRange(0, 2)], P, FRotator(0.0, Yaw - 90.0, 90.0 + Lean), FVector(Rng.FRandRange(0.9, 1.05), Height, 1.0));
			X += 3.9;
		}
	}
}

void FDungeonDresser::SetTable(const FVector& Center, double Yaw, bool bDining)
{
	PlaceProp(TEXT("SM_Table"), Center, Yaw, 105.0, true, FVector::OneVector, true);
	const FVector Along = AxisFromYaw(Yaw);
	const FVector Side = AxisFromYaw(Yaw + 90.0);
	const FVector Top(0.0, 0.0, 90.0);

	if (bDining)
	{
		for (int32 S = -1; S <= 1; S += 2)
		{
			const FVector BenchPos = Center + Side * (98.0 * S);
			PlaceProp(TEXT("SM_Bench"), BenchPos + Along * Rng.FRandRange(-10.0, 10.0), Yaw + Rng.FRandRange(-3.0, 3.0), 40.0, true, FVector::OneVector, true);
			Reserve(BenchPos + Along * 70.0, 35.0);
			Reserve(BenchPos - Along * 70.0, 35.0);
		}
		for (int32 K = -1; K <= 1; ++K)
		{
			for (int32 S = -1; S <= 1; S += 2)
			{
				if (Rng.FRand() < 0.2f)
				{
					continue;
				}
				const FVector Base = Center + Along * (K * 55.0 + Rng.FRandRange(-6.0, 6.0)) + Side * (30.0 * S) + Top;
				AddClutter(TEXT("SM_Plate"), Base, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
				if (Rng.FRand() < 0.45f)
				{
					AddClutter(TEXT("SM_Bowl"), Base + FVector(0.0, 0.0, 1.5), FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
				}
				if (Rng.FRand() < 0.6f)
				{
					AddClutter(TEXT("SM_Pint"), Base - Side * (17.0 * S) + Along * 14.0, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
				}
				if (Rng.FRand() < 0.5f)
				{
					AddClutter(TEXT("SM_Spoon"), Base + Along * 17.0, FRotator(0.0, Yaw + 90.0 + Rng.FRandRange(-20.0, 20.0), 0.0));
				}
			}
		}
		PlaceCandleCluster(Center + Top, 3, 10.0);
		for (int32 S = -1; S <= 1; S += 2)
		{
			if (Rng.FRand() < 0.5f)
			{
				PlaceProp(TEXT("SM_Stool"), Center + Along * (150.0 * S), Rng.FRandRange(0.0, 360.0), 30.0);
			}
		}
	}
	else
	{
		static const TCHAR* Books[] = { TEXT("SM_BookA"), TEXT("SM_BookB"), TEXT("SM_BookC") };
		const int32 Stacks = Rng.RandRange(2, 4);
		for (int32 i = 0; i < Stacks; ++i)
		{
			const FVector Base = Center + Along * Rng.FRandRange(-65.0, 65.0) + Side * Rng.FRandRange(-30.0, 30.0) + Top;
			const int32 Height = Rng.RandRange(1, 4);
			const double BaseYaw = Rng.FRandRange(0.0, 360.0);
			for (int32 H = 0; H < Height; ++H)
			{
				AddClutter(Books[Rng.RandRange(0, 2)], Base + FVector(Rng.FRandRange(-2.0, 2.0), Rng.FRandRange(-2.0, 2.0), 1.8 + H * 3.6), FRotator(0.0, BaseYaw + Rng.FRandRange(-15.0, 15.0), 0.0));
			}
		}
		PlaceCandleCluster(Center + Along * Rng.FRandRange(-40.0, 40.0) + Top, 2, 8.0);
		if (Rng.FRand() < 0.4f)
		{
			AddClutter(TEXT("SM_Skull"), Center + Along * Rng.FRandRange(-60.0, 60.0) + Side * 20.0 + Top, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
		}
		for (int32 S = -1; S <= 1; S += 2)
		{
			if (Rng.FRand() < 0.7f)
			{
				PlaceProp(TEXT("SM_Stool"), Center + Side * (85.0 * S) + Along * Rng.FRandRange(-40.0, 40.0), Rng.FRandRange(0.0, 360.0), 28.0);
			}
		}
	}
}

TArray<FDungeonDresser::FSlot> FDungeonDresser::GatherWallSlots(const FDungeonRoom& Room) const
{
	TArray<FSlot> Slots;
	for (const FFace& F : Faces)
	{
		if (F.Region != Room.Id || F.bDoorway)
		{
			continue;
		}
		for (int32 S = -1; S <= 1; S += 2)
		{
			FSlot Slot;
			Slot.Pos = F.Center + F.Along * (Tile * 0.5 * S) + F.Normal * WallHalfThickness;
			Slot.Normal = F.Normal;
			Slot.Along = F.Along;
			Slot.Yaw = F.Yaw;
			Slot.bTallWall = F.Tiers >= 2 && !F.bCut;
			Slots.Add(Slot);
		}
	}
	return Slots;
}

void FDungeonDresser::Shuffle(TArray<FSlot>& Slots)
{
	for (int32 i = Slots.Num() - 1; i > 0; --i)
	{
		Slots.Swap(i, Rng.RandRange(0, i));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Rooms
// ---------------------------------------------------------------------------------------------------------------------

void FDungeonDresser::DressRooms()
{
	for (const FDungeonRoom& Room : Layout.Rooms)
	{
		if (Room.Area() > 0)
		{
			DressRoom(Room);
		}
	}
	CurrentRoomId = INDEX_NONE;
	KeepOut.Reset();
}

void FDungeonDresser::DressRoom(const FDungeonRoom& Room)
{
	CurrentRoomId = Room.Id;
	KeepOut.Reset();
	for (const FFace& F : Faces)
	{
		if (F.Region == Room.Id && F.bDoorway)
		{
			KeepOut.Add({ FVector2D(F.Center + F.Normal * 160.0), 200.0 });
		}
	}

	TArray<FSlot> Slots = GatherWallSlots(Room);
	Shuffle(Slots);

	switch (Room.Theme)
	{
	case EDungeonRoomTheme::Start:    DressStart(Room, Slots); break;
	case EDungeonRoomTheme::Hall:     DressHall(Room, Slots); break;
	case EDungeonRoomTheme::Library:  DressLibrary(Room, Slots); break;
	case EDungeonRoomTheme::Dining:   DressDining(Room, Slots); break;
	case EDungeonRoomTheme::Storage:  DressStorage(Room, Slots); break;
	case EDungeonRoomTheme::Crypt:    DressCrypt(Room, Slots); break;
	case EDungeonRoomTheme::Treasure: DressTreasure(Room, Slots); break;
	}
}

void FDungeonDresser::DressStart(const FDungeonRoom& Room, TArray<FSlot>& Slots)
{
	const FVector Center = Layout.GetRoomCenter(Room.Id);
	KeepOut.Add({ FVector2D(Center), 170.0 });

	const int32 C0 = Rng.RandRange(0, 3);
	PlaceCandelabra(RoomCornerInset(Room, C0, 110.0));
	PlaceCandelabra(RoomCornerInset(Room, (C0 + 2) % 4, 110.0));

	const FVector Stash = RoomCornerInset(Room, (C0 + 1) % 4, 60.0);
	PlaceProp(TEXT("SM_Barrel"), Stash, Rng.FRandRange(0.0, 360.0), 42.0);
	PlaceProp(TEXT("SM_CrateMedium"), Stash + (Center - Stash).GetSafeNormal2D() * 110.0, Rng.FRandRange(-15.0, 15.0), 45.0);
	PlaceCandleCluster(RoomCornerInset(Room, (C0 + 3) % 4, 45.0), 5, 30.0);
	Reserve(RoomCornerInset(Room, (C0 + 3) % 4, 45.0), 35.0);

	int32 Placed = 0;
	for (const FSlot& S : Slots)
	{
		if (Placed >= 3) break;
		if (Rng.FRand() < 0.5f)
		{
			Placed += PlaceProp(TEXT("SM_Barrel"), S.Pos + S.Normal * 45.0, Rng.FRandRange(0.0, 360.0), 42.0) ? 1 : 0;
		}
		else
		{
			Placed += PlaceProp(TEXT("SM_Bench"), S.Pos + S.Normal * 35.0, S.Yaw, 60.0) ? 1 : 0;
		}
	}
}

void FDungeonDresser::DressHall(const FDungeonRoom& Room, TArray<FSlot>& Slots)
{
	const bool bLongX = Room.Size.X >= Room.Size.Y;
	const int32 LongMin = bLongX ? Room.Min.X : Room.Min.Y;
	const int32 LongSize = bLongX ? Room.Size.X : Room.Size.Y;
	const int32 ShortMin = bLongX ? Room.Min.Y : Room.Min.X;
	const int32 ShortSize = bLongX ? Room.Size.Y : Room.Size.X;

	const int32 Count = FMath::Max(2, LongSize / 2);
	const double A = (LongMin + 1) * Cell;
	const double B = (LongMin + LongSize - 1) * Cell;
	const double Rows[2] = { (ShortMin + 1) * Cell, (ShortMin + ShortSize - 1) * Cell };
	const double ColumnS = ColumnScale(Params.WallTiers);
	const double ShortCenter = (ShortMin + ShortSize * 0.5) * Cell;

	auto ToWorld = [bLongX](double L, double S) { return bLongX ? FVector(L, S, 0.0) : FVector(S, L, 0.0); };

	TArray<double> LongPositions;
	for (int32 K = 0; K < Count; ++K)
	{
		LongPositions.Add(FMath::Lerp(A, B, Count == 1 ? 0.5 : double(K) / double(Count - 1)));
	}

	for (double L : LongPositions)
	{
		for (double S : Rows)
		{
			// Medium column: slim enough not to wall off the top-down camera, uniformly scaled so the capital stays round.
			PlaceProp(TEXT("SM_ColumnMedium"), ToWorld(L, S), 90.0 * Rng.RandRange(0, 3), 60.0, true, FVector(ColumnS), true);
		}
	}
	// Altar at one end of the nave, flanked by candelabras.
	const bool bAltarAtMax = Rng.FRand() < 0.5f;
	const double AltarL = bAltarAtMax ? (LongMin + LongSize) * Cell - 190.0 : LongMin * Cell + 190.0;
	const double Facing = bAltarAtMax ? -1.0 : 1.0;
	PlaceProp(TEXT("SM_Basin"), ToWorld(AltarL, ShortCenter), Rng.FRandRange(0.0, 360.0), 60.0, true, FVector(1.5), true);
	PlaceCandelabra(ToWorld(AltarL, ShortCenter - 180.0));
	PlaceCandelabra(ToWorld(AltarL, ShortCenter + 180.0));
	for (int32 K = -2; K <= 2; ++K)
	{
		PlaceCandleCluster(ToWorld(AltarL + Facing * 95.0, ShortCenter + K * 42.0), 2, 9.0);
	}
	Reserve(ToWorld(AltarL + Facing * 95.0, ShortCenter), 90.0);

	// Pews between the colonnade, split by a central aisle.
	const double PewYaw = bLongX ? 90.0 : 0.0;
	for (int32 K = 0; K + 1 < LongPositions.Num(); ++K)
	{
		const double Mid = (LongPositions[K] + LongPositions[K + 1]) * 0.5;
		if (FMath::Abs(Mid - AltarL) < 380.0)
		{
			continue;
		}
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			for (double Row : { -70.0, 70.0 })
			{
				PlaceProp(TEXT("SM_Bench"), ToWorld(Mid + Row, ShortCenter + Side * 150.0), PewYaw + Rng.FRandRange(-2.0, 2.0), 52.0);
			}
		}
	}

	for (const FSlot& S : Slots)
	{
		const bool bAlongLong = bLongX ? FMath::Abs(S.Normal.Y) > 0.5 : FMath::Abs(S.Normal.X) > 0.5;
		if (bAlongLong && Rng.FRand() < 0.5f)
		{
			PlaceProp(TEXT("SM_Bench"), S.Pos + S.Normal * 35.0, S.Yaw, 60.0);
		}
		else if (Rng.FRand() < 0.12f)
		{
			PlaceProp(TEXT("SM_Jar"), S.Pos + S.Normal * 30.0, Rng.FRandRange(0.0, 360.0), 20.0);
		}
	}
	for (int32 C = 0; C < 4; ++C)
	{
		const FVector P = RoomCornerInset(Room, C, 50.0);
		if (IsFree(P, 35.0))
		{
			PlaceCandleCluster(P, Rng.RandRange(3, 6), 32.0);
			Reserve(P, 35.0);
		}
	}
}

void FDungeonDresser::DressLibrary(const FDungeonRoom& Room, TArray<FSlot>& Slots)
{
	int32 Shelves = 0;
	for (const FSlot& S : Slots)
	{
		if (Shelves >= 16) break;
		const FVector Pivot = S.Pos + S.Normal * 2.0;
		if (!IsFree(Pivot + S.Normal * 18.0, 62.0))
		{
			continue;
		}
		Props.Add(MakeXf(Pivot, S.Yaw), Kit(TEXT("SM_Shelf")), FVector(70.0, 17.0, 100.0));
		Reserve(Pivot + S.Normal * 18.0, 62.0);
		FillShelf(Pivot, S.Along, S.Normal, S.Yaw);
		++Shelves;
	}

	const FVector Center = Layout.GetRoomCenter(Room.Id);
	const bool bLongX = Room.Size.X >= Room.Size.Y;
	const double Yaw = bLongX ? 0.0 : 90.0;
	const FVector LongAxis = AxisFromYaw(Yaw);
	const FVector ShortAxis = AxisFromYaw(Yaw + 90.0);
	const double LongLen = (bLongX ? Room.Size.X : Room.Size.Y) * Cell;
	const double ShortLen = (bLongX ? Room.Size.Y : Room.Size.X) * Cell;

	// Reading table in the middle.
	SetTable(Center, Yaw, false);
	PlaceCandelabra(Center + ShortAxis * 175.0);

	if (ShortLen >= 1600.0)
	{
		// Freestanding back-to-back stacks form aisles on both sides of the reading area.
		const double ShortOffset = ShortLen * 0.25;
		for (double L = 430.0; L <= LongLen * 0.5 - 250.0; L += 300.0)
		{
			for (int32 SL = -1; SL <= 1; SL += 2)
			{
				for (int32 SS = -1; SS <= 1; SS += 2)
				{
					const FVector C = Center + LongAxis * (L * SL) + ShortAxis * (ShortOffset * SS);
					if (!IsFree(C, 80.0))
					{
						continue;
					}
					for (int32 FaceDir = -1; FaceDir <= 1; FaceDir += 2)
					{
						const FVector N = LongAxis * double(FaceDir);
						const double ShelfYaw = YawFromNormal(N);
						const FVector Pivot = C + N * 1.0;
						Props.Add(MakeXf(Pivot, ShelfYaw), Kit(TEXT("SM_Shelf")), FVector(70.0, 17.0, 100.0));
						FillShelf(Pivot, AxisFromYaw(ShelfYaw), N, ShelfYaw);
					}
					Reserve(C, 80.0);
				}
			}
		}
	}
	else if (LongLen >= 2000.0)
	{
		SetTable(Center + LongAxis * 300.0, Yaw, false);
		SetTable(Center - LongAxis * 300.0, Yaw, false);
	}

	static const TCHAR* Books[] = { TEXT("SM_BookA"), TEXT("SM_BookB"), TEXT("SM_BookC") };
	const FBox2D Inner = RoomInner(Room);
	for (int32 i = 0; i < 10; ++i)
	{
		const FVector P(Rng.FRandRange(Inner.Min.X + 60.0, Inner.Max.X - 60.0), Rng.FRandRange(Inner.Min.Y + 60.0, Inner.Max.Y - 60.0), 1.8);
		if (IsFree(P, 12.0))
		{
			AddClutter(Books[Rng.RandRange(0, 2)], P, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
		}
	}
}

void FDungeonDresser::DressDining(const FDungeonRoom& Room, TArray<FSlot>& Slots)
{
	const FVector Center = Layout.GetRoomCenter(Room.Id);
	const bool bLongX = Room.Size.X >= Room.Size.Y;
	const double Yaw = bLongX ? 0.0 : 90.0;
	const FVector Along = AxisFromYaw(Yaw);
	const FVector Side = AxisFromYaw(Yaw + 90.0);
	const double LongLen = FMath::Max(Room.Size.X, Room.Size.Y) * Cell;
	const double ShortLen = FMath::Min(Room.Size.X, Room.Size.Y) * Cell;

	const int32 NumTables = FMath::Clamp(int32((LongLen - 500.0) / 300.0) + 1, 1, 3);
	const int32 NumRows = ShortLen >= 2000.0 ? 2 : 1;
	for (int32 R = 0; R < NumRows; ++R)
	{
		const double SideOffset = NumRows == 1 ? 0.0 : (R == 0 ? -280.0 : 280.0);
		for (int32 T = 0; T < NumTables; ++T)
		{
			const double AlongOffset = (T - (NumTables - 1) * 0.5) * 300.0;
			SetTable(Center + Along * AlongOffset + Side * SideOffset, Yaw + Rng.FRandRange(-2.0, 2.0), true);
		}
	}

	for (int32 C = 0; C < 4; ++C)
	{
		const FVector P = RoomCornerInset(Room, C, 55.0);
		if (C % 2 == 0)
		{
			PlaceProp(TEXT("SM_Barrel"), P, Rng.FRandRange(0.0, 360.0), 42.0);
		}
		else
		{
			PlaceCandelabra(RoomCornerInset(Room, C, 80.0));
		}
	}

	int32 Shelves = 0;
	for (const FSlot& S : Slots)
	{
		if (Shelves >= 2) break;
		const FVector Pivot = S.Pos + S.Normal * 2.0;
		if (!IsFree(Pivot + S.Normal * 18.0, 62.0))
		{
			continue;
		}
		Props.Add(MakeXf(Pivot, S.Yaw), Kit(TEXT("SM_Shelf")), FVector(70.0, 17.0, 100.0));
		Reserve(Pivot + S.Normal * 18.0, 62.0);
		static const double Boards[] = { 9.6, 45.4, 82.4, 120.4, 158.4 };
		static const TCHAR* Ware[] = { TEXT("SM_Bowl"), TEXT("SM_Pint"), TEXT("SM_Plate"), TEXT("SM_CandleMedium") };
		for (double Board : Boards)
		{
			for (double X = -55.0; X <= 55.0; X += Rng.FRandRange(18.0, 30.0))
			{
				AddClutter(Ware[Rng.RandRange(0, 3)], Pivot + S.Along * X + S.Normal * 16.0 + FVector(0.0, 0.0, Board), FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
			}
		}
		++Shelves;
	}
	for (const FSlot& S : Slots)
	{
		if (Rng.FRand() < 0.15f)
		{
			PlaceProp(TEXT("SM_Barrel"), S.Pos + S.Normal * 45.0, Rng.FRandRange(0.0, 360.0), 42.0);
		}
	}
}

void FDungeonDresser::DressStorage(const FDungeonRoom& Room, TArray<FSlot>& Slots)
{
	const FVector Center = Layout.GetRoomCenter(Room.Id);
	for (int32 C = 0; C < 4; ++C)
	{
		const FVector Base = RoomCornerInset(Room, C, 62.0);
		const FVector In = (Center - Base).GetSafeNormal2D();
		const FVector Tangent(-In.Y, In.X, 0.0);
		if (PlaceProp(TEXT("SM_CrateLarge"), Base, Rng.FRandRange(-12.0, 12.0) + 90.0 * Rng.RandRange(0, 3), 60.0))
		{
			if (Rng.FRand() < 0.45f)
			{
				Props.Add(MakeXf(Base + FVector(0.0, 0.0, 144.6), Rng.FRandRange(0.0, 360.0)), Kit(TEXT("SM_CrateMedium")), FVector(40.0));
			}
		}
		PlaceProp(TEXT("SM_Barrel"), Base + In * 115.0 + Tangent * 40.0, Rng.FRandRange(0.0, 360.0), 42.0);
		PlaceProp(TEXT("SM_CrateMedium"), Base + In * 60.0 - Tangent * 85.0, Rng.FRandRange(0.0, 360.0), 45.0);
		PlaceProp(TEXT("SM_Box"), Base + In * 150.0 - Tangent * 30.0, Rng.FRandRange(0.0, 360.0), 28.0);
		PlaceProp(TEXT("SM_Jar"), Base + In * 30.0 + Tangent * 95.0, Rng.FRandRange(0.0, 360.0), 18.0);
	}

	for (const FSlot& S : Slots)
	{
		const float R = Rng.FRand();
		if (R < 0.35f)
		{
			PlaceProp(TEXT("SM_Barrel"), S.Pos + S.Normal * 45.0 + S.Along * 45.0, Rng.FRandRange(0.0, 360.0), 42.0);
			PlaceProp(TEXT("SM_Barrel"), S.Pos + S.Normal * 45.0 - S.Along * 45.0, Rng.FRandRange(0.0, 360.0), 42.0);
		}
		else if (R < 0.6f)
		{
			if (PlaceProp(TEXT("SM_CrateMedium"), S.Pos + S.Normal * 50.0, S.Yaw + Rng.FRandRange(-10.0, 10.0), 45.0) && Rng.FRand() < 0.4f)
			{
				Props.Add(MakeXf(S.Pos + S.Normal * 50.0 + FVector(0.0, 0.0, 73.7), S.Yaw + Rng.FRandRange(-25.0, 25.0)), Kit(TEXT("SM_Box")), FVector(26.0));
			}
		}
	}

	static const TCHAR* Planks[] = { TEXT("SM_PlankSmallA"), TEXT("SM_PlankMediumA"), TEXT("SM_PlankLargeA"), TEXT("SM_PlankSmallB"), TEXT("SM_PlankMediumB"), TEXT("SM_PlankLargeB") };
	const FBox2D Inner = RoomInner(Room);
	for (int32 i = 0; i < 12; ++i)
	{
		const FVector P(Rng.FRandRange(Inner.Min.X + 50.0, Inner.Max.X - 50.0), Rng.FRandRange(Inner.Min.Y + 50.0, Inner.Max.Y - 50.0), 0.0);
		if (IsFree(P, 30.0))
		{
			AddClutter(Planks[Rng.RandRange(0, 5)], P, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
		}
	}
	PlaceCandelabra(Center + FVector(Rng.FRandRange(-120.0, 120.0), Rng.FRandRange(-120.0, 120.0), 0.0));
}

void FDungeonDresser::DressCrypt(const FDungeonRoom& Room, TArray<FSlot>& Slots)
{
	const FVector Center = Layout.GetRoomCenter(Room.Id);
	PlaceProp(TEXT("SM_Basin"), Center, Rng.FRandRange(0.0, 360.0), 45.0, true, FVector(1.3), true);
	for (int32 K = 0; K < 4; ++K)
	{
		const FVector P = Center + AxisFromYaw(45.0 + 90.0 * K) * 95.0;
		PlaceCandleCluster(P, Rng.RandRange(2, 4), 14.0);
		Reserve(P, 18.0);
	}

	// Ruined colonnade.
	TArray<FVector> Spots;
	for (int32 Y = Room.Min.Y + 1; Y < Room.Max().Y; Y += 2)
	{
		for (int32 X = Room.Min.X + 1; X < Room.Max().X; X += 2)
		{
			Spots.Add(FVector(X * Cell, Y * Cell, 0.0));
		}
	}
	for (const FVector& P : Spots)
	{
		if (FVector::Dist2D(P, Center) < 150.0)
		{
			continue;
		}
		if (Rng.FRand() < 0.6f)
		{
			PlaceProp(TEXT("SM_ColumnSmallBrokenBottom"), P, Rng.FRandRange(0.0, 360.0), 40.0);
		}
		else if (Rng.FRand() < 0.6f)
		{
			const double Yaw = Rng.FRandRange(0.0, 360.0);
			PlaceProp(TEXT("SM_ColumnSmallBrokenTop"), P, Yaw, 120.0);
		}
	}

	int32 Pedestals = 0;
	for (const FSlot& S : Slots)
	{
		const float R = Rng.FRand();
		if (S.bTallWall && R < 0.4f)
		{
			AddClutter(Rng.FRand() < 0.7f ? TEXT("SM_ChainA") : TEXT("SM_ChainB"), S.Pos + S.Normal * 4.0 + FVector(0.0, 0.0, Params.WallTiers * Tier - 30.0), FRotator(0.0, S.Yaw, 0.0));
		}
		if (R > 0.8f && Pedestals < 3)
		{
			const FVector P = S.Pos + S.Normal * 45.0;
			if (PlaceProp(TEXT("SM_Pillar"), P, 0.0, 34.0))
			{
				PlaceCandleCluster(P + FVector(0.0, 0.0, 153.4), 3, 12.0);
				++Pedestals;
			}
		}
		else if (R > 0.55f)
		{
			const FVector P = S.Pos + S.Normal * 30.0;
			if (IsFree(P, 25.0))
			{
				AddClutter(TEXT("SM_Skull"), P, FRotator(0.0, S.Yaw + Rng.FRandRange(-40.0, 40.0), 0.0));
				AddClutter(TEXT("SM_BoneA"), P + S.Along * 25.0, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
				AddClutter(TEXT("SM_BoneB"), P - S.Along * 22.0 + S.Normal * 10.0, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
				Reserve(P, 25.0);
			}
		}
	}
	PlaceCandelabra(RoomCornerInset(Room, Rng.RandRange(0, 3), 100.0));
}

void FDungeonDresser::DressTreasure(const FDungeonRoom& Room, TArray<FSlot>& Slots)
{
	const FVector Center = Layout.GetRoomCenter(Room.Id);
	const double ChestYaw = 90.0;
	PlaceProp(TEXT("SM_ChestBottom"), Center, ChestYaw, 70.0, true, FVector::OneVector, true);
	Props.Add(MakeXf(Center + FVector(0.0, 0.0, 40.0), ChestYaw), Kit(TEXT("SM_ChestTop")), FVector(50.0));

	for (int32 K = 0; K < 8; ++K)
	{
		const FVector P = Center + AxisFromYaw(22.5 + 45.0 * K) * 135.0;
		PlaceCandleCluster(P, Rng.RandRange(2, 4), 12.0);
		Reserve(P, 16.0);
	}
	for (int32 K = 0; K < 4; ++K)
	{
		PlaceCandelabra(Center + AxisFromYaw(45.0 + 90.0 * K) * 240.0);
	}
	if (FMath::Min(Room.Size.X, Room.Size.Y) >= 4)
	{
		for (int32 K = 0; K < 4; ++K)
		{
			const FVector P = Center + AxisFromYaw(90.0 * K) * 330.0;
			if (PlaceProp(TEXT("SM_Pillar"), P, 0.0, 34.0))
			{
				PlaceCandleCluster(P + FVector(0.0, 0.0, 153.4), 3, 12.0);
			}
		}
	}
	for (int32 C = 0; C < 4; ++C)
	{
		const FVector Base = RoomCornerInset(Room, C, 40.0);
		const FVector In = (Center - Base).GetSafeNormal2D();
		const FVector Tangent(-In.Y, In.X, 0.0);
		PlaceProp(TEXT("SM_Jar"), Base, Rng.FRandRange(0.0, 360.0), 18.0);
		PlaceProp(TEXT("SM_Jar"), Base + In * 30.0 + Tangent * 30.0, Rng.FRandRange(0.0, 360.0), 18.0, true, FVector(0.8));
		PlaceProp(TEXT("SM_Box"), Base + In * 75.0 - Tangent * 25.0, Rng.FRandRange(0.0, 360.0), 28.0);
		AddClutter(TEXT("SM_Skull"), Base + In * 60.0 + Tangent * 60.0, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
	}
	for (const FSlot& S : Slots)
	{
		if (S.bTallWall && Rng.FRand() < 0.3f)
		{
			AddClutter(TEXT("SM_ChainB"), S.Pos + S.Normal * 4.0 + FVector(0.0, 0.0, Params.WallTiers * Tier - 30.0), FRotator(0.0, S.Yaw, 0.0));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Corridors & debris
// ---------------------------------------------------------------------------------------------------------------------

void FDungeonDresser::DressCorridors()
{
	CurrentRoomId = INDEX_NONE;
	KeepOut.Reset();
	for (const FFace& F : Faces)
	{
		if (F.Region != FDungeonLayout::Corridor || F.bDoorway)
		{
			continue;
		}
		const float R = Rng.FRand();
		const FVector Base = F.Center + F.Along * Rng.FRandRange(-110.0, 110.0) + F.Normal * WallHalfThickness;
		if (R < 0.06f)
		{
			PlaceProp(TEXT("SM_Barrel"), Base + F.Normal * 42.0, Rng.FRandRange(0.0, 360.0), 42.0);
		}
		else if (R < 0.10f)
		{
			PlaceProp(TEXT("SM_CrateMedium"), Base + F.Normal * 45.0, F.Yaw + Rng.FRandRange(-20.0, 20.0), 45.0);
		}
		else if (R < 0.14f)
		{
			PlaceProp(TEXT("SM_Rocks"), Base + F.Normal * 50.0, Rng.FRandRange(0.0, 360.0), 55.0);
		}
		else if (R < 0.18f)
		{
			const FVector P = Base + F.Normal * 30.0;
			if (IsFree(P, 25.0))
			{
				AddClutter(TEXT("SM_Skull"), P, FRotator(0.0, F.Yaw + Rng.FRandRange(-40.0, 40.0), 0.0));
				AddClutter(TEXT("SM_BoneB"), P + F.Along * 25.0, FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0));
				Reserve(P, 25.0);
			}
		}
		else if (R < 0.21f)
		{
			const FVector P = Base + F.Normal * 25.0;
			if (IsFree(P, 20.0))
			{
				PlaceCandleCluster(P, Rng.RandRange(2, 4), 18.0);
				Reserve(P, 20.0);
			}
		}
	}
}

void FDungeonDresser::EmitDebris()
{
	CurrentRoomId = INDEX_NONE;
	KeepOut.Reset();

	const TArray<FWeighted> Generic = {
		{ TEXT("SM_Rocks"), 0.45f, 50.0, 0.0 },
		{ TEXT("SM_BlockSmallA"), 0.5f, 15.0, 0.0 }, { TEXT("SM_BlockSmallB"), 0.5f, 15.0, 0.0 },
		{ TEXT("SM_BlockMediumA"), 0.25f, 18.0, 0.0 }, { TEXT("SM_BlockMediumB"), 0.25f, 18.0, 0.0 },
		{ TEXT("SM_BlockLargeA"), 0.12f, 21.0, 0.0 }, { TEXT("SM_BlockLargeB"), 0.12f, 21.0, 0.0 },
		{ TEXT("SM_BoneA"), 1.0f, 20.0, 0.0 }, { TEXT("SM_BoneB"), 0.8f, 25.0, 0.0 },
		{ TEXT("SM_PlankSmallA"), 0.6f, 28.0, 0.0 }, { TEXT("SM_PlankMediumB"), 0.5f, 38.0, 0.0 },
		{ TEXT("SM_PlankSmallB"), 0.4f, 28.0, 0.0 }, { TEXT("SM_Skull"), 0.2f, 14.0, 0.0 },
	};
	const TArray<FWeighted> Crypt = {
		{ TEXT("SM_Skull"), 1.0f, 14.0, 0.0 }, { TEXT("SM_BoneA"), 1.6f, 20.0, 0.0 }, { TEXT("SM_BoneB"), 1.4f, 25.0, 0.0 },
		{ TEXT("SM_Rocks"), 0.5f, 50.0, 0.0 }, { TEXT("SM_BlockSmallA"), 0.8f, 15.0, 0.0 }, { TEXT("SM_BlockMediumB"), 0.6f, 18.0, 0.0 },
	};
	const TArray<FWeighted> Library = {
		{ TEXT("SM_BookA"), 1.0f, 12.0, 1.8 }, { TEXT("SM_BookB"), 1.0f, 12.0, 1.8 }, { TEXT("SM_BookC"), 1.0f, 12.0, 1.8 },
		{ TEXT("SM_PlankSmallB"), 0.3f, 28.0, 0.0 }, { TEXT("SM_BlockSmallA"), 0.3f, 15.0, 0.0 },
	};
	const TArray<FWeighted> Storage = {
		{ TEXT("SM_PlankSmallA"), 1.0f, 28.0, 0.0 }, { TEXT("SM_PlankSmallB"), 1.0f, 28.0, 0.0 }, { TEXT("SM_PlankMediumA"), 0.8f, 38.0, 0.0 },
		{ TEXT("SM_PlankLargeA"), 0.5f, 48.0, 0.0 }, { TEXT("SM_BlockSmallB"), 0.6f, 15.0, 0.0 }, { TEXT("SM_BoneA"), 0.2f, 20.0, 0.0 },
	};

	for (int32 Y = 0; Y < Layout.Height; ++Y)
	{
		for (int32 X = 0; X < Layout.Width; ++X)
		{
			const FIntPoint P(X, Y);
			const int32 Region = Layout.Get(P);
			if (Region == FDungeonLayout::Empty)
			{
				continue;
			}
			const TArray<FWeighted>* Table = &Generic;
			float Chance = Params.DebrisChance;
			if (Region >= 0)
			{
				switch (Layout.Rooms[Region].Theme)
				{
				case EDungeonRoomTheme::Crypt:    Table = &Crypt; Chance *= 1.6f; break;
				case EDungeonRoomTheme::Library:  Table = &Library; Chance *= 0.7f; break;
				case EDungeonRoomTheme::Storage:  Table = &Storage; break;
				case EDungeonRoomTheme::Start:    Chance *= 0.4f; break;
				case EDungeonRoomTheme::Treasure: Table = &Crypt; Chance *= 0.6f; break;
				case EDungeonRoomTheme::Dining:   Chance *= 0.6f; break;
				default: break;
				}
			}
			else
			{
				Chance *= 1.2f;
			}

			for (int32 J = 0; J < 2; ++J)
			{
				for (int32 I = 0; I < 2; ++I)
				{
					for (int32 Try = 0; Try < 2; ++Try)
					{
						if (Rng.FRand() >= Chance)
						{
							continue;
						}
						const FWeighted* Item = PickWeighted(*Table, Rng);
						const FVector Loc(X * Cell + I * Tile + Rng.FRandRange(30.0, 170.0), Y * Cell + J * Tile + Rng.FRandRange(30.0, 170.0), Item->ZOffset);
						if (!IsFree(Loc, Item->Radius))
						{
							continue;
						}
						Debris.Add(MakeXf(Loc, Rng.FRandRange(0.0, 360.0)), Kit(Item->Mesh), FVector(Item->Radius, Item->Radius, 10.0), Rng.FRand());
					}
				}
			}
		}
	}
}
