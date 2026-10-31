#include "DWRuleLibrary.h"
#include "DWRuleCompiler.h"
#include "DWRulePawnEquipment.h"

namespace DWRules
{
namespace
{
bool NativeSheetTrait(const FDWLibraryTrait& T)
{
 const FString Path=T.Path.Replace(TEXT("\\"),TEXT("/"));
 return (Path.StartsWith(TEXT("Affixes/"),ESearchCase::IgnoreCase)||Path.Contains(TEXT("/Affixes/"),ESearchCase::IgnoreCase))&&T.Source.Sheet.Num()>0&&T.Source.Rules.IsEmpty();
}
bool CheckGrants(const TArray<FDWActionSource> &Actions, const FDWRuleLibrary &Library, FString &Error)
{
	for (const auto &A : Actions)
	{
		for (const auto &E : A.GrantEntries)
		{
			FDWTraitSource Source;
			if (!Library.SourceFromEntry(E, Source, Error))
				return false;
		}
		if (!CheckGrants(A.Then, Library, Error))
			return false;
	}
	return true;
}
} // namespace
bool FDWRuleLibrary::Validate(FString &Error) const
{
	if (Traits.IsEmpty())
	{
		Error = TEXT("no traits under Rules");
		return false;
	}
	for (const auto &P : Traits)
	{
		if (!ValidateSheet(P.Value.Source, Error))
            return false;
        for(const auto& E:P.Value.Source.Sheet)if(!E.GrantSkill.IsNone()&&!Builds.Contains(E.GrantSkill)){Error=TEXT("unknown granted skill ")+E.GrantSkill.ToString();return false;}
		for (const auto &R : P.Value.Source.Rules)
			if (!CheckGrants(R.Actions, *this, Error))
				return false;
	}
	for (const auto &P : Builds)
		for (FName Id : P.Value.Traits)
		{
			if (!Traits.Contains(Id))
			{
				Error = FString::Printf(TEXT("%s: unknown trait %s"), *P.Key.ToString(), *Id.ToString());
				return false;
			}
			if (Traits[Id].bHosted&&!NativeSheetTrait(Traits[Id])) { Error=TEXT("base skill must not directly reference hosted action traits: ")+Id.ToString(); return false; }
		}
	for (const auto &Status : Statuses)
		for (FName Id : Status.Traits)
		{
			if (!Traits.Contains(Id))
			{
				Error = FString::Printf(TEXT("%s: unknown trait %s"), *Status.Id.ToString(), *Id.ToString());
				return false;
			}
			if (Traits[Id].bHosted&&!NativeSheetTrait(Traits[Id])) { Error=TEXT("status must not directly reference hosted action traits: ")+Id.ToString(); return false; }
		}
	for (const auto& Pair : Builds)
	{
		FDWBuildSource Source;
		if (!NamedBuildSource(Pair.Key,{},Source,Error) || !FDWRuleCompiler::Compile(Source,Error)) return false;
	}
	for (const auto &Profile : ValueProfiles)
	{
		const auto *Trait = Traits.Find(Profile.Key);
		if (!Trait || Trait->Source.Params.Num() != Profile.Value.Params.Num())
		{
			Error = TEXT("invalid value profile ") + Profile.Key.ToString();
			return false;
		}
		for (const auto &P : Profile.Value.Params)
		{
			if (!Trait->Source.Params.Contains(P.Key) || !FMath::IsFinite(P.Value.Step) || P.Value.Step <= 0 ||
				P.Value.Bands.Num() != TraitTierCount)
			{
				Error = TEXT("invalid value parameter ") + P.Key.ToString();
				return false;
			}
			for (const auto &Band : P.Value.Bands)
				if (!FMath::IsFinite(Band.X) || !FMath::IsFinite(Band.Y) || Band.X > Band.Y)
				{
					Error = TEXT("invalid tier band");
					return false;
				}
		}
	}
	for (const auto &Pair : Monsters)
	{
		TArray<FDWTraitSource> Sources;
		TSet<FName> Ids;
		for (const auto &Entry : Pair.Value.Traits)
		{
			FDWTraitSource Source;
			if (Ids.Contains(Entry.Id) || Entry.With.Num() || Entry.Duration > 0 ||
				!SourceFromEntry(Entry, Source, Error))
			{
				if (Error.IsEmpty())
					Error = TEXT("invalid monster trait");
				return false;
			}
			Ids.Add(Entry.Id);
			Sources.Add(Source);
		}
		if (!FDWPawnEquipmentRules::Aggregate(*this, Pair.Value.Equipment, EPawnKind::Monster, Sources,
											  Error))
			return false;
		for (const auto &Skill : Pair.Value.Skills)
		{
			FDWBuildSource Source;
			if (!NamedBuildSource(Skill.Id, {}, Source, Error))
				return false;
			for (const auto &Extra : Sources)
			{
				auto *Current = Source.Traits.FindByPredicate([&](const auto &T) { return T.Id == Extra.Id; });
				if (Current)
					*Current = Extra;
				else
					Source.Traits.Add(Extra);
			}
			const auto Plan = FDWRuleCompiler::Compile(Source, Error);
			if (!Plan || !Plan->HasRules(uint16(EBuiltinEvent::Cast)))
			{
				if (Error.IsEmpty())
					Error = TEXT("monster skill has no cast rule");
				return false;
			}
		}
	}
	for (const auto *Collections : {&Equipment, &Items})
		for (const auto &P : *Collections)
		{
			for (FName Skill : P.Value.Skills)
				if (!Builds.Contains(Skill))
				{
					Error = TEXT("unknown skill ") + Skill.ToString();
					return false;
				}
			if (P.Value.Legendary.IsSet())
			{
				FDWTraitSource S;
				if (!SourceFromEntry(P.Value.Legendary.GetValue(), S, Error))
					return false;
			}
			for (const auto& Base : P.Value.Bases)
			{
				FDWTraitSource S;
				if (!SourceFromEntry(Base, S, Error)) return false;
				if (S.Rules.Num() || S.Sheet.ContainsByPredicate([](const auto& E) { return !E.Trigger.IsNone(); }) || (P.Value.Rarity != TEXT("legendary") && !S.Params.Num()))
				{ Error = TEXT("base must be a passive property; ordinary templates require numeric parameters"); return false; }
			}
			for (const auto &E : P.Value.Traits)
			{
				FDWTraitSource Source;
				if (!SourceFromEntry(E, Source, Error))
					return false;
			}
		}
	if (TagDefinitions.Families.Num())
		for (const auto& Pair : Items)
			if (Pair.Value.Category == TEXT("omen") && Tags.CollectionFamilies(Pair.Value.Tags).Num() != 1)
			{
				Error = TEXT("omen needs exactly one family: ") + Pair.Key.ToString();
				return false;
			}
	return true;
}
bool FDWRuleLibrary::SourceFromEntry(const FDWTraitEntry &Entry, FDWTraitSource &Out, FString &Error, FName Rename,
									 int32 Depth) const
{
	if (Depth > 3)
	{
		Error = TEXT("trait stitching exceeds 3");
		return false;
	}
	const auto *Def = Traits.Find(Entry.Id);
	if (!Def)
	{
		Error = TEXT("unknown trait ") + Entry.Id.ToString();
		return false;
	}
	if (!FDWTraitValues::Validate(*this, Entry, Error, false))
		return false;
	Out = Def->Source;
	if (!Rename.IsNone())
		Out.Id = Rename;
	for (const auto &Param : Entry.Params)
	{
		if (!Out.Params.Contains(Param.Key) || !FMath::IsFinite(Param.Value))
		{
			Error =
				FString::Printf(TEXT("%s: unknown or invalid param %s"), *Entry.Id.ToString(), *Param.Key.ToString());
			return false;
		}
		Out.Params[Param.Key] = Param.Value;
	}
	if (!Entry.With.IsEmpty() && !IsStitchable(Out))
	{
		Error = TEXT("trait cannot carry stitched payload: ") + Entry.Id.ToString();
		return false;
	}
	Out.Stitched.Reset();
	for (const auto &Child : Entry.With)
	{
		FDWTraitSource Source;
		if (!SourceFromEntry(Child, Source, Error, NAME_None, Depth + 1))
			return false;
		Out.Stitched.Add(MoveTemp(Source));
	}
	return ValidateSheet(Out, Error);
}
bool FDWRuleLibrary::TraitsSource(FName Active, const TArray<FName> &Ids, FDWBuildSource &Out, FString &Error) const
{
	Out = FDWBuildSource();
	Out.ActiveId = Active.IsNone() && !Ids.IsEmpty() ? Ids[0] : Active;
	TSet<FName> Seen;
	for (FName Id : Ids)
	{
		if (Seen.Contains(Id))
			continue;
		const auto *Def = Traits.Find(Id);
		if (!Def)
		{
			Error = TEXT("unknown trait ") + Id.ToString();
			return false;
		}
		Seen.Add(Id);
		Out.Traits.Add(Def->Source);
	}
	return true;
}
bool FDWRuleLibrary::NamedBuildSource(FName Build, const TArray<FName> &Extras, FDWBuildSource &Out,
									  FString &Error) const
{
	const auto *Def = Builds.Find(Build);
	if (!Def)
	{
		Error = TEXT("unknown build ") + Build.ToString();
		return false;
	}
	TArray<FName> Ids = Def->Traits;
	Ids.Append(Extras);
	return TraitsSource(Def->ActiveId, Ids, Out, Error);
}
bool FDWRuleLibrary::Aggregate(FName Build, const TArray<FDWCollectionPart> &Parts, double At, FDWBuildSource &Out,
							   TArray<FDWContribution> &Contributions, FString &Error) const
{
	Out = FDWBuildSource();
	Contributions.Reset();
	const auto *Def = Builds.Find(Build);
	if (!Build.IsNone() && !Def)
	{
		Error = TEXT("unknown build ") + Build.ToString();
		return false;
	}
	TSet<FName> Used;
	auto Add = [&](const FDWTraitEntry &Entry, bool EquipmentSource, FName Slot, const FString &Label) -> bool {
		FDWTraitSource Source;
		if (!SourceFromEntry(Entry, Source, Error))
			return false;
		FDWContribution Row;
		Row.OriginalId = Entry.Id;
		Row.Held.Source = Source;
		Row.Held.bEquipment = EquipmentSource;
		Row.Held.Slot = Slot;
		Row.Held.Label = Label;
		Row.Duration = Entry.Duration;
		Row.Tier = Entry.Tier;
		if (Entry.Duration > 0 && !(At < Entry.Duration))
		{
			Row.bExpired = true;
			Contributions.Add(MoveTemp(Row));
			return true;
		}
		if (Used.Contains(Source.Id))
		{
			if (!Source.Rules.IsEmpty() && Entry.With.IsEmpty())
			{
				const int32 Index = Out.Traits.IndexOfByPredicate([&](const auto &T) { return T.Id == Source.Id; });
				if (Entry.Params.IsEmpty())
				{
					Row.bMerged = true;
					Contributions.Add(MoveTemp(Row));
					return true;
				}
				Source.Params = Out.Traits[Index].Params;
				for (const auto &P : Entry.Params)
					Source.Params.Add(P.Key, P.Value);
				Out.Traits[Index] = Source;
				for (auto &Previous : Contributions)
					if (!Previous.bMerged && Previous.Held.Source.Id == Source.Id)
					{
						Previous.bMerged = true;
						break;
					}
				Row.Held.Source = Source;
				Contributions.Add(MoveTemp(Row));
				return true;
			}
			int32 N = 2;
			while (Used.Contains(FName(*FString::Printf(TEXT("%s~%d"), *Entry.Id.ToString(), N))))
				++N;
			Source.Id = FName(*FString::Printf(TEXT("%s~%d"), *Entry.Id.ToString(), N));
			Row.Held.Source = Source;
		}
		Used.Add(Source.Id);
		Out.Traits.Add(Source);
		Contributions.Add(MoveTemp(Row));
		return true;
	};
	if (Def)
	{
		Out.ActiveId = Def->ActiveId;
		for (FName Id : Def->Traits)
		{
			FDWTraitEntry Entry;
			Entry.Id = Id;
			if (!Add(Entry, false, NAME_None, Def->DisplayName))
				return false;
		}
	}
	for (const auto &Part : Parts)
	{
		auto Entries = Part.Collection.Bases;
		Entries.Append(Part.Collection.Traits);
		for (const auto &Entry : Entries)
			if (!Add(Entry, Part.bEquipment, Part.Collection.Slot,
					 Part.Label.IsEmpty() ? Part.Collection.Name : Part.Label))
				return false;
	}
	if (Out.ActiveId.IsNone())
		for (const auto &Source : Out.Traits)
			if (Source.Rules.ContainsByPredicate([](const auto &R) { return R.Event == TEXT("cast"); }))
			{
				Out.ActiveId = Source.Id;
				break;
			}
	if (Out.ActiveId.IsNone() && Build.IsNone() && Out.Traits.Num())
		Out.ActiveId = Out.Traits[0].Id;
	if (Out.ActiveId.IsNone())
	{
		Error = TEXT("no active trait in build");
		return false;
	}
	return true;
}
} // namespace DWRules
