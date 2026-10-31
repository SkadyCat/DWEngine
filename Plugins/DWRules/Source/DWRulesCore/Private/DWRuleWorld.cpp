#include "DWRuleWorld.h"

#include "Async/ParallelFor.h"
#include "HAL/PlatformTime.h"

namespace DWRules
{
	namespace
	{
		FORCEINLINE int32 CellCoord(float Value, float InvCell)
		{
			return FMath::FloorToInt32(Value * InvCell);
		}

		FORCEINLINE uint64 CellKey(int32 X, int32 Y)
		{
			return (uint64(uint32(X)) << 32) | uint64(uint32(Y));
		}

		// Earliest t in [0,1] where a segment enters a sphere; false if it never does.
		FORCEINLINE bool SweepSphere(const FVector3f& Start, const FVector3f& Delta, const FVector3f& Center, float Radius, float& OutTime)
		{
			const FVector3f Offset = Start - Center;
			const float C = Offset.SizeSquared() - Radius * Radius;
			if (C <= 0.f)
			{
				OutTime = 0.f;
				return true;
			}
			const float A = Delta.SizeSquared();
			if (A < KINDA_SMALL_NUMBER)
			{
				return false;
			}
			const float B = 2.f * FVector3f::DotProduct(Offset, Delta);
			const float Disc = B * B - 4.f * A * C;
			if (Disc < 0.f)
			{
				return false;
			}
			const float Time = (-B - FMath::Sqrt(Disc)) / (2.f * A);
			if (Time < 0.f || Time > 1.f)
			{
				return false;
			}
			OutTime = Time;
			return true;
		}

		FORCEINLINE double Now()
		{
			return FPlatformTime::Seconds() * 1000.0;
		}
	}

	void FDWTargets::Reset()
	{
		Position.Reset();
		Radius.Reset();
		Health.Reset();
		Team.Reset();
		ExternalId.Reset();
		Resistance.Reset();
	}

	int32 FDWTargets::Add(const FVector3f& InPosition, float InRadius, float InHealth, uint8 InTeam, uint32 InExternalId,
		const float* InResistance)
	{
		const int32 Index = Position.Add(InPosition);
		Radius.Add(InRadius);
		Health.Add(InHealth);
		Team.Add(InTeam);
		ExternalId.Add(InExternalId);
		for (int32 Element = 0; Element < NumResistances; ++Element)
		{
			Resistance.Add(InResistance ? InResistance[Element] : 0.f);
		}
		return Index;
	}

	void FDWProjectiles::Reserve(int32 Capacity)
	{
		Id.SetNumZeroed(Capacity);
		Root.SetNumZeroed(Capacity);
		Owner.SetNumZeroed(Capacity);
		Position.SetNumZeroed(Capacity);
		Velocity.SetNumZeroed(Capacity);
		Gravity.SetNumZeroed(Capacity);
		Age.SetNumZeroed(Capacity);
		Life.SetNumZeroed(Capacity);
		Radius.SetNumZeroed(Capacity);
		Damage.SetNumZeroed(Capacity);
		Pierce.SetNumZeroed(Capacity);
		Ricochet.SetNumZeroed(Capacity);
		Reflect.SetNumZeroed(Capacity);
		Return.SetNumZeroed(Capacity);
		Refract.SetNumZeroed(Capacity);
		Kind.SetNumZeroed(Capacity);
		EndEvent.SetNumZeroed(Capacity);
		Generation.SetNumZeroed(Capacity);
		Lineage.SetNumZeroed(Capacity);
		Element.SetNumZeroed(Capacity);
		Flags.SetNumZeroed(Capacity);
		Tags.SetNumZeroed(Capacity);
		Hits.SetNumZeroed(Capacity);
		Num = 0;
	}

	int32 FDWProjectiles::Allocate(uint32 NewId)
	{
		if (Num >= Id.Num())
		{
			return INDEX_NONE;
		}
		const int32 Index = Num++;
		Id[Index] = NewId;
		Age[Index] = 0.f;
		Gravity[Index] = 0.f;
		Pierce[Index] = 0;
		Ricochet[Index] = 0;
		Reflect[Index] = 0;
		Return[Index] = 0;
		Refract[Index] = 0;
		EndEvent[Index] = MAX_uint16;
		Flags[Index] = 0;
		Hits[Index] = FDWHitSet();
		return Index;
	}

	void FDWProjectiles::MoveSlot(int32 From, int32 To)
	{
		Id[To] = Id[From];
		Root[To] = Root[From];
		Owner[To] = Owner[From];
		Position[To] = Position[From];
		Velocity[To] = Velocity[From];
		Gravity[To] = Gravity[From];
		Age[To] = Age[From];
		Life[To] = Life[From];
		Radius[To] = Radius[From];
		Damage[To] = Damage[From];
		Pierce[To] = Pierce[From];
		Ricochet[To] = Ricochet[From];
		Reflect[To] = Reflect[From];
		Return[To] = Return[From];
		Refract[To] = Refract[From];
		Kind[To] = Kind[From];
		EndEvent[To] = EndEvent[From];
		Generation[To] = Generation[From];
		Lineage[To] = Lineage[From];
		Element[To] = Element[From];
		Flags[To] = Flags[From];
		Tags[To] = Tags[From];
		Hits[To] = Hits[From];
	}

