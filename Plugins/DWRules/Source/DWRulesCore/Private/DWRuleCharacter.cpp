#include "DWRuleCharacter.h"

namespace DWRules
{
FDWRuleCharacter::FDWRuleCharacter(uint32 Seed) : RandomState(Seed ? Seed : 1)
{
	Derive();
	Reset();
}
double FDWRuleCharacter::S(FName Path) const
{
	return Sheet.Stat(Path) + BuffStats.FindRef(Path) + Aura.FindRef(Path);
}
bool FDWRuleCharacter::Chance(double Percent)
{
	if (!(Percent > 0))
		return false;
	double Roll;
	if (RandomOverride)
		Roll = RandomOverride();
	else
	{
		RandomState = 1664525u * RandomState + 1013904223u;
		Roll = double(RandomState) / 4294967296.0;
	}
	return Roll * 100 < Percent;
}
void FDWRuleCharacter::Emit(FName Type, uint32 Attacker, double Amount, double Stopped, double OverflowValue,
							bool Critical, bool Blocked)
{
	if (CombatEvents.Num() < 400)
		CombatEvents.Add({Type, Attacker, Amount, Stopped, OverflowValue, Critical, Blocked});
}
bool FDWRuleCharacter::SetSheet(const TArray<FDWHeldTrait> &Entries, FString &Error, const FString &Key)
{
	if (!Key.IsEmpty() && Key == SheetKey)
		return false;
	FDWSheet Next;
	if (!Next.Compute(Entries, Error))
		return false;
	for (auto &Trigger : Next.Triggers)
		if (const auto *Old = Sheet.Triggers.FindByPredicate(
				[&](const auto &T) { return T.Trait == Trigger.Trait && T.Name == Trigger.Name; }))
		{
			Trigger.Cooldown = Old->Cooldown;
			Trigger.Accumulator = Old->Accumulator;
		}
	const bool WasFull = FullLife();
	Sheet = MoveTemp(Next);
	SheetKey = Key;
	Derive();
	if (WasFull && bFresh)
	{
		Life = MaxLife;
		Shield = MaxShield;
		Mana = ManaCapacity();
	}
	Life = FMath::Min(Life, MaxLife);
	Shield = FMath::Min(Shield, ShieldCap);
	Mana = FMath::Min(Mana, ManaCapacity());
	return true;
}
void FDWRuleCharacter::AddBuff(FName Stat, double Value, double Duration, FName Key)
{
	if (!(Duration > 0) || Value == 0)
		return;
	if (Key.IsNone())
		Key = Stat;
	if (auto *Live = Buffs.FindByPredicate([=](const auto &B) { return B.Key == Key; }))
	{
		Live->Value = FMath::Max(Live->Value, Value);
		Live->Left = FMath::Max(Live->Left, Duration);
	}
	else
		Buffs.Add({Stat, Key, Value, Duration});
	RebuildBuffs();
}
void FDWRuleCharacter::RebuildBuffs()
{
	BuffStats.Reset();
	for (const auto &B : Buffs)
		BuffStats.FindOrAdd(B.Stat) += B.Value;
}
void FDWRuleCharacter::Derive()
{
	Strength = S(TEXT("attr.strength"));
	Dexterity = S(TEXT("attr.dexterity"));
	Intelligence = S(TEXT("attr.intelligence"));
	Constitution = S(TEXT("attr.constitution"));
	MaxMana = FMath::Max(0., Base.Mana + S(TEXT("mana.base")));
	const double LifeBase = Base.Life + S(TEXT("life.base")) +
							S(TEXT("life.base@equipment")) * (1 + S(TEXT("life.equipment_increased")) / 100) +
							Strength * S(TEXT("life.per_strength")) + Constitution * S(TEXT("life.per_constitution"));
	MaxLife = FMath::Max(1., LifeBase * FMath::Max(0., 1 + S(TEXT("life.increased")) / 100) * S(TEXT("life.more")));
	EvasionBase = S(TEXT("defense.evasion")) +
				  S(TEXT("defense.evasion@equipment")) * (1 + S(TEXT("evasion.equipment_increased")) / 100) +
				  Dexterity * S(TEXT("evasion.per_dexterity"));
	double EquipmentShield = S(TEXT("shield.base@equipment"));
	for (const auto &P : Sheet.Stats)
	{
		FString Slot = P.Key.ToString();
		if (!Slot.RemoveFromStart(TEXT("shield.base@equipment:")))
			continue;
		const double Local = Slot == TEXT("helmet")	 ? S(TEXT("shield.slot_increased.helmet"))
							 : Slot == TEXT("chest") ? S(TEXT("shield.slot_increased.chest"))
							 : (Slot == TEXT("gloves") || Slot == TEXT("boots"))
								 ? S(TEXT("shield.slot_increased.hands_feet"))
								 : 0;
		EquipmentShield += P.Value * (1 + Local / 100);
	}
	const double ShieldBase =
		S(TEXT("shield.base")) + EquipmentShield * (1 + S(TEXT("shield.equipment_increased")) / 100) +
		Intelligence * S(TEXT("shield.per_intelligence")) + Strength * S(TEXT("shield.per_strength")) +
		MaxLife * S(TEXT("shield.from_life_percent")) / 100 + MaxMana * S(TEXT("shield.from_mana_percent")) / 100 +
		EvasionValue(false) * S(TEXT("evasion.shield_per"));
	const double SpellBlockShield = BlockChances({}, false).Spell * S(TEXT("block.shield_per_spell"));
	MaxShield = FMath::Max(0., ShieldBase * FMath::Max(0., 1 + (S(TEXT("shield.increased")) + SpellBlockShield) / 100) *
								   S(TEXT("shield.more")));
	ShieldCap = MaxShield * (1 + FMath::Max(0., S(TEXT("shield.overflow_percent"))) / 100);
	LowLifeThreshold = FMath::Clamp(Base.LowLife + S(TEXT("life.low_threshold")), 1., 99.);
	RechargeDelay = FMath::Max(0., Base.RechargeDelay / FMath::Max(.1, 1 + S(TEXT("shield.recharge_start")) / 100) -
									   S(TEXT("shield.recharge_delay_reduction")));
	RechargeRate = MaxShield * Base.RechargeRate / 100 * FMath::Max(0., 1 + S(TEXT("shield.recharge_rate")) / 100);
	StunThreshold = MaxLife + MaxShield * S(TEXT("threshold.stun_shield_percent")) / 100;
	AilmentThreshold = MaxLife + MaxShield * S(TEXT("threshold.ailment_shield_percent")) / 100;
}
double FDWRuleCharacter::EvasionValue(bool Live) const
{
	double Increased = S(TEXT("evasion.increased"));
	if (Live && bHasPools)
	{
		Increased += bMoving ? S(TEXT("evasion.moving")) : S(TEXT("evasion.still"));
		if (FullLife())
			Increased += S(TEXT("evasion.full_life"));
		if (LowLife())
			Increased += S(TEXT("evasion.low_life"));
		if (FullShield())
			Increased += S(TEXT("evasion.full_shield"));
		Increased += RecentlyHit() ? S(TEXT("evasion.recently_hit")) : S(TEXT("evasion.not_recently_hit"));
	}
	return FMath::Max(0., EvasionBase * FMath::Max(0., 1 + Increased / 100) * S(TEXT("evasion.more"))) *
		   (1 - FMath::Clamp(S(TEXT("evasion.to_armour")), 0., 100.) / 100);
}
double FDWRuleCharacter::ArmourValue() const
{
	const double FromEvasion = EvasionValue() * FMath::Clamp(S(TEXT("evasion.to_armour")), 0., 100.) /
							   FMath::Max(1.e-9, 100 - FMath::Clamp(S(TEXT("evasion.to_armour")), 0., 99.99));
	return FMath::Max(0., (S(TEXT("defense.armour")) + FromEvasion) * (1 + S(TEXT("buff.armour")) / 100));
}
FDWDefensiveChances FDWRuleCharacter::EvadeChances(const FDWHitKind &Kind, bool Live) const
{
	FDWDefensiveChances Out;
	Out.Cap = FMath::Clamp(75 + S(TEXT("evade.cap")), 0., 95.);
	const double Ev = EvasionValue(Live);
	double Attack = (Ev > 0 ? Ev / (Ev + 600) * 100 : 0) + S(TEXT("defense.evade_chance")) + EvadeFail;
	if (Kind.bMelee)
		Attack += S(TEXT("evade.melee"));
	if (Kind.bProjectile)
		Attack += S(TEXT("evade.projectile"));
	Out.Attack = FMath::Clamp(Attack, 0., Out.Cap);
	Out.Spell = FMath::Clamp(S(TEXT("evade.spell")) + (S(TEXT("evade.spell")) > 0 ? EvadeFail : 0), 0., Out.Cap);
	return Out;
}
FDWDefensiveChances FDWRuleCharacter::BlockChances(const FDWHitKind &Kind, bool Live) const
{
	double Attack = S(TEXT("defense.block_chance")), Both = 0;
	if (Offhand == TEXT("shield"))
		Attack += 15 + S(TEXT("block.shield_item")) + S(TEXT("block.with_shield"));
	if (Offhand == TEXT("weapon"))
		Attack += S(TEXT("block.dual_wield"));
	if (Live && bHasPools)
	{
		Both += bMoving ? S(TEXT("block.moving")) : S(TEXT("block.still"));
		if (LowLife())
			Both += S(TEXT("block.low_life"));
		if (RecentlyHit())
			Both += S(TEXT("block.recently_hit"));
	}
	const double AttackCap = FMath::Clamp(75 + S(TEXT("block.attack_cap")), 0., 95.),
				 SpellCap = FMath::Clamp(75 + S(TEXT("block.spell_cap")), 0., 95.);
	FDWDefensiveChances Out;
	Out.Attack = FMath::Clamp(Attack + Both, 0., AttackCap);
	Out.Spell =
		FMath::Clamp(S(TEXT("block.spell")) + Both + Attack * S(TEXT("block.attack_to_spell")) / 100, 0., SpellCap);
	Out.Overflow = FMath::Max(0., Attack + Both - AttackCap);
	return Out;
}
void FDWRuleCharacter::Reset()
{
	bHasPools = true;
	bFresh = true;
	Life = MaxLife;
	Shield = MaxShield;
	Mana = ManaCapacity();
	bDead = false;
	SinceDamaged = SinceHit = 1.e30;
	bRecharging = false;
	Grace = 0;
	LifeLeech.Reset();
	ShieldLeech.Reset();
	Recoups.Reset();
	Deferred.Reset();
	Flasks.Reset();
	GuardLeft = GuardReduction = Stunned = PendingEmpower = 0;
	bWasFullShield = MaxShield > 0;
	bWasLow = bLowSinceFull = bMoving = false;
	PendingCasts.Reset();
	PendingGrants.Reset();
	Buffs.Reset();
	BuffStats.Reset();
	CombatEvents.Reset();
	EvadeFail = AfterHitReduction = NextAttackMore = NextAttackCrit = NextCost = CounterBank = 0;
	Charges = 0;
	SelfPoison.Reset();
	Flow = {};
	Totals = {};
	for (auto &T : Sheet.Triggers)
	{
		T.Cooldown = 0;
		T.Accumulator = 0;
	}
}
} // namespace DWRules
