#include "DWRuleCharacter.h"

namespace DWRules
{
FDWTakenHit FDWRuleCharacter::TakeHit(FDWCharacterHit Hit)
{
	FDWTakenHit Out;
	if (bDead || !(Hit.Amount > 0))
		return Out;
	double Amount = Hit.Amount;
	const bool Physical = Hit.Element == EElement::Physical,
			   Elemental = Hit.Element != EElement::Physical && Hit.Element != EElement::Poison;
	if (!Hit.bDot)
	{
		const auto Evade = EvadeChances(Hit.Kind);
		if (Chance(Hit.Kind.bSpell ? Evade.Spell : Evade.Attack))
		{
			++Totals.Evaded;
			EvadeFail = 0;
			Recover(ECharacterPool::Life, S(TEXT("life.on_evade")));
			Recover(ECharacterPool::Shield, S(TEXT("shield.on_evade")));
			Recover(ECharacterPool::Mana, S(TEXT("mana.on_evade")));
			OnEvaded();
			Emit(TEXT("evade"), Hit.Attacker);
			Out.bEvaded = true;
			return Out;
		}
		if (!Hit.Kind.bSpell || S(TEXT("evade.spell")) > 0)
			for (const auto &T : Sheet.Triggers)
				if (T.Name == TEXT("evade_fail_bonus"))
					EvadeFail = FMath::Min(Evade.Cap, EvadeFail + T.Param(TEXT("n")));
		const auto Block = BlockChances(Hit.Kind);
		if (Chance(Hit.Kind.bSpell ? Block.Spell : Block.Attack))
		{
			Out.bBlocked = true;
			++Totals.Blocked;
			double Taken = Amount * (1 - FMath::Clamp(70 + S(TEXT("block.reduction")), 0., 100.) / 100);
			if (Hit.bCritical)
				Taken -= (Taken - Taken / 1.5) * FMath::Clamp(S(TEXT("block.taken_crit")), 0., 100.) / 100;
			Taken *= FMath::Max(0., 1 - (Physical	 ? S(TEXT("block.taken_physical"))
										 : Elemental ? S(TEXT("block.taken_elemental"))
													 : 0) /
											100);
			const double Stopped = Amount - Taken;
			if (Physical)
				PreventedPhysical(Stopped);
			Recover(ECharacterPool::Life, S(TEXT("life.on_block")) + MaxLife * S(TEXT("life.on_block_percent")) / 100);
			Recover(ECharacterPool::Shield, S(TEXT("shield.on_block")) +
												MaxShield * S(TEXT("shield.on_block_percent")) / 100 +
												ArmourValue() * S(TEXT("shield.on_block_armour_percent")) / 100);
			Recover(ECharacterPool::Mana, S(TEXT("mana.on_block")));
			CounterBank += Stopped * S(TEXT("block.to_counter")) / 100;
			for (const auto &T : Sheet.Triggers)
				if (T.Name == TEXT("block_recoup_life"))
					AddRecoup(ECharacterPool::Life, Stopped * T.Param(TEXT("n")) / 100, T.Param(TEXT("seconds")));
			OnBlocked();
			const double Stagger = .25 * FMath::Max(0., 1 - S(TEXT("block.stagger")) / 100) /
								   FMath::Max(.1, 1 + S(TEXT("block.recovery")) / 100);
			Stunned = FMath::Max(Stunned, Stagger);
			Emit(TEXT("block"), Hit.Attacker, 0, Stopped, Block.Overflow);
			Amount = Taken;
			if (!(Amount > 0))
				return Out;
		}
		Recover(ECharacterPool::Shield, EvasionValue() * S(TEXT("shield.on_hit_taken_evasion_percent")) / 100);
		const double Armour = ArmourValue();
		if (Physical && Armour > 0)
		{
			const double Reduction = FMath::Min(Base.ArmourCap / 100, Armour / (Armour + Base.ArmourFactor * Amount));
			Out.Prevented = Amount * Reduction;
			Amount -= Out.Prevented;
			PreventedPhysical(Out.Prevented);
		}
		if (AfterHitReduction > 0)
			Amount *= 1 - AfterHitReduction / 100;
		AfterHitReduction = FMath::Clamp(S(TEXT("defense.after_hit_reduction")), 0., 90.);
		SinceHit = 0;
	}
	const double Reduction = (Shield > 0 ? S(TEXT("shield.any.damage_taken_reduction")) : 0) +
							 (LowLife() ? S(TEXT("life.low.damage_taken_reduction")) : 0) +
							 (GuardLeft > 0 ? GuardReduction : 0) + S(TEXT("buff.damage_taken_reduction"));
	Amount *= FMath::Max(0., 1 - FMath::Clamp(Reduction, -100., 100.) / 100);
	SinceDamaged = 0;
	if (bRecharging && Grace <= 0)
		bRecharging = false;
	const double Bypass = FMath::Clamp(Base.Bypass - S(TEXT("shield.bypass_reduction")), 0., 100.) / 100;
	double ToShield = Amount * (1 - Bypass), ToLife = Amount * Bypass;
	const bool HadShield = Shield > 0;
	if (HadShield)
	{
		const double Taken = (Hit.bDot ? S(TEXT("shield.taken_dot_reduction")) : 0) +
							 (Physical	  ? S(TEXT("shield.taken_physical_reduction"))
							  : Elemental ? S(TEXT("shield.taken_elemental_reduction"))
										  : 0);
		const double Factor = FMath::Max(.01, 1 - FMath::Clamp(Taken, 0., 99.) / 100), Cost = ToShield * Factor;
		if (Cost <= Shield)
		{
			Shield -= Cost;
			Out.ShieldLost = Cost;
			ToShield = 0;
		}
		else
		{
			Out.ShieldLost = Shield;
			ToShield -= Shield / Factor;
			Shield = 0;
		}
		Totals.ShieldLost += Out.ShieldLost;
	}
	ToLife += ToShield;
	if (HadShield && Shield <= 0)
	{
		Emit(TEXT("shield_broken"), Hit.Attacker);
		for (auto &T : Sheet.Triggers)
			if (T.Name == TEXT("cast_on_shield_broken") && T.Cooldown <= 0)
			{
				T.Cooldown = T.Param(TEXT("cooldown"));
				Fire(T, true);
			}
	}
	const double DeferredSeconds = S(TEXT("life.deferred_seconds")),
				 Later =
					 DeferredSeconds > 0 ? ToLife * FMath::Clamp(S(TEXT("life.deferred_percent")), 0., 100.) / 100 : 0;
	if (Later > 0)
		Deferred.Add({ECharacterPool::Life, Later, Later / DeferredSeconds});
	LoseLife(ToLife - Later);
	if (!bDead && Chance(S(TEXT("shield.recharge_on_damaged"))))
		StartRecharge();
	if (!Hit.bDot)
	{
		AddRecoup(ECharacterPool::Shield, Amount * S(TEXT("recoup.shield.percent")) / 100,
				  S(TEXT("recoup.shield.seconds")));
		const double Pct = S(TEXT("recoup.life.all")) + (Physical ? S(TEXT("recoup.life.physical")) : 0) +
						   (Elemental ? S(TEXT("recoup.life.elemental")) : 0);
		AddRecoup(ECharacterPool::Life, Amount * Pct / 100, S(TEXT("recoup.life.seconds")));
		if (Amount >= StunThreshold * Base.StunFraction && !bDead)
		{
			Stunned = Base.StunSeconds;
			++Totals.Stuns;
		}
		Flow.HitTaken += Amount;
		Emit(TEXT("hit_taken"), Hit.Attacker, Amount, 0, 0, Hit.bCritical, Out.bBlocked);
	}
	Out.Taken = Amount;
	Out.LifeLost = ToLife;
	return Out;
}
void FDWRuleCharacter::OnEvaded()
{
	for (const auto &T : Sheet.Triggers)
	{
		if (T.Name == TEXT("evade_buff_move"))
			AddBuff(TEXT("buff.move_speed"), T.Param(TEXT("n")), T.Param(TEXT("duration")), TEXT("evade_move"));
		if (T.Name == TEXT("evade_buff_attack"))
			AddBuff(TEXT("buff.attack_speed"), T.Param(TEXT("n")), T.Param(TEXT("duration")), TEXT("evade_attack"));
		if (T.Name == TEXT("evade_buff_cast"))
			AddBuff(TEXT("buff.cast_speed"), T.Param(TEXT("n")), T.Param(TEXT("duration")), TEXT("evade_cast"));
		if (T.Name == TEXT("evade_next_attack_more"))
			NextAttackMore = FMath::Max(NextAttackMore, T.Param(TEXT("n")));
		if (T.Name == TEXT("evade_next_attack_crit"))
			NextAttackCrit = FMath::Max(NextAttackCrit, T.Param(TEXT("n")));
		if (T.Name == TEXT("evade_next_cost"))
			NextCost = FMath::Max(NextCost, T.Param(TEXT("n")));
	}
}
void FDWRuleCharacter::OnBlocked()
{
	for (const auto &T : Sheet.Triggers)
	{
		if (T.Name == TEXT("block_buff_damage"))
			AddBuff(TEXT("buff.damage"), T.Param(TEXT("n")), T.Param(TEXT("duration")), TEXT("block_damage"));
		if (T.Name == TEXT("block_buff_armour"))
			AddBuff(TEXT("buff.armour"), T.Param(TEXT("n")), T.Param(TEXT("duration")), TEXT("block_armour"));
		if (T.Name == TEXT("block_next_attack_more"))
			NextAttackMore = FMath::Max(NextAttackMore, T.Param(TEXT("n")));
		if (T.Name == TEXT("block_next_cost"))
			NextCost = FMath::Max(NextCost, T.Param(TEXT("n")));
	}
}
void FDWRuleCharacter::PreventedPhysical(double Amount)
{
	Totals.Prevented += Amount;
	AddRecoup(ECharacterPool::Shield, Amount * S(TEXT("recoup.shield.prevented_physical_percent")) / 100,
			  S(TEXT("recoup.shield.prevented_physical_seconds")));
}
void FDWRuleCharacter::OnDealt(double Amount, EElement Element, bool Critical, bool Killed, bool Dot)
{
	if (bDead)
		return;
	if (!Dot)
	{
		Recover(ECharacterPool::Life, S(TEXT("life.on_hit")));
		Recover(ECharacterPool::Shield, S(TEXT("shield.on_hit")));
		const bool Physical = Element == EElement::Physical,
				   Elemental = Element != EElement::Physical && Element != EElement::Poison;
		for (const auto Pool : {ECharacterPool::Life, ECharacterPool::Shield})
		{
			const FString Prefix = Pool == ECharacterPool::Life ? TEXT("leech.life.") : TEXT("leech.shield.");
			const double Pct = S(FName(*(Prefix + TEXT("all")))) +
							   (Physical ? S(FName(*(Prefix + TEXT("physical")))) : 0) +
							   (Elemental ? S(FName(*(Prefix + TEXT("elemental")))) : 0);
			if (Pct > 0)
				AddLeech(Pool, Amount * Pct / 100);
		}
	}
	if (Critical)
	{
		Recover(ECharacterPool::Life, S(TEXT("life.on_crit")));
		Recover(ECharacterPool::Shield, S(TEXT("shield.on_crit")));
		if (Chance(S(TEXT("shield.recharge_on_crit"))))
			StartRecharge();
	}
	if (Killed)
	{
		Recover(ECharacterPool::Life, S(TEXT("life.on_kill")) + MaxLife * S(TEXT("life.on_kill_percent")) / 100);
		Recover(ECharacterPool::Shield,
				S(TEXT("shield.on_kill")) + MaxShield * S(TEXT("shield.on_kill_percent")) / 100);
		if (Chance(S(TEXT("shield.recharge_on_kill"))))
			StartRecharge();
	}
}
} // namespace DWRules