	FDWRuleWorld::FDWRuleWorld(const FDWWorldConfig& InConfig)
		: Config(InConfig)
	{
		Carriers.Reserve(FMath::Max(1, Config.MaxProjectiles));
		Summons.Reserve(128);
	}

	int32 FDWRuleWorld::AddPlan(const TSharedRef<const FDWCompiledPlan>& Plan)
	{
		return Plans.Add(Plan);
	}

	int32 FDWRuleWorld::AddOwner(uint32 ExternalId, uint8 Team)
	{
		FDWOwner Owner;
		Owner.ExternalId = ExternalId;
		Owner.StatusId = ExternalId;
		Owner.Team = Team;
		if (FreeOwners.Num()) { const int32 I=FreeOwners.Pop(EAllowShrinking::No); Owners[I]=Owner; return I; }
		return Owners.Add(Owner);
	}

	bool FDWRuleWorld::SetTargetDefensePlan(uint32 TargetExternalId, int32 Plan)
	{
		if (TargetExternalId == InvalidId)
		{
			return false;
		}
		if (Plan == INDEX_NONE)
		{
			TargetDefensePlans.Remove(TargetExternalId);
			return true;
		}
		if (!Plans.IsValidIndex(Plan))
		{
			return false;
		}
		TargetDefensePlans.Add(TargetExternalId, Plan);
		return true;
	}

	void FDWRuleWorld::UpdateOwner(int32 Owner, const FVector3f& Position, const FVector3f& Aim)
	{
		if (!Owners.IsValidIndex(Owner))
		{
			return;
		}
		Owners[Owner].Position = Position;
		const FVector3f Flat(Aim.X, Aim.Y, 0.f);
		if (!Flat.IsNearlyZero())
		{
			Owners[Owner].Aim = Flat.GetSafeNormal();
		}
	}

	void FDWRuleWorld::SetOwnerTargetPoint(int32 Owner, const FVector3f &Point)
	{
		if (Owners.IsValidIndex(Owner) && !Point.ContainsNaN())
			Owners[Owner].TargetPoint = Point;
	}

	int32 FDWRuleWorld::Cast(int32 PlanIndex, int32 Owner, const FVector3f &Origin,
							 const FVector3f &Direction, uint64 Seed, float Power,
							 TOptional<FVector3f> TargetPoint)
	{
		const int32 Index = OpenRoot(PlanIndex, Owner, Seed);
		if (Index == INDEX_NONE) return INDEX_NONE;
		FDWRoot& Root = Roots[Index];
		Root.TargetPoint = TargetPoint;
		--Root.Live;
		FDWEvent Event;
		Event.Type = uint16(EBuiltinEvent::Cast);
		Event.Root = uint32(Index);
		Event.Sequence = Root.NextSequence++;
		Event.Position = Origin;
		const FVector3f Flat(Direction.X, Direction.Y, 0.f);
		Event.Direction = Flat.IsNearlyZero() ? Owners[Owner].Aim : Flat.GetSafeNormal();
		Event.Tags = TagBit(Tags::Manual) | TagBit(Tags::Root);
		Event.Amount = FMath::IsFinite(Power) ? FMath::Max(0.f, Power) : 0.f;
		Event.Damage = Event.Amount;
		++Root.Pending;
		Deferred.Add(Event);
		return Index;
	}

	int32 FDWRuleWorld::OpenRoot(int32 PlanIndex, int32 Owner, uint64 Seed)
	{
		if (!Plans.IsValidIndex(PlanIndex) || !Owners.IsValidIndex(Owner) || RetiredOwners.Contains(Owner) || FreeOwners.Contains(Owner))
		{
			return INDEX_NONE;
		}
		int32 Index = INDEX_NONE;
		if (FreeRoots.Num() > 0)
		{
			Index = FreeRoots.Pop(EAllowShrinking::No);
		}
		else
		{
			Index = Roots.AddElement(FDWRoot());
		}
		FDWRoot& Root = Roots[Index];
		const FDWCompiledPlan& CastPlan = *Plans[PlanIndex];
		Root.Plan = PlanIndex;
		Root.Owner = Owner;
		Root.Seed = Seed;
		Root.NextSequence = 1;
		Root.Live = 1;
		Root.Pending = 0;
		Root.Work = 0;
		Root.bActive = true;
		Root.Identity = NextRootIdentity++;
        Root.DamageMultiplier = 1.f;
		Root.TargetPoint.Reset();
		Root.ChainCount.Reset();
		Root.ChainCount.SetNumZeroed(CastPlan.Rules.Num());
		Root.CastState.Reset();
		Root.OverflowHits.Reset();

		return Index;
	}

	int32 FDWRuleWorld::ActiveRoots() const
	{
		int32 Count = 0;
		for (const FDWRoot& Root : Roots)
		{
			Count += Root.bActive ? 1 : 0;
		}
		return Count;
	}

