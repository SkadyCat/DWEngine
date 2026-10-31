#pragma once
#include "DWRuleCombat.h"

namespace DWRules
{
constexpr uint32 MinionTargetBase = 0x30000000;
struct FDWMinionBuff
{
	double Value = 0, Until = 0;
};
struct FDWMinionState
{
	uint32 Id = 0, Target = InvalidId, CommandTarget = InvalidId;
	FDWSummon Snapshot;
	FName Kind, Reason;
	bool bMobile = false, bTemporary = false, bEmpowered = false, bDead = false, bCommanded = false;
	double Born = 0, BaseInterval = 1, BaseRange = 0, ImmuneUntil = 0, SinceHit = 9;
	double Life = 0, MaxLife = 0, Shield = 0, MaxShield = 0, CommandAt = 1.e30, BaseDuration = 8, RemainingAtDeath = 0;
	int32 Kills = 0, Merged = 0, Plan = INDEX_NONE;
	uint8 Element = 3;
	TMap<FName, FDWMinionBuff> Buffs;
};
class DWRULESCORE_API FDWMinions
{
  public:
	explicit FDWMinions(FDWCombatRuntime &Runtime) : R(Runtime) {}
	FDWCombatRuntime &R;
	FDWMinions(const FDWMinions &) = delete;
	FDWMinions &operator=(const FDWMinions &) = delete;
	TMap<uint32, TUniquePtr<FDWMinionState>> States;
	int32 Sacrificed = 0, LastCap = 1;
	double YouCritUntil = 0, YouCritValue = 0;
	TArray<FDWMinionState *> Alive() const;
	TArray<FDWMinionState *> Counted() const;
	FDWMinionState *StateOfTarget(uint32 Id) const;
	FDWSummon &SummonOf(FDWMinionState &State) const;
	int32 BaseCap(int32 Cap);
	double Reservation() const;
	int32 CapFor(int32 Cap, uint16 Kind, int32 Root);
	void OnSummon(FDWSummon &Summon, int32 Root, const FDWSummon *Replaced);
	FDWMinionState &Create(const FDWSummon &Summon, bool Mobile = false, bool Temporary = false,
						   bool Empowered = false);
	FDWMinionState *Copy(const FDWSummon &Summon, FVector3d Offset = FVector3d(.8, 0, 0), bool Temporary = false,
						 bool Empowered = false, double Duration = -1, TOptional<FVector3d> At = {},
						 int32 PlanOverride = INDEX_NONE);
	void Spawned(FDWMinionState &State);
	int32 EmptySlots() const;
	double MaxLife(const FDWMinionState &State);
	double MaxShield(const FDWMinionState &State) const;
	double Armour() const;
	double Evasion() const;
	void Heal(FDWMinionState &State, double Amount);
	double SpeedBonus(const FDWMinionState &State);
	void DamageMod(FDWDamageMod &Mod, FDWCombatRoot *Root, uint8 Element, uint32 Target);
	int32 Target(FDWSummon &Summon, int32 Nearest);
	void Tick(double Dt);
	FDWMinionState *Engage(const FDWCombatEnemy &Enemy) const;
	void AddTargets();
	double GuardReduction();
	void TakeDamage(FDWMinionState &State, double Amount, uint8 Element, bool Dot = false, bool Area = false,
					bool Spell = false, bool Transferred = false);
	double AbsorbForHero(double Amount, uint8 Element);
	void Remove(FDWMinionState &State);
	void Die(FDWMinionState &State, FName Reason);
	void OnEnd(const FDWSummon &Summon);
	bool Sacrifice(int32 Count=1,FName Kind=NAME_None);
    int32 ControlCap() const;
    void EnforceControlCap(FDWMinionState* Incoming=nullptr);
    void SacrificeUnit(FDWMinionState& State);
	void OnHit(const FDWCombatHit &Hit);
	void OnKill(const FDWCombatHit &Hit);
	void OnHeroHit(const FDWCombatHit &Hit);
	void OnFlask(double Amount);
	void OnHeroCast();

  private:
	struct FRevive
	{
		double At;
		FDWSummon Summon;
		int32 Plan;
	};
	TArray<FRevive> Revives;
	TOptional<double> LastReplacedLife;
	bool bCloning = false;
	double S(FName Name) const
	{
		return R.S(Name);
	}
};
} // namespace DWRules
