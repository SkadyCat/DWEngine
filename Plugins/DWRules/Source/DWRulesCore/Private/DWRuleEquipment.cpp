#include "DWRuleEquipment.h"
#include "DWRulePawnEquipment.h"

namespace DWRules
{
namespace
{
void EquipmentIds(const FDWTraitEntry &E, TSet<FName> &Ids)
{
	Ids.Add(E.Id);
	for (const auto &Child : E.With)
		EquipmentIds(Child, Ids);
}
FDWTraitEntry EquipmentEntry(const FDWLibraryTrait &T)
{
	FDWTraitEntry E;
	E.Id = T.Source.Id;
	E.Params = T.Source.Params;
	return E;
}
int32 EquipmentInteger(TFunctionRef<double()> Random, int32 Min, int32 Max)
{
	return Min + FMath::FloorToInt(FDWTraitValues::Unit(Random()) * (Max - Min + 1));
}
} // namespace
TArray<FDWEquipmentAffix> FDWEquipmentRules::Pool(const FDWRuleLibrary &Library)
{
	TArray<FDWEquipmentAffix> Out;
	static const TSet<FName> Excluded{TEXT("hit_retaliation_nova"),TEXT("shock"),   TEXT("ignite"),	TEXT("bleed"),	   TEXT("freeze"),
									  TEXT("brittle"), TEXT("shatter"), TEXT("flask_life"),
									  TEXT("minion_seek_range"), TEXT("player_stun_buildup"), TEXT("player_stun_threshold"), TEXT("player_stun_recovery"),
                                      TEXT("self_targeted_cast"),TEXT("independent_skill_cooldown"),TEXT("hostile_projectile_attraction"),TEXT("damage_immunity")};
	for (const auto &Pair : Library.Traits)
	{
		const auto &T = Pair.Value;
		if (T.Source.Id.ToString().StartsWith(TEXT("monster_")) || T.Source.Id.ToString().StartsWith(TEXT("map_")) || Excluded.Contains(T.Source.Id) ||
			IsStitchable(T.Source) ||
			T.Source.Rules.ContainsByPredicate([](const auto &R) { return R.Event == TEXT("cast"); }))
			continue;
		Out.Add(
			{T.Source.Id, T.DisplayName.IsEmpty() ? T.Source.Id.ToString() : T.DisplayName, {EquipmentEntry(T)}, 0});
	}
	struct FCombination
	{
		const TCHAR *Id;
		const TCHAR *Name;
		const TCHAR *First;
		const TCHAR *Second;
	};
	for (const auto &C :
		 {FCombination{TEXT("shocking_hits"), TEXT("闪电感电"), TEXT("lightning_damage"), TEXT("shock")},
		  FCombination{TEXT("igniting_hits"), TEXT("火焰点燃"), TEXT("fire_damage"), TEXT("ignite")},
		  FCombination{TEXT("bleeding_hits"), TEXT("物理流血"), TEXT("physical_damage"), TEXT("bleed")},
		  FCombination{TEXT("freezing_hits"), TEXT("冰缓冻结"), TEXT("chill"), TEXT("freeze")},
		  FCombination{TEXT("brittle_hits"), TEXT("冰缓易碎"), TEXT("chill"), TEXT("brittle")},
		  FCombination{TEXT("shattering_kills"), TEXT("冰缓碎裂"), TEXT("chill"), TEXT("shatter")}})
	{
		const auto *A = Library.Traits.Find(C.First);
		const auto *B = Library.Traits.Find(C.Second);
		if (A && B)
			Out.Add({C.Id, C.Name, {EquipmentEntry(*A), EquipmentEntry(*B)}, 0});
	}
	return Out;
}
bool FDWEquipmentRules::Conflict(const FDWEquipmentAffix &A, const FDWEquipmentAffix &B)
{
	TSet<FName> Ids, Other;
	for (const auto &E : A.Traits)
		EquipmentIds(E, Ids);
	for (const auto &E : B.Traits)
		EquipmentIds(E, Other);
	for (FName Id : Other)
		if (Ids.Contains(Id))
			return true;
	return false;
}
bool FDWEquipmentRules::RollAffix(const FDWRuleLibrary &Library, const FDWEquipmentAffix &Definition,
								  TFunctionRef<double()> Random, FDWEquipmentAffix &Out, FString &Error, int32 MaxTier)
{
	if (MaxTier < 1 || MaxTier > TraitTierCount)
	{
		Error = TEXT("equipment tier must be 1..10");
		return false;
	}
	FDWEquipmentAffix Next = Definition;
	TFunction<bool(const FDWTraitEntry &)> Numeric = [&](const auto &E) {
		FDWTraitValueProfile Profile;
		if (FDWTraitValues::Profile(Library, E.Id, Profile) && Profile.Params.Num())
			return true;
		for (const auto &Child : E.With)
			if (Numeric(Child))
				return true;
		return false;
	};
	int32 Tier = 0;
	for (const auto &E : Next.Traits)
		if (Numeric(E))
		{
			Tier = EquipmentInteger(Random, 1, MaxTier);
			break;
		}
	Next.Tier = Tier;
	TFunction<bool(FDWTraitEntry &)> Vary = [&](auto &E) {
		FDWTraitValueProfile Profile;
		if (FDWTraitValues::Profile(Library, E.Id, Profile) && Profile.Params.Num())
		{
			E.Tier = Tier;
			if (!FDWTraitValues::Roll(Profile, Tier, Random, E.Params, Error))
				return false;
		}
		for (auto &Child : E.With)
			if (!Vary(Child))
				return false;
		return true;
	};
	for (auto &E : Next.Traits)
		if (!Vary(E))
			return false;
	Out = MoveTemp(Next);
	return true;
}
static bool RollInherentAffix(const FDWRuleLibrary& Library, const FDWEquipmentAffix& Definition,
	TFunctionRef<double()> Random, FDWEquipmentAffix& Out, FString& Error, int32 MaxTier)
{
	FDWEquipmentAffix Varied;
	if (!FDWEquipmentRules::RollAffix(Library, Definition, Random, Varied, Error, MaxTier)) return false;
	TFunction<bool(const FDWTraitEntry&)> Ranked = [&](const auto& Entry) {
		FDWTraitValueProfile Profile;
		if (FDWTraitValues::Profile(Library, Entry.Id, Profile) && Profile.Params.Num() && Entry.Tier == 0) return false;
		for (const auto& Child : Entry.With) if (!Ranked(Child)) return false;
		return true;
	};
	for (const auto& Entry : Definition.Traits)
		if (!Ranked(Entry)) { Out = MoveTemp(Varied); return true; }
	FDWEquipmentAffix Next = Definition;
	Next.Tier = 0;
	TFunction<bool(FDWTraitEntry&)> Materialize = [&](auto& Entry) {
		if (Entry.Tier > MaxTier)
		{
			FDWTraitValueProfile Profile;
			Entry.Tier = MaxTier;
			if (!FDWTraitValues::Profile(Library, Entry.Id, Profile) || !FDWTraitValues::Roll(Profile, MaxTier, Random, Entry.Params, Error)) return false;
		}
		Next.Tier = FMath::Max(Next.Tier, Entry.Tier);
		for (auto& Child : Entry.With) if (!Materialize(Child)) return false;
		return true;
	};
	for (auto& Entry : Next.Traits) if (!Materialize(Entry)) return false;
	Out = MoveTemp(Next);
	return true;
}

bool FDWEquipmentRules::Sample(const FDWRuleLibrary &Library, const TArray<FDWEquipmentAffix> &Pool, int32 Count,
							   TFunctionRef<double()> Random, const TArray<FDWEquipmentAffix> &Excluded,
							   TArray<FDWEquipmentAffix> &Out, FString &Error, int32 MaxTier)
{
	TArray<FDWEquipmentAffix> Chosen;
	for (int32 I = 0; I < Count; ++I)
	{
		TArray<const FDWEquipmentAffix *> Available;
		for (const auto &Def : Pool)
		{
			const auto Test = [&](const auto &Other) { return Other.Id == Def.Id || Conflict(Def, Other); };
			if (!Excluded.ContainsByPredicate(Test) && !Chosen.ContainsByPredicate(Test))
				Available.Add(&Def);
		}
		if (!Available.Num())
		{
			Error = TEXT("可用词条不足，无法生成指定数量且不重复的词条");
			return false;
		}
		FDWEquipmentAffix Rolled;
		if (!RollAffix(Library, *Available[EquipmentInteger(Random, 0, Available.Num() - 1)], Random, Rolled, Error,
					   MaxTier))
			return false;
		Chosen.Add(MoveTemp(Rolled));
	}
	Out = MoveTemp(Chosen);
	return true;
}
TArray<FDWEquipmentAffix> FDWEquipmentRules::BasePool(const FDWRuleLibrary& Library, bool Defensive)
{
	auto Out = Pool(Library);
	const auto DefenseStat = [](const FString& Path) {
		bool Prefix = false;
		for (const TCHAR* P : {TEXT("defense."), TEXT("armour."), TEXT("evasion."), TEXT("shield."), TEXT("life."),
			TEXT("resistance."), TEXT("block."), TEXT("evade."), TEXT("recoup."), TEXT("leech."), TEXT("heal.")})
			Prefix |= Path.StartsWith(P);
		for (const TCHAR* Word : {TEXT("damage"), TEXT("critical"), TEXT(".crit"), TEXT("counter"), TEXT("reflect"), TEXT("attack_speed"), TEXT("cast_speed"), TEXT("added"), TEXT("overflow")})
			if (Path.Contains(Word)) return false;
		return Prefix;
	};
	Out.RemoveAll([&](const auto& A) {
		if (A.Traits.Num() != 1) return true;
		const auto& Source = Library.Traits.FindChecked(A.Traits[0].Id).Source;
		if (Source.Rules.Num() || !Source.Params.Num() || Source.Sheet.ContainsByPredicate([](const auto& E) { return !E.Trigger.IsNone(); })) return true;
		if (!Defensive) return false;
		if (!Source.Sheet.Num() && !Source.Modifiers.Num()) return true;
		for (const auto& E : Source.Sheet) if (E.Stat.IsNone() || !DefenseStat(E.Stat.ToString())) return true;
		for (const auto& M : Source.Modifiers)
			if (!(M.Stat == TEXT("owner.max_life") || M.Stat == TEXT("owner.max_mana") ||
				(M.Stat.StartsWith(TEXT("element.")) && (M.Stat.EndsWith(TEXT(".resistance")) || M.Stat.EndsWith(TEXT(".max_resistance")))))) return true;
		return false;
	});
	Out.Sort([](const auto& A, const auto& B) { return A.Id.LexicalLess(B.Id); });
	return Out;
}
bool FDWEquipmentRules::RollBases(const FDWRuleLibrary& Library, const FDWCollection& Template, int32 Tier,
	TFunctionRef<double()> Random, TArray<FDWEquipmentAffix>& Out, FString& Error)
{
	Out.Reset();
	if (!Template.Bases.Num()) return true;
	if (Template.Rarity == TEXT("legendary"))
	{
		for (const auto& Entry : Template.Bases)
		{
			const auto* Trait = Library.Traits.Find(Entry.Id);
			if (!Trait) { Error = TEXT("unknown fixed base trait"); return false; }
			FDWEquipmentAffix Definition{Entry.Id, Trait->DisplayName, {Entry}, 0}, Rolled;
			if (!RollInherentAffix(Library, Definition, Random, Rolled, Error, Tier)) return false;
			Out.Add(MoveTemp(Rolled));
		}
		return true;
	}
	if (Template.Bases.Num() != 1) { Error = TEXT("equipment requires one authored base"); return false; }
	const auto& Entry = Template.Bases[0];
	const auto* Trait = Library.Traits.Find(Entry.Id);
	if (!Trait) { Error = TEXT("unknown equipment base"); return false; }
	FDWEquipmentAffix Fixed{Entry.Id, Trait->DisplayName, {Entry}, 0}, Rolled;
	if (!RollAffix(Library, Fixed, Random, Rolled, Error, Tier)) return false;
	TArray<FDWEquipmentAffix> Second;
	if (!Sample(Library, BasePool(Library, Template.Slot != TEXT("weapon")), 1, Random, {Rolled}, Second, Error, Tier)) return false;
	Out.Add(MoveTemp(Rolled)); Out.Append(MoveTemp(Second));
	return true;
}
bool FDWEquipmentRules::EnsureBases(const FDWRuleLibrary& Library, FDWEquipmentInstance& Item, FString& Error)
{
	if (!Item.bIdentified) return true;
	const auto* Template = Library.Equipment.Find(Item.BaseId);
	if (!Template || !Template->Bases.Num()) return true;
	if (Template->Rarity == TEXT("legendary") && !Item.bSynthesized)
	{
		FDWEquipmentRandom Random(uint32(HashName(TEXT("bases:") + Item.Uid)));
		bool Same = Item.Bases.Num() == Template->Bases.Num();
		for (int32 I = 0; Same && I < Item.Bases.Num(); ++I) Same = Item.Bases[I].Id == Template->Bases[I].Id;
		if (!Same && !RollBases(Library, *Template, Item.Tier, [&]() { return Random.Next(); }, Item.Bases, Error)) return false;
		if (Item.Rarity == TEXT("legendary") && Template->Legendary.IsSet() &&
			(!Item.Legendary.IsSet() || Item.Legendary.GetValue().Traits.Num() != 1 || Item.Legendary.GetValue().Traits[0].Id != Template->Legendary.GetValue().Id))
		{
			const auto& Entry = Template->Legendary.GetValue();
			const auto* Trait = Library.Traits.Find(Entry.Id);
			FDWEquipmentAffix Definition{FName(*(TEXT("legendary:") + Template->Id.ToString())), Trait ? Trait->DisplayName : Entry.Id.ToString(), {Entry}, 0}, Rolled;
			if (!RollInherentAffix(Library, Definition, [&]() { return Random.Next(); }, Rolled, Error, Item.Tier)) return false;
			if (Item.Legendary.IsSet()) if (const auto* Previous = Item.Legendary.GetValue().Traits.FindByPredicate([&](const auto& E) { return E.Id == Entry.Id; })) Rolled.Traits[0] = *Previous;
			Item.Legendary = MoveTemp(Rolled);
		}
		return true;
	}
	if (Item.Bases.Num()) return true;
	FDWEquipmentRandom Random(uint32(HashName(TEXT("bases:") + Item.Uid)));
	return RollBases(Library, *Template, Item.Tier, [&]() { return Random.Next(); }, Item.Bases, Error);
}
bool FDWEquipmentRules::Roll(const FDWRuleLibrary &Library, const FDWCollection &Template, FName Rarity,
							 TFunctionRef<double()> Random, const FString &Uid, FDWEquipmentInstance &Out,
							 FString &Error, int32 Tier)
{
	FDWEquipmentInstance Next;
	Next.Uid = Uid.IsEmpty() ? TEXT("eq_") + FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower) : Uid;
	Next.BaseId = Template.Id;
	Next.Name = Template.Name.IsEmpty() ? Template.Id.ToString() : Template.Name;
	Next.Slot = Template.Slot;
	Next.Level = Template.Level ? Template.Level : 1;
	Next.Tier = Tier ? Tier : Template.Tier;
	if (Next.Tier < 1 || Next.Tier > TraitTierCount)
	{
		Error = TEXT("equipment tier must be 1..10");
		return false;
	}
	Next.Rarity = Rarity;
	Next.Skill = Template.Skills.Num() ? Template.Skills[0] : NAME_None;
	int32 Count;
	if (Rarity == TEXT("common"))
		Count = 0;
	else if (Rarity == TEXT("magic"))
		Count = EquipmentInteger(Random, 1, 2);
	else if (Rarity == TEXT("rare"))
		Count = EquipmentInteger(Random, 3, 6);
	else if (Rarity == TEXT("legendary"))
		Count = 5;
	else
	{
		Error = TEXT("未知装备品质或缺少传奇词条数量配置");
		return false;
	}
	TArray<FDWEquipmentAffix> Excluded;
	if (Rarity == TEXT("legendary"))
	{
		const FDWTraitEntry *Fixed =
			Template.Legendary.IsSet()
				? &Template.Legendary.GetValue()
				: Template.Traits.FindByPredicate([](const auto &E) { return E.With.Num() > 0; });
		if (!Fixed)
			Fixed = Template.Traits.FindByPredicate([&](const auto &E) {
				const auto *T = Library.Traits.Find(E.Id);
				return T && T->Source.Rules.Num();
			});
		if (!Fixed && Template.Traits.Num())
			Fixed = &Template.Traits[0];
		if (!Fixed)
		{
			Error = Next.Name + TEXT(" 没有可用的固定传奇词条");
			return false;
		}
		const auto *T = Library.Traits.Find(Fixed->Id);
		FDWEquipmentAffix A{FName(*(TEXT("legendary:") + Template.Id.ToString())),
							T ? T->DisplayName : Fixed->Id.ToString(),
							{*Fixed},
							0};
		FDWEquipmentAffix Rolled;
		if (!(Template.Rarity == TEXT("legendary") ? RollInherentAffix : FDWEquipmentRules::RollAffix)(Library, A, Random, Rolled, Error, Next.Tier))
			return false;
		Next.Legendary = Rolled;
		Excluded.Add(Rolled);
	}
	if (!Sample(Library, Pool(Library), Count, Random, Excluded, Next.Affixes, Error, Next.Tier))
		return false;
	if (!RollBases(Library, Template, Next.Tier, Random, Next.Bases, Error)) return false;
	Out = MoveTemp(Next);
	return true;
}
bool FDWEquipmentRules::Reroll(const FDWRuleLibrary &Library, const FDWEquipmentInstance &Item,
							   TFunctionRef<double()> Random, FDWEquipmentInstance &Out, FString &Error, int32 Index,
							   FName Family)
{
	if (!Item.Affixes.Num())
	{
		Error = TEXT("普通装备没有可洗炼的词条");
		return false;
	}
	if (Index != INDEX_NONE && !Item.Affixes.IsValidIndex(Index))
	{
		Error = TEXT("只能洗炼随机词条");
		return false;
	}
	auto Candidates = Pool(Library);
	if (!Family.IsNone())
		Candidates.RemoveAll([&](const auto &Def) {
			TSet<FName> Ids;
			for (const auto &E : Def.Traits)
				EquipmentIds(E, Ids);
			for (FName Id : Ids)
				if (Library.Tags.FamiliesOf(Id).Contains(Family))
					return false;
			return true;
		});
	TArray<FDWEquipmentAffix> Excluded;
	if (Item.Legendary.IsSet())
		Excluded.Add(Item.Legendary.GetValue());
	Excluded.Append(Item.AdditionalLegendary);
	if (Index != INDEX_NONE)
		for (int32 I = 0; I < Item.Affixes.Num(); ++I)
			if (I != Index)
				Excluded.Add(Item.Affixes[I]);
	TArray<FDWEquipmentAffix> Rolled;
	if (!Sample(Library, Candidates, Index == INDEX_NONE ? Item.Affixes.Num() : 1, Random, Excluded, Rolled, Error,
				Item.Tier))
		return false;
	FDWEquipmentInstance Next = Item;
	if (Index == INDEX_NONE)
		Next.Affixes = MoveTemp(Rolled);
	else
		Next.Affixes[Index] = MoveTemp(Rolled[0]);
	Out = MoveTemp(Next);
	return true;
}
bool FDWEquipmentRules::AddAffixes(const FDWRuleLibrary &Library, const FDWEquipmentInstance &Item,
								   int32 Count, TFunctionRef<double()> Random, FDWEquipmentInstance &Out,
								   FString &Error, FName Family)
{
	if (Count < 1 || FDWPawnEquipmentRules::IsFixedSlot(Item.Slot))
	{
		Error = TEXT("invalid affix append target or count");
		return false;
	}
	auto Candidates = Pool(Library);
	if (!Family.IsNone())
		Candidates.RemoveAll([&](const auto &Def) {
			TSet<FName> Ids;
			for (const auto &E : Def.Traits)
				EquipmentIds(E, Ids);
			for (FName Id : Ids)
				if (Library.Tags.FamiliesOf(Id).Contains(Family))
					return false;
			return true;
		});
	auto Excluded = Item.Affixes;
	if (Item.Legendary.IsSet())
		Excluded.Add(Item.Legendary.GetValue());
	Excluded.Append(Item.AdditionalLegendary);
	TArray<FDWEquipmentAffix> Added;
	if (!Sample(Library, Candidates, Count, Random, Excluded, Added, Error, Item.Tier))
		return false;
	Out = Item;
	Out.Affixes.Append(MoveTemp(Added));
	return true;
}
bool FDWEquipmentRules::RemoveAffixes(const FDWEquipmentInstance& Item, int32 Count,
	TFunctionRef<double()> Random, FDWEquipmentInstance& Out, FString& Error)
{
	if (!Item.bIdentified || FDWPawnEquipmentRules::IsFixedSlot(Item.Slot) || Count < 1 || Count > Item.Affixes.Num())
	{
		Error = TEXT("没有足够的可移除随机词缀");
		return false;
	}
	TSet<int32> Seen;
	for (int32 Target : Item.ReforgeTargets)
	{
		if (!Item.Affixes.IsValidIndex(Target) || Seen.Contains(Target) || Seen.Num() >= 3)
		{
			Error = TEXT("装备洗炼记录无效");
			return false;
		}
		Seen.Add(Target);
	}
	FDWEquipmentInstance Next = Item;
	for (int32 N = 0; N < Count; ++N)
	{
		const int32 Index = EquipmentInteger(Random, 0, Next.Affixes.Num() - 1);
		for (int32 Grade = 0; Grade < Next.ReforgeTargets.Num(); ++Grade)
		{
			if (Next.ReforgeTargets[Grade] == Index)
			{
				// Grade bindings are sequential. Retain the valid earlier grades;
				// never silently assign a surviving line to a different currency grade.
				Next.ReforgeTargets.SetNum(Grade);
				break;
			}
			if (Next.ReforgeTargets[Grade] > Index) --Next.ReforgeTargets[Grade];
		}
		Next.Affixes.RemoveAt(Index);
	}
	Out = MoveTemp(Next);
	return true;
}
bool FDWEquipmentRules::RollAffixes(const FDWRuleLibrary& Library, const FDWEquipmentInstance& Item,
	int32 Count, TFunctionRef<double()> Random, FDWEquipmentInstance& Out, FString& Error, FName Family)
{
	// Use the same pool, family filtering and conflict rules as append, excluding only
	// the fixed legendary line. Old random lines and their currency bindings are replaced.
	FDWEquipmentInstance Next = Item;
	Next.Affixes.Reset();
	Next.ReforgeTargets.Reset();
	return AddAffixes(Library, Next, Count, Random, Out, Error, Family);
}
FDWCollection FDWEquipmentRules::Document(const FDWEquipmentInstance &Item, const FDWRuleLibrary *Library)
{
	FDWCollection D;
	D.Id = FName(*Item.Uid);
	D.Name = Item.Name;
	D.Slot = Item.Slot;
	D.Rarity = Item.Rarity;
	D.Level = Item.Level;
	D.Tier = Item.Tier;
	if (Library)
		if (const auto *Template = Library->Equipment.Find(Item.BaseId))
		{
			D.Tags = Template->Tags;
			D.Description = Template->Description;
		}
	if (!Item.Skill.IsNone())
		D.Skills.Add(Item.Skill);
	for (const auto &Base : Item.Bases) D.Traits.Append(Base.Traits);
	if (Item.Legendary.IsSet())
		D.Traits.Append(Item.Legendary->Traits);
	for (const auto& A : Item.AdditionalLegendary) D.Traits.Append(A.Traits);
	for (const auto &A : Item.Affixes)
		D.Traits.Append(A.Traits);
	return D;
}
FName FDWEquipmentRules::RandomRarity(TFunctionRef<double()> Random)
{
	const double Value = FDWTraitValues::Unit(Random());
	return Value < .5 ? TEXT("common") : Value < .8 ? TEXT("magic") : Value < .98 ? TEXT("rare") : TEXT("legendary");
}
bool FDWEquipmentRules::Drop(const FDWRuleLibrary &Library, TFunctionRef<double()> Random, double Chance,
							 const FString &Uid, FDWEquipmentInstance &Out, FString &Error)
{
	if (FDWTraitValues::Unit(Random()) >= Chance || !Library.Equipment.Num())
		return false;
	TArray<FName> Ids;
	Library.Equipment.GetKeys(Ids);
	Ids.RemoveAll([&](FName Id) { return FDWPawnEquipmentRules::IsFixedSlot(Library.Equipment[Id].Slot); });
	if (Ids.IsEmpty())
		return false;
	const auto &Template = Library.Equipment[Ids[EquipmentInteger(Random, 0, Ids.Num() - 1)]];
	const FName Rarity = RandomRarity(Random);
	const int32 Tier = EquipmentInteger(Random, 1, TraitTierCount);
	return Roll(Library, Template, Rarity, Random, Uid, Out, Error, Tier);
}
FName FDWEquipmentRules::OmenFamily(const FDWRuleLibrary &Library, const FDWCollection &Item)
{
	if (Item.Category != TEXT("omen"))
		return NAME_None;
	const auto Families = Library.Tags.CollectionFamilies(Item.Tags);
	return Families.Num() == 1 ? Families[0] : NAME_None;
}
} // namespace DWRules
