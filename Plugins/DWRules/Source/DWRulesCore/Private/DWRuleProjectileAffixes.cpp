#include "DWRuleProjectileAffixes.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleMinions.h"

namespace DWRules
{
bool FDWProjectileAffixes::Homing() const
{
	return S(TEXT("proj.homing_range")) > 0 || S(TEXT("proj.homing_turn")) > 0 || S(TEXT("proj.retarget")) > 0 ||
		   S(TEXT("proj.retarget_on_death")) > 0 || S(TEXT("proj.prefer_elite")) > 0 ||
		   S(TEXT("proj.prefer_low")) > 0 || R.Has(TEXT("proj_prefer_status"));
}
FDWAffixProjectile &FDWProjectileAffixes::NewState(int32 Index)
{
	auto &C = this->C();
	auto Ptr = MakeUnique<FDWAffixProjectile>();
	auto &St = *Ptr;
	St.Retargets = 1 + FMath::FloorToInt(S(TEXT("proj.retarget")));
	St.ChainsLeft = FMath::FloorToInt(S(TEXT("proj.chain")));
	St.BouncesLeft = FMath::FloorToInt(S(TEXT("proj.bounce")));
	St.SplitsLeft = 1 + FMath::FloorToInt(S(TEXT("proj.split_times")));
	St.Speed0 = FVector2f(C.Velocity[Index].X, C.Velocity[Index].Y).Size();
	St.Radius0 = C.Radius[Index];
	St.Origin = FVector3d(C.Position[Index]);
	States.Add(C.Id[Index], MoveTemp(Ptr));
	return St;
}
void FDWProjectileAffixes::OnSpawn(int32 Index, int32 Root, const FDWSpawnInfo &Data)
{
	if (R.World.GetRoot(Root).Owner != R.Hero.Owner)
		return;
	auto &C = this->C();
	auto *RI = R.RootInfo(Root);
	const auto &Plan = R.World.GetPlan(R.World.GetRoot(Root).Plan);
	auto &St = NewState(Index);
	St.bMinion = (RI && RI->bMinion) || R.HasTag(Plan, C.Tags[Index], TEXT("summon"));
	St.Side = Data.Ordinal % 2 ? -1 : 1;
	St.Ordinal = Data.Ordinal;
	St.Count = Data.Count;
	const double Aim = FMath::Atan2(double(Data.Direction.Y), double(Data.Direction.X)), Speed = St.Speed0;
	auto SetDir = [&](double A) {
		C.Velocity[Index].X = float(FMath::Cos(A) * Speed);
		C.Velocity[Index].Y = float(FMath::Sin(A) * Speed);
	};
    if(St.bMinion&&RI){if(RI->VolleyTime!=R.Time){RI->VolleyTime=R.Time;RI->VolleyBatch=MakeShared<TMap<uint32,int32>>();RI->RepeatBatches.Reset();}St.Batch=RI->VolleyBatch;}
	if (!St.bMinion && (!RI || (RI->Kind != TEXT("cast") && !RI->bTriggered)))
		return;
	if (S(TEXT("proj.orbit")))
		C.Life[Index] = float(FMath::Max(double(C.Life[Index]), 3 * (1 + S(TEXT("proj.lifetime")) / 100)));
	const auto *Owner = R.World.GetOwner(R.Hero.Owner);
	const double HeroAim = Owner ? FMath::Atan2(double(Owner->Aim.Y), double(Owner->Aim.X)) : 0;
	if (S(TEXT("proj.layout_ring")) && Data.Count > 1)
		SetDir((St.bMinion?Aim:HeroAim) + double(Data.Ordinal) / Data.Count * 2 * UE_DOUBLE_PI);
	else if (S(TEXT("proj.layout_fan")) && Data.Count > 1)
		SetDir((St.bMinion?Aim:HeroAim) + (double(Data.Ordinal) / (Data.Count - 1) - .5) * UE_DOUBLE_PI / 4);
	if (S(TEXT("proj.layout_line")))
		St.Hold = Data.Ordinal * .06;
	if (S(TEXT("proj.layout_alternate")))
		C.Position[Index] +=
			FVector3f(float(-FMath::Sin(Aim) * .5 * St.Side), float(FMath::Cos(Aim) * .5 * St.Side), 0);
	if (S(TEXT("proj.layout_converge")))
	{
		const auto Near = R.EnemiesNear(R.HeroPosition() + FVector3d(FMath::Cos(Aim), FMath::Sin(Aim), 0) * 3, 6);
		if (Near.Num())
		{
			const auto P = R.Enemy(Near[0])->Position;
			const double A = double(Data.Ordinal) / FMath::Max(1, Data.Count) * 2 * UE_DOUBLE_PI;
			C.Position[Index].X = float(P.X + FMath::Cos(A) * 3);
			C.Position[Index].Y = float(P.Y + FMath::Sin(A) * 3);
			SetDir(FMath::Atan2(P.Y - C.Position[Index].Y, P.X - C.Position[Index].X));
		}
	}
	const int32 Origins = FMath::FloorToInt(S(TEXT("proj.origins")));
	const double Spacing = .6 * (1 + S(TEXT("proj.origin_spacing")) / 100);
	TArray<int32> Emitted{Index};
    for (int32 K = 1; K <= Origins; ++K)
	{
		const double Step = FMath::CeilToDouble(K / 2.) * Spacing * (K % 2 ? 1 : -1);
		const int32 Copy=Clone(Index, Root, 0, {-FMath::Sin(Aim) * Step, FMath::Cos(Aim) * Step, 0});if(Copy!=INDEX_NONE)Emitted.Add(Copy);
    }
    if(St.bMinion&&RI&&RI->bMinion){
        const int32 Repeats=FMath::FloorToInt(S(TEXT("proj.volleys")));
        const double Interval=.15*FMath::Max(.05,1-S(TEXT("proj.volley_interval"))/100);
        for(int32 K=1;K<=Repeats;++K){auto& Batch=RI->RepeatBatches.FindOrAdd(K);if(!Batch)Batch=MakeShared<TMap<uint32,int32>>();
            FVolley V;V.At=R.Time+K*Interval;V.Plan=R.World.GetRoot(Root).Plan;V.MinionId=RI->MinionId;V.bTriggered=RI->bTriggered;V.Batch=Batch;for(int32 Shot:Emitted)V.Shots.Add(Snapshot(Shot));Volleys.Add(MoveTemp(V));}
    }
}
FDWProjectileAffixes::FShot FDWProjectileAffixes::Snapshot(int32 I){const auto& C=this->C();FShot V;V.Position=C.Position[I];V.Velocity=C.Velocity[I];V.Life=C.Life[I];V.Radius=C.Radius[I];V.Damage=C.Damage[I];V.Element=C.Element[I];V.Kind=C.Kind[I];V.Tags=C.Tags[I];V.Generation=C.Generation[I];V.Lineage=C.Lineage[I];V.Flags=C.Flags[I]&FDWProjectiles::Parabolic;V.Pierce=C.Pierce[I];V.Ricochet=C.Ricochet[I];V.Return=C.Return[I];V.Reflect=C.Reflect[I];V.Refract=C.Refract[I];V.EndEvent=C.EndEvent[I];V.Gravity=C.Gravity[I];if(const auto* Entry=States.Find(C.Id[I])){V.Side=(*Entry)->Side;V.Gen=(*Entry)->Generation;V.Hold=(*Entry)->Hold;}return V;}
void FDWProjectileAffixes::RepeatVolley(const FVolley& V){auto* Entry=R.Minions->States.Find(V.MinionId);if(!Entry||(*Entry)->bDead||R.Minions->SummonOf(**Entry).bDead)return;const int32 Root=R.OpenRoot(V.Plan);if(Root==INDEX_NONE)return;auto* RI=R.RootInfo(Root);RI->bMinion=true;RI->MinionId=V.MinionId;RI->bTriggered=V.bTriggered;auto& C=this->C();for(const auto& Shot:V.Shots){auto D=FVector3f(Shot.Velocity.X,Shot.Velocity.Y,0);const float Speed=D.Size();const int32 I=R.World.SpawnProjectile(R.World.Roots[Root],Root,Shot.Position,D.GetSafeNormal(),Speed,Shot.Life,Shot.Radius,Shot.Damage,Shot.Element,Shot.Kind,Shot.Tags,Shot.Generation,Shot.Lineage);if(I==INDEX_NONE)break;C.Pierce[I]=Shot.Pierce;C.Ricochet[I]=Shot.Ricochet;C.Return[I]=Shot.Return;C.Reflect[I]=Shot.Reflect;C.Refract[I]=Shot.Refract;C.EndEvent[I]=Shot.EndEvent;C.Gravity[I]=Shot.Gravity;C.Velocity[I].Z=Shot.Velocity.Z;C.Flags[I]|=Shot.Flags;auto& St=NewState(I);St.bMinion=true;St.Batch=V.Batch;St.Side=Shot.Side;St.Generation=Shot.Gen;St.Hold=Shot.Hold;}R.World.ReleaseHeldRoot(Root);}
void FDWProjectileAffixes::DamageSource(int32 I,FDWStrikeOptions& O){const int32 Root=C().Root[I];if(auto* RI=R.RootInfo(Root);RI&&RI->bMinion){O.Plan=R.World.GetRoot(Root).Plan;O.MinionId=RI->MinionId;O.bTriggered=RI->bTriggered;}}
int32 FDWProjectileAffixes::Clone(int32 Index, int32 Root, double Angle, FVector3d Offset, double DamageScale,
								  double LifeScale, int32 Generation)
{
	auto &C = this->C();
	const auto *ParentEntry = States.Find(C.Id[Index]);
	const FDWAffixProjectile *Parent = ParentEntry ? ParentEntry->Get() : nullptr;
	const double Speed = FVector2f(C.Velocity[Index].X, C.Velocity[Index].Y).Size() / (Parent ? Parent->HomingScale : 1);
	const auto Direction =
		FVector3f(C.Velocity[Index].X,C.Velocity[Index].Y,0).GetSafeNormal().RotateAngleAxis(float(Angle * 180 / UE_DOUBLE_PI), FVector3f::UpVector);
	const int32 Spawned = R.World.SpawnProjectile(
		R.World.Roots[Root], Root, C.Position[Index] + FVector3f(Offset), Direction, float(Speed),
		float(C.Life[Index] * LifeScale), C.Radius[Index], float(C.Damage[Index] * DamageScale), C.Element[Index],
		C.Kind[Index], C.Tags[Index], C.Generation[Index], C.Lineage[Index]);
	if (Spawned == INDEX_NONE)
		return INDEX_NONE;
	C.Pierce[Spawned] = C.Pierce[Index];
	C.Gravity[Spawned] = C.Gravity[Index];
	C.Velocity[Spawned].Z = C.Velocity[Index].Z;
	C.Flags[Spawned] |= C.Flags[Index] & FDWProjectiles::Parabolic;
	C.Ricochet[Spawned] = C.Ricochet[Index];
	C.Return[Spawned] = C.Return[Index];
	C.Reflect[Spawned] = C.Reflect[Index];
    C.Refract[Spawned] = C.Refract[Index];
	C.EndEvent[Spawned] = C.EndEvent[Index];
	auto &St = NewState(Spawned);
	if (Parent)
	{
		St.bMinion = Parent->bMinion;
        St.Batch=Parent->Batch;
		St.Hold = Parent->Hold;
		St.Side = -Parent->Side;
	}
	St.Generation = Generation != INDEX_NONE ? Generation : Parent ? Parent->Generation : 0;
	return Spawned;
}
void FDWProjectileAffixes::Fire(const FDWProjectileFire &F)
{
	FDWStrikeOptions Source;if(F.FromIndex!=INDEX_NONE)DamageSource(F.FromIndex,Source);
    const int32 Plan=Source.Plan==INDEX_NONE?R.HeroPlan():Source.Plan,Root=R.OpenRoot(Plan);
	if (Root == INDEX_NONE)
		return;
	R.RootInfo(Root)->Depth = 2;R.RootInfo(Root)->MinionId=Source.MinionId;R.RootInfo(Root)->bMinion=Source.MinionId!=0;R.RootInfo(Root)->bTriggered=Source.bTriggered;
	auto &C = this->C();
	const auto &P = R.World.GetPlan(Plan);
	const int32 Kind = Source.MinionId?C.Kind[F.FromIndex]:FMath::Max(0, P.FindKind(F.Kind));
	const uint64 Tags = (Source.MinionId?C.Tags[F.FromIndex]:R.TagMask(P, F.Tags)) | TagBit(Tags::Projectile);
	for (int32 K = 0; K < F.Count; ++K)
	{
		const double A = F.bRadial ? F.Direction + double(K) / F.Count * 2 * UE_DOUBLE_PI
								   : F.Direction + (F.Count > 1 ? (double(K) / (F.Count - 1) - .5) * .5 : 0);
		const FVector3f Position(float(F.Position.X), float(F.Position.Y), .6f),
			Direction(float(FMath::Cos(A)), float(FMath::Sin(A)), 0);
		const int32 Spawned =
			R.World.SpawnProjectile(R.World.Roots[Root], Root, Position, Direction, float(F.Speed), float(F.Life),
									float(F.Radius), float(F.Damage), F.Element, uint16(Kind), Tags, 3, 0);
		if (Spawned == INDEX_NONE)
			break;
		auto &St = NewState(Spawned);
		St.Generation = F.Generation;St.bMinion=Source.MinionId!=0;if(St.bMinion&&F.From)St.Batch=F.From->Batch;
		if (F.From)
		{
			St.ChainsLeft = F.From->ChainsLeft;
			St.SplitsLeft = 0;
		}
		St.ChainsLeft = F.From && R.Chance(S(TEXT("proj.secondary_inherit_chain"))) ? F.From->ChainsLeft : 0;
		C.Pierce[Spawned] = F.From && R.Chance(S(TEXT("proj.secondary_inherit_pierce"))) && F.FromIndex != INDEX_NONE
								? C.Pierce[F.FromIndex]
								: 0;
		if (F.Exclude != InvalidId)
			R.World.HitAdd(Spawned, R.World.Roots[Root], F.Exclude);
	}
	R.World.ReleaseHeldRoot(Root);
}
void FDWProjectileAffixes::Secondary(int32 Index, FDWAffixProjectile &St, int32 Count, TOptional<double> Direction)
{
	auto &C = this->C();
	FDWProjectileFire F;
	F.Position = FVector3d(C.Position[Index]);
	F.Count = Count;
	F.bRadial = !Direction.IsSet();
	F.Direction = Direction.Get(0);
	F.Damage = C.Damage[Index] * .4;
	F.Element = C.Element[Index];
	F.Generation = 2;
	F.From = &St;
	F.FromIndex = Index;
	Fire(F);
}
uint32 FDWProjectileAffixes::PickTarget(const FVector3d &Position, double Range, const TSet<uint32> &Exclude)
{
	const auto List = R.EnemiesNear(Position, Range, Exclude);
	if (!List.Num())
		return InvalidId;
	if (S(TEXT("proj.prefer_elite")))
		for (uint32 Id : List)
			if (R.Enemy(Id)->bElite)
				return Id;
	if (S(TEXT("proj.prefer_low")))
		for (uint32 Id : List)
			if (R.LifeFraction(Id) <= .5)
				return Id;
	for (const auto *T : R.T(TEXT("proj_prefer_status")))
		for (uint32 Id : List)
			if (R.HasStatusChoice(Id, int32(T->Param(TEXT("status")))))
				return Id;
	return List[0];
}
void FDWProjectileAffixes::OnMotion(int32 Index, double Dt)
{
	auto &C = this->C();
	auto *P = States.Find(C.Id[Index]);
	if (!P)
		return;
	auto &St = **P;
	St.Age += Dt;
	FVector3d Velocity(C.Velocity[Index]);
	double Speed = FVector2d(Velocity.X, Velocity.Y).Size();
	if (Speed == 0)
		Speed = St.Speed0;
	if (St.Hold > 0)
	{
		St.Hold -= Dt;
		if (!St.SavedVelocity.IsSet())
			St.SavedVelocity = C.Velocity[Index];
		C.Velocity[Index] = FVector3f::ZeroVector;
		C.Life[Index] += float(Dt);
		if (St.Hold <= 0)
		{
			C.Velocity[Index] = St.SavedVelocity.GetValue();
			St.SavedVelocity.Reset();
		}
		return;
	}
	// Remove only the previous steering brake, preserving external speed modifiers.
	Speed /= St.HomingScale;
	St.HomingScale = 1;
	double Angle = FMath::Atan2(Velocity.Y, Velocity.X);
	if (St.bReturning)
	{
		const FVector3d Delta = R.HeroPosition() - FVector3d(C.Position[Index]);
		if (FVector2d(Delta.X, Delta.Y).Size() < .7)
		{
			Recall(Index, St);
			return;
		}
		Angle = FMath::Atan2(Delta.Y, Delta.X);
		Speed = St.Speed0 * (1 + S(TEXT("proj.return_speed")) / 100);
		C.Life[Index] = FMath::Max(C.Life[Index], C.Age[Index] + 1);
		if (R.Has(TEXT("proj_return_secondary")) && R.Time - St.LastSecondary >= .3)
		{
			St.LastSecondary = R.Time;
			const auto Near = R.EnemiesNear(FVector3d(C.Position[Index]), 5);
			if (Near.Num())
			{
				const auto Pos = R.Enemy(Near[0])->Position;
				for (const auto *T : R.T(TEXT("proj_return_secondary")))
					Secondary(Index, St, int32(T->Param(TEXT("n"))),
							  FMath::Atan2(Pos.Y - C.Position[Index].Y, Pos.X - C.Position[Index].X));
			}
		}
	}
	else if (S(TEXT("proj.orbit")) && St.Generation == 0)
	{
		const auto H = R.HeroPosition();
		const double Radius = 2 * (1 + S(TEXT("proj.orbit_radius")) / 100);
		const double Phi =
			St.Phi.IsSet() ? St.Phi.GetValue() : FMath::Atan2(C.Position[Index].Y - H.Y, C.Position[Index].X - H.X);
		St.Phi = Phi + 2 * UE_DOUBLE_PI * (1 + S(TEXT("proj.orbit_speed")) / 100) * Dt;
		C.Velocity[Index].X = float((H.X + FMath::Cos(St.Phi.GetValue()) * Radius - C.Position[Index].X) / Dt);
		C.Velocity[Index].Y = float((H.Y + FMath::Sin(St.Phi.GetValue()) * Radius - C.Position[Index].Y) / Dt);
		if (R.Time >= St.RehitAt)
		{
			C.Hits[Index] = {};
			St.RehitAt = R.Time + .5 * FMath::Max(.05, 1 - S(TEXT("proj.rehit_interval")) / 100);
		}
		St.Distance += FVector2f(C.Velocity[Index].X, C.Velocity[Index].Y).Size() * Dt;
		return;
	}
	else
	{
		if (S(TEXT("proj.accel")))
			Speed *= 1 + S(TEXT("proj.accel")) / 100 * Dt;
		if (S(TEXT("proj.decel")))
			Speed = FMath::Max(St.Speed0 * .2, Speed * (1 - .4 * Dt));
		if (S(TEXT("proj.curve")))
			Angle += St.Side * UE_DOUBLE_PI / 2 * Dt;
		if (S(TEXT("proj.wave")))
			Angle += .6 * FMath::Sin(UE_DOUBLE_PI * 3 * St.Age) - .6 * FMath::Sin(UE_DOUBLE_PI * 3 * (St.Age - Dt));
		if (Homing())
		{
			const auto &Root = R.World.GetRoot(C.Root[Index]);
			const double Range = 4 * (1 + S(TEXT("proj.homing_range")) / 100);
			auto *Target = R.Enemy(St.Target);
			if (Target && !(Target->Health > 0))
			{
				Target = nullptr;
				St.Target = InvalidId;
				if (!S(TEXT("proj.retarget_on_death")))
					St.bNoHoming = true;
			}
			if (Target && (R.World.HitContains(Index, Root, Target->Id) ||
				FVector2d(Target->Position.X - C.Position[Index].X, Target->Position.Y - C.Position[Index].Y).Size() > Range + Target->Radius))
			{
				Target = nullptr;
				St.Target = InvalidId;
			}
			if (!Target && !St.bNoHoming && St.Retargets > 0)
			{
				TSet<uint32> Exclude;
				for (const auto &Enemy : R.Enemies)
					if (R.World.HitContains(Index, Root, Enemy.Id)) Exclude.Add(Enemy.Id);
				St.Target = PickTarget(FVector3d(C.Position[Index]), Range, Exclude);
				Target = R.Enemy(St.Target);
				if (Target)
					--St.Retargets;
			}
			if (Target)
			{
				double Delta =
					FMath::Atan2(Target->Position.Y - C.Position[Index].Y, Target->Position.X - C.Position[Index].X) -
					Angle;
				while (Delta > UE_DOUBLE_PI)
					Delta -= 2 * UE_DOUBLE_PI;
				while (Delta < -UE_DOUBLE_PI)
					Delta += 2 * UE_DOUBLE_PI;
				const double TurnRate = FMath::Max(0., UE_DOUBLE_PI * (1 + S(TEXT("proj.homing_turn")) / 100));
				const double Turn = TurnRate * Dt, Applied = FMath::Clamp(Delta, -Turn, Turn);
				Angle += Applied;
				const double Distance = FVector2d(Target->Position.X - C.Position[Index].X, Target->Position.Y - C.Position[Index].Y).Size();
				const double Error = FMath::Abs(Delta - Applied), HitRadius = C.Radius[Index] + Target->Radius;
				// Braking keeps the target bearing slower than the available turn rate.
				// Aligned shots retain full speed; collision still uses the ordinary sweep.
				if (TurnRate > 0 && Distance > HitRadius && Error > FMath::Asin(FMath::Min(1., HitRadius / Distance)))
				{
					const double Cruise = Speed;
					Speed = FMath::Min(Speed, .35 * TurnRate * Distance / FMath::Max(.15, FMath::Sin(FMath::Min(Error, UE_DOUBLE_PI / 2))));
					St.HomingScale = Cruise > 0 ? Speed / Cruise : 1;
				}
			}
		}
	}
	C.Velocity[Index].X = float(FMath::Cos(Angle) * Speed);
	C.Velocity[Index].Y = float(FMath::Sin(Angle) * Speed);
	St.Distance += Speed * Dt;
	const auto Next = C.Position[Index] + C.Velocity[Index] * float(Dt);
	if ((FMath::Abs(Next.X) > 50 || FMath::Abs(Next.Y) > 50) && St.BouncesLeft > 0 && !St.bReturning)
	{
		if (FMath::Abs(Next.X) > 50)
			C.Velocity[Index].X = -C.Velocity[Index].X;
		if (FMath::Abs(Next.Y) > 50)
			C.Velocity[Index].Y = -C.Velocity[Index].Y;
		Bounced(Index, St);
	}
	if (!St.bSplitDone && St.Generation == 0)
	{
		for (const auto *T : R.T(TEXT("proj_split_after_distance")))
			if (St.Distance >= T->Param(TEXT("meters")))
			{
				St.bSplitDone = true;
				Split(Index, St, true);
				break;
			}
		for (const auto *T : R.T(TEXT("proj_split_after_time")))
			if (!St.bSplitDone && St.Age >= T->Param(TEXT("seconds")))
			{
				St.bSplitDone = true;
				Split(Index, St, true);
				break;
			}
	}
	if (St.bReturning)
		C.Radius[Index] = float(St.Radius0 * (1 + S(TEXT("proj.return_radius")) / 100));
}
void FDWProjectileAffixes::Bounced(int32 Index, FDWAffixProjectile &St)
{
	++St.Bounced;
	--St.BouncesLeft;
	auto &C = this->C();
	if (S(TEXT("proj.bounce_retarget")))
	{
		const uint32 Id = PickTarget(FVector3d(C.Position[Index]), 10);
		if (const auto *M = R.Enemy(Id))
		{
			const double A = FMath::Atan2(M->Position.Y - C.Position[Index].Y, M->Position.X - C.Position[Index].X),
						 V = FVector2f(C.Velocity[Index].X, C.Velocity[Index].Y).Size();
			C.Velocity[Index].X = float(FMath::Cos(A) * V);
			C.Velocity[Index].Y = float(FMath::Sin(A) * V);
			St.Target = Id;
		}
	}
	C.Age[Index] = 0;
}
bool FDWProjectileAffixes::OnWall(int32 Index)
{
	auto *P = States.Find(C().Id[Index]);
	if (!P || (*P)->BouncesLeft <= 0)
		return false;
	C().Velocity[Index].X *= -1;
	C().Velocity[Index].Y *= -1;
	Bounced(Index, **P);
	return true;
}
void FDWProjectileAffixes::Split(int32 Index, FDWAffixProjectile &St, bool Force)
{
	if (St.SplitsLeft <= 0 || (!Force && !(S(TEXT("proj.split_on_hit")) || R.Chance(S(TEXT("proj.split_chance"))))))
		return;
	--St.SplitsLeft;
	auto &C = this->C();
	const int32 Count = 2 + FMath::FloorToInt(S(TEXT("proj.split_count")));
	const double Spread = FMath::Max(0., 60 - S(TEXT("proj.split_spread"))) * UE_DOUBLE_PI / 180;
	for (int32 K = 0; K < Count; ++K)
	{
		const double A = Count > 1 ? (double(K) / (Count - 1) - .5) * Spread : 0;
		const int32 Spawned =
			Clone(Index, int32(C.Root[Index]), A, FVector3d::ZeroVector, .5 * (1 + S(TEXT("proj.split_damage")) / 100),
				  1 + S(TEXT("proj.split_distance")) / 100, St.Generation);
		if (Spawned != INDEX_NONE)
		{
			C.Hits[Spawned] = C.Hits[Index];
			if (auto *P = States.Find(C.Id[Spawned]))
			{
				(*P)->SplitsLeft = St.SplitsLeft;
				(*P)->bSplitDone = true;
			}
		}
	}
}
} // namespace DWRules
