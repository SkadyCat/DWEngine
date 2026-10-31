#include "DWRuleSheet.h"

namespace DWRules
{
namespace
{
struct FCatalog
{
	TMap<FName, ESheetCombine> Stats;
	TMap<FName, FDWSheetTriggerSpec> Triggers;
	FCatalog()
	{
#include "Generated/DWRuleSheetCatalog.inl"
        for(const TCHAR* Stat:{TEXT("retaliation.hit_percent"),TEXT("retaliation.radius"),TEXT("retaliation.speed")})Stats.Add(Stat,ESheetCombine::Max);
        // Additional RuleStudio contracts; formulas and tier data stay in JSON.
        for(const TCHAR* Stat:{TEXT("proj.ground_contact"),TEXT("proj.ground_tint"),TEXT("trigger.store_event")})Stats.Add(Stat,ESheetCombine::Max);
        Triggers.Add(TEXT("area_roam"),FDWSheetTriggerSpec{{TEXT("speed"),TEXT("range"),TEXT("interval"),TEXT("duration")},EStitchRole::None});
        Triggers.Add(TEXT("proj_roam"),FDWSheetTriggerSpec{{TEXT("range"),TEXT("interval")},EStitchRole::None});
        for(const TCHAR* Name:{TEXT("hit_burning_ground"),TEXT("hit_chilled_ground"),TEXT("hit_poison_ground"),TEXT("hit_shocked_ground")})Triggers.Add(Name,FDWSheetTriggerSpec{{TEXT("chance"),TEXT("radius"),TEXT("duration"),TEXT("cooldown"),TEXT("amount")},EStitchRole::None});
        Triggers.Add(TEXT("hit_mirror_ground"),FDWSheetTriggerSpec{{TEXT("chance"),TEXT("radius"),TEXT("duration"),TEXT("cooldown")},EStitchRole::None});
        Triggers.Add(TEXT("projectile_capture"),FDWSheetTriggerSpec{{TEXT("chance"),TEXT("bend")},EStitchRole::None});
        for(const TCHAR* Name:{TEXT("aura_skill"),TEXT("trigger_skill")})Triggers.Add(Name,FDWSheetTriggerSpec{{},EStitchRole::None});
	}
};
const FCatalog &Catalog()
{
	static const FCatalog Instance;
	return Instance;
}
bool HasStitchedGrant(const TArray<FDWActionSource> &Actions)
{
	for (const FDWActionSource &A : Actions)
		if ((A.Op == EOp::GrantTraits && A.bGrantStitched) || HasStitchedGrant(A.Then))
			return true;
	return false;
}
} // namespace
const TMap<FName, ESheetCombine> &SheetStatCatalog()
{
	return Catalog().Stats;
}
const TMap<FName, FDWSheetTriggerSpec> &SheetTriggerCatalog()
{
	return Catalog().Triggers;
}
bool ValidateSheet(const FDWTraitSource &Trait, FString &Error)
{
	for (int32 I = 0; I < Trait.Sheet.Num(); ++I)
	{
		const FDWSheetEntry &Entry = Trait.Sheet[I];
		if (!Entry.GrantSkill.IsNone()) continue;
        if (!Entry.Trigger.IsNone())
		{
			const auto *Spec = SheetTriggerCatalog().Find(Entry.Trigger);
			if (!Spec)
			{
				Error = FString::Printf(TEXT("%s.sheet[%d]: unknown trigger %s"), *Trait.Id.ToString(), I,
										*Entry.Trigger.ToString());
				return false;
			}
			for (FName Param : Spec->Params)
				if (!Trait.Params.Contains(Param))
				{
					Error = FString::Printf(TEXT("%s.sheet[%d]: trigger %s needs param %s"), *Trait.Id.ToString(), I,
											*Entry.Trigger.ToString(), *Param.ToString());
					return false;
				}
		}
		else
		{
			if (!SheetStatCatalog().Contains(Entry.Stat))
			{
				Error = FString::Printf(TEXT("%s.sheet[%d]: unknown sheet stat %s"), *Trait.Id.ToString(), I,
										*Entry.Stat.ToString());
				return false;
			}
			double Value;
			if (!Entry.Value.Evaluate(Trait.Params, Value))
			{
				Error = FString::Printf(TEXT("%s.sheet[%d]: value must be a number or an expression over params"),
										*Trait.Id.ToString(), I);
				return false;
			}
		}
	}
	return true;
}
EStitchRole SheetStitchRole(const FDWTraitSource &Trait)
{
	for (const auto &Entry : Trait.Sheet)
		if (const auto *Spec = SheetTriggerCatalog().Find(Entry.Trigger); Spec && Spec->Stitched != EStitchRole::None)
			return Spec->Stitched;
	return EStitchRole::None;
}
bool IsStitchable(const FDWTraitSource &Trait)
{
	if (SheetStitchRole(Trait) != EStitchRole::None)
		return true;
	for (const auto &Rule : Trait.Rules)
		if (HasStitchedGrant(Rule.Actions))
			return true;
	return false;
}
double FDWSheet::Stat(FName Path) const
{
	if (const double *V = Stats.Find(Path))
		return *V;
	const auto *Combine = SheetStatCatalog().Find(Path);
	return Combine && *Combine == ESheetCombine::More ? 1.0 : 0.0;
}
bool FDWSheet::Compute(const TArray<FDWHeldTrait> &Entries, FString &Error)
{
	FDWSheet Next;
	for (const auto &Held : Entries)
	{
		if (!ValidateSheet(Held.Source, Error))
			return false;
		for (const auto &Entry : Held.Source.Sheet)
		{
			if (!Entry.GrantSkill.IsNone()) continue;
        if (!Entry.Trigger.IsNone())
			{
				FDWSheetTrigger Trigger;
				Trigger.Name = Entry.Trigger;
				Trigger.Trait = Held.Source.Id;
				Trigger.Label = Held.Label.IsEmpty() ? Held.Source.Id.ToString() : Held.Label;
				Trigger.Stitched = Held.Source.Stitched;
				for (FName Param : SheetTriggerCatalog()[Entry.Trigger].Params)
					Trigger.Params.Add(Param, Held.Source.Params[Param]);
				Next.Triggers.Add(MoveTemp(Trigger));
				continue;
			}
			double Value;
			if (!Entry.Value.Evaluate(Held.Source.Params, Value))
				return false;
			const ESheetCombine Combine = SheetStatCatalog()[Entry.Stat];
			FName Key = Entry.Stat;
			if (Held.bEquipment &&
				(Key == TEXT("life.base") || Key == TEXT("shield.base") || Key == TEXT("defense.evasion")))
				Key = FName(*(
					Key.ToString() + TEXT("@equipment") +
					(Key == TEXT("shield.base") && !Held.Slot.IsNone() ? TEXT(":") + Held.Slot.ToString() : TEXT(""))));
			const double *Previous = Next.Stats.Find(Key);
			if (Combine == ESheetCombine::More)
				Next.Stats.Add(Key, (Previous ? *Previous : 1.0) * (1.0 + Value / 100.0));
			else if (Combine == ESheetCombine::Max)
				Next.Stats.Add(Key, FMath::Max(Previous ? *Previous : 0.0, Value));
			else
				Next.Stats.Add(Key, (Previous ? *Previous : 0.0) + Value);
		}
	}
	*this = MoveTemp(Next);
	return true;
}
} // namespace DWRules
