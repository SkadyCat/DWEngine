#include "DWRuleHost.h"
#include "DWRuleCompiler.h"

namespace DWRules
{
namespace
{
FString SourcesKey(const TArray<FDWTraitSource> &Sources)
{
	FString Key;
	for (const auto &Source : Sources)
		Key += TraitSourceKey(Source).ToLower() + TEXT(";");
	return Key;
}
} // namespace
FDWRuleHost::FDWRuleHost(const FDWWorldConfig &Config) : World(Config)
{
	World.OnGrantTraits = [this](uint32 Target, int32 Owner, float Duration, const FDWGrantDef &Grant) {
		GrantFromRule(Target, Owner, Duration, Grant);
	};
}
bool FDWRuleHost::SetLibrary(FDWRuleLibrary InLibrary, FString &Error)
{
	if (!InLibrary.Validate(Error))
		return false;
	Library = MoveTemp(InLibrary);
	World.SetStatusDefs(Library.Statuses);
	// Plans already referenced by roots remain valid. New compiles never reuse old definitions.
	PlanByHash.Reset();
	GrantedPlans.Reset();
	return true;
}
int32 FDWRuleHost::CompileSource(const FDWBuildSource &Source, FString &Error)
{
	const auto Plan = FDWRuleCompiler::Compile(Source, Error);
	if (!Plan)
		return INDEX_NONE;
	if (const int32 *Existing = PlanByHash.Find(Plan->Hash))
		return *Existing;
	const int32 Index = World.AddPlan(Plan.ToSharedRef());
	PlanByHash.Add(Plan->Hash, Index);
	PlanSources.Add(Index, Source);
	return Index;
}
int32 FDWRuleHost::CompileTraits(FName Active, const TArray<FName> &Ids, FString &Error)
{
	FDWBuildSource Source;
	return Library.TraitsSource(Active, Ids, Source, Error) ? CompileSource(Source, Error) : INDEX_NONE;
}
int32 FDWRuleHost::CompileNamedBuild(FName Build, const TArray<FName> &Extras, FString &Error)
{
	FDWBuildSource Source;
	return Library.NamedBuildSource(Build, Extras, Source, Error) ? CompileSource(Source, Error) : INDEX_NONE;
}
int32 FDWRuleHost::PlanWithSources(int32 Plan, const TArray<FDWTraitSource> &Sources, FString &Error)
{
	if (Sources.IsEmpty())
		return Plan;
	const FString Key = FString::FromInt(Plan) + TEXT("|") + SourcesKey(Sources);
	if (const int32 *Existing = GrantedPlans.Find(Key))
		return *Existing;
	const auto *Base = PlanSources.Find(Plan);
	if (!Base)
	{
		Error = TEXT("missing source for held-trait composition");
		return INDEX_NONE;
	}
	FDWBuildSource Source = *Base;
	for (const auto &HeldSource : Sources)
	{
		const int32 Index = Source.Traits.IndexOfByPredicate([&](const auto &T) { return T.Id == HeldSource.Id; });
		if (Index == INDEX_NONE)
			Source.Traits.Add(HeldSource);
		else
			Source.Traits[Index] = HeldSource;
	}
	const int32 Result = CompileSource(Source, Error);
	if (Result != INDEX_NONE)
		GrantedPlans.Add(Key, Result);
	return Result;
}
int32 FDWRuleHost::PlanWithGrantedTraits(int32 Plan, int32 Owner, FString &Error)
{
	TArray<FDWTraitSource> Sources;
	HeldSources(World.GetOwnerStatusId(Owner), Sources);
	return PlanWithSources(Plan, Sources, Error);
}
int32 FDWRuleHost::GrantTraits(uint32 Target, const TArray<FDWTraitEntry> &Entries, float Duration,
							   const FString &Label, FString &Error)
{
	if (!(Duration > 0.f) || !FMath::IsFinite(Duration) || Target == InvalidId)
		return INDEX_NONE;
	TArray<FDWTraitSource> Sources;
	for (const auto &Entry : Entries)
	{
		FDWTraitSource Source;
		if (!Library.SourceFromEntry(Entry, Source, Error))
			return INDEX_NONE;
		Sources.Add(MoveTemp(Source));
	}
	return GrantSources(Target, Sources, Duration, Label);
}
int32 FDWRuleHost::GrantSources(uint32 Target, const TArray<FDWTraitSource> &Sources, float Duration,
								const FString &Label)
{
	if (!(Duration > 0.f) || !FMath::IsFinite(Duration) || Sources.IsEmpty() || Target == InvalidId)
		return INDEX_NONE;
	FDWHeldGrant Grant;
	Grant.Id = NextGrant++;
	Grant.Target = Target;
	Grant.Sources = Sources;
	Grant.Expire = World.GetClock() + Duration;
	Grant.Label = Label;
	for (auto &Source : Grant.Sources)
		if (!Source.Stitched.IsEmpty())
			Source.Id = FName(*FString::Printf(TEXT("%s~g%d"), *Source.Id.ToString(), Grant.Id));
	const int32 Id = Grant.Id;
	Held.Add(MoveTemp(Grant));
	return Id;
}
void FDWRuleHost::GrantFromRule(uint32 Target, int32 Owner, float Duration, const FDWGrantDef &Def)
{
	TArray<FDWTraitSource> Sources = Def.Sources;
	FString Error;
	for (const auto &Entry : Def.Entries)
	{
		FDWTraitSource Source;
		if (!Library.SourceFromEntry(Entry, Source, Error))
			return; // Library validation guarantees this before binding.
		Sources.Add(MoveTemp(Source));
	}
	if (Sources.IsEmpty())
		return;
	const float Clock = World.GetClock(), Expire = Clock + Duration;
	for (auto &Grant : Held)
		if (Grant.Key == Def.Key && Grant.Target == Target && Grant.Expire > Clock)
		{
			Grant.Expire = FMath::Max(Grant.Expire, Expire);
			return;
		}
	FDWHeldGrant Grant;
	Grant.Id = NextGrant++;
	Grant.Target = Target;
	Grant.Sources = MoveTemp(Sources);
	Grant.Expire = Expire;
	Grant.Key = Def.Key;
	Grant.Label = Def.Key.Left(Def.Key.Find(TEXT("(")));
	if (const auto *Trait = Library.Traits.Find(FName(*Grant.Label)))
		Grant.Label = Trait->DisplayName;
	Held.Add(MoveTemp(Grant));
}
void FDWRuleHost::RevokeGrant(int32 Id)
{
	Held.RemoveAll([=](const auto &G) { return G.Id == Id; });
}
void FDWRuleHost::HeldSources(uint32 Target, TArray<FDWTraitSource> &Out) const
{
	Out.Reset();
	auto Add = [&](const auto &Source) {
		if (!Out.ContainsByPredicate([&](const auto &T) { return T.Id == Source.Id; }))
			Out.Add(Source);
	};
	TArray<FName> Ids;
	World.GetGrantedTraits(Target, Ids);
	for (FName Id : Ids)
		if (const auto *Def = Library.Traits.Find(Id))
			Add(Def->Source);
	for (const auto &Grant : Held)
		if (Grant.Target == Target && Grant.Expire > World.GetClock())
			for (const auto &Source : Grant.Sources)
				Add(Source);
}
bool FDWRuleHost::BindDefense(uint32 Target, const TArray<FDWTraitSource> &Sources, FString &Error)
{
	if (Sources.IsEmpty())
		return World.SetTargetDefensePlan(Target, INDEX_NONE);
	FDWBuildSource Source;
	Source.ActiveId = Sources[0].Id;
	Source.Traits = Sources;
	const int32 Plan = CompileSource(Source, Error);
	return Plan != INDEX_NONE && World.SetTargetDefensePlan(Target, Plan);
}
bool FDWRuleHost::SetTargetDefenseTraits(uint32 Target, const TArray<FName> &Ids, FString &Error)
{
	FDWBuildSource Source;
	if (!Ids.IsEmpty() && !Library.TraitsSource(NAME_None, Ids, Source, Error))
		return false;
	if (!BindDefense(Target, Source.Traits, Error))
		return false;
	if (Ids.IsEmpty())
		BaseDefense.Remove(Target);
	else
		BaseDefense.Add(Target, Source.Traits);
	AppliedDefense.Remove(Target);
	return true;
}
void FDWRuleHost::RefreshGrantedDefense()
{
	const float Clock = World.GetClock();
	Held.RemoveAll([=](const auto &Grant) { return Grant.Expire <= Clock; });
	TSet<uint32> Holders;
	World.GetIdsWithGrantedTraits(Holders);
	for (const auto &Grant : Held)
		Holders.Add(Grant.Target);
	FString Error;
	for (auto It = AppliedDefense.CreateIterator(); It; ++It)
		if (!Holders.Contains(It.Key()))
		{
			BindDefense(It.Key(), BaseDefense.FindRef(It.Key()), Error);
			It.RemoveCurrent();
		}
	for (uint32 Target : Holders)
	{
		TArray<FDWTraitSource> Sources = BaseDefense.FindRef(Target), Granted;
		HeldSources(Target, Granted);
		for (const auto &Source : Granted)
			if (!Sources.ContainsByPredicate([&](const auto &T) { return T.Id == Source.Id; }))
				Sources.Add(Source);
		const FString Key = SourcesKey(Sources);
		const FString *Applied = AppliedDefense.Find(Target);
		if (Applied && *Applied == Key)
			continue;
		if (BindDefense(Target, Sources, Error))
			AppliedDefense.Add(Target, Key);
	}
}
bool FDWRuleHost::SetTargetDefenseSources(uint32 Target, const TArray<FDWTraitSource>& Sources, FString& Error)
{
	if (!BindDefense(Target, Sources, Error)) return false;
	if (Sources.IsEmpty()) BaseDefense.Remove(Target); else BaseDefense.Add(Target, Sources);
	AppliedDefense.Remove(Target);
	return true;
}
void FDWRuleHost::ForgetTarget(uint32 Target)
{
	Held.RemoveAll([=](const auto &G) { return G.Target == Target; });
	World.ClearTargetStatuses(Target);
	World.SetTargetDefensePlan(Target, INDEX_NONE);
	BaseDefense.Remove(Target);
	AppliedDefense.Remove(Target);
}
void FDWRuleHost::Step(float DeltaSeconds)
{
	RefreshGrantedDefense();
	World.Step(DeltaSeconds);
}
} // namespace DWRules