	uint64 FDWRuleWorld::StateHash() const
	{
		uint64 Hash = Mix64(uint64(Carriers.Num));
		for (int32 Index = 0; Index < Carriers.Num; ++Index)
		{
			const FVector3f& P = Carriers.Position[Index];
			const uint64 Quantized = (uint64(uint32(FMath::RoundToInt32(P.X * 100.f))) << 32) ^ uint64(uint32(FMath::RoundToInt32(P.Y * 100.f)));
			Hash = Mix64(Hash ^ Quantized ^ uint64(Carriers.Id[Index]));
		}
		for (float Health : TargetData.Health)
		{
			Hash = Mix64(Hash ^ uint64(uint32(FMath::RoundToInt32(Health * 1000.f))));
		}
		if (Summons.Num() > 0)
		{
			Hash = Mix64(Hash ^ uint64(Summons.Num()));
			for (const FDWSummon& Summon : Summons)
			{
				const uint64 Position = (uint64(uint32(FMath::RoundToInt32(Summon.Position.X * 100.f))) << 32) ^ uint64(uint32(FMath::RoundToInt32(Summon.Position.Y * 100.f)));
				Hash = Mix64(Hash ^ Position ^ uint64(Summon.Id) ^ uint64(uint32(FMath::RoundToInt32(Summon.Expire * 1000.f))));
			}
		}
        for(const auto& Root:Roots)if(Root.bActive&&Root.DamageMultiplier!=1.f)Hash=Mix64(Hash^Root.Seed^uint64(uint32(FMath::RoundToInt32(Root.DamageMultiplier*1000.f))));
		return Hash;
	}

	void FDWRuleWorld::Step(float DeltaSeconds)
	{
		const double Start = Now();
		FrameStats = FDWFrameStats();
		FrameWork = 0;
		Accumulator = FMath::Min(Accumulator + FMath::Max(0.f, DeltaSeconds), Config.FixedStep * Config.MaxSubsteps);
		// Deferred events and new casts first: casts spawn carriers that move this frame.
		Current = MoveTemp(Deferred);
		Deferred.Reset();
		PruneStatuses();
		// The host may replace, move or remove targets between frames. Deferred area casts
		// must query this export, including frames shorter than a simulation substep.
		BuildGrid();
		DrainWaves();
		while (Accumulator >= Config.FixedStep * 0.999f)
		{
			Substep(Config.FixedStep);
			Accumulator -= Config.FixedStep;
			++FrameStats.Substeps;
		}
		Compact();
		ReleaseRoots();
		ReleaseOwners();
		FrameStats.Projectiles = Carriers.Num;
		FrameStats.Deferred = Deferred.Num();
		FrameStats.Statuses = Statuses.Num();
		FrameStats.TotalMs = Now() - Start;
	}

	void FDWRuleWorld::Substep(float Dt)
	{
		Clock += Dt;
		const double MotionStart = Now();
		BuildGrid();
		TickSummons();
        TickMaintainedEmitters();
		DrainWaves();
		MoveAndSweep(Dt);
		FrameStats.MotionMs += Now() - MotionStart;

		const double RulesStart = Now();
		ResolveContacts();
		ResolveWalls();
        if(Extension)Extension->AfterMotion();
		DrainWaves();
		EndPendingImpacts();
		ExpireProjectiles();
		DrainWaves();
		TickStatuses();
		FrameStats.RulesMs += Now() - RulesStart;
	}

	void FDWRuleWorld::TickSummons()
	{
		for (FDWSummon& Summon : Summons)
		{
            if(Summon.bDead)continue;
			FDWRoot& Root = Roots[Summon.Root];
			if (Clock >= Summon.Expire)
			{
				if (Extension) Extension->SummonEnd(Summon);
				--Root.Live;
				Summon.bDead = true;
				continue;
			}
			int32 Nearest = INDEX_NONE;
			float Distance = Summon.Range * Summon.Range;
            if (Summon.bStorm) Summon.Position = Owners[Root.Owner].Position;
			for (int32 Index = 0; Index < TargetData.Num(); ++Index)
			{
				if (!(TargetData.Health[Index] > 0.f) || !IsHostile(Root.Owner, Index)) continue;
				const float X = TargetData.Position[Index].X - Summon.Position.X;
				const float Y = TargetData.Position[Index].Y - Summon.Position.Y;
				const float D2 = X * X + Y * Y;
				if (D2 < Distance || (D2 == Distance && (Nearest < 0 || TargetData.ExternalId[Index] < TargetData.ExternalId[Nearest])))
				{
					Nearest = Index;
					Distance = D2;
				}
			}
			if (Extension) Nearest = Extension->SummonTarget(Summon, Nearest);
			if (Nearest >= 0)
			{
				const FVector3f Delta = TargetData.Position[Nearest] - Summon.Position;
				Summon.Direction = FVector3f(Delta.X, Delta.Y, 0.f).GetSafeNormal();
			}
			if (Clock < Summon.NextFire) continue;
			if (Summon.bHold) continue;
			Summon.NextFire = Clock + Summon.Interval;
			if (Nearest < 0) continue;
			FDWEvent Event;
			Event.Type = Summon.Event;
			Event.Root = Summon.Root;
			Event.Sequence = Root.NextSequence++;
            Event.Position = Summon.bStorm ? TargetData.Position[Nearest] : Summon.Position;
			Event.Direction = Summon.Direction;
			Event.Amount = Event.Damage = Summon.Power;
			Event.Tags = Summon.Tags;
			Event.TargetIndex = Nearest;
			Event.TargetId = TargetData.ExternalId[Nearest];
			PushEvent(Event);
		}
		Summons.RemoveAll([](const FDWSummon& Summon) { return Summon.bDead; });
	}

