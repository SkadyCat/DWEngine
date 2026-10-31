#include "DWRulePawnEquipment.h"
#include "DWRuleLibrary.h"
#include <limits>

namespace DWRules
{
bool FDWPawnEquipmentRules::IsFixedSlot(FName S)
{
	return S == TEXT("lineage") || S == TEXT("rarity") || S == TEXT("tower") || S == TEXT("character") ||
		   S == TEXT("default") || S == TEXT("map");
}
bool FDWPawnEquipmentRules::IsSlot(FName S, EPawnKind K)
{
	if (K == EPawnKind::Monster)
		return S == TEXT("lineage") || S == TEXT("rarity") || S == TEXT("tower");
	if (K == EPawnKind::Summon)
		return S == TEXT("default");
	return S == TEXT("tower") || S == TEXT("character") || S == TEXT("weapon") || S == TEXT("offhand") ||
		   S == TEXT("helmet") || S == TEXT("chest") || S == TEXT("gloves") || S == TEXT("boots") ||
		   S == TEXT("belt") || S == TEXT("amulet") || S == TEXT("ring");
}
FDWPawnEquipment FDWPawnEquipmentRules::Defaults(EPawnKind K)
{
	FDWPawnEquipment E;
	if (K == EPawnKind::Player)
	{
		E.Ids.Add(TEXT("tower"), TEXT("player_tower_equipment"));
		E.Ids.Add(TEXT("character"), TEXT("player_character_equipment"));
	}
	else if (K == EPawnKind::Monster)
	{
		E.Ids.Add(TEXT("lineage"), NAME_None);
		E.Ids.Add(TEXT("rarity"), NAME_None);
		E.Ids.Add(TEXT("tower"), TEXT("monster_tower_equipment"));
	}
	else
		E.Ids.Add(TEXT("default"), NAME_None);
	return E;
}
bool FDWPawnEquipmentRules::Resolve(const FDWRuleLibrary &L, const FDWPawnEquipment &E, EPawnKind K,
									TArray<FDWCollectionPart> &Out, FString &Error)
{
	TSet<FName> Slots;
	for (const auto &P : E.Ids)
		Slots.Add(P.Key);
	for (const auto &P : E.Overrides)
		Slots.Add(P.Key);
	TArray<FName> Ordered = Slots.Array();
	Ordered.Sort(FNameLexicalLess());
	for (FName Slot : Ordered)
	{
		if (!IsSlot(Slot, K))
		{
			Error = TEXT("invalid pawn equipment slot: ") + Slot.ToString();
			return false;
		}
		const FDWCollection *D = E.Overrides.Find(Slot);
		const FName Id = E.Ids.FindRef(Slot);
		if (!D && !Id.IsNone())
			D = L.Equipment.Find(Id);
		if (!D && Id.IsNone())
			continue;
		if (!D || D->Slot != Slot)
		{
			Error = TEXT("invalid equipment for slot: ") + Slot.ToString();
			return false;
		}
		Out.Add({*D, true, D->Name});
	}
	return true;
}
bool FDWPawnEquipmentRules::Aggregate(const FDWRuleLibrary &L, const FDWPawnEquipment &E, EPawnKind K,
									  TArray<FDWTraitSource> &Out, FString &Error)
{
	TArray<FDWCollectionPart> Parts;
	if (!Resolve(L, E, K, Parts, Error))
		return false;
	if (!Parts.ContainsByPredicate([](const auto &P) { return P.Collection.Traits.Num() > 0; }))
		return true;
	FDWBuildSource Source;
	TArray<FDWContribution> Rows;
	if (!L.Aggregate(NAME_None, Parts, std::numeric_limits<double>::infinity(), Source, Rows, Error))
		return false;
	Out.Append(Source.Traits);
	return true;
}
bool FDWPawnEquipmentRules::SetTower(const FDWRuleLibrary &L, FDWPawnEquipment &E, int32 Floor,
									 FString &Error)
{
	const FName Id = E.Ids.FindRef(TEXT("tower"));
	if (Id.IsNone())
	{
		E.Overrides.Remove(TEXT("tower"));
		return true;
	}
	const auto *D = L.Equipment.Find(Id);
	if (!D || D->Slot != TEXT("tower") || Floor < 1)
	{
		Error = TEXT("invalid tower equipment or floor");
		return false;
	}
	auto Copy = *D;
	for (auto &T : Copy.Traits)
		if (T.Id == TEXT("monster_tower_level"))
			T.Params.Add(TEXT("level"), Floor);
	E.Overrides.Add(TEXT("tower"), MoveTemp(Copy));
	return true;
}
FName FDWPawnEquipmentRules::Rarity(const FDWPawnEquipment &E)
{
	const FString Id = E.Ids.FindRef(TEXT("rarity")).ToString();
	return Id.StartsWith(TEXT("monster_rarity_")) ? FName(*Id.RightChop(15)) : NAME_None;
}
} // namespace DWRules
