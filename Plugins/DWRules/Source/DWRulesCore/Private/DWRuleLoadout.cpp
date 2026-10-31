#include "DWRuleLoadout.h"
#include "DWRulePawnEquipment.h"
#include <limits>

namespace DWRules
{
FDWRuleLoadout::FDWRuleLoadout()
{
	Skills.SetNum(ActiveSkillSlotCount);
}
TArray<FDWAvailableSkill> FDWRuleLoadout::AvailableSkills(const FDWRuleLibrary &Library) const
{
	TArray<FDWAvailableSkill> Out;
	TSet<FName> Seen;
	TSet<FName> EquipmentOnly;
    for(const auto& Pair:Library.Traits)for(const auto& E:Pair.Value.Source.Sheet)if(!E.GrantSkill.IsNone())EquipmentOnly.Add(E.GrantSkill);
	auto Add = [&](FName Id, bool Granted) {
		if(!Granted&&EquipmentOnly.Contains(Id))return;
		if (Seen.Contains(Id))
			return;
		if (const auto *Build = Library.Builds.Find(Id))
		{
			FDWBuildSource Source;TArray<FDWContribution> Rows;FString Error;
            if(!Library.Aggregate(Id,{},std::numeric_limits<double>::max(),Source,Rows,Error))return;
            for(const auto& Trait:Source.Traits)for(const auto& E:Trait.Sheet)
                if(E.Trigger==TEXT("aura_skill")||E.Trigger==TEXT("trigger_skill"))return;
			Seen.Add(Id);
			Out.Add({Id, Build->DisplayName, Granted});
		}
	};
	for (const auto &Pair : Owned)
		if (Pair.Value != 0)
			Add(Pair.Key, false);
	for (const auto &Part : EquipmentParts(Library))
		for (FName Skill : Part.Collection.Skills)
        {
			Add(Skill, true);
            FDWBuildSource Bound;TArray<FDWContribution> Rows;FString Error;
            if(Library.Aggregate(Skill,{},std::numeric_limits<double>::max(),Bound,Rows,Error))
                for(const auto& T:Bound.Traits)for(const auto& E:T.Sheet)if(!E.GrantSkill.IsNone())Add(E.GrantSkill,true);
        }
    FDWBuildSource Source;TArray<FDWContribution> Contributions;FString Error;
    if(Library.Aggregate(NAME_None,EquipmentParts(Library),std::numeric_limits<double>::max(),Source,Contributions,Error))
        for(const auto& T:Source.Traits)for(const auto& E:T.Sheet)if(!E.GrantSkill.IsNone())Add(E.GrantSkill,true);
	return Out;
}
void FDWRuleLoadout::Validate(const FDWRuleLibrary &Library)
{
	Skills.SetNum(ActiveSkillSlotCount);
	const auto Choices = AvailableSkills(Library);
	TSet<FName> Seen;
	for (auto &Id : Skills)
	{
		if (Id.IsNone() || Seen.Contains(Id) || !Choices.ContainsByPredicate([&](const auto &S) { return S.Id == Id; }))
			Id = NAME_None;
		else
			Seen.Add(Id);
	}
	TMap<FName, int32> Used;
	Gems.RemoveAll([&](const auto &Row) { return !Library.Builds.Contains(Row.Skill); });
	for (auto &Row : Gems)
	{
		Row.Slots.SetNum(SkillGemSlotCount);
		for (auto &Id : Row.Slots)
		{
			const auto *Item = Library.Items.Find(Id);
			const int32 Count = Used.FindRef(Id);
			if (!Item || Item->Category != TEXT("gem") || Count >= FMath::Max(0, Owned.FindRef(Id)))
				Id = NAME_None;
			else
				Used.Add(Id, Count + 1);
		}
	}
}
bool FDWRuleLoadout::ChangeSkill(const FDWRuleLibrary &Library, int32 Slot, FName Value, FString &Error)
{
	if (Slot < 0 || Slot >= ActiveSkillSlotCount)
	{
		Error = TEXT("主动技能槽必须为 1–4");
		return false;
	}
	const auto Choices = AvailableSkills(Library);
	if (!Value.IsNone() && !Choices.ContainsByPredicate([&](const auto &S) { return S.Id == Value; }))
	{
		Error = TEXT("请先拥有该技能或装备授予该技能的物品");
		return false;
	}
	FDWRuleLoadout Next = *this;
	Next.Validate(Library);
	for (auto &Id : Next.Skills)
		if (Id == Value)
			Id = NAME_None;
	Next.Skills[Slot] = Value;
	*this = MoveTemp(Next);
	return true;
}
int32 FDWRuleLoadout::SocketedCount(FName Gem, FName ExceptSkill, int32 ExceptSlot) const
{
	int32 Count = 0;
	for (const auto &Row : Gems)
		for (int32 I = 0; I < FMath::Min(Row.Slots.Num(), SkillGemSlotCount); ++I)
			if (Row.Slots[I] == Gem && !(Row.Skill == ExceptSkill && I == ExceptSlot))
				++Count;
	return Count;
}
int32 FDWRuleLoadout::AvailableGems(FName Gem, FName ExceptSkill, int32 ExceptSlot) const
{
	return FMath::Max(0, FMath::Max(0, Owned.FindRef(Gem)) - SocketedCount(Gem, ExceptSkill, ExceptSlot));
}
bool FDWRuleLoadout::ChangeGem(const FDWRuleLibrary &Library, FName Skill, int32 Slot, FName Value, FString &Error)
{
	if (!Library.Builds.Contains(Skill))
	{
		Error = TEXT("技能不存在");
		return false;
	}
	if (Slot < 0 || Slot >= SkillGemSlotCount)
	{
		Error = TEXT("宝石槽必须为 1–6");
		return false;
	}
	if (!Value.IsNone())
	{
		const auto *Item = Library.Items.Find(Value);
		if (!Item || Item->Category != TEXT("gem"))
		{
			Error = TEXT("只能镶嵌宝石类物品");
			return false;
		}
		if (!AvailableGems(Value, Skill, Slot))
		{
			Error = TEXT("没有可用的这颗宝石，请先卸下其他槽位中的宝石或前往商店购买");
			return false;
		}
	}
	auto *Row = Gems.FindByPredicate([&](const auto &R) { return R.Skill == Skill; });
	if (!Row)
	{
		FDWSkillGems New;
		New.Skill = Skill;
		Gems.Add(New);
		Row = &Gems.Last();
	}
	Row->Slots.SetNum(SkillGemSlotCount);
	Row->Slots[Slot] = Value;
	return true;
}
TArray<FDWCollectionPart> FDWRuleLoadout::EquipmentParts(const FDWRuleLibrary &Library) const
{
	TArray<FDWCollectionPart> Out;
	FString Error;
	if (!ResolveEquipment(Library, Out, Error))
		Out.Reset();
	return Out;
}
bool FDWRuleLoadout::ResolveEquipment(const FDWRuleLibrary &Library, TArray<FDWCollectionPart> &Out,
									  FString &Error) const
{
	auto Fixed = FDWPawnEquipmentRules::Defaults(EPawnKind::Player);
	for (const auto &Pair : Equipment)
		if (FDWPawnEquipmentRules::IsFixedSlot(Pair.Key))
			Fixed.Ids.Add(Pair.Key, Pair.Value);
	if (!FDWPawnEquipmentRules::Resolve(Library, Fixed, EPawnKind::Player, Out, Error))
		return false;
	for (const auto &Pair : Equipment)
	{
		if (FDWPawnEquipmentRules::IsFixedSlot(Pair.Key))
			continue;
		if (const auto *Item = Library.Equipment.Find(Pair.Value))
			Out.Add({*Item, true, Item->Name});
		else if (const auto *Instance =
					 Instances.FindByPredicate([&](const auto &Item) { return FName(*Item.Uid) == Pair.Value; }))
		{
			if (!Instance->bIdentified) continue;
			const auto InstanceDocument = FDWEquipmentRules::Document(*Instance, &Library);
			Out.Add({InstanceDocument, true, InstanceDocument.Name});
		}
	}
	Out.Append(PassiveParts);
    for(const auto& Group:RuntimeGroups)Out.Append(Group.Value);
	return true;
}
TArray<FDWCollectionPart> FDWRuleLoadout::GemParts(const FDWRuleLibrary &Library, FName Skill) const
{
	TArray<FDWCollectionPart> Out;
	const auto *Row = Gems.FindByPredicate([&](const auto &R) { return R.Skill == Skill; });
	if (!Row)
		return Out;
	for (int32 I = 0; I < FMath::Min(Row->Slots.Num(), SkillGemSlotCount); ++I)
		if (const auto *Item = Library.Items.Find(Row->Slots[I]))
			if (Item->Category == TEXT("gem"))
				Out.Add({*Item, false, FString::Printf(TEXT("宝石槽 %d · %s"), I + 1, *Item->Name)});
	return Out;
}
bool FDWRuleLoadout::Compose(const FDWRuleLibrary &Library, int32 Slot, FDWBuildSource &Source,
							 TArray<FDWContribution> &Contributions, FString &Error) const
{
	if (!Skills.IsValidIndex(Slot) || Skills[Slot].IsNone())
	{
		Error = TEXT("主动技能槽为空");
		return false;
	}
	const FName Skill = Skills[Slot];
	TArray<FDWCollectionPart> Parts;
	if (!ResolveEquipment(Library, Parts, Error))
		return false;
	Parts.Append(GemParts(Library, Skill));
	return Library.Aggregate(Skill, Parts, std::numeric_limits<double>::infinity(), Source, Contributions, Error);
}
} // namespace DWRules