	void FDWRuleWorld::BuildGrid()
	{
		const float InvCell = 1.f / FMath::Max(0.1f, Config.CellSize);
		const int32 Count = TargetData.Num();
		TArray<TPair<uint64, int32>> Keys;
		Keys.Reserve(Count);
		MaxTargetRadius = 0.f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (TargetData.Health[Index] <= 0.f)
			{
				continue;
			}
			const FVector3f& P = TargetData.Position[Index];
			Keys.Add({ CellKey(CellCoord(P.X, InvCell), CellCoord(P.Y, InvCell)), Index });
			MaxTargetRadius = FMath::Max(MaxTargetRadius, TargetData.Radius[Index]);
		}
		Keys.Sort([](const TPair<uint64, int32>& A, const TPair<uint64, int32>& B)
		{
			return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value;
		});
		Cells.Reset();
		CellOrder.Reset(Keys.Num());
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			CellOrder.Add(Keys[Index].Value);
			if (TPair<int32, int32>* Range = Cells.Find(Keys[Index].Key))
			{
				++Range->Value;
			}
			else
			{
				Cells.Add(Keys[Index].Key, TPair<int32, int32>(Index, 1));
			}
		}
	}

	template <typename FVisit>
	void FDWRuleWorld::QueryTargets(const FVector3f& Min, const FVector3f& Max, FVisit&& Visit) const
	{
		const float InvCell = 1.f / FMath::Max(0.1f, Config.CellSize);
		const int32 X0 = CellCoord(Min.X - MaxTargetRadius, InvCell);
		const int32 X1 = CellCoord(Max.X + MaxTargetRadius, InvCell);
		const int32 Y0 = CellCoord(Min.Y - MaxTargetRadius, InvCell);
		const int32 Y1 = CellCoord(Max.Y + MaxTargetRadius, InvCell);
		for (int32 X = X0; X <= X1; ++X)
		{
			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				if (const TPair<int32, int32>* Range = Cells.Find(CellKey(X, Y)))
				{
					for (int32 Slot = Range->Key; Slot < Range->Key + Range->Value; ++Slot)
					{
						Visit(CellOrder[Slot]);
					}
				}
			}
		}
	}

	void FDWRuleWorld::ForEachTargetInRadius(const FVector3f& Center, float Radius, TFunctionRef<void(int32)> Visit) const
	{
		const FVector3f Extent(Radius, Radius, 0.f);
		QueryTargets(Center - Extent, Center + Extent, [&](int32 Target)
		{
			if (TargetData.Health[Target] <= 0.f)
			{
				return;
			}
			const float Reach = Radius + TargetData.Radius[Target];
			if (FVector2f::DistSquared(FVector2f(Center.X, Center.Y), FVector2f(TargetData.Position[Target].X, TargetData.Position[Target].Y)) <= Reach * Reach)
			{
				Visit(Target);
			}
		});
	}

	bool FDWRuleWorld::IsHostile(int32 Owner, int32 Target) const
	{
		return Owners.IsValidIndex(Owner) && TargetData.Team[Target] != Owners[Owner].Team;
	}

	bool FDWRuleWorld::HitContains(int32 Projectile, const FDWRoot& Root, uint32 TargetId) const
	{
		const FDWHitSet& Set = Carriers.Hits[Projectile];
		for (int32 Index = 0; Index < Set.Num; ++Index)
		{
			if (Set.Ids[Index] == TargetId)
			{
				return true;
			}
		}
		return Set.bOverflow && Root.OverflowHits.Contains((uint64(Carriers.Id[Projectile]) << 32) | TargetId);
	}

	void FDWRuleWorld::HitAdd(int32 Projectile, FDWRoot& Root, uint32 TargetId)
	{
		FDWHitSet& Set = Carriers.Hits[Projectile];
		if (Set.Num < FDWHitSet::Inline)
		{
			Set.Ids[Set.Num++] = TargetId;
			return;
		}
		Set.bOverflow = true;
		Root.OverflowHits.Add((uint64(Carriers.Id[Projectile]) << 32) | TargetId);
	}

	int32 FDWRuleWorld::NearestTarget(const FVector3f& Center, float Radius, int32 Projectile, const FDWRoot& Root, uint8 Team) const
	{
		int32 Best = INDEX_NONE;
		float BestDistance = Radius * Radius;
		const FVector3f Extent(Radius, Radius, 0.f);
		QueryTargets(Center - Extent, Center + Extent, [&](int32 Target)
		{
			if (TargetData.Health[Target] <= 0.f || TargetData.Team[Target] == Team)
			{
				return;
			}
			if (Projectile != INDEX_NONE && HitContains(Projectile, Root, TargetData.ExternalId[Target]))
			{
				return;
			}
			const float Distance = FVector3f::DistSquared(Center, TargetData.Position[Target]);
			if (Distance < BestDistance || (Distance == BestDistance && Best != INDEX_NONE && Target < Best))
			{
				BestDistance = Distance;
				Best = Target;
			}
		});
		return Best;
	}

	void FDWRuleWorld::MoveAndSweep(float Dt)
	{
		if (Extension)
		{
			const int32 InitialCount = Carriers.Num;
			for (int32 Index = 0; Index < InitialCount; ++Index)
				if (!(Carriers.Flags[Index] & FDWProjectiles::Dead)) Extension->Motion(Index, Dt);
		}
		const int32 Count = Carriers.Num;
		WallTimes.Init(2.f,Count);
		SweepStarts.SetNum(Count);
		MotionDelta.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			SweepStarts[I] = Carriers.Position[I];
			const bool Arc = (Carriers.Flags[I] & FDWProjectiles::Parabolic) != 0;
			const float Step = Arc ? FMath::Clamp(Carriers.Life[I] - Carriers.Age[I], 0.f, Dt) : Dt;
			MotionDelta[I] = Carriers.Velocity[I] * Step;
			if (Arc)
			{
				MotionDelta[I].Z += .5f * Carriers.Gravity[I] * Step * Step;
				Carriers.Velocity[I].Z += Carriers.Gravity[I] * Step;
			}
		}
		if (SweepGeometry)
			for (int32 I=0;I<Count;++I)
				if (!(Carriers.Flags[I]&FDWProjectiles::Dead))
				{
					float T=1;
					if (SweepGeometry(Carriers.Position[I], Carriers.Position[I] + MotionDelta[I],
									  Extension?Extension->WallRadius(I,Carriers.Radius[I]):Carriers.Radius[I], T))
						WallTimes[I] = FMath::Clamp(T, 0.f, 1.f);
				}
		const int32 ChunkSize = FMath::Max(16, Config.ParallelChunk);
		const int32 Chunks = (Count + ChunkSize - 1) / ChunkSize;
		if (ChunkContacts.Num() < Chunks)
		{
			ChunkContacts.SetNum(Chunks);
		}
		auto Work = [this, Dt, Count, ChunkSize](int32 Chunk)
		{
			TArray<FContact>& Out = ChunkContacts[Chunk];
			Out.Reset();
			const int32 End = FMath::Min(Count, (Chunk + 1) * ChunkSize);
			for (int32 Index = Chunk * ChunkSize; Index < End; ++Index)
			{
				if (Carriers.Flags[Index] & FDWProjectiles::Dead)
				{
					continue;
				}
				const FVector3f Start = Carriers.Position[Index];
				const FVector3f Delta = MotionDelta[Index];
				const FVector3f Finish = Start + Delta;
				const float Radius = Carriers.Radius[Index];
				const FDWRoot& Root = Roots[Carriers.Root[Index]];
				const uint8 Team = Owners[Root.Owner].Team;
				const FVector3f Min = Start.ComponentMin(Finish) - FVector3f(Radius);
				const FVector3f Max = Start.ComponentMax(Finish) + FVector3f(Radius);
				QueryTargets(Min, Max, [&](int32 Target)
				{
					if (TargetData.Team[Target] == Team || TargetData.Health[Target] <= 0.f)
					{
						return;
					}
					float Time = 0.f;
					if (!SweepSphere(Start, Delta, TargetData.Position[Target], Radius + TargetData.Radius[Target], Time))
					{
						return;
					}
					if (HitContains(Index, Root, TargetData.ExternalId[Target]))
					{
						return;
					}
					if (Time > WallTimes[Index]) return;
					Out.Add({ Carriers.Id[Index], Index, Target, Time });
				});
				Carriers.Position[Index] = Finish;
				Carriers.Age[Index] += Dt;
			}
		};
		if (Config.bParallel && Chunks > 1)
		{
			ParallelFor(Chunks, Work);
		}
		else
		{
			for (int32 Chunk = 0; Chunk < Chunks; ++Chunk)
			{
				Work(Chunk);
			}
		}
		Contacts.Reset();
		for (int32 Chunk = 0; Chunk < Chunks; ++Chunk)
		{
			Contacts.Append(ChunkContacts[Chunk]);
		}
		// Stable order independent of thread count: carrier creation order, then time.
		Contacts.Sort([](const FContact& A, const FContact& B)
		{
			if (A.ProjectileId != B.ProjectileId)
			{
				return A.ProjectileId < B.ProjectileId;
			}
			return A.Time != B.Time ? A.Time < B.Time : A.Target < B.Target;
		});
		FrameStats.Contacts += Contacts.Num();
	}
	void FDWRuleWorld::ResolveWalls()
	{
		for (int32 I=0;I<WallTimes.Num() && I<Carriers.Num;++I)
		{
			if (WallTimes[I]>1 || (Carriers.Flags[I]&(FDWProjectiles::Dead|FDWProjectiles::PendingImpact))) continue;
			Carriers.Position[I] = SweepStarts[I] + MotionDelta[I] * WallTimes[I];
            if(Extension)Extension->ProjectileHit(I,Carriers.Position[I]);
			if (!Extension || !Extension->Wall(I)) EndProjectile(I,EEndReason::Impact);
		}
	}

	void FDWRuleWorld::ResolveContacts()
	{
		for (const FContact& Contact : Contacts)
		{
			const int32 Index = Contact.Projectile;
			const uint8 Flags = Carriers.Flags[Index];
			if ((Flags & (FDWProjectiles::Dead | FDWProjectiles::PendingImpact)) || TargetData.Health[Contact.Target] <= 0.f)
			{
				continue;
			}
			const uint32 RootIndex = Carriers.Root[Index];
			FDWRoot& Root = Roots[RootIndex];
			const uint32 TargetId = TargetData.ExternalId[Contact.Target];
			if (HitContains(Index, Root, TargetId))
			{
				continue;
			}
            // Interception is resolved before damage/status/end delivery.
            const FVector3f ContactAt=SweepStarts[Index]+MotionDelta[Index]*Contact.Time;
            if(Extension&&Extension->Intercept(Index,Contact.Target,ContactAt))continue;
			HitAdd(Index, Root, TargetId);
			const FVector3f Velocity = Carriers.Velocity[Index];
			const FVector3f At =
				(Carriers.Flags[Index] & FDWProjectiles::Parabolic)
					? SweepStarts[Index] + MotionDelta[Index] * Contact.Time
					: Carriers.Position[Index] - Velocity * (Config.FixedStep * (1.f - Contact.Time));

			FDWEvent Base;
			Base.Root = RootIndex;
			Base.Generation = uint8(FMath::Min(int32(Carriers.Generation[Index]) + 1, MaxGeneration));
			Base.Lineage = Carriers.Lineage[Index];
			Base.Element = Carriers.Element[Index];
			Base.SourceIndex = Index;
			Base.SourceId = Carriers.Id[Index];
			Base.Position = At;
			Base.Direction = Velocity.GetSafeNormal();
			Base.Tags = Carriers.Tags[Index] | TagBit(Tags::Projectile) | TagBit(Tags::Direct);
			if (Carriers.Hits[Index].Num == 1 && !Carriers.Hits[Index].bOverflow)
			{
				Base.Tags |= TagBit(Tags::FirstHit);
			}
			const uint32 Sequence = Root.NextSequence++;
			bool bCritical = false;
			bool bKilled = false;
			const float Requested = Carriers.Damage[Index];
			FDWDamageInfo Info;
			Info.RootIndex = int32(RootIndex); Info.Tags = Base.Tags;
			Info.SourceIndex = Index; Info.SourceId = Carriers.Id[Index];
			Info.Position = At; Info.Kind = TEXT("projectile");
			bool Missed = false;
			const float Applied = ResolveDamage(Root, Contact.Target, Requested, Carriers.Element[Index], Sequence, bCritical, bKilled, Info, &Missed);
			if (Missed) continue;
            if(Extension)Extension->ProjectileHit(Index,At);
			PushHitEvents(Root, int32(RootIndex), Base, Contact.Target, Applied, Requested, bCritical, bKilled, uint16(EBuiltinEvent::Hit));
			ApplyRefraction(Index, At);
			if (Extension && Extension->Contact(Index, Contact.Target, {Applied, bCritical, bKilled, false})) continue;
			if (Carriers.Pierce[Index] > 0)
			{
				--Carriers.Pierce[Index];
			}
			else
			{
				Carriers.Flags[Index] |= FDWProjectiles::PendingImpact;
			}
		}
	}

	void FDWRuleWorld::ApplyRefraction(int32 Index, const FVector3f& At)
	{
		const int32 Count = Carriers.Refract[Index];
		if (Count <= 0 || (Carriers.Tags[Index] & TagBit(Tags::Refraction)))
		{
			return;
		}
		Carriers.Refract[Index] = 0;
		const uint32 RootIndex = Carriers.Root[Index];
		FDWRoot& Root = Roots[RootIndex];
		const FDWCompiledPlan& RootPlan = *Plans[Root.Plan];
		const float Angle = RootPlan.Stats.Get(EStat::ProjectileRefractionAngle);
		const FVector3f Velocity = Carriers.Velocity[Index];
		const float Speed = Velocity.Size();
		const FVector3f Direction = Velocity.GetSafeNormal();
		for (int32 Child = 0; Child < Count; ++Child)
		{
			const float Offset = Count > 1 ? Angle * (float(Child) / float(Count - 1) - 0.5f) : 0.f;
			const FVector3f ChildDirection = Direction.RotateAngleAxis(Offset, FVector3f::UpVector);
			const uint64 ChildTags = (Carriers.Tags[Index] & ~NonInheritedTags) | TagBit(Tags::Refraction);
			const int32 Spawned = SpawnProjectile(Root, int32(RootIndex), At, ChildDirection, Speed, Carriers.Life[Index],
				Carriers.Radius[Index], Carriers.Damage[Index], Carriers.Element[Index], Carriers.Kind[Index], ChildTags,
				uint8(FMath::Min(int32(Carriers.Generation[Index]) + 1, MaxGeneration)), Carriers.Lineage[Index]);
			if (Spawned == INDEX_NONE)
			{
				break;
			}
			// The parent target must not be hit again by its own refraction.
			Carriers.Hits[Spawned] = Carriers.Hits[Index];
			Carriers.Flags[Spawned] |= FDWProjectiles::Explicit;
		}
	}

	float FDWRuleWorld::ResolveDamage(FDWRoot& Root, int32 Target, float Amount, uint8 Element, uint32 Sequence,
		bool& bOutCritical, bool& bOutKilled, FDWDamageInfo Info, bool* Missed)
	{
		bOutCritical = false;
		bOutKilled = false;
		float& Health = TargetData.Health[Target];
		if (Health <= 0.f || Amount <= 0.f || InvulnerableTargets.Contains(TargetData.ExternalId[Target]))
		{
			return 0.f;
		}
		const FDWStatBlock& Stats = Plans[Root.Plan]->Stats;
		const uint32 OwnerStatus = Owners[Root.Owner].StatusId;
		const FDWDamageMod Mod = Extension ? Extension->ModifyDamage(Root, Target, Amount, Element, Info) : FDWDamageMod();
		if (Missed) *Missed = Mod.bMiss;
		if (Mod.bMiss) return 0;
		const float Flat = float(Stats.Get(ElementStatId(Element, EElementStat::DamageFlat)) + Mod.Flat);
		const float Increased = float(Stats.Get(ElementStatId(Element, EElementStat::DamagePercent)) + Mod.Increased);
		float CriticalChance = Stats.Get(EStat::OwnerCriticalChance);
		if (EffectDefs[int32(EStatusEffect::CriticalChance)].Num() > 0)
		{
			CriticalChance = CriticalChance + EffectSum(OwnerStatus, EStatusEffect::CriticalChance);
		}
		CriticalChance = float(CriticalChance * Mod.CriticalScale + Mod.Critical);
		bOutCritical = Random01(Root.Seed, Sequence, 0xC817) * 100.f < CriticalChance;
		const float CriticalFactor = bOutCritical ? float((Stats.Get(EStat::OwnerCriticalMultiplier) + Mod.CriticalMultiplier) / 100.) : 1.f;
		float Raw = (Amount + Flat) * FMath::Max(0.f, 1.f + Increased / 100.f) * CriticalFactor;
		Raw *= Stats.Get(EStat::OwnerDamageMultiplier);
		if (Mod.More != 1) Raw = float(Raw * FMath::Max(0., Mod.More));
		if (EffectDefs[int32(EStatusEffect::DamageDealt)].Num() > 0)
		{
			const float DealtBonus = EffectSum(OwnerStatus, EStatusEffect::DamageDealt);
			if (DealtBonus != 0.f)
			{
				Raw = Raw * FMath::Max(0.f, 1.f + DealtBonus / 100.f);
			}
		}
		Raw *= Root.DamageMultiplier;
		if (Info.bReflected) { Raw = Amount; bOutCritical = false; }
		Info.OriginalAmount = FMath::Max(0.f, Raw);
        const float Effective = EffectiveResistance(Target, Element, float(Stats.Get(ElementStatId(Element, EElementStat::Penetration)) + Mod.Penetration));
		float Dealt = ApplyDamageTaken(Target, FMath::Max(0.f, Raw * (1.f - Effective / 100.f)));
		if (Mod.Taken != 1) Dealt = float(Dealt * FMath::Max(0., Mod.Taken));
		if (Mod.Adjust) Dealt = float(Mod.Adjust(Dealt, bOutCritical));
		const float Applied = FMath::Min(Health, Dealt);
		Health -= Dealt;
		bOutKilled = Health <= 0.f;
		FDWDamageRecord Record;
		Record.TargetExternalId = TargetData.ExternalId[Target];
		Record.OwnerExternalId = Owners[Root.Owner].ExternalId;
		Record.Amount = Applied;
		Record.Element = Element;
		Record.bCritical = bOutCritical;
		Record.bKilled = bOutKilled;
		Record.Info = Info;
		Damage.Add(Record);
		++FrameStats.Hits;
		FrameStats.Kills += bOutKilled ? 1 : 0;
		return Applied;
	}

	int32 FDWRuleWorld::SpawnProjectile(FDWRoot& Root, int32 RootIndex, const FVector3f& Origin, const FVector3f& Direction,
		float Speed, float Life, float Radius, float DamageValue, uint8 Element, uint16 Kind, uint64 TagMask,
		uint8 Generation, uint32 Lineage)
	{
		FDWOwner& Owner = Owners[Root.Owner];
		const int32 Capacity = int32(Plans[Root.Plan]->Stats.Get(EStat::ProjectileCapacity));
		if (Owner.LiveProjectiles >= Capacity)
		{
			++FrameStats.CapacityDrops;
			return INDEX_NONE;
		}
		const int32 Index = Carriers.Allocate(NextProjectileId++);
		if (Index == INDEX_NONE)
		{
			++FrameStats.CapacityDrops;
			return INDEX_NONE;
		}
		Carriers.Root[Index] = uint32(RootIndex);
		Carriers.Owner[Index] = uint32(Root.Owner);
		Carriers.Position[Index] = Origin;
		Carriers.Velocity[Index] = Direction.GetSafeNormal() * Speed;
		Carriers.Life[Index] = Life;
		Carriers.Radius[Index] = Radius;
		Carriers.Damage[Index] = DamageValue;
		Carriers.Kind[Index] = Kind;
		Carriers.Generation[Index] = Generation;
		Carriers.Lineage[Index] = Lineage;
		Carriers.Element[Index] = Element;
		Carriers.Tags[Index] = TagMask;
		++Root.Live;
		++Owner.LiveProjectiles;
		return Index;
	}

	void FDWRuleWorld::EndProjectile(int32 Index, EEndReason Reason,bool Silent)
	{
		if (Carriers.Flags[Index] & FDWProjectiles::Dead)
		{
			return;
		}
		if (!Silent && Extension && Extension->End(Index, Reason)) return;
		Carriers.Flags[Index] |= FDWProjectiles::Dead;
		const uint32 RootIndex = Carriers.Root[Index];
		FDWRoot& Root = Roots[RootIndex];
		--Root.Live;
		--Owners[Root.Owner].LiveProjectiles;
        if(Silent)return;
		const FDWCompiledPlan& RootPlan = *Plans[Root.Plan];
		const uint16 EndEvent = Carriers.EndEvent[Index];
		const bool bEndRules = RootPlan.HasRules(uint16(EBuiltinEvent::ProjectileEnd));
		const bool bCustom = Reason == EEndReason::Expired && EndEvent != MAX_uint16 && RootPlan.HasRules(EndEvent);
		if (!bEndRules && !bCustom)
		{
			return;
		}
		FDWEvent Event;
		Event.Root = RootIndex;
		Event.Generation = Carriers.Generation[Index];
		Event.Lineage = Carriers.Lineage[Index];
		Event.Element = Carriers.Element[Index];
		Event.SourceIndex = Index;
		Event.SourceId = Carriers.Id[Index];
		Event.Position = Carriers.Position[Index];
		Event.Direction = Carriers.Velocity[Index].GetSafeNormal();
		const uint32 EndTag = Reason == EEndReason::Expired ? Tags::EndExpired : (Reason == EEndReason::Impact ? Tags::EndImpact : Tags::EndCapacity);
		Event.Tags = (Carriers.Tags[Index] & ~NonInheritedTags) | TagBit(EndTag);
		if (bEndRules)
		{
			Event.Type = uint16(EBuiltinEvent::ProjectileEnd);
			Event.Sequence = Root.NextSequence++;
			PushEvent(Event);
		}
		if (bCustom)
		{
			Event.Type = EndEvent;
			Event.Sequence = Root.NextSequence++;
			PushEvent(Event);
		}
	}

	void FDWRuleWorld::EndPendingImpacts()
	{
		for (int32 Index = 0; Index < Carriers.Num; ++Index)
		{
			if ((Carriers.Flags[Index] & (FDWProjectiles::PendingImpact | FDWProjectiles::Dead)) == FDWProjectiles::PendingImpact)
			{
				EndProjectile(Index, EEndReason::Impact);
			}
		}
	}

	void FDWRuleWorld::ExpireProjectiles()
	{
		for (int32 Index = 0; Index < Carriers.Num; ++Index)
		{
			if (!(Carriers.Flags[Index] & FDWProjectiles::Dead) && Carriers.Age[Index] >= Carriers.Life[Index])
			{
				EndProjectile(Index, EEndReason::Expired);
			}
		}
	}

	void FDWRuleWorld::Compact()
	{
		int32 Write = 0;
		for (int32 Read = 0; Read < Carriers.Num; ++Read)
		{
			if (Carriers.Flags[Read] & FDWProjectiles::Dead)
			{
				continue;
			}
			if (Write != Read)
			{
				Carriers.MoveSlot(Read, Write);
			}
			++Write;
		}
		Carriers.Num = Write;
	}

	void FDWRuleWorld::ReleaseRoots()
	{
		for (int32 Index = 0; Index < Roots.Num(); ++Index)
		{
			FDWRoot& Root = Roots[Index];
			if (Root.bActive && Root.Live <= 0 && Root.Pending <= 0)
			{
				Root.bActive = false;
				Root.CastState.Reset();
				Root.OverflowHits.Reset();
				FreeRoots.Add(Index);
			}
		}
	}
}
