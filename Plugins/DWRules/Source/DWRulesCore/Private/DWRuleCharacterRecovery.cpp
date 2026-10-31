#include "DWRuleCharacter.h"

namespace DWRules
{
namespace
{
FName LeechPath(ECharacterPool Pool, const TCHAR *Tail)
{
	return FName(*(FString(Pool == ECharacterPool::Life ? TEXT("leech.life.") : TEXT("leech.shield.")) + Tail));
}
} // namespace
double FDWRuleCharacter::Recover(ECharacterPool Pool, double Amount, bool AllowOverflow)
{
	if (bDead || !(Amount > 0))
		return 0;
	if (Pool == ECharacterPool::Mana)
	{
		const double Gained = FMath::Min(FMath::Max(0., ManaCapacity() - Mana), Amount);
		Mana += Gained;
		Flow.ManaRestored += Gained;
		Totals.ManaRecovered += Gained;
		return Gained;
	}
	if (Pool == ECharacterPool::Life)
	{
		const double Scaled =
			Amount *
			FMath::Max(0., 1 + (S(TEXT("heal.received")) + (LowLife() ? S(TEXT("heal.received_low")) : 0)) / 100);
		const bool WasFull = FullLife();
		const double Gained = FMath::Min(FMath::Max(0., MaxLife - Life), Scaled);
		Life += Gained;
		Totals.LifeRecovered += Gained;
		Flow.LifeRestored += Gained;
		if (!WasFull && FullLife())
			Emit(TEXT("life_full"));
		const double Excess = Scaled - Gained;
		if (Excess > 0 && S(TEXT("heal.overflow_to_shield")) > 0)
			Recover(ECharacterPool::Shield, Excess * S(TEXT("heal.overflow_to_shield")) / 100);
		if (!LowLife() && FullLife() && bLowSinceFull)
			OnFullLifeFromLow();
		AccumulateRestored(TEXT("cast_on_life_restored"), Gained);
		return Gained;
	}
	const double Gained = FMath::Min(FMath::Max(0., (AllowOverflow ? ShieldCap : MaxShield) - Shield), Amount);
	Shield += Gained;
	Totals.ShieldRecovered += Gained;
	Flow.ShieldRestored += Gained;
	AccumulateRestored(TEXT("cast_on_shield_restored"), Gained);
	return Gained;
}
void FDWRuleCharacter::Fire(FDWSheetTrigger &Trigger, bool Cast)
{
	++Totals.Triggers;
	if (Trigger.Stitched.IsEmpty())
		return;
	FDWPendingPayload Payload;
	Payload.Sources = Trigger.Stitched;
	Payload.Label = Trigger.Label;
	Payload.Duration = Trigger.Param(TEXT("duration"));
	if (Cast)
		PendingCasts.Add(MoveTemp(Payload));
	else
		PendingGrants.Add(MoveTemp(Payload));
}
void FDWRuleCharacter::AccumulateRestored(FName Name, double Gained)
{
	if (!(Gained > 0))
		return;
	for (auto &T : Sheet.Triggers)
	{
		const double Per = T.Param(TEXT("per"));
		if (T.Name != Name || !(Per > 0))
			continue;
		T.Accumulator += Gained;
		while (T.Accumulator >= Per)
		{
			T.Accumulator -= Per;
			Fire(T, true);
		}
	}
}
void FDWRuleCharacter::StartRecharge()
{
	if (MaxShield <= 0 || Shield >= MaxShield)
		return;
	if (!bRecharging)
		Emit(TEXT("recharge_start"));
	bRecharging = true;
	Grace = S(TEXT("shield.recharge_grace"));
	SinceDamaged = FMath::Max(SinceDamaged, RechargeDelay);
}
void FDWRuleCharacter::OnFullLifeFromLow()
{
	bLowSinceFull = false;
	for (auto &T : Sheet.Triggers)
		if (T.Name == TEXT("buff_on_full_life") && T.Cooldown <= 0)
		{
			T.Cooldown = T.Param(TEXT("cooldown"));
			Fire(T, false);
		}
}
void FDWRuleCharacter::Tick(double Dt, bool Moving)
{
	if (bDead)
		return;
	bFresh = false;
	bMoving = Moving;
	SinceDamaged += Dt;
	SinceHit += Dt;
	Stunned = FMath::Max(0., Stunned - Dt);
	GuardLeft = FMath::Max(0., GuardLeft - Dt);
	for (auto &T : Sheet.Triggers)
		T.Cooldown = FMath::Max(0., T.Cooldown - Dt);
	if (!Buffs.IsEmpty())
	{
		for (auto &B : Buffs)
			B.Left -= Dt;
		const int32 Before = Buffs.Num();
		Buffs.RemoveAll([](const auto &B) { return B.Left <= 0; });
		if (Before != Buffs.Num())
			RebuildBuffs();
	}
	if (!SelfPoison.IsEmpty())
	{
		for (double &Left : SelfPoison)
			Left -= Dt;
		SelfPoison.RemoveAll([](double Left) { return Left <= 0; });
		if (!SelfPoison.IsEmpty())
			LoseLife(FMath::Min(Life - 1, MaxLife * .01 * SelfPoison.Num() * Dt));
	}
	Recover(ECharacterPool::Mana, Base.ManaRegen * Dt);
	const double LifePct =
		S(TEXT("life.regen_percent")) +
		(Moving ? S(TEXT("life.regen_moving_percent")) : S(TEXT("life.regen_still_percent"))) +
		(RecentlyDamaged() ? S(TEXT("life.regen_recent_hit_percent")) : S(TEXT("life.regen_no_recent_hit_percent"))) +
		SelfPoison.Num() * .5;
	const double LifeRegen =
		(Base.LifeRegen + S(TEXT("life.regen_flat")) + MaxLife * LifePct / 100) *
		FMath::Max(0., 1 + (S(TEXT("life.regen_speed")) + (LowLife() ? S(TEXT("life.regen_low_speed")) : 0)) / 100) *
		Dt;
	if (LifeRegen > 0)
	{
		const bool WasFull = FullLife();
		const double Gained = Recover(ECharacterPool::Life, LifeRegen);
		if (S(TEXT("shield.from_life_regen_percent")) > 0)
			Recover(ECharacterPool::Shield, LifeRegen * S(TEXT("shield.from_life_regen_percent")) / 100);
		const double Spill = WasFull ? LifeRegen : FMath::Max(0., LifeRegen - Gained);
		if (Spill > 0 && S(TEXT("shield.from_overflow_life_regen_percent")) > 0)
			Recover(ECharacterPool::Shield, Spill * S(TEXT("shield.from_overflow_life_regen_percent")) / 100);
	}
	if (MaxShield > 0)
	{
		const double Pct = S(TEXT("shield.regen_percent")) +
						   (Moving ? S(TEXT("shield.regen_moving_percent")) : S(TEXT("shield.regen_still_percent")));
		const double Speed = S(TEXT("shield.regen_speed")) + (LowShield() ? S(TEXT("shield.regen_low_speed")) : 0) +
							 MissingShieldPercent() * S(TEXT("shield.missing.regen_speed"));
		Recover(ECharacterPool::Shield, (S(TEXT("shield.regen_flat")) + MaxShield * Pct / 100) *
											FMath::Max(0., 1 + Speed / 100) *
											FMath::Max(0., 1 + S(TEXT("shield.sustain_efficiency")) / 100) * Dt);
		if (!bRecharging && SinceDamaged >= RechargeDelay && Shield < MaxShield)
			StartRecharge();
		if (bRecharging)
		{
			Grace = FMath::Max(0., Grace - Dt);
			Recover(ECharacterPool::Shield, RechargeRate * Dt, false);
			if (Shield >= MaxShield)
				bRecharging = false;
		}
		if (Shield > MaxShield)
			Shield = FMath::Max(MaxShield,
								Shield - (Shield - MaxShield) * Base.OverflowDecay / 100 *
											 FMath::Max(0., 1 - S(TEXT("shield.overflow_decay_reduction")) / 100) * Dt);
	}
	TickLeech(ECharacterPool::Life, Dt);
	TickLeech(ECharacterPool::Shield, Dt);
	for (auto &R : Recoups)
	{
		const double Take = FMath::Min(R.Left, R.Rate * Dt);
		R.Left -= Take;
		Recover(R.Pool, Take);
	}
	Recoups.RemoveAll([](const auto &R) { return R.Left <= 1.e-6; });
	for (auto &R : Flasks)
	{
		const double Take = FMath::Min(R.Left, R.Rate * Dt);
		R.Left -= Take;
		Recover(ECharacterPool::Life, Take);
	}
	Flasks.RemoveAll([](const auto &R) { return R.Left <= 1.e-6; });
	// A lethal recovery can clear the live queue. Iterate a stable batch, as JS does over its old array.
	TArray<FDWRecovery> Batch = MoveTemp(Deferred);
	const int32 Cheats = Totals.CheatedDeath;
	for (auto &R : Batch)
	{
		const double Take = FMath::Min(R.Left, R.Rate * Dt);
		R.Left -= Take;
		LoseLife(Take);
	}
	if (Cheats == Totals.CheatedDeath)
		for (const auto &R : Batch)
			if (R.Left > 1.e-6)
				Deferred.Add(R);
	const bool Low = LowLife();
	if (Low)
		bLowSinceFull = true;
	if (Low && !bWasLow)
	{
		Emit(TEXT("enter_low"));
		for (auto &T : Sheet.Triggers)
			if (T.Name == TEXT("guard_on_low_life") && T.Cooldown <= 0)
			{
				T.Cooldown = T.Param(TEXT("cooldown"));
				GuardLeft = T.Param(TEXT("duration"));
				GuardReduction = T.Param(TEXT("reduction"));
				++Totals.Triggers;
			}
	}
	bWasLow = Low;
	const bool Full = FullShield();
	if (Full && !bWasFullShield)
	{
		Emit(TEXT("shield_full"));
		for (auto &T : Sheet.Triggers)
			if (T.Name == TEXT("buff_on_full_shield"))
				Fire(T, false);
	}
	bWasFullShield = Full;
}
void FDWRuleCharacter::TickLeech(ECharacterPool Pool, double Dt)
{
	auto &List = Pool == ECharacterPool::Life ? LifeLeech : ShieldLeech;
	if (List.IsEmpty())
		return;
	const double Max = Pool == ECharacterPool::Life ? MaxLife : MaxShield;
	const bool Full = Pool == ECharacterPool::Life ? FullLife() : Shield >= MaxShield;
	if (Full && !(S(LeechPath(Pool, TEXT("keep_full"))) > 0))
	{
		List.Reset();
		return;
	}
	double Budget = Max * Base.LeechCap / 100 * FMath::Max(0., 1 + S(LeechPath(Pool, TEXT("cap"))) / 100) * Dt;
	for (auto &R : List)
	{
		if (Budget <= 0)
			break;
		const double Take = FMath::Min3(R.Left, R.Rate * Dt, Budget);
		R.Left -= Take;
		Budget -= Take;
		Recover(Pool, Take, false);
	}
	List.RemoveAll([](const auto &R) { return R.Left <= 1.e-6; });
}
void FDWRuleCharacter::AddLeech(ECharacterPool Pool, double Amount)
{
	if (!(Amount > 0))
		return;
	const double Max = Pool == ECharacterPool::Life ? MaxLife : MaxShield;
	if (!(Max > 0))
		return;
	const double Instant = Amount * FMath::Clamp(S(LeechPath(Pool, TEXT("instant"))), 0., 100.) / 100;
	if (Instant > 0)
		Recover(Pool, Instant, false);
	const double Speed =
		S(LeechPath(Pool, TEXT("speed"))) +
		(Pool == ECharacterPool::Life ? MissingLifePercent() * S(TEXT("life.missing.leech_speed")) : 0);
	const double Rate = Max * Base.LeechRate / 100 * FMath::Max(.01, 1 + Speed / 100),
				 Left = (Amount - Instant) * FMath::Max(0., 1 + S(LeechPath(Pool, TEXT("duration"))) / 100);
	auto &List = Pool == ECharacterPool::Life ? LifeLeech : ShieldLeech;
	if (Left > 0 && List.Num() < 200)
		List.Add({Pool, Left, Rate});
}
void FDWRuleCharacter::AddRecoup(ECharacterPool Pool, double Amount, double Seconds)
{
	if (!(Amount > 0) || !(Seconds > 0))
		return;
	if (Pool == ECharacterPool::Life)
		Seconds /= FMath::Max(.1, 1 + S(TEXT("recoup.life.speed")) / 100);
	if (Recoups.Num() < 300)
		Recoups.Add({Pool, Amount, Amount / Seconds});
}
void FDWRuleCharacter::LoseLife(double Amount)
{
	if (bDead || !(Amount > 0))
		return;
	Life -= Amount;
	Totals.LifeLost += Amount;
	Flow.LifeLost += Amount;
	if (Life <= 0)
		OnLethal();
}
void FDWRuleCharacter::OnLethal()
{
	auto *Heal = Sheet.Triggers.FindByPredicate(
		[](const auto &T) { return T.Name == TEXT("cheat_death_heal") && T.Cooldown <= 0; });
	auto *Keep = Sheet.Triggers.FindByPredicate(
		[](const auto &T) { return T.Name == TEXT("cheat_death_keep") && T.Cooldown <= 0; });
	if (auto *T = Heal ? Heal : Keep)
	{
		T->Cooldown = T->Param(TEXT("cooldown"));
		Life = Heal ? FMath::Max(1., MaxLife * T->Param(TEXT("percent")) / 100) : 1;
		Deferred.Reset();
		++Totals.CheatedDeath;
		Emit(TEXT("lethal"));
		return;
	}
	Life = 0;
	bDead = true;
}
void FDWRuleCharacter::UseItem(const TArray<FDWTraitSource> &Sources)
{
	for (const auto &Source : Sources)
	{
		FDWHeldTrait Held;
		Held.Source = Source;
		FDWSheet Item;
		FString Error;
		if (!Item.Compute({Held}, Error))
			continue;
		for (const auto &T : Item.Triggers)
			if (T.Name == TEXT("flask_life") && T.Param(TEXT("amount")) > 0)
			{
				const double Total = T.Param(TEXT("amount")) * FMath::Max(0., 1 + S(TEXT("flask.life.amount")) / 100),
							 Instant = Total * FMath::Clamp(S(TEXT("flask.life.instant")), 0., 100.) / 100;
				Recover(ECharacterPool::Life, Instant);
				const double Seconds =
					FMath::Max(.05, T.Param(TEXT("seconds")) / FMath::Max(.1, 1 + S(TEXT("flask.life.speed")) / 100));
				if (Total - Instant > 0)
					Flasks.Add({ECharacterPool::Life, Total - Instant, (Total - Instant) / Seconds});
				Emit(TEXT("flask"), InvalidId, Total);
			}
	}
}
} // namespace DWRules
