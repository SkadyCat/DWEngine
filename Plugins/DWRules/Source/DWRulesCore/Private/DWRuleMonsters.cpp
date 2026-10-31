#include "DWRuleMonsters.h"
#include "DWRuleHost.h"
#include "DWRulePawnEquipment.h"

namespace DWRules
{
bool FDWMonsterRules::Compile(FDWRuleHost &Host, const FDWMonsterDefinition &Definition, FDWCompiledMonster &Out,
							  FString &Error)
{
	FDWCompiledMonster Next;
	Next.Definition = Definition;
	Next.Sources.Append(Definition.SummonerSources);
	for (const auto& Source : Definition.SummonerSources) Next.Sheet.Add({Source});
	for (const auto &E : Definition.Traits)
	{
		FDWTraitSource Source;
		if (E.Id == TEXT("monster_tower_level") || !E.With.IsEmpty() || E.Duration > 0)
		{
			Error = TEXT("monster traits cannot carry duration or stitching");
			return false;
		}
		if (!Host.Library.SourceFromEntry(E, Source, Error))
			return false;
		Next.Sources.Add(Source);
		Next.Sheet.Add({Source});
	}
	TArray<FDWCollectionPart> Parts;
	if (!FDWPawnEquipmentRules::Resolve(Host.Library, Definition.Equipment, EPawnKind::Monster, Parts, Error))
		return false;
	if (Parts.ContainsByPredicate([](const auto &P) { return P.Collection.Traits.Num() > 0; }))
	{
		FDWBuildSource Source;
		TArray<FDWContribution> Rows;
		if (!Host.Library.Aggregate(NAME_None, Parts, TNumericLimits<double>::Max(), Source, Rows, Error))
			return false;
		Next.Sources.Append(Source.Traits);
		for (const auto &Row : Rows)
			if (!Row.bMerged && !Row.bExpired)
				Next.Sheet.Add(Row.Held);
	}
	// Sheet-only sources stack across innate traits, equipment and summoner inheritance.
	// Give independent instances unique compile identities; their stat keys and values stay unchanged.
	TSet<FName> Used;
	for (auto& Entry : Next.Sources)
	{
		const FName BaseId = Entry.Id;
		int32 N = 2;
		if (Entry.Rules.IsEmpty() && Entry.Stitched.IsEmpty())
			while (Used.Contains(Entry.Id)) Entry.Id = FName(*FString::Printf(TEXT("%s~%d"), *BaseId.ToString(), N++));
		Used.Add(Entry.Id);
	}
	if (Next.Sources.Num())
	{
		FDWBuildSource Source;
		Source.ActiveId = Next.Sources[0].Id;
		Source.Traits = Next.Sources;
		Next.DefensePlan = Host.CompileSource(Source, Error);
		if (Next.DefensePlan == INDEX_NONE)
			return false;
	}
	for (const auto &Skill : Definition.Skills)
	{
		FDWCompiledMonsterSkill S;
		static_cast<FDWMonsterSkill &>(S) = Skill;
		const int32 Base = Host.CompileNamedBuild(Skill.Id, {}, Error);
		if (Base == INDEX_NONE)
			return false;
		S.Plan = Host.PlanWithSources(Base, Next.Sources, Error);
		if (S.Plan == INDEX_NONE)
			return false;
		if (!Host.World.GetPlan(S.Plan).HasRules(uint16(EBuiltinEvent::Cast)))
		{
			Error = TEXT("monster skill has no cast rules: ") + Skill.Id.ToString();
			return false;
		}
		if (Skill.bEnabled)
			Next.Skills.Add(S);
	}
	Out = MoveTemp(Next);
	return true;
}
bool FDWMonsterRules::Equip(FDWRuleHost &Host, const FDWCompiledMonster &M, double BaseLife,
							double BaseShield, FDWRuleCharacter &C, FString &Error)
{
	C.Base.Life = BaseLife;
	C.Base.Mana = 0;
	C.Base.LifeRegen = 0;
	if (!C.SetSheet(M.Sheet, Error) && !Error.IsEmpty())
		return false;
	const double SheetLife = C.MaxLife;
	const double Life =
		M.DefensePlan >= 0
			? Host.World.GetPlan(M.DefensePlan).Stats.From(EStat::OwnerMaxLife, float(SheetLife))
			: SheetLife;
	C.MaxShield = (BaseShield * SheetLife / BaseLife + C.MaxShield) * Life / SheetLife;
	C.MaxLife = Life;
	C.ShieldCap = C.MaxShield * (1 + FMath::Max(0., C.S(TEXT("shield.overflow_percent"))) / 100);
	C.Life = C.MaxLife;
	C.Shield = C.MaxShield;
	return true;
}
int32 FDWMonsterRules::OffensePlan(FDWRuleHost &Host, const FDWCompiledMonster &M, FDWRuleCharacter &C,
								   int32 Plan, FString &Error)
{
	TArray<FName> Events = {TEXT("hit")};
	if (const auto *Source = Host.PlanSource(Plan))
		if (const auto *Active =
				Source->Traits.FindByPredicate([&](const auto &T) { return T.Id == Source->ActiveId; }))
		{
			TFunction<void(const TArray<FDWActionSource> &)> Inspect;
			Inspect = [&](const auto &Actions) {
				for (const auto &A : Actions)
				{
					if ((A.Op == EOp::DamageArea || A.Op == EOp::DamageTarget) && !A.EmitEvent.IsNone())
						Events.AddUnique(A.EmitEvent);
					Inspect(A.Then);
				}
			};
			for (const auto &Rule : Active->Rules)
				if (Rule.Event == TEXT("cast"))
					Inspect(Rule.Actions);
		}
	FDWTraitSource Offense;
	return C.Offense().MakeSource(Host.World.GetPlan(Plan).Stats, Offense, Events)
			   ? Host.PlanWithSources(Plan, {Offense}, Error)
			   : Plan;
}
int32 FDWMonsterRules::SelectSkill(const FDWCompiledMonster &Monster, FDWMonsterCooldowns &State, double Dt,
								   double DistanceCm, double ActionScale, bool Frozen, bool TargetAlive, bool CanAttack)
{
	if (State.Skills.Num() != Monster.Skills.Num())
		State.Skills.SetNumZeroed(Monster.Skills.Num());
	if (Frozen)
		return INDEX_NONE;
	const double ActionDt = Dt * FMath::Min(1., ActionScale);
	State.Shared -= ActionDt;
	for (auto &V : State.Skills)
		V = FMath::Max(0., V - ActionDt);
	if (!TargetAlive || !CanAttack || State.Shared > 0)
		return INDEX_NONE;
	for (int32 I = 0; I < Monster.Skills.Num(); ++I)
	{
		if (State.Skills[I] <= 0 && DistanceCm <= Monster.Skills[I].RangeCm)
		{
			State.Skills[I] = Monster.Skills[I].Cooldown;
			State.Shared = .35;
			return I;
		}
	}
	return INDEX_NONE;
}
} // namespace DWRules
