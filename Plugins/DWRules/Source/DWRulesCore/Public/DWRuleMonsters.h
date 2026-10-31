#pragma once
#include "DWRuleCollection.h"
#include "DWRuleCharacter.h"

namespace DWRules
{
class FDWRuleHost;
struct FDWMonsterSkill
{
	FName Id = TEXT("monster_claw");
	bool bEnabled = true;
	double RangeCm = 150, Cooldown = 1.2, Power = 10;
};
struct FDWMonsterDefinition
{
	FName Id;
	FString Name, Description, Color = TEXT("#a82e32");
	bool bEnabled = true;
	double Weight = 1, Health = 80, SpeedCm = 300, StopDistanceCm = 130;
    double VisualScale=1,FollowDistanceCm=0;
	int32 MinFloor = 1;
	FDWPawnEquipment Equipment;
	TArray<FDWTraitEntry> Traits;
	// Existing rule sources inherited from the summoner (e.g. an equipped map).
	TArray<FDWTraitSource> SummonerSources;
	TArray<FDWMonsterSkill> Skills;
};
struct FDWCompiledMonsterSkill : FDWMonsterSkill
{
	int32 Plan = INDEX_NONE;
};
struct FDWCompiledMonster
{
	FDWMonsterDefinition Definition;
	TArray<FDWTraitSource> Sources;
	TArray<FDWHeldTrait> Sheet;
	TArray<FDWCompiledMonsterSkill> Skills;
	int32 DefensePlan = INDEX_NONE;
};
struct FDWMonsterCooldowns
{
	double Shared = 0;
	TArray<double> Skills;
};
class DWRULESCORE_API FDWMonsterRules
{
  public:
	static bool Compile(FDWRuleHost &Host, const FDWMonsterDefinition &Definition, FDWCompiledMonster &Out,
						FString &Error);
	static bool Equip(FDWRuleHost &Host, const FDWCompiledMonster &Monster, double BaseLife,
					  double BaseShield, FDWRuleCharacter &Character, FString &Error);
	static int32 OffensePlan(FDWRuleHost &Host, const FDWCompiledMonster &Monster,
							 FDWRuleCharacter &Character, int32 Plan, FString &Error);
	// Returns the first in-range ready skill, in authored order. Freezing pauses every countdown.
	static int32 SelectSkill(const FDWCompiledMonster &Monster, FDWMonsterCooldowns &State, double Dt,
							 double DistanceCm, double ActionScale = 1, bool Frozen = false, bool TargetAlive = true,
							 bool CanAttack = true);
};
} // namespace DWRules
