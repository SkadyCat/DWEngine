#include "DWRuleMinions.h"
#include "DWRuleCombatTriggers.h"

namespace DWRules
{
namespace
{
double MinionDistance(const FVector3d &A, const FVector3d &B)
{
	return FVector2d::Distance({A.X, A.Y}, {B.X, B.Y});
}
} // namespace
FDWSummon &FDWMinions::SummonOf(FDWMinionState &State) const
{
	for (auto &S : R.World.Summons)
		if (S.Id == State.Id)
			return S;
	return State.Snapshot;
}
TArray<FDWMinionState *> FDWMinions::Alive() const
{
	TArray<FDWMinionState *> Out;
	for (const auto &Sm : R.World.Summons)
		if (!Sm.bDead && !Sm.bStorm)
		{
			const auto *P = States.Find(Sm.Id);
			if (P && !(*P)->bDead && R.World.GetRoot(Sm.Root).Owner == R.Hero.Owner)
				Out.Add(P->Get());
		}
	return Out;
}
TArray<FDWMinionState *> FDWMinions::Counted() const
{
	auto List = Alive();
	List.RemoveAll([](const auto *S) { return S->bTemporary || S->bEmpowered; });
	return List;
}
FDWMinionState *FDWMinions::StateOfTarget(uint32 Id) const
{
	const auto *P = Id >= MinionTargetBase ? States.Find(Id - MinionTargetBase) : nullptr;
	return P ? P->Get() : nullptr;
}
int32 FDWMinions::BaseCap(int32 Cap)
{
	double N = Cap + FMath::FloorToDouble(S(TEXT("minion.cap"))) + FMath::FloorToDouble(S(TEXT("minion.cap_same")));
	for (const auto *T : R.T(TEXT("minion_fewer_damage")))
		N -= T->Param(TEXT("cap"));
	for (const auto *T : R.T(TEXT("minion_fewer_life")))
		N -= T->Param(TEXT("cap"));
	for (const auto *T : R.T(TEXT("minion_more_weaker")))
		N += T->Param(TEXT("cap"));
	return int32(FMath::Max(1., N));
}
double FDWMinions::Reservation() const
{
	return 20 * FMath::Max(.05, 1 - S(TEXT("minion.spirit")) / 100);
}
int32 FDWMinions::CapFor(int32 Cap, uint16 Kind, int32 Root)
{
	if (R.World.GetRoot(Root).Owner != R.Hero.Owner)
		return Cap;
	int32 N = BaseCap(Cap);
	if (R.Has(TEXT("minion_merge")))
		N = 1;
	const FName Name = R.World.GetPlan(R.World.GetRoot(Root).Plan).KindNames[Kind];
	int32 Others = 0;
	for (const auto *St : Counted())
		Others += St->Kind != Name;
	LastCap = N;
	return FMath::Max(0, FMath::Min(N, FMath::FloorToInt(100 / Reservation()) - Others));
}
void FDWMinions::OnSummon(FDWSummon &Sm, int32 Root, const FDWSummon *Replaced)
{
	if (R.World.GetRoot(Root).Owner != R.Hero.Owner || Sm.bStorm)
		return;
	const int32 Plan = R.World.GetRoot(Root).Plan;
	const auto *RI = R.RootInfo(Root);
	const bool Triggered = RI && RI->bTriggered;
	const int32 Own = R.OpenRoot(Plan);
	if (Own != INDEX_NONE)
	{
		Sm.Root = Own;
		--R.World.Roots[Root].Live;
		auto *OI = R.RootInfo(Own);
		OI->Kind = TEXT("minion");
		OI->bTriggered = Triggered;
	}
	const auto Family = R.SkillFamily(Plan);
	const double Scale = 1 + (S(TEXT("minion.duration")) + (Triggered ? S(TEXT("trigger.duration")) : 0) +
							  (Family.bFrost ? S(TEXT("frost.duration")) : 0)) /
								 100;
	Sm.Expire = float(R.World.GetClock() + FMath::Max(.05, (Sm.Expire - R.World.GetClock()) * Scale));
	auto &St = Create(Sm, R.HasTag(R.World.GetPlan(Plan), Sm.Tags, TEXT("melee")));
	if (auto *Info = R.RootInfo(Own))
	{
		Info->bMinion = true;
		Info->MinionId = St.Id;
	}
	if (Replaced && LastReplacedLife.IsSet())
	{
		St.Life = FMath::Min(St.MaxLife * 1.5,
							 St.Life + LastReplacedLife.GetValue() * S(TEXT("minion.replace_transfer")) / 100);
		LastReplacedLife.Reset();
	}
	EnforceControlCap(&St);
    Spawned(St);
	if (!bCloning)
	{
		TGuardValue<bool> Guard(bCloning, true);
		for (int32 K = 0; K < FMath::FloorToInt(S(TEXT("minion.per_cast"))); ++K)
			Copy(Sm, {(K + 1) * .8, (K % 2 ? 1 : -1) * .6, 0});
	}
	for (const auto *T : R.T(TEXT("minion_merge")))
	{
		for (auto *Other : Alive())
			if (Other != &St && Other->Kind == St.Kind)
			{
				St.Merged += 1 + Other->Merged;
				Remove(*Other);
			}
	}
}
FDWMinionState &FDWMinions::Create(const FDWSummon &Sm, bool Mobile, bool Temporary, bool Empowered)
{
	auto Ptr = MakeUnique<FDWMinionState>();
	auto &St = *Ptr;
	St.Id = Sm.Id;
	St.Snapshot = Sm;
	St.Kind = R.World.GetPlan(R.World.GetRoot(Sm.Root).Plan).KindNames[Sm.Kind];
	St.bMobile = Mobile;
	St.bTemporary = Temporary;
	St.bEmpowered = Empowered;
	St.Born = R.Time;
	St.BaseInterval = Sm.Interval;
	St.BaseRange = Sm.Range;
	St.ImmuneUntil = R.Time + S(TEXT("minion.immune"));
	St.Element = St.Kind == TEXT("sentinel") ? 1 : 3;
	St.BaseDuration = FMath::Max(1., double(Sm.Expire - R.World.GetClock()));
	St.Plan = R.World.GetRoot(Sm.Root).Plan;
	States.Add(St.Id, MoveTemp(Ptr));
	St.MaxLife = St.Life = MaxLife(St);
	St.MaxShield = St.Shield = MaxShield(St);
	return St;
}
FDWMinionState *FDWMinions::Copy(const FDWSummon &Source, FVector3d Offset, bool Temporary, bool Empowered,
								 double Duration, TOptional<FVector3d> At, int32 PlanOverride)
{
	const auto *Original = States.Find(Source.Id);
	const int32 Plan = PlanOverride != INDEX_NONE ? PlanOverride
					   : Original				  ? (*Original)->Plan
												  : R.World.GetRoot(Source.Root).Plan;
	const int32 Own = R.OpenRoot(Plan);
	if (Own == INDEX_NONE || R.World.Summons.Num() >= 128)
	{
		if (Own != INDEX_NONE)
			R.World.ReleaseHeldRoot(Own);
		return nullptr;
	}
	FDWSummon Sm = Source;
	Sm.Id = R.World.NextSummonId++;
	Sm.Root = Own;
	Sm.bDead = Sm.bReplaced = Sm.bHold = false;
	Sm.Position = FVector3f((At.IsSet() ? At.GetValue() : FVector3d(Source.Position)) + Offset);
	Sm.Expire = Duration >= 0 ? float(R.World.GetClock() + Duration) : Source.Expire;
	Sm.NextFire = R.World.GetClock();
	Sm.Interval = Original ? float((*Original)->BaseInterval) : Source.Interval;
	R.World.Summons.Add(Sm);
	auto &St = Create(Sm, Original && (*Original)->bMobile, Temporary, Empowered);
	auto *Info = R.RootInfo(Own);
	Info->Kind = TEXT("minion");
	Info->bMinion = true;
	Info->MinionId = St.Id;
	EnforceControlCap(&St);
    Spawned(St);
	return &St;
}
void FDWMinions::Spawned(FDWMinionState &St)
{
	const auto Sm = SummonOf(St);
	if (S(TEXT("minion.spawn_blast")))
	{
		FDWStrikeOptions O;
		O.Effect = TEXT("minion_spawn");
		O.Depth = 2;
		R.Strike(FVector3d(Sm.Position), 2.5 * R.AreaScale(-1, false, true), R.Hero.Power, St.Element, O);
	}
	if (S(TEXT("minion.spawn_heal")) > 0)
	{
		R.Character.Recover(ECharacterPool::Life, S(TEXT("minion.spawn_heal")));
		for (auto *O : Alive())
			if (O != &St)
				Heal(*O, S(TEXT("minion.spawn_heal")));
	}
	for (const auto *T : R.T(TEXT("minion_spawn_speed")))
		St.Buffs.Add(TEXT("spawn"), {T->Param(TEXT("n")), R.Time + T->Param(TEXT("duration"))});
	R.Triggers->Event(TEXT("trig_summon_spawn"), {FVector3d(Sm.Position), InvalidId, true});
}
int32 FDWMinions::EmptySlots() const
{
	return FMath::Max(0, LastCap - Counted().Num());
}
double FDWMinions::MaxLife(const FDWMinionState &St)
{
	double Inc = S(TEXT("minion.life_increased")) + EmptySlots() * S(TEXT("minion.empty_life"));
	for (const auto *T : R.T(TEXT("minion_kill_life")))
		Inc += FMath::Min(T->Param(TEXT("max")), St.Kills * T->Param(TEXT("n")));
	const auto Merge = R.T(TEXT("minion_merge"));
	const double MergeN = Merge.Num() ? Merge[0]->Param(TEXT("n")) : 0;
	double More = S(TEXT("minion.life_more")) * (1 + St.Merged * MergeN / 100) * (St.bEmpowered ? 2 : 1);
	for (const auto *T : R.T(TEXT("minion_fewer_life")))
		More *= 1 + T->Param(TEXT("n")) / 100;
	const double Inherit =
		(R.Character.Base.Life + R.Character.S(TEXT("life.base"))) * S(TEXT("minion.inherit_life")) / 100;
	return FMath::Max(1., (150 + S(TEXT("minion.life_base")) + Inherit) * FMath::Max(0., 1 + Inc / 100) * More);
}
double FDWMinions::MaxShield(const FDWMinionState &St) const
{
	const double Inherit = R.Character.S(TEXT("shield.base")) * S(TEXT("minion.inherit_shield")) / 100;
	return FMath::Max(0., (S(TEXT("minion.shield_base")) + Inherit) *
							  FMath::Max(0., 1 + S(TEXT("minion.shield_increased")) / 100) * (St.bEmpowered ? 2 : 1));
}
double FDWMinions::Armour() const
{
	return (100 + R.Character.ArmourValue() * S(TEXT("minion.inherit_armour")) / 100) *
		   (1 + S(TEXT("minion.armour")) / 100);
}
double FDWMinions::Evasion() const
{
	return (100 + R.Character.EvasionValue() * S(TEXT("minion.inherit_evasion")) / 100) *
		   (1 + S(TEXT("minion.evasion")) / 100);
}
void FDWMinions::Heal(FDWMinionState &St, double Amount)
{
	if (!St.bDead && Amount > 0)
		St.Life = FMath::Min(St.MaxLife, St.Life + Amount * FMath::Max(0., 1 + S(TEXT("minion.heal_received")) / 100));
}
double FDWMinions::SpeedBonus(const FDWMinionState &St)
{
	double Bonus =
		(St.bMobile ? S(TEXT("minion.attack_speed")) : S(TEXT("minion.cast_speed"))) + S(TEXT("minion.skill_cooldown"));
	Bonus += R.Character.ActionSpeed(St.bMobile) * S(TEXT("minion.inherit_speed")) / 100;
	if (St.Life <= St.MaxLife * .35)
		Bonus += S(TEXT("minion.low_life_speed"));
	int32 Same = 0;
	for (const auto *O : Alive())
		Same += O->Kind == St.Kind;
	for (const auto *T : R.T(TEXT("minion_per_same_speed")))
		Bonus += FMath::Min(T->Param(TEXT("max")), T->Param(TEXT("n")) * Same);
	if (const auto *Buff = St.Buffs.Find(TEXT("spawn")))
		if (Buff->Until > R.Time)
			Bonus += Buff->Value;
	return Bonus;
}
void FDWMinions::DamageMod(FDWDamageMod &M, FDWCombatRoot *RI, uint8 Element, uint32 Target)
{
	auto *Ptr = RI ? States.Find(RI->MinionId) : nullptr;
	auto *St = Ptr ? Ptr->Get() : nullptr;
	M.Increased += S(TEXT("minion.damage")) + (Element == 3 ? S(TEXT("minion.inc.physical")) : 0) +
				   (Element <= 2 ? S(TEXT("minion.inc.elemental")) : 0) +
				   (Element == 5 ? S(TEXT("minion.inc.void")) : 0);
	M.Increased += EmptySlots() * S(TEXT("minion.empty_damage"));
	for (const auto *T : R.T(TEXT("minion_per_minion_damage")))
		M.Increased += FMath::Min(T->Param(TEXT("max")), T->Param(TEXT("n")) * Alive().Num());
	M.More *= S(TEXT("minion.more"));
	for (const auto *T : R.T(TEXT("minion_fewer_damage")))
		M.More *= 1 + T->Param(TEXT("n")) / 100;
	for (const auto *T : R.T(TEXT("minion_more_weaker")))
		M.More *= FMath::Max(0., 1 - T->Param(TEXT("n")) / 100);
	if (R.Time < YouCritUntil)
		M.Increased += YouCritValue;
	static const TCHAR *Names[] = {TEXT("fire"),	 TEXT("frost"),	 TEXT("lightning"),
								   TEXT("physical"), TEXT("poison"), TEXT("void")};
	M.Flat += S(FName(*(FString(TEXT("minion.flat.")) + Names[Element]))) +
			  R.Character.Offense().Flat[Element] * S(TEXT("minion.inherit_flat")) / 100;
	M.CriticalScale += S(TEXT("minion.crit")) / 100;
	if (R.Hero.Plan >= 0)
		M.Critical += FMath::Max(0., double(R.World.GetPlan(R.Hero.Plan).Stats.Get(EStat::OwnerCriticalChance)) - 5) *
					  S(TEXT("minion.inherit_crit")) / 100;
	M.CriticalMultiplier += S(TEXT("minion.crit_multi"));
	if (!R.Chance(FMath::Min(100., 90 * (1 + S(TEXT("minion.accuracy")) / 100))))
	{
		M.bMiss = true;
		return;
	}
	if (!St)
		return;
	const auto Merge = R.T(TEXT("minion_merge"));
	M.More *= (1 + St->Merged * (Merge.Num() ? Merge[0]->Param(TEXT("n")) : 0) / 100) * (St->bEmpowered ? 2 : 1);
	if (St->Life >= St->MaxLife - 1.e-6)
		M.Increased += S(TEXT("minion.full_life_damage"));
	if (Target == R.Marked)
		M.Increased += S(TEXT("minion.marked_damage"));
	if (const auto *Buff = St->Buffs.Find(TEXT("command")))
		if (Buff->Until > R.Time)
			M.Increased += Buff->Value;
	for (const auto *T : R.T(TEXT("minion_age_damage")))
		M.Increased +=
			FMath::Min(T->Param(TEXT("max")),
					   FMath::FloorToDouble((R.Time - St->Born) / FMath::Max(.1, T->Param(TEXT("seconds")))) *
						   T->Param(TEXT("n")));
	for (const auto *T : R.T(TEXT("minion_ending_damage")))
		if (SummonOf(*St).Expire - R.World.GetClock() < T->Param(TEXT("seconds")))
			M.Increased += T->Param(TEXT("n"));
}
int32 FDWMinions::Target(FDWSummon &Sm, int32 Nearest)
{
	auto *Ptr = States.Find(Sm.Id);
	if (!Ptr || Sm.bStorm)
		return Nearest;
	auto &St = **Ptr;
	const double BaseRange = R.Character.Sheet.Stats.Contains(TEXT("minion.seek_range")) ? S(TEXT("minion.seek_range")) : St.BaseRange;
	Sm.Range = float(FMath::Max(0., BaseRange) * FMath::Max(0., 1 + S(TEXT("minion.seek")) / 100));
	const auto List = Sm.Range > 0 ? R.EnemiesNear(FVector3d(Sm.Position), Sm.Range) : TArray<uint32>{};
	uint32 Pick = InvalidId;
	if (St.CommandAt <= R.Time && List.Contains(St.CommandTarget))
		Pick = St.CommandTarget;
	auto Prefer = [&](TFunctionRef<bool(uint32)> Test) {
		if (Pick == InvalidId)
			for (uint32 Id : List)
				if (Test(Id))
				{
					Pick = Id;
					break;
				}
	};
	if (S(TEXT("minion.prefer_marked")))
		Prefer([&](uint32 Id) { return Id == R.Marked; });
	if (S(TEXT("minion.prefer_elite")))
		Prefer([&](uint32 Id) { return R.Enemy(Id)->bElite; });
	if (S(TEXT("minion.prefer_low")))
		Prefer([&](uint32 Id) { return R.LifeFraction(Id) <= .5; });
	if (S(TEXT("minion.prefer_attacker")))
		Prefer([&](uint32 Id) {
			const auto *T = R.Attackers.Find(Id);
			return T && *T > R.Time - 4;
		});
	for (const auto *T : R.T(TEXT("minion_prefer_status")))
		Prefer([&](uint32 Id) { return R.HasStatusChoice(Id, int32(T->Param(TEXT("status")))); });
	if (Pick == InvalidId && List.Num())
		Pick = List[0];
	St.Target = Pick;
	if (St.bMobile)
	{
		const auto *M = R.Enemy(Pick);
		const double Reach = 1.3 * (1 + S(TEXT("minion.attack_range")) / 100) + (M ? M->Radius : 0);
		Sm.bHold = !M || MinionDistance(M->Position, FVector3d(Sm.Position)) > Reach;
	}
	return Pick != InvalidId ? R.World.TargetIndexOf(Pick) : (Nearest >= 0 && List.Num() ? Nearest : INDEX_NONE);
}
void FDWMinions::Tick(double Dt)
{
    EnforceControlCap();
	const FVector3d Hero = R.HeroPosition();
	TArray<uint32> Ids;
	States.GetKeys(Ids);
	for (uint32 Id : Ids)
	{
		auto &St = *States[Id];
		if (!R.World.Summons.ContainsByPredicate([&](const auto &Sm) { return Sm.Id == Id; }))
		{
			if (St.bDead)
				States.Remove(Id);
			continue;
		}
		auto &Sm = SummonOf(St);
		if (Sm.bDead || St.bDead)
		{
			if (Sm.bDead)
				States.Remove(Id);
			continue;
		}
		St.MaxLife = MaxLife(St);
		St.MaxShield = MaxShield(St);
		Sm.Interval = float(FMath::Max(.05, St.BaseInterval / FMath::Max(.1, 1 + SpeedBonus(St) / 100)));
		Heal(St, (S(TEXT("minion.regen_flat")) + St.MaxLife * S(TEXT("minion.regen_percent")) / 100) * Dt);
		St.SinceHit += Dt;
		if (St.SinceHit > 3)
			St.Shield = FMath::Min(St.MaxShield, St.Shield + St.MaxShield * .2 * Dt);
		if (St.CommandAt <= R.Time && !St.bCommanded)
		{
			St.bCommanded = true;
			for (const auto *T : R.T(TEXT("minion_command_damage")))
				St.Buffs.Add(TEXT("command"), {T->Param(TEXT("n")), R.Time + T->Param(TEXT("duration"))});
			for (const auto *T : R.T(TEXT("minion_command_move")))
				St.Buffs.Add(TEXT("move"), {T->Param(TEXT("n")), R.Time + T->Param(TEXT("duration"))});
		}
		if (St.bMobile)
		{
			const auto *Target = R.Enemy(St.Target);
			FVector3d Goal = Hero + FVector3d(1, 1, 0);
			if (Target && MinionDistance(Target->Position, Hero) <= 9 * (1 + S(TEXT("minion.chase")) / 100))
				Goal = Target->Position;
			const auto *Buff = St.Buffs.Find(TEXT("move"));
			const double Bonus = Buff && Buff->Until > R.Time ? Buff->Value : 0;
			const double Speed = 4 * (1 + (S(TEXT("minion.move_speed")) + Bonus) / 100) * Dt;
			FVector3d Delta = Goal - FVector3d(Sm.Position);
			Delta.Z = 0;
			const double D = Delta.Size(), Stop = Target ? 1 : 1.5;
			if (D > Stop)
				Sm.Position += FVector3f(Delta / D * FMath::Min(Speed, D - Stop));
		}
		for (const auto *T : R.T(TEXT("minion_teleport")))
			if (MinionDistance(FVector3d(Sm.Position), Hero) > T->Param(TEXT("meters")))
			{
				Sm.Position.X = float(Hero.X + 1);
				Sm.Position.Y = float(Hero.Y);
			}
		St.Snapshot = Sm;
	}
	const auto Due = Revives.FilterByPredicate([&](const auto &V) { return V.At <= R.Time; });
	Revives.RemoveAll([&](const auto &V) { return V.At <= R.Time; });
	for (const auto &V : Due)
		if (V.Summon.Expire - R.World.GetClock() > .5)
			Copy(V.Summon, FVector3d::ZeroVector, false, false, V.Summon.Expire - R.World.GetClock(),
				 Hero + FVector3d(1, 0, 0), V.Plan);
	const double Share = S(TEXT("minion.recovery_share")), Restored = R.Character.Flow.LifeRestored;
	if (Share > 0 && Restored > 0)
	{
		const auto List = Alive();
		for (auto *St : List)
			Heal(*St, Restored * Share / 100 / FMath::Max(1, List.Num()));
	}
}
FDWMinionState *FDWMinions::Engage(const FDWCombatEnemy &M) const
{
	const double HeroDistance = MinionDistance(R.HeroPosition(), M.Position);
	double BestDistance = 2.5;
	FDWMinionState *Best = nullptr;
	for (auto *St : Alive())
	{
		const double D = MinionDistance(FVector3d(SummonOf(*St).Position), M.Position);
		if (D < BestDistance && D < HeroDistance)
		{
			Best = St;
			BestDistance = D;
		}
	}
	return Best;
}
void FDWMinions::AddTargets()
{
	const float Res = float(S(TEXT("minion.resistance")));
	const float Resistance[] = {Res, Res, Res, 0};
	for (auto *St : Alive())
		R.World.Targets().Add(SummonOf(*St).Position, .4f, float(St->Life + St->Shield), 0, MinionTargetBase + St->Id,
							  Resistance);
}
double FDWMinions::GuardReduction()
{
	int32 Near = 0;
	for (auto *St : Alive())
		Near += MinionDistance(FVector3d(SummonOf(*St).Position), R.HeroPosition()) <= 8;
	double Total = 0;
	for (const auto *T : R.T(TEXT("minion_nearby_guard")))
		Total += FMath::Min(T->Param(TEXT("max")), T->Param(TEXT("n")) * Near);
	return Total;
}
void FDWMinions::TakeDamage(FDWMinionState &St, double Amount, uint8 Element, bool Dot, bool Area, bool Spell,
							bool Transferred)
{
	if (St.bDead || R.Time < St.ImmuneUntil || !(Amount > 0))
		return;
	if (!Dot)
	{
		const double Ev = Evasion();
		if (!Spell && R.Chance(Ev / (Ev + 600) * 100))
			return;
		if (R.Chance(FMath::Min(75., Spell ? S(TEXT("minion.block_spell")) : S(TEXT("minion.block_attack")))))
			Amount *= .3;
		if (Element == 3)
		{
			const double A = Armour();
			Amount -= Amount * FMath::Min(.9, A / (A + 5 * Amount));
		}
	}
	Amount *= FMath::Max(0., 1 - S(TEXT("minion.taken")) / 100) *
			  (Area ? FMath::Max(0., 1 - S(TEXT("minion.taken_area")) / 100) : 1) *
			  (Dot ? FMath::Max(0., 1 - S(TEXT("minion.taken_dot")) / 100) : 1);
	if (!Transferred && S(TEXT("minion.you_take")) > 0)
	{
		const double Part = Amount * FMath::Min(100., S(TEXT("minion.you_take"))) / 100;
		Amount -= Part;
		FDWCharacterHit H;
		H.Amount = Part;
		H.Element = EElement(Element);
		H.bDot = Dot;
		R.HeroDamage(H, true);
	}
	St.SinceHit = 0;
	const double ToShield = FMath::Min(St.Shield, Amount);
	St.Shield -= ToShield;
	St.Life -= Amount - ToShield;
	if (St.Life <= 0)
		Die(St, TEXT("killed"));
}
double FDWMinions::AbsorbForHero(double Amount, uint8 Element)
{
	const double N = S(TEXT("minion.take_for_you"));
	if (!(N > 0))
		return 0;
	FDWMinionState *Holder = nullptr;
	for (auto *St : Alive())
		if (MinionDistance(FVector3d(SummonOf(*St).Position), R.HeroPosition()) <= 8 &&
			(!Holder || St->Life > Holder->Life))
			Holder = St;
	if (!Holder)
		return 0;
	const double Part = Amount * FMath::Min(100., N) / 100;
	TakeDamage(*Holder, Part, Element, false, false, false, true);
	return Part;
}
void FDWMinions::Remove(FDWMinionState &St)
{
	St.bDead = true;
	auto &Sm = SummonOf(St);
	Sm.Expire = R.World.GetClock();
	St.Snapshot = Sm;
}
void FDWMinions::Die(FDWMinionState &St, FName Reason)
{
	if (St.bDead)
		return;
	St.RemainingAtDeath = FMath::Max(0., double(SummonOf(St).Expire - R.World.GetClock()));
	Remove(St);
	St.Reason = Reason;
	const auto Sm = SummonOf(St);
	if (Reason != TEXT("killed"))
		return;
	if (S(TEXT("minion.death_explosion")))
	{
		FDWStrikeOptions O;
		O.Effect = TEXT("minion_death");
		O.Depth = 2;
		R.Strike(FVector3d(Sm.Position),
				 2.5 * FMath::Sqrt(1 + S(TEXT("minion.death_area")) / 100) * R.AreaScale(-1, false, true),
				 St.MaxLife * (30 + S(TEXT("minion.death_damage"))) / 100, 0, O);
	}
	if (S(TEXT("minion.death_heal")) > 0)
	{
		R.Character.Recover(ECharacterPool::Life, S(TEXT("minion.death_heal")));
		for (auto *O : Alive())
			Heal(*O, S(TEXT("minion.death_heal")));
	}
	R.Triggers->Event(TEXT("trig_summon_death"), {FVector3d(Sm.Position), InvalidId, true});
	if (!St.bTemporary && R.Chance(S(TEXT("minion.resummon_chance"))))
		Copy(Sm, FVector3d::ZeroVector, false, false, FMath::Max(1., St.BaseDuration), R.HeroPosition());
	else if (!St.bTemporary && !St.bEmpowered)
	{
		auto Snapshot = Sm;
		Snapshot.Expire = float(R.World.GetClock() + FMath::Max(0., St.RemainingAtDeath));
		Revives.Add({R.Time + 5 * FMath::Max(.05, 1 - S(TEXT("minion.revive")) / 100), Snapshot, St.Plan});
	}
}
void FDWMinions::OnEnd(const FDWSummon &Sm)
{
	auto *Ptr = States.Find(Sm.Id);
	if (!Ptr || (*Ptr)->bDead)
		return;
	auto &St = **Ptr;
	St.bDead = true;
	St.Snapshot = Sm;
	if (Sm.bReplaced)
	{
		LastReplacedLife = St.Life;
		return;
	}
	if (S(TEXT("minion.end_attack")))
	{
		FDWStrikeOptions O;
		O.Effect = TEXT("minion_end");
		O.Depth = 2;
		R.Strike(FVector3d(Sm.Position), 6, R.Hero.Power * 2, St.Element, O);
	}
}
int32 FDWMinions::ControlCap() const{return FMath::Max(1,1+FMath::FloorToInt(S(TEXT("minion.control_cap"))));}
void FDWMinions::EnforceControlCap(FDWMinionState* Incoming)
{
 auto List=Alive();List.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
 while(List.Num()>ControlCap())
 {
  auto* Old=List[0];List.RemoveAt(0);auto& Sm=SummonOf(*Old);
  if(Incoming){auto& Next=SummonOf(*Incoming);Next.NextFire=FMath::Max3(Next.NextFire,Sm.NextFire,float(R.World.GetClock()));}
  Sm.bDead=Sm.bReplaced=true;R.World.ReleaseHeldRoot(Sm.Root);OnEnd(Sm);
  if(Incoming&&LastReplacedLife.IsSet())Incoming->Life=FMath::Min(Incoming->MaxLife*1.5,Incoming->Life+LastReplacedLife.GetValue()*S(TEXT("minion.replace_transfer"))/100);
  LastReplacedLife.Reset();
 }
}
bool FDWMinions::Sacrifice(int32 Count,FName Kind)
{
 if(R.Character.bDead)return false;auto List=Alive();List.RemoveAll([&](const auto* St){return !Kind.IsNone()&&St->Kind!=Kind;});
 List.Sort([](const auto& A,const auto& B){return A.Born==B.Born?A.Id<B.Id:A.Born<B.Born;});
 const int32 N=FMath::Min(List.Num(),FMath::Clamp(Count,0,32));for(int32 I=0;I<N;I++)SacrificeUnit(*List[I]);return N>0;
}
void FDWMinions::SacrificeUnit(FDWMinionState& State)
{
 auto* St=&State;
 Die(*St,TEXT("sacrificed"));auto& C=R.Character;
 C.Recover(ECharacterPool::Life,C.MaxLife*S(TEXT("minion.sacrifice_life"))/100);
 C.Recover(ECharacterPool::Shield,FMath::Max(0.,St->Life)*S(TEXT("minion.sacrifice_shield"))/100);
 for(const auto* T:R.T(TEXT("minion_sacrifice_empower")))C.PendingEmpower+=T->Param(TEXT("n"));++Sacrificed;
 for(const auto* T:R.T(TEXT("minion_sacrifice_elite")))if(Sacrificed%FMath::Max(1,int32(T->Param(TEXT("per"))))==0)Copy(SummonOf(*St),FVector3d::ZeroVector,false,true,8,R.HeroPosition());
 R.Triggers->Event(TEXT("trig_sacrifice"),{FVector3d(SummonOf(*St).Position),InvalidId,true});
}
void FDWMinions::OnHit(const FDWCombatHit &H)
{
	auto *P = H.Root ? States.Find(H.Root->MinionId) : nullptr;
	if (P)
		Heal(**P,
			 S(TEXT("minion.life_on_hit")) +
				 H.Amount * (S(TEXT("minion.leech")) + (H.Id == R.Marked ? S(TEXT("minion.marked_leech")) : 0)) / 100);
	if (S(TEXT("minion.leech_to_you")) > 0)
		R.Character.AddLeech(ECharacterPool::Life, H.Amount * S(TEXT("minion.leech_to_you")) / 100);
	R.Character.Recover(ECharacterPool::Shield, S(TEXT("minion.hit_your_shield")));
	if (H.bCritical)
	{
		R.Character.Recover(ECharacterPool::Mana, S(TEXT("minion.crit_your_mana")));
		FVector3d Pos;
		const bool Has = R.TargetPosition(H.Id, Pos);
		R.Triggers->Event(TEXT("trig_summon_crit"), {Pos, H.Id, Has});
	}
	static const TCHAR *Names[] = {TEXT("fire"),	 TEXT("frost"),	 TEXT("lightning"),
								   TEXT("physical"), TEXT("poison"), TEXT("void")};
	for (uint8 E = 0; E < 6; ++E)
		if (E != H.Element && E != 4 && H.Info.Strike.IsNone())
		{
			const double Flat = S(FName(*(FString(TEXT("minion.flat.")) + Names[E])));
			if (Flat > 0)
			{
				FDWStrikeOptions O;
				O.Effect = TEXT("minion_flat");
				R.Hit(H.Id, Flat, E, O);
			}
		}
}
void FDWMinions::OnKill(const FDWCombatHit &H)
{
	if (!H.bMinion)
		return;
	auto *P = H.Root ? States.Find(H.Root->MinionId) : nullptr;
	auto *St = P ? P->Get() : nullptr;
	if (St)
		++St->Kills;
	R.Character.Recover(ECharacterPool::Life, S(TEXT("minion.kill_your_life")));
	if (St && !St->bTemporary && R.Chance(S(TEXT("minion.kill_temp_chance"))))
		Copy(SummonOf(*St), {.6, .6, 0}, true, false, 4);
}
void FDWMinions::OnHeroHit(const FDWCombatHit &H)
{
	if (S(TEXT("minion.you_hit_heal")) > 0)
		for (auto *St : Alive())
			if (MinionDistance(FVector3d(SummonOf(*St).Position), R.HeroPosition()) <= 8)
				Heal(*St, S(TEXT("minion.you_hit_heal")));
	if (H.bCritical)
		for (const auto *T : R.T(TEXT("minion_you_crit_damage")))
		{
			YouCritUntil = R.Time + T->Param(TEXT("duration"));
			YouCritValue = T->Param(TEXT("n"));
		}
}
void FDWMinions::OnFlask(double Amount)
{
	if (S(TEXT("minion.flask_share")) > 0)
		for (auto *St : Alive())
			Heal(*St, Amount * S(TEXT("minion.flask_share")) / 100);
}
void FDWMinions::OnHeroCast()
{
	const double Delay = .6 * FMath::Max(0., 1 - S(TEXT("minion.command_response")) / 100);
	for (auto *St : Alive())
	{
		St->CommandAt = R.Time + Delay;
		St->CommandTarget = R.Marked;
		St->bCommanded = false;
	}
}
} // namespace DWRules
