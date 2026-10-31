#pragma once

#include "DWRuleSheet.h"
#include "DWRuleStats.h"

namespace DWRules
{
enum class ECharacterPool : uint8
{
	Life,
	Shield,
	Mana
};
struct FDWCharacterBase
{
	double Life = 400, LifeRegen = 3, Mana = 100, ManaRegen = 8, CastCost = 2;
	double RechargeDelay = 2, RechargeRate = 20, LeechRate = 2, LeechCap = 10;
	double LowLife = 35, LowShield = 35, Recently = 4, OverflowDecay = 20, Bypass = 10;
	double StunFraction = 0.25, StunSeconds = 0.4, ArmourFactor = 5, ArmourCap = 90;
};
struct FDWHitKind
{
	bool bAttack = false, bSpell = false, bMelee = false, bProjectile = false, bArea = false;
};
struct FDWCharacterHit
{
	double Amount = 0;
	EElement Element = EElement::Physical;
	bool bDot = false, bCritical = false;
	FDWHitKind Kind;
	uint32 Attacker = InvalidId;
	double OriginalAmount = 0;
};
struct FDWTakenHit
{
	double Taken = 0, ShieldLost = 0, LifeLost = 0, Prevented = 0;
	bool bBlocked = false, bEvaded = false;
};
struct FDWCharacterEvent
{
	FName Type;
	uint32 Attacker = InvalidId;
	double Amount = 0, Stopped = 0, Overflow = 0;
	bool bCritical = false, bBlocked = false;
};
struct FDWPendingPayload
{
	TArray<FDWTraitSource> Sources;
	double Duration = 0;
	FString Label;
};
struct FDWCastPayment
{
	bool bOk = false;
	FName Reason;
	double DamageMore = 0, AreaIncreased = 0, CritBonus = 0, ManaCost = 0, ShieldCost = 0, LifeCost = 0;
};
struct FDWCharacterOffense
{
	double Increased[NumElements] = {}, More[NumElements] = {1, 1, 1, 1, 1, 1}, Flat[NumElements] = {};
	double Critical = 0, Area = 0;
	DWRULESCORE_API bool MakeSource(const FDWStatBlock &BaseStats, FDWTraitSource &Out,
									const TArray<FName> &HitEvents = {FName(TEXT("hit"))}) const;
};
struct FDWCharacterTotals
{
	double LifeRecovered = 0, ShieldRecovered = 0, LifeLost = 0, ShieldLost = 0, Prevented = 0, ManaRecovered = 0;
	int32 Blocked = 0, Evaded = 0, Stuns = 0, CheatedDeath = 0, Triggers = 0;
};
struct FDWCharacterFlow
{
	double LifeRestored = 0, ShieldRestored = 0, ManaRestored = 0, ManaSpent = 0, LifeSpent = 0, ShieldSpent = 0,
		   LifeLost = 0, HitTaken = 0;
};
struct FDWDefensiveChances
{
	double Attack = 0, Spell = 0, Cap = 0, Overflow = 0;
};
struct FDWRecovery
{
	ECharacterPool Pool = ECharacterPool::Life;
	double Left = 0, Rate = 0;
};
struct FDWCharacterBuff
{
	FName Stat, Key;
	double Value = 0, Left = 0;
};

// Native mirror of character.mjs. Host-level resource math deliberately uses doubles;
// only the synthetic offense trait crosses into the float32 compiled rule world.
class DWRULESCORE_API FDWRuleCharacter
{
  public:
	explicit FDWRuleCharacter(uint32 Seed = 9);
	FDWCharacterBase Base;
	FDWSheet Sheet;
	TMap<FName, double> Aura;
	FDWCharacterTotals Totals;
	FDWCharacterFlow Flow;
	TArray<FDWCharacterEvent> CombatEvents;
	TArray<FDWPendingPayload> PendingCasts, PendingGrants;
	TArray<FDWCharacterBuff> Buffs;
	TArray<FDWRecovery> LifeLeech, ShieldLeech, Recoups, Deferred, Flasks;
	TArray<double> SelfPoison;
	double Life = 0, Shield = 0, Mana = 0, MaxLife = 400, MaxShield = 0, MaxMana = 100, ShieldCap = 0;
    double ManaReservationPercent=0;
    double ManaCapacity() const {return MaxMana*FMath::Clamp(1-ManaReservationPercent/100,0.,1.);}
    void ReserveMana(double Percent){ManaReservationPercent=FMath::Clamp(Percent,0.,100.);Mana=FMath::Min(Mana,ManaCapacity());}
	double Strength = 0, Dexterity = 0, Intelligence = 0, Constitution = 0, EvasionBase = 0;
	double RechargeDelay = 2, RechargeRate = 0, LowLifeThreshold = 35, StunThreshold = 400, AilmentThreshold = 400;
	double SinceDamaged = 1.e30, SinceHit = 1.e30, Grace = 0, Stunned = 0, GuardLeft = 0, GuardReduction = 0;
	double PendingEmpower = 0, EvadeFail = 0, AfterHitReduction = 0, NextAttackMore = 0, NextAttackCrit = 0,
		   NextCost = 0, CounterBank = 0;
	int32 Charges = 0;
	bool bDead = false, bFresh = true, bMoving = false, bRecharging = false;
	FName Offhand;
	TFunction<double()> RandomOverride;
	double S(FName Path) const;
	bool SetSheet(const TArray<FDWHeldTrait> &Entries, FString &Error, const FString &Key = FString());
	void Derive();
	void Reset();
	void Tick(double Dt, bool Moving = false);
	double Recover(ECharacterPool Pool, double Amount, bool Overflow = true);
	void AddBuff(FName Stat, double Value, double Duration, FName Key = NAME_None);
	void RebuildBuffs();
	void AddLeech(ECharacterPool Pool, double Amount);
	void AddRecoup(ECharacterPool Pool, double Amount, double Seconds);
	void LoseLife(double Amount);
	FDWTakenHit TakeHit(FDWCharacterHit Hit);
	void OnDealt(double Amount, EElement Element, bool Critical = false, bool Killed = false, bool Dot = false);
	void UseItem(const TArray<FDWTraitSource> &Sources);
	FDWCastPayment BeforeCast(double BaseCost = -1, bool Attack = false, double CostScale = 1);
	FDWCharacterOffense Offense(double DamageMore = 0, double AreaIncreased = 0, double CritBonus = 0) const;
	double ActionSpeed(bool Attack = false) const;
	double MoveSpeed() const;
	double EvasionValue(bool Live = true) const;
	double ArmourValue() const;
	FDWDefensiveChances EvadeChances(const FDWHitKind &Kind = {}, bool Live = true) const;
	FDWDefensiveChances BlockChances(const FDWHitKind &Kind = {}, bool Live = true) const;
	bool Chance(double Percent);
	bool RecentlyHit() const
	{
		return SinceHit < Base.Recently;
	}
	bool RecentlyDamaged() const
	{
		return SinceDamaged < Base.Recently;
	}
	bool FullLife() const
	{
		return Life >= MaxLife - 1.e-6;
	}
	bool LowLife() const
	{
		return Life <= MaxLife * LowLifeThreshold / 100;
	}
	bool FullShield() const
	{
		return MaxShield > 0 && Shield >= MaxShield - 1.e-6;
	}
	bool LowShield() const
	{
		return MaxShield > 0 && Shield <= MaxShield * Base.LowShield / 100;
	}
	double MissingLifePercent() const
	{
		return FMath::Clamp((1 - Life / MaxLife) * 100, 0., 100.);
	}
	double MissingShieldPercent() const
	{
		return MaxShield > 0 ? FMath::Clamp((1 - Shield / MaxShield) * 100, 0., 100.) : 0.;
	}
	double Overflow() const
	{
		return FMath::Max(0., Shield - MaxShield);
	}
	double AilmentScale() const
	{
		return FMath::Clamp(MaxLife / FMath::Max(1., AilmentThreshold), 0., 1.);
	}
	void Emit(FName Type, uint32 Attacker = InvalidId, double Amount = 0, double Stopped = 0, double Overflow = 0,
			  bool Critical = false, bool Blocked = false);

  private:
	void TickLeech(ECharacterPool Pool, double Dt);
	void AccumulateRestored(FName Name, double Gained);
	void Fire(FDWSheetTrigger &Trigger, bool Cast);
	void StartRecharge();
	void OnFullLifeFromLow();
	void OnLethal();
	void OnEvaded();
	void OnBlocked();
	void PreventedPhysical(double Amount);
	TMap<FName, double> BuffStats;
	FString SheetKey;
	uint32 RandomState = 9;
	bool bHasPools = false, bWasFullShield = false, bWasLow = false, bLowSinceFull = false;
};
} // namespace DWRules
