#include "DWRuleCombatTriggers.h"
#include "DWRuleMinions.h"

namespace DWRules
{
void FDWCombatTriggers::Forget(uint32 Id)
{
	Elites.Remove(Id);
	HitEver.Remove(Id);
	const FString Prefix = FString::Printf(TEXT("%u|"), Id);
	for (auto It = AilmentFired.CreateIterator(); It; ++It)
		if (It->StartsWith(Prefix))
			It.RemoveCurrent();
}
namespace
{
bool UsesChance(FName Name)
{
	static const TSet<FName> Names = {TEXT("trig_cast"),		 TEXT("trig_cast_spell"),
									  TEXT("trig_cast_attack"),	 TEXT("trig_hit"),
									  TEXT("trig_crit"),		 TEXT("trig_kill"),
									  TEXT("trig_execute"),		 TEXT("trig_hit_elite"),
									  TEXT("trig_hit_full"),	 TEXT("trig_hit_low"),
									  TEXT("trig_taken_hit"),	 TEXT("trig_taken_crit"),
									  TEXT("trig_block"),		 TEXT("trig_evade"),
									  TEXT("trig_charge_spent"), TEXT("trig_charge_gained"),
									  TEXT("trig_poison"),		 TEXT("trig_shock"),
									  TEXT("trig_freeze"),		 TEXT("trig_ignite"),
									  TEXT("trig_erosion"),		 TEXT("trig_ailment_consumed"),
									  TEXT("trig_pierce"),		 TEXT("trig_chain"),
									  TEXT("trig_return"),		 TEXT("trig_summon_spawn"),
									  TEXT("trig_summon_crit"),	 TEXT("trig_summon_death"),
									  TEXT("trig_dodge")};
	return Names.Contains(Name);
}
FDWTriggerContext PositionContext(const FVector3d &Position, uint32 Id = InvalidId)
{
	return {Position, Id, true};
}
} // namespace
FString FDWCombatTriggers::Key(const FDWSheetTrigger &T)
{
	return T.Trait.ToString().ToLower() + TEXT("|") + T.Name.ToString().ToLower();
}
FDWTriggerState &FDWCombatTriggers::State(FDWSheetTrigger &T)
{
	const FString K = Key(T);
	if (!States.Contains(K))
	{
		FDWTriggerState V;
		V.Charges = 1 + FMath::FloorToInt(R.S(TEXT("trigger.charges")));
		States.Add(K, V);
	}
	return States[K];
}
void FDWCombatTriggers::Event(FName Name, const FDWTriggerContext &Context)
{
	int32 N = 0;
	const int32 Limit = 1 + FMath::FloorToInt(R.S(TEXT("trigger.per_event")));
	for (auto *T : R.T(Name))
	{
		if (N >= Limit)
			break;
		if (TryFire(*T, Context))
			++N;
	}
}
bool FDWCombatTriggers::TryFire(FDWSheetTrigger &T, const FDWTriggerContext &Context)
{
	auto &RT = State(T);
	if (UsesChance(T.Name) && T.Params.Contains(TEXT("chance")))
	{
		if (!R.Chance(T.Param(TEXT("chance")) + R.S(TEXT("trigger.chance")) + RT.Fail))
		{
			RT.Fail += R.S(TEXT("trigger.fail_bonus"));
            Defer(T,Context);
			return false;
		}
		RT.Fail = 0;
	}
	if (RT.Charges <= 0)
	{
		RT.Layers = FMath::Min(10, RT.Layers + 1);
        Defer(T,Context);
		return false;
	}
    RT.Pending.Reset();
	if (!R.Chance(R.S(TEXT("trigger.keep_charge"))))
		--RT.Charges;
	Fire(T, Context);
	if (R.Chance(R.S(TEXT("trigger.extra_chance"))))
		Fire(T, Context);
	return true;
}
void FDWCombatTriggers::Defer(FDWSheetTrigger& T,const FDWTriggerContext& Context)
{if(!R.S(TEXT("trigger.store_event")))return;auto& RT=State(T);if(!RT.Pending.IsSet())RT.Pending=Context;RT.RetryAt=R.Time+.25;}
TArray<TArray<FDWTraitSource>> FDWCombatTriggers::Mounted(const TArray<FDWTraitSource> &Sources)
{
	TArray<FDWTraitSource> Supports;
	TArray<TArray<FDWTraitSource>> Out;
	for (const auto &Source : Sources)
	{
		const bool Active =
			Source.Rules.ContainsByPredicate([](const auto &Rule) { return Rule.Event == TEXT("cast"); });
		if (Active)
			Out.Add({Source});
		else
			Supports.Add(Source);
	}
	for (auto &List : Out)
		List.Append(Supports);
	return Out;
}
void FDWCombatTriggers::Fire(FDWSheetTrigger &T, const FDWTriggerContext &Context)
{
	auto Skills = Mounted(T.Stitched);
	const int32 Max = FMath::Max(0, 1 + FMath::FloorToInt(R.S(TEXT("trigger.mount"))));
	if (Skills.Num() > Max)
		Skills.SetNum(Max);
	if (!Skills.Num())
		return;
	auto &RT = State(T);
	if (R.S(TEXT("trigger.order_random")))
	{
		auto Chosen = Skills[FMath::FloorToInt(R.Random() * Skills.Num())];
		Skills = {MoveTemp(Chosen)};
	}
	else if (R.S(TEXT("trigger.order_rotate")))
	{
		auto Chosen = Skills[RT.Rotate++ % Skills.Num()];
		Skills = {MoveTemp(Chosen)};
	}
	const int32 Layers = RT.Layers;
	RT.Layers = 0;
	const double Delay = .1 * FMath::Max(0., 1 - R.S(TEXT("trigger.delay")) / 100),
				 Gap = .2 * FMath::Max(.05, 1 - R.S(TEXT("trigger.repeat_interval")) / 100);
	const int32 Repeats = 1 + FMath::FloorToInt(R.S(TEXT("trigger.repeat")));
	int32 K = 0;
	for (const auto &Sources : Skills)
	{
		for (int32 I = 0; I < Repeats; ++I)
			Queue.Add({R.Time + Delay + I * Gap + (R.S(TEXT("trigger.order_sequence")) ? K * .15 : 0),
					   Sources, Context, Key(T), Layers});
		++K;
	}
	++Fired;
	++R.Character.Totals.Triggers;
}
void FDWCombatTriggers::CastFromCharacter(const FDWPendingPayload &Request)
{
	const auto Skills = Mounted(Request.Sources);
	Queue.Add({R.Time, Skills.Num() ? Skills[0] : Request.Sources, {}, {}, 0});
}
void FDWCombatTriggers::Cast(const FJob &Job)
{
	auto &C = R.Character;
	const double Cost = 3 * FMath::Max(0., 1 - R.S(TEXT("trigger.cost")) / 100);
	if (C.Mana < Cost && !bGod)
		return;
	if (!bGod)
	{
		C.Mana -= Cost;
		C.Flow.ManaSpent += Cost;
		if (R.Chance(R.S(TEXT("trigger.refund"))))
			C.Mana = FMath::Min(C.MaxMana, C.Mana + Cost);
	}
	if (!Job.Sources.Num())
		return;
	FDWBuildSource Source;
	Source.ActiveId = Job.Sources[0].Id;
	Source.Traits = Job.Sources;
    TArray<FDWTraitSource> Supports;if(const auto* Parent=R.Host.PlanSource(R.HeroPlan()))Supports=Parent->Traits;
    if(const auto* Owner=R.World.GetOwner(R.Hero.Owner)){TArray<FDWTraitSource> Held;R.Host.HeldSources(Owner->StatusId,Held);for(const auto& T:Held){auto* Existing=Supports.FindByPredicate([&](const auto& X){return X.Id==T.Id;});if(Existing)*Existing=T;else Supports.Add(T);}}
    for(const auto& T:Supports)if(!T.Rules.ContainsByPredicate([](const auto& X){return X.Event==TEXT("cast");})&&T.Modifiers.ContainsByPredicate([](const auto& M){return M.Stat.StartsWith(TEXT("projectile."));})){auto* Existing=Source.Traits.FindByPredicate([&](const auto& X){return X.Id==T.Id;});if(Existing)*Existing=T;else Source.Traits.Add(T);}
	FString Error;
	int32 Plan = R.Host.CompileSource(Source, Error);
	if (Plan == INDEX_NONE)
		return;
	// Match manual player casts; summons keep their independent inheritance rules.
	if (!R.SkillFamily(Plan).bSummon)
	{
		FDWTraitSource Offense;
		if (C.Offense().MakeSource(R.World.GetPlan(Plan).Stats, Offense))
			Plan = R.Host.PlanWithSources(Plan, {Offense}, Error);
		if (Plan == INDEX_NONE)
			return;
	}
    FDWTraitSource Shape;if(R.PlanSource(Shape,R.SkillFamily(Plan).bSummon,true))Plan=R.Host.PlanWithSources(Plan,{Shape},Error);if(Plan==INDEX_NONE)return;
	const FVector3d Hero = R.HeroPosition();
	FVector3d Origin = Hero;
	if (R.S(TEXT("trigger.at_event")) && Job.Context.bHasPosition)
		Origin = Job.Context.Position;
	const auto List = R.EnemiesNear(Origin, 10 * (1 + R.S(TEXT("trigger.seek")) / 100));
	uint32 Target = InvalidId;
	if (R.S(TEXT("trigger.at_source")) && R.Enemy(Job.Context.Id))
		Target = Job.Context.Id;
	auto Prefer = [&](TFunctionRef<bool(uint32)> Test) {
		if (Target == InvalidId)
			for (uint32 Id : List)
				if (Test(Id))
				{
					Target = Id;
					break;
				}
	};
	if (R.S(TEXT("trigger.prefer_marked")))
		Prefer([&](uint32 Id) { return Id == R.Marked; });
	if (R.S(TEXT("trigger.prefer_elite")))
		Prefer([&](uint32 Id) { return R.Enemy(Id)->bElite; });
	if (R.S(TEXT("trigger.prefer_unhit")))
		Prefer([&](uint32 Id) { return !HitEver.Contains(Id); });
	if (Target == InvalidId && List.Num())
		Target = List[0];
	if (R.S(TEXT("trigger.at_self")))
		Origin = Hero;
	const auto *Owner = R.World.GetOwner(R.Hero.Owner);
	FVector3d Direction = Owner ? FVector3d(Owner->Aim) : FVector3d(1, 0, 0);
	if (const auto *M = R.Enemy(Target))
	{
		const double A = FMath::Atan2(M->Position.Y - Origin.Y, M->Position.X - Origin.X);
		Direction = {FMath::Cos(A), FMath::Sin(A), 0};
		if (!R.S(TEXT("trigger.at_self")) && !R.S(TEXT("trigger.at_event")) && R.SkillFamily(Plan).bArea)
			Origin = M->Position;
	}
	double More = 1;
	const FName Id = Job.Sources[0].Id;
	if (LastSkill == Id)
	{
		++Streak;
		for (const auto *T : R.T(TEXT("trigger_streak")))
			More *= 1 + FMath::Min(T->Param(TEXT("max")), T->Param(TEXT("n")) * Streak) / 100;
	}
	else
	{
		if (!LastSkill.IsNone())
			More *= 1 + R.S(TEXT("trigger.variety")) / 100;
		Streak = 0;
	}
	LastSkill = Id;
	More *= 1 + Job.Layers * R.S(TEXT("trigger.consume_layers")) / 100;
	Origin.Z = .6;
	const int32 Root = R.World.Cast(Plan, R.Hero.Owner, FVector3f(Origin), FVector3f(Direction),
									uint64(FMath::FloorToDouble(R.Random() * 1.e9)), float(R.Hero.Power),R.Enemy(Target)?TOptional<FVector3f>(FVector3f(R.Enemy(Target)->Position.X,R.Enemy(Target)->Position.Y,0)):Job.Context.bHasPosition?TOptional<FVector3f>(FVector3f(Job.Context.Position.X,Job.Context.Position.Y,0)):Owner?Owner->TargetPoint:TOptional<FVector3f>());
	if (auto *Info = R.RootInfo(Root, true))
	{
		Info->Kind = TEXT("trigger");
		Info->bTriggered = true;
		Info->Depth = 1;
		Info->More = More;
		Info->TriggerKey = Job.TriggerKey;
	}
}
void FDWCombatTriggers::OnDealt(const FDWCombatHit &H)
{
	if (H.bDot)
		return;
	HitEver.Add(H.Id);
	Combat();
	if (H.bMinion || H.bTriggered || !H.Info.Strike.IsNone())
	{
		if (H.bTriggered && H.bKilled && R.S(TEXT("trigger.kill_restore")) && H.Root &&
			!H.Root->TriggerKey.IsEmpty())
			if (auto *State = States.Find(H.Root->TriggerKey))
				State->Charges =
					FMath::Min(1 + FMath::FloorToInt(R.S(TEXT("trigger.charges"))), State->Charges + 1);
		return;
	}
	FVector3d P;
	const bool HasP = R.TargetPosition(H.Id, P);
	const FDWTriggerContext Context{P, H.Id, HasP};
	Event(TEXT("trig_hit"), Context);
	if (H.bCritical)
		Event(TEXT("trig_crit"), Context);
	if (H.bKilled)
		Event(TEXT("trig_kill"), Context);
	if (H.bKilled && H.Info.bExecuted)
		Event(TEXT("trig_execute"), Context);
	const auto *M = R.Enemy(H.Id);
	if (M && M->bElite)
		Event(TEXT("trig_hit_elite"), Context);
	const double Life = M ? (M->Health + H.Amount) / FMath::Max(1., M->MaxHealth) : 0;
	if (Life >= .999)
		Event(TEXT("trig_hit_full"), Context);
	if (Life <= .35)
		Event(TEXT("trig_hit_low"), Context);
	Count(TEXT("hits"), 1, Context);
	if (H.bCritical)
		Count(TEXT("crits"), 1, Context);
	if (H.bKilled)
		Count(TEXT("kills"), 1, Context);
	if (H.Root && !H.Root->bMultiFired)
		for (auto *T : R.T(TEXT("trig_multi_hit")))
			if (H.Root->Victims.Num() >= T->Param(TEXT("n")))
			{
				H.Root->bMultiFired = true;
				TryFire(*T, Context);
			}
}
void FDWCombatTriggers::Count(FName Kind, double Amount, const FDWTriggerContext &Context)
{
	FName Name = Kind == TEXT("hits")	  ? TEXT("trig_hits_count")
				 : Kind == TEXT("crits")  ? TEXT("trig_crits_count")
				 : Kind == TEXT("kills")  ? TEXT("trig_kills_count")
				 : Kind == TEXT("blocks") ? TEXT("trig_blocks_count")
										  : TEXT("trig_evades_count");
	for (auto *T : R.T(Name))
	{
		const double Per = FMath::Max(1., T->Param(TEXT("per")) -
											  FMath::FloorToDouble(R.S(TEXT("trigger.count_reduction"))));
		T->Accumulator += Amount;
		while (T->Accumulator >= Per)
		{
			T->Accumulator -= Per;
			TryFire(*T, Context);
		}
	}
}
void FDWCombatTriggers::OnCast(bool Attack, const FDWCastPayment &Payment)
{
	const auto Context = PositionContext(R.HeroPosition());
	Combat();
	Event(TEXT("trig_cast"), Context);
	Event(Attack ? TEXT("trig_cast_attack") : TEXT("trig_cast_spell"), Context);
	const double Spent = Payment.ManaCost + Payment.ShieldCost + Payment.LifeCost;
	for (auto *T : R.T(TEXT("trig_big_cost")))
		if (Spent >= T->Param(TEXT("n")))
			TryFire(*T, Context);
}
void FDWCombatTriggers::Ailment(FName Type, uint32 Id, int32 Layers)
{
    if(Type==TEXT("consumed")&&Layers>0)R.RuleEvent(TEXT("ailment_consumed"),Id,1);
	static const TMap<FName, FName> Names = {
		{TEXT("poison"), TEXT("trig_poison")},	 {TEXT("shock"), TEXT("trig_shock")},
		{TEXT("freeze"), TEXT("trig_freeze")},	 {TEXT("ignite"), TEXT("trig_ignite")},
		{TEXT("erosion"), TEXT("trig_erosion")}, {TEXT("consumed"), TEXT("trig_ailment_consumed")}};
	FDWTriggerContext Context;
	Context.Id = Id;
	Context.bHasPosition = R.TargetPosition(Id, Context.Position);
	if (const auto *Name = Names.Find(Type))
		Event(*Name, Context);
	if (Type != TEXT("consumed") && Type != TEXT("freeze"))
		for (auto *T : R.T(TEXT("trig_ailment_stacks")))
		{
			const FString K = FString::Printf(TEXT("%u|%s"), Id, *Type.ToString());
			if (Layers >= T->Param(TEXT("n")) && !AilmentFired.Contains(K))
			{
				AilmentFired.Add(K);
				TryFire(*T, Context);
			}
			if (Layers < T->Param(TEXT("n")))
				AilmentFired.Remove(K);
		}
}
void FDWCombatTriggers::Combat()
{
	if (R.Time - LastCombat >= 5)
		Event(TEXT("trig_combat_start"), PositionContext(R.HeroPosition()));
	LastCombat = R.Time;
}
void FDWCombatTriggers::Tick(double Dt)
{
	auto &C = R.Character;
	const double Cooldown =
		FMath::Max(.05, .5 / FMath::Max(.1, 1 + R.S(TEXT("trigger.cooldown_recovery")) / 100) -
							R.S(TEXT("trigger.cooldown_flat")));
	const int32 Max = 1 + FMath::FloorToInt(R.S(TEXT("trigger.charges")));
    TSet<FString> Equipped;for(const auto& T:C.Sheet.Triggers)if(T.Name.ToString().StartsWith(TEXT("trig_")))Equipped.Add(Key(T));
    for(auto It=States.CreateIterator();It;++It)if(!Equipped.Contains(It.Key()))It.RemoveCurrent();
	for (auto &T : C.Sheet.Triggers)
		if (T.Name.ToString().StartsWith(TEXT("trig_")))
		{
			auto &RT = State(T);
			if (RT.Charges >= Max)
			{
				RT.Recharge = 0;
				continue;
			}
			RT.Recharge += Dt;
			while (RT.Recharge >= Cooldown && RT.Charges < Max)
			{
				RT.Recharge -= Cooldown;
				++RT.Charges;
			}
		}

    for(auto& T:C.Sheet.Triggers)if(T.Name.ToString().StartsWith(TEXT("trig_")))
    {auto& RT=State(T);if(!R.S(TEXT("trigger.store_event")))RT.Pending.Reset();else if(RT.Pending.IsSet()&&RT.Charges>0&&RT.RetryAt<=R.Time){const auto Context=RT.Pending.GetValue();TryFire(T,Context);}}
	const FVector3d Hero = R.HeroPosition();
	const auto Events = MoveTemp(C.CombatEvents);
	C.CombatEvents.Reset();
	static const TMap<FName, FName> EventNames = {{TEXT("evade"), TEXT("trig_evade")},
												  {TEXT("block"), TEXT("trig_block")},
												  {TEXT("enter_low"), TEXT("trig_enter_low")},
												  {TEXT("life_full"), TEXT("trig_life_full")},
												  {TEXT("shield_broken"), TEXT("trig_shield_broken")},
												  {TEXT("shield_full"), TEXT("trig_shield_full")},
												  {TEXT("recharge_start"), TEXT("trig_recharge_start")},
												  {TEXT("lethal"), TEXT("trig_lethal")},
												  {TEXT("charge_spent"), TEXT("trig_charge_spent")},
												  {TEXT("charge_gained"), TEXT("trig_charge_gained")},
												  {TEXT("charge_max"), TEXT("trig_charge_max")}};
	for (const auto &E : Events)
	{
		const auto *M = R.Enemy(E.Attacker);
		const auto Context = PositionContext(M ? M->Position : Hero, M ? M->Id : InvalidId);
		if (E.Type == TEXT("hit_taken"))
		{
			Combat();
			Event(TEXT("trig_taken_hit"), Context);
			if (E.bCritical)
				Event(TEXT("trig_taken_crit"), Context);
			continue;
		}
		if (E.Type == TEXT("flask"))
		{
			R.Minions->OnFlask(E.Amount);
			continue;
		}
		if (E.Type == TEXT("evade"))
		{
			Count(TEXT("evades"), 1, Context);
			R.Counter(E.Attacker, TEXT("evade"), E);
		}
		if (E.Type == TEXT("block"))
		{
			Count(TEXT("blocks"), 1, Context);
			R.Counter(E.Attacker, TEXT("block"), E);
		}
		if (const auto *Name = EventNames.Find(E.Type))
			Event(*Name, Context);
	}
	const auto Context = PositionContext(Hero);
	auto Flow = [&](FName Name, double Amount, bool Percent = false) {
		if (!(Amount > 0))
			return;
		for (auto *T : R.T(Name))
		{
			double Per =
				FMath::Max(1., T->Param(TEXT("per")) *
								   FMath::Max(.1, 1 - R.S(TEXT("trigger.resource_reduction")) / 100));
			if (Percent)
				Per *= C.MaxLife / 100;
			T->Accumulator += Amount;
			while (T->Accumulator >= Per)
			{
				T->Accumulator -= Per;
				TryFire(*T, Context);
			}
		}
	};
	Flow(TEXT("trig_mana_spent"), C.Flow.ManaSpent);
	Flow(TEXT("trig_life_spent"), C.Flow.LifeSpent);
	Flow(TEXT("trig_shield_spent"), C.Flow.ShieldSpent);
	Flow(TEXT("trig_mana_restored"), C.Flow.ManaRestored);
	Flow(TEXT("trig_damage_taken"), C.Flow.HitTaken);
	Flow(TEXT("trig_life_lost"), C.Flow.LifeLost, true);
	C.Flow = {};
	for (auto *T : R.T(TEXT("trig_interval")))
	{
		T->Accumulator += Dt;
		if (T->Accumulator >= FMath::Max(.1, T->Param(TEXT("seconds"))))
		{
			T->Accumulator = 0;
			TryFire(*T, Context);
		}
	}
	const double Moved =
		LastPosition.IsSet()
			? FVector2d::Distance(FVector2d(Hero.X, Hero.Y), FVector2d(LastPosition->X, LastPosition->Y))
			: 0;
	LastPosition = Hero;
	for (auto *T : R.T(TEXT("trig_distance")))
	{
		T->Accumulator += Moved;
		if (T->Accumulator >= FMath::Max(.5, T->Param(TEXT("meters"))))
		{
			T->Accumulator = 0;
			TryFire(*T, Context);
		}
	}
	if (Moved > 1.e-4)
	{
		MoveTime += Dt;
		StillTime = 0;
	}
	else
	{
		StillTime += Dt;
		MoveTime = 0;
	}
	for (auto *T : R.T(TEXT("trig_moving")))
		if (MoveTime >= T->Param(TEXT("seconds")))
		{
			MoveTime = 0;
			TryFire(*T, Context);
		}
	for (auto *T : R.T(TEXT("trig_still")))
		if (StillTime >= T->Param(TEXT("seconds")))
		{
			StillTime = 0;
			TryFire(*T, Context);
		}
	const int32 Near = R.EnemiesNear(Hero, 6).Num();
	double MinCrowd = 1.e30;
	for (auto *T : R.T(TEXT("trig_crowd")))
	{
		MinCrowd = FMath::Min(MinCrowd, T->Param(TEXT("n")));
		if (Near >= T->Param(TEXT("n")) && !bCrowd)
		{
			bCrowd = true;
			TryFire(*T, Context);
		}
	}
	if (Near < MinCrowd)
		bCrowd = false;
	for (auto *T : R.T(TEXT("trig_elite_near")))
		for (uint32 Id : R.EnemiesNear(Hero, T->Param(TEXT("meters"))))
			if (R.Enemy(Id)->bElite && !Elites.Contains(Id))
			{
				Elites.Add(Id);
				TryFire(*T, PositionContext(R.Enemy(Id)->Position, Id));
			}
	if (bCasting)
	{
		Channel += Dt;
		for (auto *T : R.T(TEXT("trig_channel")))
		{
			T->Accumulator += Dt;
			if (T->Accumulator >= FMath::Max(.1, T->Param(TEXT("seconds"))))
			{
				T->Accumulator = 0;
				TryFire(*T, Context);
			}
		}
	}
	else
	{
		Channel = 0;
		for (auto *T : R.T(TEXT("trig_channel")))
			T->Accumulator = 0;
	}
	const auto Due = Queue.FilterByPredicate([&](const auto &J) { return J.At <= R.Time; });
	Queue.RemoveAll([&](const auto &J) { return J.At <= R.Time; });
	for (const auto &J : Due)
		Cast(J);
}
} // namespace DWRules
