#include "DWRuleCharacter.h"

namespace DWRules
{
FDWCastPayment FDWRuleCharacter::BeforeCast(double BaseCost, bool Attack, double CostScale)
{
	FDWCastPayment Out;
	if (bDead || Stunned > 0)
	{
		Out.Reason = bDead ? TEXT("dead") : TEXT("stunned");
		return Out;
	}
	if (BaseCost < 0)
		BaseCost = Base.CastCost;
	BaseCost *= FMath::Max(0., CostScale) * FMath::Max(0., 1 - NextCost / 100);
	const double ShieldShare = FMath::Clamp(S(TEXT("cost.shield_percent")), 0., 100.),
				 LifeShare = FMath::Clamp(S(TEXT("cost.life_percent")), 0., 100 - ShieldShare);
	const double ShieldCost = BaseCost * ShieldShare / 100 * FMath::Max(0., 1 - S(TEXT("cost.shield_reduction")) / 100),
				 LifeCost = BaseCost * LifeShare / 100 * FMath::Max(0., 1 - S(TEXT("cost.life_reduction")) / 100),
				 ManaCost = BaseCost * (100 - ShieldShare - LifeShare) / 100;
	if (Mana < ManaCost - 1.e-9 || Shield < ShieldCost - 1.e-9 || Life <= LifeCost)
	{
		Out.Reason = TEXT("cost");
		return Out;
	}
	NextCost = 0;
	Mana -= ManaCost;
	Shield -= ShieldCost;
	Life -= LifeCost;
	double SpentShield = ShieldCost, SpentLife = LifeCost;
	Out.DamageMore = PendingEmpower;
	PendingEmpower = 0;
	if (Attack)
	{
		Out.DamageMore += NextAttackMore;
		Out.CritBonus += NextAttackCrit;
		NextAttackMore = NextAttackCrit = 0;
	}
	if (Charges > 0)
	{
		--Charges;
		Out.DamageMore += 10;
		Emit(TEXT("charge_spent"));
	}
	const double MaxShieldCost = MaxShield * S(TEXT("cast.max_shield_cost_percent")) / 100;
	if (MaxShieldCost > 0 && Shield >= MaxShieldCost)
	{
		Shield -= MaxShieldCost;
		SpentShield += MaxShieldCost;
		Out.DamageMore += S(TEXT("cast.max_shield_cost_damage"));
	}
	const double CurrentLifeCost = Life * S(TEXT("cast.current_life_cost_percent")) / 100;
	if (CurrentLifeCost > 0 && Life - CurrentLifeCost >= 1)
	{
		Life -= CurrentLifeCost;
		SpentLife += CurrentLifeCost;
		Out.DamageMore += S(TEXT("cast.current_life_cost_damage"));
	}
	const double MaxLifeCost = MaxLife * S(TEXT("cast.max_life_cost_percent")) / 100;
	if (MaxLifeCost > 0 && Life - MaxLifeCost >= 1)
	{
		Life -= MaxLifeCost;
		SpentLife += MaxLifeCost;
		Out.AreaIncreased += S(TEXT("cast.max_life_cost_area"));
	}
	Totals.ShieldLost += SpentShield;
	Totals.LifeLost += SpentLife;
	Flow.ManaSpent += ManaCost;
	Flow.LifeSpent += SpentLife;
	Flow.ShieldSpent += SpentShield;
	if (ManaCost > 0)
		Recover(ECharacterPool::Life, ManaCost * S(TEXT("life.from_mana_spent_percent")) / 100);
	Recover(ECharacterPool::Life, S(TEXT("life.on_cast")));
	for (auto &T : Sheet.Triggers)
	{
		const double Spent = T.Name == TEXT("empower_on_shield_spent") ? SpentShield
							 : T.Name == TEXT("empower_on_life_spent") ? SpentLife
																	   : 0,
					 Per = T.Param(TEXT("per"));
		if (Spent > 0 && Per > 0)
		{
			T.Accumulator += Spent;
			while (T.Accumulator >= Per)
			{
				T.Accumulator -= Per;
				PendingEmpower += T.Param(TEXT("bonus"));
				++Totals.Triggers;
			}
		}
		if (T.Name == TEXT("empower_from_overflow") && Overflow() > 0)
		{
			Shield -= Overflow() * FMath::Clamp(T.Param(TEXT("percent")), 0., 100.) / 100;
			PendingEmpower += T.Param(TEXT("bonus"));
			++Totals.Triggers;
		}
	}
	Out.bOk = true;
	Out.ManaCost = ManaCost;
	Out.ShieldCost = SpentShield;
	Out.LifeCost = SpentLife;
	return Out;
}
FDWCharacterOffense FDWRuleCharacter::Offense(double DamageMore, double AreaIncreased, double CritBonus) const
{
	double Generic = MissingShieldPercent() * S(TEXT("shield.missing.damage")) +
					 MissingLifePercent() * S(TEXT("life.missing.damage")) + MaxShield * S(TEXT("damage.per_shield")) +
					 MaxLife * S(TEXT("damage.per_life")) + S(TEXT("buff.damage")) +
					 BlockChances().Attack * S(TEXT("block.damage_per_attack")) + SelfPoison.Num() * 4;
	if (FullShield())
		Generic += S(TEXT("shield.full.damage"));
	if (FullLife())
		Generic += S(TEXT("life.full.damage"));
	if (LowLife())
		Generic += S(TEXT("life.low.damage"));
	if (Overflow() > 0)
		Generic += S(TEXT("shield.overflow.damage"));
	FDWCharacterOffense Out;
	Out.Area = AreaIncreased + MaxShield * S(TEXT("area.per_shield")) + MaxLife * S(TEXT("area.per_life"));
	for (int32 I = 0; I < NumElements; ++I)
	{
		const EElement E = EElement(I);
		const FString Name = ElementName(E);
		const bool Elemental = E != EElement::Physical && E != EElement::Poison;
		Out.Increased[I] = Generic + S(FName(*(TEXT("damage.increased.") + Name))) +
						   (Elemental ? S(TEXT("damage.increased.elemental")) : 0);
		Out.More[I] = S(FName(*(TEXT("damage.more.") + Name))) * (1 + DamageMore / 100) *
					  (I < 3 ? S(TEXT("damage.more.elemental")) : 1);
		Out.Flat[I] = S(FName(*(TEXT("damage.flat.") + Name))) +
					  MaxShield * S(FName(*(TEXT("damage.from_shield.") + Name))) / 100 +
					  MaxLife * S(FName(*(TEXT("damage.from_life.") + Name))) / 100;
	}
	Out.Critical = (FullShield() ? S(TEXT("shield.full.critical_chance")) : 0) +
				   (FullLife() ? S(TEXT("life.full.critical_chance")) : 0) +
				   EvasionValue() * S(TEXT("evasion.crit_per")) + CritBonus;
	return Out;
}
double FDWRuleCharacter::ActionSpeed(bool Attack) const
{
	return (bRecharging && !Attack ? S(TEXT("shield.recharging.cast_speed")) : 0) +
		   (!ShieldLeech.IsEmpty() && Attack ? S(TEXT("shield.leeching.attack_speed")) : 0) +
		   (LowLife() ? (Attack ? S(TEXT("life.low.attack_speed")) : S(TEXT("life.low.cast_speed"))) : 0) +
		   (Attack ? S(TEXT("buff.attack_speed")) + EvasionValue() * S(TEXT("evasion.attack_speed_per"))
				   : S(TEXT("buff.cast_speed")));
}
double FDWRuleCharacter::MoveSpeed() const
{
	return (FullShield() ? S(TEXT("shield.full.move_speed")) : 0) + (FullLife() ? S(TEXT("life.full.move_speed")) : 0) +
		   S(TEXT("buff.move_speed"));
}
bool FDWCharacterOffense::MakeSource(const FDWStatBlock &BaseStats, FDWTraitSource &Out,
									 const TArray<FName> &HitEvents) const
{
	Out = FDWTraitSource();
	Out.Id = TEXT("__character");
	if (HitEvents.Num() != 1 || HitEvents[0] != TEXT("hit"))
	{
		FString Id = TEXT("__character");
		for (FName Event : HitEvents)
			Id += TEXT("_") + Event.ToString();
		Out.Id = FName(*Id);
	}
	auto Round = [](double V) { return FMath::FloorToDouble(V / .1 + .5) * .1; };
	auto Modifier = [&](const FString &Stat, EModOp Op, FName Param, double Value) {
		Out.Params.Add(Param, Value);
		FDWModifierSource M;
		M.Stat = Stat;
		M.Op = Op;
		M.Value = FDWExprSource::Param(Param);
		Out.Modifiers.Add(M);
	};
	// Preserve Web source order; it defines secondary-hit event order.
	for (const EElement E :
		 {EElement::Physical, EElement::Lightning, EElement::Fire, EElement::Frost, EElement::Void, EElement::Poison})
	{
		const int32 I = int32(E);
		const FString Name = ElementName(E);
		if (FMath::Abs(Increased[I]) > 1.e-6 || FMath::Abs(More[I] - 1) > 1.e-6)
		{
			uint16 Stat;
			FindStat(TEXT("element.") + Name + TEXT(".damage_percent"), Stat);
			const double Current = BaseStats.Get(Stat),
						 Target = Round(((1 + (Current + Increased[I]) / 100) * More[I] - 1) * 100);
			if (FMath::Abs(Target - Current) > 1.e-6)
			{
				const FName Param(*(TEXT("pin_") + Name));
				Modifier(TEXT("element.") + Name + TEXT(".damage_percent"), EModOp::Min, Param,
						 FMath::Max(-100., Target));
				Modifier(TEXT("element.") + Name + TEXT(".damage_percent"), EModOp::Max, Param,
						 FMath::Max(-100., Target));
			}
		}
		const double FlatDamage = Round(Flat[I]);
		if (FlatDamage > 0)
		{
			const FName Param(*(TEXT("flat_") + Name));
			Out.Params.Add(Param, FlatDamage);
			FDWRuleSource Rule;
			Rule.Id = FName(*(TEXT("character_") + Name + TEXT("_on_hit")));
			Rule.Event = TEXT("hit");
			FDWActionSource Action;
			Action.Op = EOp::DamageTarget;
			Action.Element = I;
			Action.Tags.Add(FName(*Name));
			if (E == EElement::Frost)
				Action.Tags.Add(TEXT("cold"));
			Action.EmitEvent = FName(*(TEXT("character_") + Name + TEXT("_hit")));
			Action.SetExpr(TEXT("amount"), FDWExprSource::Param(Param));
			Rule.Actions.Add(Action);
			for (int32 N = 0; N < HitEvents.Num(); ++N)
			{
				auto Copy = Rule;
				Copy.Event = HitEvents[N];
				if (N)
					Copy.Id = FName(*(Rule.Id.ToString() + FString::Printf(TEXT("_%d"), N)));
				Out.Rules.Add(MoveTemp(Copy));
			}
		}
	}
	if (FMath::Abs(Critical) > 1.e-6)
		Modifier(TEXT("owner.critical_chance"), EModOp::Add, TEXT("critical"), Round(Critical));
	if (FMath::Abs(Area) > 1.e-6)
	{
		Modifier(TEXT("area.radius"), EModOp::IncreasedPercent, TEXT("area"), Round(Area));
		Modifier(TEXT("explosion.radius"), EModOp::IncreasedPercent, TEXT("area"), Round(Area));
	}
	return !Out.Modifiers.IsEmpty() || !Out.Rules.IsEmpty();
}
} // namespace DWRules
