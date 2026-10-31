#pragma once
#include "DWRuleCombat.h"
#include "DWRuleLoadout.h"

namespace DWRules
{
struct FDWSessionSkill
{
	FName Id;
	int32 Plan = INDEX_NONE;
	TArray<FDWHeldTrait> Sheet;
	TArray<FDWTraitSource> GemDefense;
};
// Player orchestration only: inventory choices -> plans, character resources -> casts.
// The same host/world executes player, monster and derived rules. Positions are meters.
class DWRULESCORE_API FDWRuleSession
{
  public:
	FDWRuleSession(FDWRuleHost &InHost, int32 Owner, uint32 Target, uint32 Seed = 7);
	FDWRuleHost &Host;
	FDWRuleCharacter Character;
	FDWCombatRuntime Combat;
	FDWRuleLoadout Loadout;
	TArray<FDWSessionSkill> Skills;
	TArray<FName> Belt;
	int32 Selected = 0;
    uint64 LoadoutRevision = 0;
	bool bConfigured = false, bGod = false;
	uint32 TargetId;
	bool SetLoadout(FDWRuleLoadout Value, FString &Error);
	bool Select(int32 Slot, FString &Error);
	bool SetDirectPlan(int32 Plan, FName Skill, FString &Error);
	bool RefreshSheet(FString &Error);
	void TickCharacter(double Dt, bool Moving);
	void BeforeWorld(double Dt);
	void AfterWorld();
	int32 Cast(int32 Plan, const FVector3f &Origin, const FVector3f &Direction, double Power, uint64 Seed,
			   FString &Error);
	bool UseItem(FName Id, const FVector3f &Origin, const FVector3f &Direction, uint64 Seed, FString &Error);
	void RunCharacterTriggers();
	void Respawn();
	bool Dodge();
	bool Sacrifice();
	void Forget(uint32 Id);
	TArray<FDWCombatEffect> DrainEffects();

  private:
	TArray<FDWTraitSource> Defense;
	TArray<FDWHeldTrait> BaseSheet, DefenseSheet;
	double DodgeCooldown = 0;
	FString SheetKey;
	TMap<uint64, TPair<float, int32>> SeenAilments;
	bool BindDefense(FString &Error);
};
} // namespace DWRules
