#include "DWRuleCombatTriggers.h"
#include "DWRuleProjectileAffixes.h"

namespace DWRules
{
bool FDWProjectileAffixes::OnContact(int32 Index, int32 Target, const FDWDamageResult &Result)
{
	auto &C = this->C();
	auto *P = States.Find(C.Id[Index]);
	if (!P)
		return false;
	auto &St = **P;
	++St.Hits;
	const uint32 TargetId = R.World.Targets().ExternalId[Target];
	const FVector3d Position(C.Position[Index]);
	{
		if (Result.Applied > 0 && R.Chance(S(TEXT("proj.explode_chance"))))
		{
			FDWStrikeOptions O;
			O.Effect = TEXT("projectile_explosion");
			O.Depth = 2;DamageSource(Index,O);
			R.Strike(Position,
					 1.5 * FMath::Sqrt(1 + S(TEXT("proj.explode_area")) / 100) * R.AreaScale(C.Element[Index]),
					 Result.Applied * .6 * (1 + S(TEXT("proj.explode_damage")) / 100), C.Element[Index], O);
		}
		if (S(TEXT("proj.hit_zone")))
			{FDWStrikeOptions Source;DamageSource(Index,Source);Fields.Add({Position, 2, 0, FMath::Max(1., double(Result.Applied)) * .2, C.Element[Index],Source.Plan,Source.MinionId,Source.bTriggered});}
		if (St.Generation <= 1)
			Split(Index, St);
	}
	if (St.ChainsLeft > 0)
	{
		TSet<uint32> Exclude{TargetId};
		if (!S(TEXT("proj.chain_rehit")) || S(TEXT("proj.prefer_unhit")))
			for (int32 K = 0; K < C.Hits[Index].Num; ++K)
				Exclude.Add(C.Hits[Index].Ids[K]);
		auto Near = R.EnemiesNear(Position, 8 * (1 + S(TEXT("proj.chain_distance")) / 100), Exclude);
		if (!Near.Num() && S(TEXT("proj.chain_rehit")))
			Near = R.EnemiesNear(Position, 8 * (1 + S(TEXT("proj.chain_distance")) / 100), {TargetId});
		if (Near.Num())
		{
			--St.ChainsLeft;
			++St.Chained;
			const auto M = R.Enemy(Near[0])->Position;
			const double A = FMath::Atan2(M.Y - Position.Y, M.X - Position.X),
						 V = FVector2f(C.Velocity[Index].X, C.Velocity[Index].Y).Size();
			C.Velocity[Index].X = float(FMath::Cos(A) * V);
			C.Velocity[Index].Y = float(FMath::Sin(A) * V);
			C.Age[Index] = 0;
			if (S(TEXT("proj.chain_rehit")))
			{
				C.Hits[Index] = {};
				C.Hits[Index].Ids[0] = TargetId;
				C.Hits[Index].Num = 1;
			}
			for (const auto *T : R.T(TEXT("proj_chain_secondary")))
				Secondary(Index, St, int32(T->Param(TEXT("n"))));
			R.Triggers->Event(TEXT("trig_chain"), {Position, TargetId, true});
			if (St.ChainsLeft == 0 && S(TEXT("proj.return_chain_done")))
				StartReturn(Index, St);
			return true;
		}
	}
	if (St.bReturning || (S(TEXT("proj.orbit")) && St.Generation == 0))
		return true;
	if (R.Chance(S(TEXT("proj.pierce_chance"))))
	{
		Pierced(Index, St, Position, TargetId);
		return true;
	}
	if (C.Pierce[Index] > 0)
	{
		Pierced(Index, St, Position, TargetId);
		return false;
	}
	if (S(TEXT("proj.return_pierce_done")) && (St.Pierced > 0 || C.Pierce[Index] == 0))
	{
		StartReturn(Index, St);
		return true;
	}
	return false;
}
void FDWProjectileAffixes::Pierced(int32 Index, FDWAffixProjectile &St, const FVector3d &Position, uint32 Target)
{
	++St.Pierced;
	auto &C = this->C();
	C.Velocity[Index] *= float(1 + S(TEXT("proj.pierce_speed")) / 100);
	C.Radius[Index] = float(C.Radius[Index] * (1 + S(TEXT("proj.pierce_radius")) / 100));
	R.Triggers->Event(TEXT("trig_pierce"), {Position, Target, true});
	for (const auto *T : R.T(TEXT("proj_pierce_secondary")))
		if (++St.PiercedSinceSecondary >= FMath::Max(1., T->Param(TEXT("per"))))
		{
			St.PiercedSinceSecondary = 0;
			Secondary(Index, St, 2);
		}
}
void FDWProjectileAffixes::StartReturn(int32 Index, FDWAffixProjectile &St)
{
	if (St.bReturning)
		return;
	St.bReturning = true;
	auto &C = this->C();
	C.Flags[Index] &= ~(FDWProjectiles::PendingImpact | FDWProjectiles::Parabolic);
	C.Gravity[Index] = 0;
	C.Velocity[Index].Z = 0;
	if (S(TEXT("proj.return_rehit")))
		C.Hits[Index] = {};
	R.Triggers->Event(TEXT("trig_return"), {FVector3d(C.Position[Index]), InvalidId, true});
}
void FDWProjectileAffixes::Recall(int32 Index, FDWAffixProjectile &St)
{
	St.bRecalled = true;
	R.Character.Recover(ECharacterPool::Mana, S(TEXT("proj.recall_mana")));
	for (auto *T : R.T(TEXT("proj_recall_empower")))
	{
		T->Accumulator += 1;
		if (T->Accumulator >= FMath::Max(1., T->Param(TEXT("per"))))
		{
			T->Accumulator = 0;
			R.Character.PendingEmpower += T->Param(TEXT("n"));
		}
	}
	++Recalled;
	R.World.EndProjectile(Index, EEndReason::Impact);
}
bool FDWProjectileAffixes::OnEnd(int32 Index, EEndReason Reason)
{
	auto &C = this->C();
	const uint32 Id = C.Id[Index];
	auto *P = States.Find(Id);
	if (!P)
		return false;
	auto &St = **P;
	if (St.bRecalled)
	{
		States.Remove(Id);
		return false;
	}
	if (!St.bReturning && St.Generation == 0)
	{
		if (Reason == EEndReason::Expired && S(TEXT("proj.return_at_max")))
		{
			StartReturn(Index, St);
			C.Age[Index] = 0;
			return true;
		}
		if (Reason == EEndReason::Impact && S(TEXT("proj.return_pierce_done")))
		{
			StartReturn(Index, St);
			return true;
		}
	}
	{
		if (S(TEXT("proj.explode_on_expire")))
		{
			FDWStrikeOptions O;
			O.Effect = TEXT("projectile_explosion");
			O.Depth = 2;DamageSource(Index,O);
			R.Strike(FVector3d(C.Position[Index]),
					 1.5 * FMath::Sqrt(1 + S(TEXT("proj.explode_area")) / 100) * R.AreaScale(C.Element[Index]),
					 C.Damage[Index] * .6 * (1 + S(TEXT("proj.explode_damage")) / 100), C.Element[Index], O);
		}
		if (St.Generation < 2)
			for (const auto *T : R.T(TEXT("proj_expire_secondary")))
				Secondary(Index, St, int32(T->Param(TEXT("n"))));
	}
	States.Remove(Id);
	return false;
}
int32 FDWProjectileAffixes::OnRicochet(int32 Index, const FDWEvent &Event, double Radius)
{
	if (!S(TEXT("proj.chain_rehit")))
		return INDEX_NONE;
	const auto &T = R.World.Targets();
	int32 Best = INDEX_NONE;
	double BestDistance = 1.e30;
	for (int32 I = 0; I < T.Num(); ++I)
	{
		if (!(T.Health[I] > 0) || I == Event.TargetIndex || T.Team[I] == 0)
			continue;
		const double D = FVector2f(T.Position[I].X - Event.Position.X, T.Position[I].Y - Event.Position.Y).Size();
		if (D < BestDistance && D <= 8 * (1 + S(TEXT("proj.chain_distance")) / 100))
		{
			BestDistance = D;
			Best = I;
		}
	}
	return Best;
}
void FDWProjectileAffixes::OnRedirect(int32 Index, ERedirect Mode)
{
	auto &C = this->C();
	auto *P = States.Find(C.Id[Index]);
	if (!P)
		return;
	auto &St = **P;
	if (Mode == ERedirect::Return)
	{
		St.bReturning = true;
		if (S(TEXT("proj.return_rehit")))
			C.Hits[Index] = {};
		R.Triggers->Event(TEXT("trig_return"), {FVector3d(C.Position[Index]), InvalidId, true});
	}
	else if (Mode == ERedirect::Ricochet)
	{
		++St.Chained;
		for (const auto *T : R.T(TEXT("proj_chain_secondary")))
			Secondary(Index, St, int32(T->Param(TEXT("n"))));
		R.Triggers->Event(TEXT("trig_chain"), {FVector3d(C.Position[Index]), InvalidId, true});
	}
}
void FDWProjectileAffixes::DamageMod(FDWDamageMod &M, const FDWDamageInfo &Info, uint32 Target, FDWCombatRoot *RI)
{
	const auto *P = States.Find(Info.SourceId);
	const auto *St = P ? P->Get() : nullptr;
	if (St)
	{
		M.More *= 1 + FMath::Min(100., (St->Hits - 1) * S(TEXT("proj.per_hit_damage"))) / 100;
		M.More *= FMath::Pow(1 - .15 * FMath::Max(0., 1 - S(TEXT("proj.pierce_decay")) / 100), St->Pierced) *
				  (1 + FMath::Min(100., St->Pierced * S(TEXT("proj.pierce_damage"))) / 100);
		M.More *= FMath::Pow(1 - .15 * FMath::Max(0., 1 - S(TEXT("proj.chain_decay")) / 100), St->Chained) *
				  (1 + FMath::Min(100., St->Chained * S(TEXT("proj.chain_damage"))) / 100);
		M.More *= 1 + St->Bounced * S(TEXT("proj.bounce_damage")) / 100;
		if (St->bReturning)
			M.More *= 1 + S(TEXT("proj.return_damage")) / 100;
		if (St->Distance < 3)
			M.Increased += S(TEXT("proj.near_damage"));
		if (St->Distance > 7)
			M.Increased += S(TEXT("proj.far_damage"));
		for (const auto *T : R.T(TEXT("proj_distance_damage")))
			M.Increased += FMath::Min(T->Param(TEXT("max")),
									  FMath::FloorToDouble(St->Distance / FMath::Max(.1, T->Param(TEXT("meters")))) *
										  T->Param(TEXT("n")));
		if (S(TEXT("proj.decel")))
		{
			const auto &C = this->C();
			const auto V = C.Velocity[Info.SourceIndex];
			M.More *=
				1 + .2 * FMath::Min(1., FMath::Max(0., 1 - FVector2f(V.X, V.Y).Size() / FMath::Max(1.e-6, St->Speed0)) /
											.4);
		}
	}
	auto* Batch=St&&St->Batch.IsValid()?St->Batch.Get():(RI&&RI->Kind==TEXT("cast")?&RI->Batch:nullptr);
    if(Batch)
    {
        const int32 Count=Batch->FindRef(Target);
		if (Count >= 3 + FMath::FloorToInt(S(TEXT("proj.batch_cap"))) && !(St && St->bReturning) &&
			!S(TEXT("proj.orbit")))
		{
			M.bMiss = true;
			return;
		}
		Batch->Add(Target,Count+1);
		if (Count > 0)
			M.More *= FMath::Pow(1 - .3 * FMath::Max(0., 1 - S(TEXT("proj.batch_decay")) / 100), Count);
	}
}
void FDWProjectileAffixes::OnCast(int32 Root, int32 Plan)
{
	const int32 Count = FMath::FloorToInt(S(TEXT("proj.volleys")));
	if (!(Count > 0) || !R.SkillFamily(Plan).bProjectile || R.SkillFamily(Plan).bSummon)
		return;
	const double Interval = .15 * FMath::Max(.05, 1 - S(TEXT("proj.volley_interval")) / 100);
	for (int32 K = 1; K <= Count; ++K)
		Volleys.Add({R.Time + K * Interval, INDEX_NONE, Plan, 0, R.World.GetRoot(Root).TargetPoint});
}
void FDWProjectileAffixes::Tick(double Dt)
{
	const auto Due = Volleys.FilterByPredicate([&](const auto &V) { return V.At <= R.Time; });
	Volleys.RemoveAll([&](const auto &V) { return V.At <= R.Time; });
	for (const auto &V : Due)
	{
        if(!V.Shots.IsEmpty()){RepeatVolley(V);continue;}
		const auto *Owner = R.World.GetOwner(R.Hero.Owner);
		const FVector3f Aim = Owner ? Owner->Aim : FVector3f(1, 0, 0);
		FVector3f Origin = FVector3f(R.HeroPosition()) + Aim * .6f;
		Origin.Z = .6f;
		const int32 Root =
			R.World.Cast(V.Plan, R.Hero.Owner, Origin, Aim, uint64(FMath::FloorToDouble(R.Random() * 1.e9)),
						 float(R.Hero.Power), V.TargetPoint);
		if (auto *Info = R.RootInfo(Root, true))
			Info->Kind = TEXT("cast");
	}
	for (auto &F : Fields)
	{
		F.Left -= Dt;
		F.Tick += Dt;
		if (F.Tick >= .5)
		{
			F.Tick -= .5;
			FDWStrikeOptions O;
			O.Effect = TEXT("projectile_zone");
			O.Depth = 2;O.Plan=F.Plan;O.MinionId=F.MinionId;O.bTriggered=F.bTriggered;
			R.Strike(F.Position, FieldRadius, F.Damage * .5, F.Element, O);
		}
	}
	Fields.RemoveAll([](const auto &F) { return F.Left <= 0; });
	if (States.Num() > 5000)
	{
		TArray<uint32> Ids;
		States.GetKeys(Ids);
		for (int32 I = 0; I < FMath::Min(2500, Ids.Num()); ++I)
			States.Remove(Ids[I]);
	}
}
} // namespace DWRules
