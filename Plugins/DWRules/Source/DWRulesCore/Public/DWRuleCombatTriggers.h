#pragma once
#include "DWRuleCombat.h"

namespace DWRules
{
struct FDWTriggerContext
{
	FVector3d Position = FVector3d::ZeroVector;
	uint32 Id = InvalidId;
	bool bHasPosition = false;
};
struct FDWTriggerState
{
	double Cooldown = 0, Fail = 0, Recharge = 0;
	int32 Charges = 1, Layers = 0, Rotate = 0;
    TOptional<FDWTriggerContext> Pending;
    double RetryAt=0;
};
class DWRULESCORE_API FDWCombatTriggers
{
  public:
	explicit FDWCombatTriggers(FDWCombatRuntime &Runtime) : R(Runtime) {}
	FDWCombatRuntime &R;
	bool bGod = false, bCasting = false;
	int32 Fired = 0;
	void Event(FName Name, const FDWTriggerContext &Context = {});
	bool TryFire(FDWSheetTrigger &Trigger, const FDWTriggerContext &Context);
	void Fire(FDWSheetTrigger &Trigger, const FDWTriggerContext &Context);
	void CastFromCharacter(const FDWPendingPayload &Request);
	void OnDealt(const FDWCombatHit &Hit);
	void Count(FName Kind, double Amount, const FDWTriggerContext &Context);
	void OnCast(bool Attack, const FDWCastPayment &Payment);
	void Ailment(FName Type, uint32 Id, int32 Layers = 1);
	void Combat();
	void Tick(double Dt);
	void Forget(uint32 Id);
	FDWTriggerState &State(FDWSheetTrigger &Trigger);

  private:
	struct FJob
	{
		double At = 0;
		TArray<FDWTraitSource> Sources;
		FDWTriggerContext Context;
		FString TriggerKey;
		int32 Layers = 0;
	};
	TArray<FJob> Queue;
	TMap<FString, FDWTriggerState> States;
	FName LastSkill;
	int32 Streak = 0;
	double LastCombat = -1.e30, MoveTime = 0, StillTime = 0, Channel = 0;
	bool bCrowd = false;
	TOptional<FVector3d> LastPosition;
	TSet<uint32> Elites, HitEver;
	TSet<FString> AilmentFired;
	void Cast(const FJob &Job);
	static TArray<TArray<FDWTraitSource>> Mounted(const TArray<FDWTraitSource> &Sources);
	static FString Key(const FDWSheetTrigger &Trigger);
    void Defer(FDWSheetTrigger& Trigger,const FDWTriggerContext& Context);
};
} // namespace DWRules
