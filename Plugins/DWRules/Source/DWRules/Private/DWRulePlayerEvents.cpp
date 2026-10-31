#include "DWRuleWorldSubsystem.h"
#include "DWRulePawnEquipment.h"

using namespace DWRules;
bool UDWRuleWorldSubsystem::IsPlayerItemLocked(const FString& Uid) const {return PlayerEventLocks.Contains(Uid);}
bool UDWRuleWorldSubsystem::LockPlayerEventItem(const FString& Uid,const FString& Token,FString& Error)
{
 if(!Player||Token.IsEmpty()){Error=TEXT("祭坛尚未准备好");return false;}
 const auto* Item=PlayerProfile.Loadout.Instances.FindByPredicate([&](const auto& I){return I.Uid==Uid;});
 if(!Item||!Item->bIdentified||FDWPawnEquipmentRules::IsFixedSlot(Item->Slot)){Error=TEXT("请放入一件已鉴定的背包装备");return false;}
 if(PlayerEventLocks.Contains(Uid)){Error=TEXT("这件装备已被祭坛锁定");return false;}
 if(PlayerProfile.Loadout.Equipment.FindRef(Item->Slot)==FName(*Uid)){Error=TEXT("请先卸下这件装备，再放入祭坛");return false;}
 PlayerEventLocks.Add(Uid,Token);Error.Reset();return true;
}
bool UDWRuleWorldSubsystem::UnlockPlayerEventItem(const FString& Uid,const FString& Token,FString& Error)
{
 const auto* Lock=PlayerEventLocks.Find(Uid);
 if(!Lock||*Lock!=Token){Error=TEXT("装备不属于此祭坛");return false;}
 PlayerEventLocks.Remove(Uid);Error.Reset();return true;
}
bool UDWRuleWorldSubsystem::CraftPlayerEventItem(const FString& Uid,const FString& Token,FName Currency,int32 Index,FString& Error)
{
 const auto* Lock=PlayerEventLocks.Find(Uid);
 if(!Lock||*Lock!=Token){Error=TEXT("请先放入并锁定本祭坛的装备");return false;}
 const auto* Money=Host?Host->Library.Items.Find(Currency):nullptr;
 if(!Money||Money->Category!=TEXT("currency")||(Money->CraftOp!=TEXT("reroll")&&Money->CraftOp!=TEXT("roll_affixes")&&Money->CraftOp!=TEXT("add_affixes")&&Money->CraftOp!=TEXT("remove_affixes")))
 {Error=TEXT("本祭坛通货不可用");return false;}
 auto Next=PlayerProfile;const auto* Owned=Next.Loadout.Owned.Find(Currency);const bool Had=Owned!=nullptr;const int32 Count=Owned?*Owned:0;const auto Belt=Next.Belt;
 // Supply one transaction-local charge, then restore the real inventory exactly.
 // No currency is minted and ordinary crafting retains its payment rules.
 Next.Loadout.Owned.Add(Currency,1);
 if(!Next.Craft(Host->Library,Uid,Currency,NAME_None,Index,Error))return false;
 if(Had)Next.Loadout.Owned.Add(Currency,Count);else Next.Loadout.Owned.Remove(Currency);Next.Belt=Belt;
 return ApplyPlayerProfile(MoveTemp(Next),Error);
}
bool UDWRuleWorldSubsystem::CommitPlayerEventItems(const TArray<FString>& Consumed,const TArray<FDWEquipmentInstance>& Granted,const FString& Token,FString& Error)
{
 if(!Player){Error=TEXT("玩家尚未准备好");return false;}
 auto Next=PlayerProfile;TSet<FString> Seen;
 for(const auto& Uid:Consumed)
 {
  const auto* Lock=PlayerEventLocks.Find(Uid);
  if(!Lock||*Lock!=Token||Seen.Contains(Uid)||!Next.Loadout.Instances.ContainsByPredicate([&](const auto& I){return I.Uid==Uid;}))
  {Error=TEXT("祭坛材料不完整或已被使用");return false;}
  Seen.Add(Uid);
 }
 for(const auto& I:Granted)
 {
  if(I.Uid.IsEmpty()||Seen.Contains(I.Uid)||Next.Loadout.Instances.ContainsByPredicate([&](const auto& V){return V.Uid==I.Uid;}))
  {Error=TEXT("祭坛产物重复或无效");return false;}
  Seen.Add(I.Uid);
 }
 Next.Loadout.Instances.RemoveAll([&](const auto& I){return Consumed.Contains(I.Uid);});
 Next.Loadout.Instances.Append(Granted);if(Next.bProgression)Next.Drops+=Granted.Num();
 Next.Loadout.Validate(Host->Library);
 if(!ApplyPlayerProfile(MoveTemp(Next),Error))return false;
 for(const auto& Uid:Consumed)PlayerEventLocks.Remove(Uid);
 return true;
}
