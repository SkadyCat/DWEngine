#include "DWRuleProfile.h"
#include "DWRulePawnEquipment.h"
#include <limits>

namespace DWRules
{
void FDWRuleProfile::Reset(const FDWRuleLibrary &Library)
{
 *this = FDWRuleProfile();
 // resetPlayer starts the existing progression kit, using the catalogue's weapon when supplied.
 for(const auto& Key:Library.Shop.Starter)
 {
  FString Kind,Text; Key.Split(TEXT(":"),&Kind,&Text);
  if(Kind==TEXT("equipment")) if(const auto* D=Library.Equipment.Find(FName(*Text));D&&D->Slot==TEXT("weapon")) { StarterWeapon=D->Id; break; }
 }
 FString StartupError; if(!StartProgression(Library,StartupError)) return;
 Stones = Library.Shop.StartingStones;
 for (const auto &Key : Library.Shop.Starter)
	{
		FString Kind, Text;
		Key.Split(TEXT(":"), &Kind, &Text);
		const FName Id(*Text);
  if(Kind==TEXT("equipment")&&Id==StarterWeapon) continue;
		if (Kind == TEXT("skill") && Library.Builds.Contains(Id))
			Loadout.Owned.Add(Id, 1);
		else if (Kind == TEXT("item"))
		{
			const auto *D = Library.Items.Find(Id);
			if (!D)
				continue;
			Loadout.Owned.FindOrAdd(Id) += 3;
			if (D->Category == TEXT("consumable") && Belt.Num() < 4)
				Belt.AddUnique(Id);
		}
		else if (Kind == TEXT("equipment"))
		{
			const auto *D = Library.Equipment.Find(Id);
			if (!D)
				continue;
			OwnedEquipment.FindOrAdd(Id)++;
			if (!Loadout.Equipment.Contains(D->Slot))
				Loadout.Equipment.Add(D->Slot, Id);
		}
	}
	bInitialized = true;
}
bool FDWRuleProfile::StartProgression(const FDWRuleLibrary &Library, FString &Error)
{
	const auto *Template = Library.Equipment.Find(StarterWeapon);
	if (!Template || Template->Slot != TEXT("weapon") || !Template->Skills.Num())
	{
		Error = TEXT("starter weapon must grant a skill");
		return false;
	}
	FDWEquipmentInstance Item;
	FDWEquipmentRandom StarterRandom(1);
	if (!FDWEquipmentRules::Roll(
			Library, *Template, TEXT("common"), [&]() { return StarterRandom.Next(); }, TEXT("starter_weapon"), Item, Error,
			1))
		return false;
	const FName WeaponId=StarterWeapon;
	*this = FDWRuleProfile();
 StarterWeapon=WeaponId;
	bInitialized = bProgression = true;
	Loadout.Instances.Add(Item);
	Loadout.Equipment.Add(TEXT("weapon"), FName(*Item.Uid));
	Loadout.Skills[0] = Item.Skill;
	return true;
}
bool FDWRuleProfile::ApplyPreset(const FDWRuleLibrary &Library, const FDWRuleProfile &Preset, FString &Error)
{
	auto Next = *this;
	for (const auto &P : Preset.Loadout.Owned)
		Next.Loadout.Owned.FindOrAdd(P.Key) = FMath::Max(Next.Loadout.Owned.FindRef(P.Key), P.Value);
	for (const auto &P : Preset.OwnedEquipment)
		Next.OwnedEquipment.FindOrAdd(P.Key) = FMath::Max(Next.OwnedEquipment.FindRef(P.Key), P.Value);
	for (const auto &I : Preset.Loadout.Instances)
		if (!Next.Loadout.Instances.ContainsByPredicate([&](const auto &Old) { return Old.Uid == I.Uid; }))
			Next.Loadout.Instances.Add(I);
	Next.Loadout.Skills = Preset.Loadout.Skills;
	Next.Loadout.Gems = Preset.Loadout.Gems;
	Next.Loadout.Equipment = Preset.Loadout.Equipment;
	Next.Belt = Preset.Belt;
	Next.Loadout.Validate(Library);
	for (int32 I = 0; I < Next.Loadout.Skills.Num(); ++I)
		if (!Next.Loadout.Skills[I].IsNone())
		{
			FDWBuildSource S;
			TArray<FDWContribution> Rows;
			if (!Next.Loadout.Compose(Library, I, S, Rows, Error))
				return false;
		}
	*this = MoveTemp(Next);
	return true;
}
bool FDWRuleProfile::Buy(const FDWRuleLibrary &Library, const FString &Key, FString &Error, TOptional<int64> PriceOverride)
{
	const auto Listings = FDWShopRules::Listings(Library);
	const auto *Listing = Listings.FindByPredicate([&](const auto &L) { return L.Key == Key; });
	if (!Listing)
	{
		Error = TEXT("unknown or hidden shop listing");
		return false;
	}
	if (Listing->Kind == TEXT("skill") && Loadout.Owned.FindRef(Listing->Id) > 0)
	{
		Error = TEXT("已拥有该技能");
		return false;
	}
	if (Listing->Kind == TEXT("item") && Loadout.Owned.FindRef(Listing->Id) >= Library.Items[Listing->Id].Stack)
	{
		Error = TEXT("物品已达到堆叠上限");
		return false;
	}
 const int64 Price=PriceOverride.IsSet()?PriceOverride.GetValue():Listing->Price;
 if(Price<0) { Error=TEXT("价格无效"); return false; }
 if (Stones < Price) { Error=TEXT("灵石不足"); return false; }
 auto Stream=Random; FDWEquipmentInstance Rolled;
 if(Listing->Kind==TEXT("equipment"))
 {
  const auto* Template=Library.Equipment.Find(Listing->Id);
  if(!Template) { Error=TEXT("装备底材不存在"); return false; }
  // Existing web rollEquipmentDrop acquisition order: chance, selection, then tier.
  Stream.Next(); Stream.Next();
  const int32 Tier=1+FMath::Min(Template->Tier-1,int32(FMath::FloorToDouble(Stream.Next()*Template->Tier)));
  if(!FDWEquipmentRules::Roll(Library,*Template,Template->Rarity,[&](){return Stream.Next();},FString(),Rolled,Error,Tier)) return false;
 }
 Stones-=Price; Spent+=Price;
 if(Listing->Kind==TEXT("equipment")) { Random=Stream; Loadout.Instances.Add(MoveTemp(Rolled)); }
 else Loadout.Owned.FindOrAdd(Listing->Id)++;

	return true;
}
bool FDWRuleProfile::Equip(const FDWRuleLibrary &Library, FName Slot, FName Id, FString &Error)
{
	if (Id.IsNone() || Loadout.Equipment.FindRef(Slot) == Id)
	{
		Loadout.Equipment.Remove(Slot);
		Loadout.Validate(Library);
		return true;
	}
	FName ActualSlot;
	if (const auto *Item = Library.Equipment.Find(Id); Item && OwnedEquipment.FindRef(Id) > 0)
		ActualSlot = Item->Slot;
	else if (const auto *Instance =
				 Loadout.Instances.FindByPredicate([&](const auto &I) { return FName(*I.Uid) == Id; }))
		{
		if (!Instance->bIdentified) { Error=TEXT("请先鉴定装备"); return false; }
		ActualSlot = Instance->Slot;
	}
	if (ActualSlot.IsNone() || ActualSlot != Slot)
	{
		Error = TEXT("装备不属于此槽位或尚未拥有");
		return false;
	}
	Loadout.Equipment.Add(Slot, Id);
	Loadout.Validate(Library);
	return true;
}
bool FDWRuleProfile::Identify(const FDWRuleLibrary &Library, FName Id, FString &Uid, FString &Error)
{
	if (auto* Existing = Loadout.Instances.FindByPredicate([&](const auto& I){ return FName(*I.Uid)==Id; }))
	{
		if (Existing->bIdentified) { Error=TEXT("装备已鉴定"); return false; }
		const auto* Base=Library.Equipment.Find(Existing->BaseId); if(!Base) { Error=TEXT("装备模板不存在"); return false; }
		auto Stream=Random; FDWEquipmentInstance Rolled;
		if(!FDWEquipmentRules::Roll(Library,*Base,Existing->Rarity,[&](){return Stream.Next();},Existing->Uid,Rolled,Error,Existing->Tier)) return false;
		Uid=Rolled.Uid; *Existing=MoveTemp(Rolled); Random=Stream; return true;
	}
	const auto *Template = Library.Equipment.Find(Id);
	if (!Template || OwnedEquipment.FindRef(Id) <= 0)
	{
		Error = TEXT("背包里没有这件装备");
		return false;
	}
	FDWEquipmentInstance Item;
	if (!FDWEquipmentRules::Roll(
			Library, *Template, Template->Rarity, [&]() { return Random.Next(); }, FString(), Item, Error))
		return false;
	if (--OwnedEquipment[Id] <= 0)
	{
		OwnedEquipment.Remove(Id);
		for (auto &E : Loadout.Equipment)
			if (E.Value == Id)
				E.Value = FName(*Item.Uid);
	}
	Uid = Item.Uid;
	Loadout.Instances.Add(MoveTemp(Item));
	return true;
}
bool FDWRuleProfile::Craft(const FDWRuleLibrary &Library, const FString &Uid, FName Currency, FName Omen, int32 Index,
						   FString &Error)
{
	auto *Item = Loadout.Instances.FindByPredicate([&](const auto &I) { return I.Uid == Uid; });
	const auto *Money = Library.Items.Find(Currency);
	if (!Item || !Item->bIdentified || !Money || Money->Category != TEXT("currency") || Loadout.Owned.FindRef(Currency) <= 0)
	{
		Error = TEXT("装备或通货不可用");
		return false;
	}
	if (FDWPawnEquipmentRules::IsFixedSlot(Item->Slot))
	{
		Error = TEXT("固定装备槽不能加工词条");
		return false;
	}
	if (Money->CraftOp == TEXT("reroll"))
	{
		TSet<int32> Seen;
		for (int32 Target : Item->ReforgeTargets)
		{
			if (!Item->Affixes.IsValidIndex(Target) || Seen.Contains(Target))
			{
				Error = TEXT("装备洗炼记录无效");
				return false;
			}
			Seen.Add(Target);
		}
		const int32 Grade = Money->CraftGrade;
		if (Seen.Num() > 3 || Grade < 1 || Grade > 3 || !Item->Affixes.IsValidIndex(Index))
		{
			Error = TEXT("请选择要洗炼的随机词条，灵石品级必须为1–3");
			return false;
		}
		const int32 Bound = Item->ReforgeTargets.Find(Index);
		if (Bound != INDEX_NONE ? Bound + 1 != Grade : Grade != Seen.Num() + 1)
		{
			Error = TEXT("灵石必须按下、中、上品依次绑定不同词条，绑定后只能洗炼对应词条");
			return false;
		}
	}
	if (Money->CraftOp == TEXT("remove_affixes") && !Omen.IsNone())
	{
		Error = TEXT("移除词缀不能搭配预兆");
		return false;
	}
	FName Family;
	if (!Omen.IsNone())
	{
		const auto *D = Library.Items.Find(Omen);
		Family = D ? FDWEquipmentRules::OmenFamily(Library, *D) : NAME_None;
		if (Family.IsNone() || Loadout.Owned.FindRef(Omen) <= 0)
		{
			Error = TEXT("预兆不可用");
			return false;
		}
	}
	FDWEquipmentInstance Next;
	// Roll against a copy of the random stream too: failed operations are atomic.
	auto RollRandom = Random;
	const auto NextRandom = [&]() { return RollRandom.Next(); };
	if (Money->CraftOp == TEXT("add_affixes") || Money->CraftOp == TEXT("roll_affixes") || Money->CraftOp == TEXT("remove_affixes"))
	{
		if (Index != INDEX_NONE)
		{
			Error = TEXT("该通货作用于整件装备");
			return false;
		}
		const bool Rolled = Money->CraftOp == TEXT("remove_affixes")
			? FDWEquipmentRules::RemoveAffixes(*Item, Money->CraftCount, NextRandom, Next, Error)
			: Money->CraftOp == TEXT("roll_affixes")
			? FDWEquipmentRules::RollAffixes(Library, *Item, Money->CraftCount, NextRandom, Next, Error, Family)
			: FDWEquipmentRules::AddAffixes(Library, *Item, Money->CraftCount, NextRandom, Next, Error, Family);
		if (!Rolled)
			return false;
	}
	else if (Money->CraftOp != TEXT("reroll") ||
			 !FDWEquipmentRules::Reroll(Library, *Item, NextRandom, Next, Error, Index, Family))
		return false;
	if (Money->CraftOp == TEXT("reroll")) Next.ReforgeTargets.AddUnique(Index);
	Random = RollRandom;
	*Item = MoveTemp(Next);
	Consume(Currency);
	if (!Omen.IsNone())
		Consume(Omen);
	return true;
}
bool FDWRuleProfile::Consume(FName Id)
{
	if (Loadout.Owned.FindRef(Id) <= 0)
		return false;
	if (--Loadout.Owned[Id] <= 0)
	{
		Loadout.Owned.Remove(Id);
		for (FName& Existing : Belt) if (Existing == Id) Existing = NAME_None;
	}
	return true;
}
void FDWRuleProfile::RecordKill(int64 Reward)
{ if(Reward>0) { Stones+=Reward; Earned+=Reward; } ++Kills; }
bool FDWRuleProfile::AwardKill(const FDWRuleLibrary &Library, double MaxHealth, FString &Error, bool bDropEquipment)
{
	const int64 Reward = FMath::Max<int64>(1, int64(FMath::FloorToDouble(2 + MaxHealth / 40 + .5)));
	RecordKill(Reward);
	if (!bDropEquipment) return true;
	FDWEquipmentInstance Item;
	if (FDWEquipmentRules::Drop(Library, [&]() { return Random.Next(); }, .3, FString(), Item, Error))
	{
		Loadout.Instances.Add(MoveTemp(Item));
		if (bProgression)
			++Drops;
		return true;
	}
	return false;
}
void FDWRuleProfile::Discard(const FString &Uid)
{
	Loadout.Instances.RemoveAll([&](const auto &I) { return I.Uid == Uid; });
	for (auto It = Loadout.Equipment.CreateIterator(); It; ++It)
		if (It.Value() == FName(*Uid))
			It.RemoveCurrent();
}
bool FDWRuleProfile::SetBeltSlot(const FDWRuleLibrary& Library,int32 Slot,FName Id,FString& Error)
{
 if(Slot<0 || Slot>=4) { Error=TEXT("消耗品槽必须为 1–4"); return false; }
 if(!Id.IsNone()) { const auto* D=Library.Items.Find(Id); if(!D || D->Category!=TEXT("consumable") || Loadout.Owned.FindRef(Id)<=0) { Error=TEXT("请先拥有该消耗品"); return false; } }
 // Q/E/T/G are fixed positions. Empty or depleted slots must not shift neighbours.
 Belt.SetNum(4);
 if(!Id.IsNone())for(FName& Existing:Belt)if(Existing==Id)Existing=NAME_None;
 Belt[Slot]=Id;return true;
}
bool FDWRuleProfile::QuoteSale(const FDWRuleLibrary& Library,const FString& Id,int64& Price,FString& Error) const
{
 const auto* Instance=Loadout.Instances.FindByPredicate([&](const auto& I){return I.Uid==Id;});
 const auto* Template=Library.Equipment.Find(Instance?Instance->BaseId:FName(*Id));
 if(!Template || (!Instance && OwnedEquipment.FindRef(FName(*Id))<=0)) { Error=TEXT("背包里没有这件装备"); return false; }
 const FName Slot=Instance?Instance->Slot:Template->Slot;
 if(FDWPawnEquipmentRules::IsFixedSlot(Slot) || !FDWPawnEquipmentRules::IsSlot(Slot,EPawnKind::Player)) { Error=TEXT("固定装备不能出售"); return false; }
 const FName Rarity=Instance?Instance->Rarity:Template->Rarity;
 static const TMap<FName,int32> Rates{{TEXT("common"),1},{TEXT("magic"),2},{TEXT("rare"),4},{TEXT("legendary"),8},{TEXT("unique"),10}};
 const int32* Rate=Rates.Find(Rarity); const int32 Tier=Instance?Instance->Tier:Template->Tier;
 if(!Rate || Tier<1 || Tier>TraitTierCount) { Error=TEXT("装备品质或T阶无效"); return false; }
 Price=int64(*Rate)*Tier;
 if(const auto* Configured=Library.Shop.Prices.Find(TEXT("equipment:")+Template->Id.ToString())) { if(*Configured<0) { Error=TEXT("装备价格无效"); return false; } Price=FMath::Min(Price,*Configured/4); }
 return true;
}
bool FDWRuleProfile::Sell(const FDWRuleLibrary& Library,const FString& Id,int64& Price,FString& Error)
{
 if(!QuoteSale(Library,Id,Price,Error)) return false;
 if(Stones>std::numeric_limits<int64>::max()-Price || Earned>std::numeric_limits<int64>::max()-Price) { Error=TEXT("灵石数量超出上限"); return false; }
 const bool Instance=Loadout.Instances.ContainsByPredicate([&](const auto& I){return I.Uid==Id;}); bool Remove=true;
 if(Instance) Loadout.Instances.RemoveAll([&](const auto& I){return I.Uid==Id;});
 else { auto* Count=OwnedEquipment.Find(FName(*Id)); if(!Count || *Count<=0) { Error=TEXT("装备数量无效"); return false; } if(--*Count<=0) OwnedEquipment.Remove(FName(*Id)); else Remove=false; }
 if(Remove) for(auto It=Loadout.Equipment.CreateIterator();It;++It) if(It.Value()==FName(*Id)) It.RemoveCurrent();
 Loadout.Validate(Library); Stones+=Price; Earned+=Price; return true;
}
} // namespace DWRules
