#include "DWRuleSession.h"
#include "DWRuleAilments.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleMinions.h"

namespace DWRules
{
FDWRuleSession::FDWRuleSession(FDWRuleHost &InHost, int32 Owner, uint32 Target, uint32 Seed)
	: Host(InHost), Character(Seed), Combat(Host, Character, Seed), TargetId(Target)
{
	Combat.Hero.Owner = Owner;
	Host.World.SetOwnerStatusId(Owner, Target);
	Skills.SetNum(ActiveSkillSlotCount);
}
bool FDWRuleSession::SetLoadout(FDWRuleLoadout Value, FString &Error)
{
    for(const auto& Group:Loadout.RuntimeGroups)if(!Value.RuntimeGroups.Contains(Group.Key))Value.RuntimeGroups.Add(Group.Key,Group.Value);
	for (auto& Item : Value.Instances) if (!FDWEquipmentRules::EnsureBases(Host.Library, Item, Error)) return false;
	TArray<FDWCollectionPart> Parts;
	if (!Value.ResolveEquipment(Host.Library, Parts, Error))
		return false;
	Value.Validate(Host.Library);
	TArray<FDWSessionSkill> Next;
	Next.SetNum(ActiveSkillSlotCount);
	for (int32 I = 0; I < Next.Num(); ++I)
	{
		if (Value.Skills[I].IsNone())
			continue;
		FDWBuildSource Source;
		TArray<FDWContribution> Rows;
		if (!Value.Compose(Host.Library, I, Source, Rows, Error))
			return false;
		auto &S = Next[I];
		S.Id = Value.Skills[I];
		S.Plan = Host.CompileSource(Source, Error);
		if (S.Plan < 0)
			return false;
		const auto &Plan = Host.World.GetPlan(S.Plan);
		const auto Gems = Value.GemParts(Host.Library, S.Id);
		for (const auto &Row : Rows)
			if (!Row.bMerged && !Row.bExpired && Plan.TraitIds.Contains(Row.Held.Source.Id))
			{
				S.Sheet.Add(Row.Held);
				if (Gems.ContainsByPredicate([&](const auto &P) { return P.Label == Row.Held.Label; }))
					S.GemDefense.Add(Row.Held.Source);
			}
	}
	TArray<FDWTraitSource> NewDefense;
	TArray<FDWHeldTrait> NewDefenseSheet;
	FName Offhand;
	for (const auto &Part : Parts)
	{
		if (Part.Collection.Slot == TEXT("offhand"))
		{
			const bool Weapon = Part.Collection.Tags.ContainsByPredicate([](const FString &Tag) {
				return Tag.Contains(TEXT("武器")) || Tag.Contains(TEXT("双持")) ||
					   Tag.Contains(TEXT("weapon"));
			});
			Offhand = Weapon ? TEXT("weapon") : TEXT("shield");
		}
	}
	if (Parts.ContainsByPredicate([](const auto &P) { return P.Collection.Traits.Num() > 0 || P.Collection.Bases.Num() > 0; }))
	{
		FDWBuildSource Source;
		TArray<FDWContribution> Rows;
		if (!Host.Library.Aggregate(NAME_None, Parts, TNumericLimits<double>::Max(), Source, Rows, Error))
			return false;
		NewDefense = MoveTemp(Source.Traits);
		for (const auto &Row : Rows)
			if (!Row.bMerged && !Row.bExpired)
				NewDefenseSheet.Add(Row.Held);
	}
	Loadout = MoveTemp(Value);
	Skills = MoveTemp(Next);
	Defense = MoveTemp(NewDefense);
	DefenseSheet = MoveTemp(NewDefenseSheet);
	Character.Offhand = Offhand;
	bConfigured = true;
	const bool Ok=Select(Selected,Error);if(Ok)++LoadoutRevision;return Ok;
}
bool FDWRuleSession::Select(int32 Slot, FString &Error)
{
	if (!Skills.IsValidIndex(Slot))
	{
		Error = TEXT("active slot must be 0..3");
		return false;
	}
	Selected = Slot;
	Combat.Hero.Plan = Skills[Slot].Plan;
	Combat.Hero.Skill = Skills[Slot].Id;
    Combat.Cooldown=FMath::Max(0.,Combat.SkillReadyAt.FindRef(Combat.Hero.Skill)-Combat.Time);
	BaseSheet = Skills[Slot].Sheet;
	if (Combat.Hero.Plan < 0)
		BaseSheet = DefenseSheet;
	if (!BindDefense(Error))
		return false;
	SheetKey.Reset();
	return RefreshSheet(Error);
}
bool FDWRuleSession::SetDirectPlan(int32 Plan, FName Skill, FString &Error)
{
	if (bConfigured)
		return true;
	if (Combat.Hero.Plan == Plan)
		return true;
	const auto *Source = Host.PlanSource(Plan);
	if (!Source)
	{
		Error = TEXT("unknown player plan");
		return false;
	}
	Combat.Hero.Plan = Plan;
	Combat.Hero.Skill = Skill.IsNone() ? Source->ActiveId : Skill;
	BaseSheet.Reset();
	for (const auto &S : Source->Traits)
		if (Host.World.GetPlan(Plan).TraitIds.Contains(S.Id))
			BaseSheet.Add({S});
	SheetKey.Reset();
	return RefreshSheet(Error);
}
bool FDWRuleSession::BindDefense(FString &Error)
{
	TArray<FDWTraitSource> Sources = Defense;
	if (Skills.IsValidIndex(Selected))
		for (const auto &S : Skills[Selected].GemDefense)
		{
			const int32 I = Sources.IndexOfByPredicate([&](const auto &Other) { return Other.Id == S.Id; });
			if (I < 0)
				Sources.Add(S);
			else
				Sources[I] = S;
		}
	return Host.SetTargetDefenseSources(TargetId, Sources, Error);
}
bool FDWRuleSession::RefreshSheet(FString &Error)
{
	TArray<FDWHeldTrait> Entries = BaseSheet;
	TArray<FDWTraitSource> Held;
	Host.HeldSources(TargetId, Held);
	for (const auto &S : Held)
		if (S.Sheet.Num())
			Entries.Add({S});
	FString Key = FString::FromInt(Selected);
	for (const auto &E : Entries)
		Key +=
			TEXT("|") + TraitSourceKey(E.Source) + (E.bEquipment ? TEXT("e") : TEXT("s")) + E.Slot.ToString();
	if (Key == SheetKey)
		return true;
	Error.Reset();
	Character.SetSheet(Entries, Error, Key);
	if (!Error.IsEmpty())
		return false;
	SheetKey = MoveTemp(Key);
	return true;
}
void FDWRuleSession::RunCharacterTriggers()
{
	auto Casts = MoveTemp(Character.PendingCasts);
	Character.PendingCasts.Reset();
	for (const auto &C : Casts)
		Combat.Triggers->CastFromCharacter(C);
	auto Grants = MoveTemp(Character.PendingGrants);
	Character.PendingGrants.Reset();
	for (const auto &G : Grants)
		Host.GrantSources(TargetId, G.Sources, G.Duration, G.Label);
}
void FDWRuleSession::TickCharacter(double Dt, bool Moving)
{
	Host.RefreshGrantedDefense();
	FString Error;
	RefreshSheet(Error);
	DodgeCooldown = FMath::Max(0., DodgeCooldown - Dt);
	Combat.Triggers->bGod = bGod;
	Character.Tick(Dt, Moving);
	RunCharacterTriggers();
}
void FDWRuleSession::BeforeWorld(double Dt)
{
	Combat.Minions->AddTargets();
	Combat.BeforeStep(Dt);
}
void FDWRuleSession::AfterWorld()
{
	Combat.ConsumeRows();
	Combat.ScaleHeroAilments(SeenAilments);
	if (bGod && !Character.bDead)
	{
		Character.Life = Character.MaxLife;
		Character.Shield = FMath::Max(Character.Shield, Character.MaxShield);
		Character.Mana = Character.ManaCapacity();
	}
}
int32 FDWRuleSession::Cast(int32 Plan, const FVector3f &Origin, const FVector3f &Direction, double Power,
						   uint64 Seed, FString &Error)
{
	if (bConfigured)
		Plan = Combat.Hero.Plan;
	else if (!SetDirectPlan(Plan, NAME_None, Error))
		return INDEX_NONE;
	float Remaining = 0;
	if (Character.bDead || Character.Stunned > 0 || Combat.Cooldown > 0 ||
		Host.World.GetStatusEffect(TargetId, EStatusEffect::Control, Remaining) > 0)
	{
		Error = TEXT("player cannot act");
		return INDEX_NONE;
	}
	if (Plan < 0)
	{
		Error = TEXT("active skill slot is empty");
		return INDEX_NONE;
	}
	Combat.Hero.Power = Power;
	Host.RefreshGrantedDefense();
	if (!RefreshSheet(Error))
		return INDEX_NONE;
	auto Pay = Character.BeforeCast(bGod ? 0 : Character.Base.CastCost, Combat.IsAttack(Plan),
									Combat.CostScale(Plan));
	if (!Pay.bOk)
	{
		Error = Pay.Reason.ToString();
		return INDEX_NONE;
	}
	int32 FinalPlan = Host.PlanWithGrantedTraits(Plan, Combat.Hero.Owner, Error);
	if (FinalPlan < 0)
		return INDEX_NONE;
	TArray<FDWTraitSource> Extras;
	if (!Combat.SkillFamily(Plan).bSummon)
	{
		FDWTraitSource Offense, Shape;
		if (Character.Offense(Pay.DamageMore, Pay.AreaIncreased, Pay.CritBonus)
				.MakeSource(Host.World.GetPlan(FinalPlan).Stats, Offense))
			Extras.Add(Offense);
		if (Combat.PlanSource(Shape))
			Extras.Add(Shape);
	}
	if (Extras.Num())
		FinalPlan = Host.PlanWithSources(FinalPlan, Extras, Error);
	if (FinalPlan < 0)
		return INDEX_NONE;
	const auto *Owner = Host.World.GetOwner(Combat.Hero.Owner);
	const int32 Root = Host.World.Cast(FinalPlan, Combat.Hero.Owner, Origin, Direction, Seed, float(Power),
									   Owner ? Owner->TargetPoint : TOptional<FVector3f>());
	if (Root >= 0)
	{
		++Combat.Casts;
		Combat.OnHeroCast(Root, Pay);
	}
	RunCharacterTriggers();
	return Root;
}
bool FDWRuleSession::UseItem(FName Id, const FVector3f &Origin, const FVector3f &Direction, uint64 Seed,
							 FString &Error)
{
	const auto *Item = Host.Library.Items.Find(Id);
	if (!Item || Character.bDead)
	{
		Error = TEXT("item unavailable");
		return false;
	}
	TMap<double, TArray<FDWTraitEntry>> Timed;
	TArray<FDWTraitEntry> Untimed;
	for (const auto &E : Item->Traits)
	{
		FDWTraitSource Source;
		if (!Host.Library.SourceFromEntry(E, Source, Error))
			return false;
		FDWSheet Test;
		Test.Compute({{Source}}, Error);
		if (Test.Triggers.ContainsByPredicate([](const auto &T) { return T.Name == TEXT("flask_life"); }))
			Character.UseItem({Source});
		else if (E.Duration > 0)
			Timed.FindOrAdd(E.Duration).Add(E);
		else
			Untimed.Add(E);
	}
	for (const auto &Group : Timed)
		if (Host.GrantTraits(TargetId, Group.Value, float(Group.Key), Item->Name, Error) < 0)
			return false;
	if (Item->Skills.Num())
	{
		auto Parts = Loadout.EquipmentParts(Host.Library);
		Parts.Add({*Item, false, Item->Name});
		FDWBuildSource Source;
		TArray<FDWContribution> Rows;
		if (!Host.Library.Aggregate(Item->Skills[0], Parts, 3600, Source, Rows, Error))
			return false;
		int32 Plan = Host.CompileSource(Source, Error);
		Plan = Host.PlanWithGrantedTraits(Plan, Combat.Hero.Owner, Error);
		if (Plan < 0 || Host.World.Cast(Plan, Combat.Hero.Owner, Origin, Direction, Seed, 30) < 0)
			return false;
	}
	else if (Untimed.Num())
		if (Host.GrantTraits(TargetId, Untimed, 3600, Item->Name, Error) < 0)
			return false;
	RefreshSheet(Error);
	RunCharacterTriggers();
	return true;
}
void FDWRuleSession::Respawn()
{
	Host.ForgetTarget(TargetId);
	SeenAilments.Reset();
	FString Error;
	BindDefense(Error);
	SheetKey.Reset();
	RefreshSheet(Error);
	Character.Reset();
}
bool FDWRuleSession::Dodge()
{
	if (DodgeCooldown > 0 || Character.bDead)
		return false;
	DodgeCooldown = 1;
	Combat.Triggers->Event(TEXT("trig_dodge"), {Combat.HeroPosition(), InvalidId, true});
	return true;
}
bool FDWRuleSession::Sacrifice()
{
	return Combat.Minions->Sacrifice();
}
void FDWRuleSession::Forget(uint32 Id)
{
	Combat.Ailments->Forget(Id);
	Combat.Triggers->Forget(Id);
	Combat.Enemies.RemoveAll([&](const auto &M) { return M.Id == Id; });
	Combat.Attackers.Remove(Id);
	Combat.LastCritical.Remove(Id);
	Host.ForgetTarget(Id);
}
TArray<FDWCombatEffect> FDWRuleSession::DrainEffects()
{
	auto Out = MoveTemp(Combat.Effects);
	Combat.Effects.Reset();
	return Out;
}
} // namespace DWRules
